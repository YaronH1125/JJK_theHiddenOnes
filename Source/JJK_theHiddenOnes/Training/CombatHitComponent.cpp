// Copyright Epic Games, Inc. All Rights Reserved.

#include "Training/CombatHitComponent.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Training/AttackDefinition.h"
#include "Training/CombatTypes.h"
#include "Training/DamageGameplayEffect.h"
#include "Training/FighterAbilitySystemComponent.h"
#include "Training/FighterAttributeSet.h"
#include "Training/FighterCharacter.h"

namespace
{
	FGameplayEffectSpecHandle MakeDamageSpec(UAbilitySystemComponent* SourceASC, float Damage)
	{
		FGameplayEffectSpecHandle Handle = SourceASC->MakeOutgoingSpec(UDamageGameplayEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
		if (Handle.IsValid())
		{
			Handle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, -FMath::Abs(Damage));
		}
		return Handle;
	}
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
	SegmentId = Definition ? Definition->SegmentId : 0;
	SetWindowTickEnabled(false);

	UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 开始攻击实例 %llu（段 %d，伤害 %.0f）"),
		*GetNameSafe(GetOwner()), ActiveInstanceId, SegmentId, Definition ? Definition->Damage : 0.f);
	return ActiveInstanceId;
}

void UCombatHitComponent::HandleAnimWindowNotify(bool bOpen, const UAnimSequenceBase* Animation)
{
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
		SetWindowTickEnabled(true);
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
	bWindowOpen = false;
	SetWindowTickEnabled(false);
	Phase = EAttackPhase::Recovery;
	// 去重键保持到本段结束：段结束即窗口关闭；新攻击实例各自独立
	DedupKeys.Reset();
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
	SetWindowTickEnabled(false);
}

void UCombatHitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bWindowOpen || !bAttackActive)
	{
		SetWindowTickEnabled(false);
		return;
	}
	ProcessSweep();
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
		if (Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(TAG_State_Dead))
		{
			continue;
		}

		const FCombatHitDedupKey Key{ActiveInstanceId, SegmentId, Target};
		if (DedupKeys.Contains(Key))
		{
			continue; // 同实例同段同目标只结算一次
		}
		DedupKeys.Add(Key);
		ValidContacts.Push({Target, Candidate.HitLocation});
	}

	if (ValidContacts.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 实例 %llu 批次结算：有效接触 %d 个"),
		*GetNameSafe(Attacker), ActiveInstanceId, ValidContacts.Num());

	for (const FValidContact& Contact : ValidContacts)
	{
		AFighterCharacter* Target = Contact.Target;
		UAbilitySystemComponent* TargetASC = Target->GetFighterAbilitySystemComponent();
		if (AttackerASC == nullptr || TargetASC == nullptr)
		{
			continue;
		}

		// 伤害经 GE 生效（SetByCaller Data.Damage，负值扣减生命）
		const FGameplayEffectSpecHandle Spec = MakeDamageSpec(AttackerASC, Def->Damage);
		if (Spec.IsValid())
		{
			AttackerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
		++HitCountThisAttack;

		const bool bLethal = Target->GetFighterAttributeSet()->GetHealth() <= 0.f;
		UE_LOG(LogTemp, Log, TEXT("[CombatHit] %s 命中 %s（实例 %llu 段 %d，伤害 %.0f，累计命中 %d，致死=%d）"),
			*GetNameSafe(Attacker), *GetNameSafe(Target), ActiveInstanceId, SegmentId, Def->Damage, HitCountThisAttack, bLethal ? 1 : 0);

		// 受击/死亡进入受击方延迟队列：受击方自身下一 Tick 处理中断；
		// 本帧（同一批次窗口内）双方已有效接触仍换血，被打断后未来接触失效
		Target->QueueCombatEvent({Attacker, Def->HitStunDuration, bLethal, Contact.HitLocation});
	}
}
