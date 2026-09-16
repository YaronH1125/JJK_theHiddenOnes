// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatHitComponent.h"

#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatTypes.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"
#include "Training/FighterDefinition.h"
#include "Training/TrainingGameMode.h"

namespace
{
	// 命中调试显示开关（T10：表现开关不影响结算）
	TAutoConsoleVariable<int32> CVarJJKDebugHitFX(
		TEXT("JJK.DebugHitFX"),
		0,
		TEXT("命中时绘制调试球（1=开，0=关）"),
		ECVF_Default);

}

UCombatHitComponent::UCombatHitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 仅在有效窗口期启用 Tick 采样（M2.3；窗口外零成本）
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatHitComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AFighterCharacter* F = GetOwnerFighter()) AddTickPrerequisiteComponent(F->GetMesh());
}

AFighterCharacter* UCombatHitComponent::GetOwnerFighter() const
{
	return Cast<AFighterCharacter>(GetOwner());
}

void UCombatHitComponent::SetWindowTickEnabled(bool bEnabled)
{
	SetComponentTickEnabled(bEnabled);
	if (bEnabled)
	{
		PrimaryComponentTick.TickInterval = 0.f;
	}
}

uint64 UCombatHitComponent::BeginAttack(const UAttackDefinition* Definition)
{
	++InstanceCounter;
	ActiveInstanceId = InstanceCounter;
	ActiveDefinition = Definition;
	bAttackActive = true;
	bWindowOpen = false;
	bHasLastSocketLocation = false;
	DedupKeys.Reset();
	HitCountThisAttack = 0;
	bCursedEnergyGranted = false;
	bHadContact = false;
	SegmentId = Definition ? Definition->SegmentId : 0;
	SegmentBeginTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 开始攻击实例 %llu（段 %d，伤害 %.0f）"),
		*GetNameSafe(GetOwner()), ActiveInstanceId, SegmentId, Definition ? Definition->Damage : 0.f);
	return ActiveInstanceId;
}

bool UCombatHitComponent::IsCancelWindowOpen() const
{
	const UAttackDefinition* Def = ActiveDefinition.Get();
	if (Def == nullptr || !bAttackActive)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - SegmentBeginTime);
	return Def->CancelWindowEndTime > 0.f && Elapsed >= Def->CancelWindowStartTime && Elapsed <= Def->CancelWindowEndTime;
}

float UCombatHitComponent::GetSegmentElapsedTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? static_cast<float>(World->GetTimeSeconds() - SegmentBeginTime) : 0.f;
}

void UCombatHitComponent::HandleAnimWindowNotify(bool bOpen, const UAnimSequenceBase* Animation)
{
	if (Animation && (!ActiveDefinition.IsValid() || !ActiveDefinition->bWindowFromAnimNotifies)) return;
	if (!bAttackActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 拒绝窗口通知（%s）：无活动攻击实例（旧动画遗留）"),
			*GetNameSafe(GetOwner()), bOpen ? TEXT("开") : TEXT("关"));
		return;
	}

	if (bOpen)
	{
		if (bWindowOpen)
		{
			return; // 重复开启按幂等处理
		}
		bWindowOpen = true;
		bHasLastSocketLocation = false;
		Phase = EAttackPhase::Active;
		SetComponentTickEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 实例 %llu 窗口开启（段 %d）"), *GetNameSafe(GetOwner()), ActiveInstanceId, SegmentId);
	}
	else
	{
		if (!bWindowOpen)
		{
			return;
		}
		CloseWindow();
	}
}

void UCombatHitComponent::CloseWindow()
{
	if (!bWindowOpen)
	{
		return;
	}
 if (!bHadContact) if (auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>()) GM->RecordContact(GetOwnerFighter(),nullptr,ETrainingContact::Whiff,0,0,0);
 bWindowOpen = false;
 Phase = EAttackPhase::Recovery;
	// 关闭窗口不清去重；只有 BeginAttack 为新段清空，避免重复通知绕过结果。
	UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 实例 %llu 窗口关闭（有效命中 %d）"), *GetNameSafe(GetOwner()), ActiveInstanceId, HitCountThisAttack);
}

void UCombatHitComponent::EndAttack()
{
	if (bWindowOpen)
	{
		CloseWindow();
	}
	bAttackActive = false;
	ActiveDefinition = nullptr;
	Phase = EAttackPhase::None;

	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter == nullptr || !Fighter->HasPendingCombatEvents())
	{
		SetComponentTickEnabled(false);
	}
}

void UCombatHitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 先扫掠（窗口期），再处理受击/死亡事件：
	// 保证同帧互中时双方已完成扫掠的接触都有效（M2.5 换血），被打断者未来接触失效
	if (bWindowOpen && bAttackActive)
	{
		ProcessSweep();
	}

	AFighterCharacter* Fighter = GetOwnerFighter();
	if (Fighter != nullptr)
	{
		Fighter->ProcessCombatEvents();
	}

	if (!bAttackActive && !bWindowOpen)
	{
		const bool bHasPending = Fighter != nullptr && Fighter->HasPendingCombatEvents();
		if (!bHasPending)
		{
			SetComponentTickEnabled(false);
		}
	}
}

void UCombatHitComponent::ProcessSweep()
{
	const AFighterCharacter* Owner = GetOwnerFighter();
	const UAttackDefinition* Def = ActiveDefinition.Get();
	const UWorld* World = GetWorld();
	if (Owner == nullptr || Def == nullptr || World == nullptr)
	{
		return;
	}

	const USkeletalMeshComponent* Mesh = Owner->GetMesh();
	if (Mesh == nullptr)
	{
		return;
	}

	const FVector SocketPos = Mesh->GetSocketLocation(Def->TraceSocket);
	TArray<FContactCandidate> Contacts;

	FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(JJKAttackSweep));
	QueryParams.AddIgnoredActor(Owner);

	FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Def->TraceRadius, 1.f));
	if (bHasLastSocketLocation)
	{
		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(Hits, LastSocketLocation, SocketPos, FQuat::Identity, ObjectParams, Shape, QueryParams);
		for (const FHitResult& Hit : Hits)
		{
			if (AFighterCharacter* Target = Cast<AFighterCharacter>(Hit.GetActor()))
			{
				Contacts.Push({Target, Hit.ImpactPoint});
			}
		}
	}
	else
	{
		// 首帧无扫掠起点，退化为当前球体重叠查询
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SocketPos, FQuat::Identity, ObjectParams, Shape, QueryParams);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AFighterCharacter* Target = Cast<AFighterCharacter>(Overlap.GetActor()))
			{
				Contacts.Push({Target, SocketPos});
			}
		}
	}

	LastSocketLocation = SocketPos;
	bHasLastSocketLocation = true;

	ApplyBatch(Contacts);
}

void UCombatHitComponent::ApplyBatch(const TArray<FContactCandidate>& Contacts)
{
	AFighterCharacter* Attacker = GetOwnerFighter();
	const UAttackDefinition* Def = ActiveDefinition.Get();
	if (Attacker == nullptr || Def == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* AttackerASC = Attacker->GetFighterAbilitySystemComponent();

	// 批次处理（M2.5）：先统一筛出本帧有效接触（验证 → 去重 → 死亡过滤），
	// 再按稳定次序（去重集合插入序，即本帧扫掠发现序）结算；
	// 受击/死亡中断进入受击方延迟队列，双方本帧已有效接触仍换血。
	struct FValidContact
	{
		AFighterCharacter* Target;
		FVector HitLocation;
	};
	TArray<FValidContact> ValidContacts;

	for (const FContactCandidate& Candidate : Contacts)
	{
		AFighterCharacter* Target = Candidate.Target.Get();
		if (Target == nullptr || Target == Attacker)
		{
			continue;
		}
		if (Target->IsPendingKillPending() || !Target->HasActorBegunPlay())
		{
			continue;
		}
		// 倒地保护与死亡：不进入普通连段结算，也不占用去重键
		if (Target->IsDead() || Target->IsThrowPaired() || Target->HasCombatTag(TAG_State_KnockedDown))
		{
			continue;
		}

		const FCombatHitDedupKey Key{ActiveInstanceId, SegmentId, Target};
		if (DedupKeys.Contains(Key))
		{
			continue; // 同实例同段同目标只结算一次
		}
		DedupKeys.Add(Key);
		bHadContact = true;
		ValidContacts.Push({Target, Candidate.HitLocation});
	}

	if (ValidContacts.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 实例 %llu 批次结算：有效接触 %d 个"),
		*GetNameSafe(Attacker), ActiveInstanceId, ValidContacts.Num());

	// 命中结果分类：普通命中 / 防御 / 免疫 / 投技（M3.3/M3.5）。
	// 被防住/免疫的接触同样占用去重键：同一窗口内后续采样不会绕过首次结果（T14）。
	for (const FValidContact& Contact : ValidContacts)
	{
		AFighterCharacter* Target = Contact.Target;
		UAbilitySystemComponent* TargetASC = Target->GetFighterAbilitySystemComponent();
		if (AttackerASC == nullptr || TargetASC == nullptr)
		{
			continue;
		}

		// 免疫（闪避无敌窗口）：无伤害、不受击
  if (Def->bDodgeable && Target->HasCombatTag(TAG_State_DodgeInvulnerable))
  {
   Target->NotifyDodgeAvoided();
   if (auto* GM=GetWorld()->GetAuthGameMode<ATrainingGameMode>()) GM->RecordContact(Attacker,Target,ETrainingContact::Immune,Def->Damage,0,0);
			UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 的接触被 %s 闪避免疫（实例 %llu）"),
				*GetNameSafe(Attacker), *GetNameSafe(Target), ActiveInstanceId);
			continue;
		}

		// 防御判定：目标防御意图生效、本攻击可被防御、且来向在目标正面弧内
		const bool bFront = Target->IsAttackFromFront(Attacker, Target->GetGuardFrontArcHalfAngle());
		const bool bGuarded = Target->IsGuarding() && Def->bBlockable && bFront;

		// 条件投技（M3.5）：可转投段 + 目标防御 + 距离/朝向/地面/配对占用检查
		if (bGuarded && Def->bCanThrow && Attacker->CanThrowTarget(Target, Def->bCanThrow))
		{
			const UFighterDefinition* AttackerDef = Attacker->GetDefinition();
			UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 对防御中的 %s 转投技（实例 %llu）"),
				*GetNameSafe(Attacker), *GetNameSafe(Target), ActiveInstanceId);
			if (Attacker->BeginThrowPair(Target, AttackerDef->ThrowConfig.PairDuration,
				AttackerDef->ThrowConfig.Damage, AttackerDef->ThrowConfig.PairDistance)) return;
			// 配对站位非法，继续原防御结果
		}

		if (bGuarded)
		{
			// 防御结果：无伤害，防御方进入防御硬直（防住结果占用去重键，后续采样不绕过）
			++HitCountThisAttack;
            const auto& Guard = Target->GetDefinition()->GuardConfig;
            Attacker->ApplyCombatDamage(Target,Def->Damage,Guard.bChipDamage ? Def->Damage * Guard.ChipDamageRatio : 0.f,ETrainingContact::Guard);
			FCombatEvent GuardEvent;
			GuardEvent.Type = FCombatEvent::EType::GuardStun;
			GuardEvent.Instigator = Attacker;
			GuardEvent.StunDuration = Def->GuardStunDuration;
			GuardEvent.HitLocation = Contact.HitLocation;
			Target->QueueCombatEvent(GuardEvent);
			UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 的攻击被 %s 防御（实例 %llu）"),
				*GetNameSafe(Attacker), *GetNameSafe(Target), ActiveInstanceId);
			continue;
		}

		if (!bCursedEnergyGranted) { Attacker->RestoreCursedEnergyOnHit(); bCursedEnergyGranted = true; }
		// 普通命中：伤害经 GE 生效（SetByCaller Data.Damage，负值扣减生命）
		Attacker->ApplyCombatDamage(Target,Def->Damage,Def->Damage,ETrainingContact::Hit);
		++HitCountThisAttack;

		if (CVarJJKDebugHitFX.GetValueOnGameThread() != 0 && GetWorld() != nullptr)
		{
			DrawDebugSphere(GetWorld(), Contact.HitLocation, Def->TraceRadius, 12, FColor::Red, false, 1.0f);
		}

		const bool bLethal = Target->GetFighterAttributeSet()->GetHealth() <= 0.f;
		UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 命中 %s（实例 %llu 段 %d，伤害 %.0f，累计命中 %d，致死=%d）"),
			*GetNameSafe(Attacker), *GetNameSafe(Target), ActiveInstanceId, SegmentId, Def->Damage, HitCountThisAttack, bLethal ? 1 : 0);

		// 受击/倒地进入受击方延迟队列：受击方自身下一 Tick 处理中断；
		// 本帧（同一批次窗口内）双方已有效接触仍换血，被打断后未来接触失效
		FCombatEvent Event;
		Event.Type = Def->bKnockdown ? FCombatEvent::EType::Knockdown : FCombatEvent::EType::HitReact;
		Event.Instigator = Attacker;
		Event.InterruptLevel = Def->InterruptLevel;
		Event.StunDuration = Def->HitStunDuration;
		Event.bLethal = bLethal;
		Event.HitLocation = Contact.HitLocation;
		Event.KnockbackStrength = Def->KnockbackStrength;
		Event.KnockbackDirection = (Target->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
		Target->QueueCombatEvent(Event);
	}
}
