#include "Training/Feedback/Visual/CombatRangedVisualConsumer.h"

#include "Training/Feedback/Visual/CombatRangedVisualProfile.h"
#include "Training/CombatFeedbackComponent.h"
#include "Training/DomainOrb.h"
#include "Training/FighterCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"

UCombatRangedVisualConsumer::UCombatRangedVisualConsumer()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) OrbTrailMesh = Sphere.Object;
	static ConstructorHelpers::FObjectFinder<UCombatRangedVisualProfile> Profile(
		TEXT("/Game/CombatFeedback/Profiles/Visual/DA_RangedVisual.DA_RangedVisual"));
	if (Profile.Succeeded()) VisualProfile = Profile.Object;
}

void UCombatRangedVisualConsumer::BeginPlay()
{
	Super::BeginPlay();
	Fighter = Cast<AFighterCharacter>(GetOwner());
	Feedback = Fighter.IsValid() ? Fighter->GetCombatFeedback() : nullptr;
	if (!Feedback.IsValid()) return;
	Feedback->OnAction.AddUniqueDynamic(this, &UCombatRangedVisualConsumer::HandleAction);
	Feedback->OnContact.AddUniqueDynamic(this, &UCombatRangedVisualConsumer::HandleContact);
	Feedback->OnLifecycle.AddUniqueDynamic(this, &UCombatRangedVisualConsumer::HandleLifecycle);
	if (!VisualProfile) UE_LOG(LogTemp, Warning, TEXT("[RangedVisual] %s has no visual profile"), *GetNameSafe(GetOwner()));
}

void UCombatRangedVisualConsumer::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Feedback.IsValid())
	{
		Feedback->OnAction.RemoveDynamic(this, &UCombatRangedVisualConsumer::HandleAction);
		Feedback->OnContact.RemoveDynamic(this, &UCombatRangedVisualConsumer::HandleContact);
		Feedback->OnLifecycle.RemoveDynamic(this, &UCombatRangedVisualConsumer::HandleLifecycle);
	}
	StopAll();
	Shots.Reset();
	Super::EndPlay(Reason);
}

void UCombatRangedVisualConsumer::HandleAction(const FCombatActionFeedback& Event)
{
	if (!Feedback.IsValid() || !Feedback->IsCurrent(Event.RoundId, Event.Generation) || Event.Source.Get() != Fighter.Get()) return;
	if (Event.Stage != ECombatActionStage::Fire || !Event.AttackInstanceId) return;
	if (Event.Tier != ECombatFeedbackTier::MobileBlast && Event.Tier != ECombatFeedbackTier::SuperBlast && Event.Tier != ECombatFeedbackTier::DomainOrb) return;
	Shots.Add(Event.AttackInstanceId, {Event.Tier, FMath::Clamp(Event.PaidQ, 0.f, 1.f), false});
	if (Event.Tier == ECombatFeedbackTier::DomainOrb && Feedback->IsChannelEnabled(ECombatFeedbackChannel::RangedFX)) AttachOrbTrail(Event.AttackInstanceId);
}

void UCombatRangedVisualConsumer::HandleContact(const FCombatContactFeedback& Event)
{
	if (!Event.bRanged || !Feedback.IsValid() || !Feedback->IsCurrent(Event.RoundId, Event.SourceGeneration)
		|| Event.Source.Get() != Fighter.Get()) return;
	FShot& Shot = Shots.FindOrAdd(Event.AttackInstanceId);
	Shot.Tier = Event.Tier;
	Shot.PaidQ = FMath::Clamp(Event.PaidQ, 0.f, 1.f);
	Shot.bHadContact = true;
	if (!Feedback->IsChannelEnabled(ECombatFeedbackChannel::RangedFX) || !VisualProfile) return;
	const FCombatRangedVisualCue* Cue = nullptr;
	switch (Event.Result)
	{
	case ECombatFeedbackResult::Hit:
		Cue = Event.Tier == ECombatFeedbackTier::SuperBlast ? &VisualProfile->SuperHit
			: Event.Tier == ECombatFeedbackTier::DomainOrb ? &VisualProfile->DomainHit : &VisualProfile->MobileHit;
		break;
	case ECombatFeedbackResult::Guard: Cue = &VisualProfile->Guard; break;
	case ECombatFeedbackResult::Immune: Cue = &VisualProfile->Immune; break;
	case ECombatFeedbackResult::WorldImpact: Cue = &VisualProfile->WorldImpact; break;
	default: return;
	}
	// Surface impacts face away from the wall. Character contacts face the incoming shot.
	const FVector Facing = Event.Result == ECombatFeedbackResult::WorldImpact
		? Event.Normal.GetSafeNormal() : -Event.Direction.GetSafeNormal();
	const float Strength = Event.Tier == ECombatFeedbackTier::DomainOrb ? .7f
		: FMath::Lerp(.6f, 1.f, Shot.PaidQ);
	SpawnCue(Event.AttackInstanceId, *Cue, Event.Location, Facing, Strength);
}

void UCombatRangedVisualConsumer::HandleLifecycle(const FCombatLifecycleFeedback& Event)
{
	if (!Feedback.IsValid() || Event.Source.Get() != Fighter.Get() || !Feedback->IsCurrent(Event.RoundId, Event.Generation)) return;
	if (Event.AttackInstanceId == 0)
	{
		StopAll();
		Shots.Reset();
		return;
	}
	const FShot* Shot = Shots.Find(Event.AttackInstanceId);
	// A may clear the input generation while an already-fired projectile remains
	// in flight. That clears our shot cache, but its fresh Expire still merits a cue.
	if (Event.Reason == ECombatFeedbackEnd::Expire && (!Shot || !Shot->bHadContact) && VisualProfile
		&& Feedback->IsChannelEnabled(ECombatFeedbackChannel::RangedFX))
	{
		SpawnCue(Event.AttackInstanceId, VisualProfile->Expire, Event.Location, FVector::UpVector, .55f);
	}
	RemoveTrail(Event.AttackInstanceId);
	if (Event.Reason != ECombatFeedbackEnd::Completed && Event.Reason != ECombatFeedbackEnd::Expire)
		StopOwned(Event.AttackInstanceId);
	Shots.Remove(Event.AttackInstanceId);
}

void UCombatRangedVisualConsumer::SpawnCue(int64 AttackId, const FCombatRangedVisualCue& Cue,
	FVector Location, FVector Facing, float Strength)
{
	if (!Cue.Effect || !GetWorld()) return;
	const FRotator Rotation = Facing.IsNearlyZero() ? FRotator::ZeroRotator : Facing.Rotation();
	UParticleSystemComponent* Component = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Cue.Effect,
		Location, Rotation, FVector(FMath::Max(.01f, Cue.Scale * Strength)), false);
	if (!Component) return;
	const double EmitEnd = GetWorld()->GetTimeSeconds() + FMath::Max(.05f, Cue.MaxLife);
	Cues.Add({AttackId, Component, EmitEnd, EmitEnd + FMath::Clamp(Cue.FadeLife, .05f, 1.f), false});
	LastCueLocation = Location;
	LastCueRotation = Rotation;
	LastCueEffect = Cue.Effect->GetFName();
	++SpawnedCueCount;
	ActiveCueCount = Cues.Num();
}

void UCombatRangedVisualConsumer::AttachOrbTrail(int64 AttackId)
{
	if (!Fighter.IsValid() || !OrbTrailMesh || !GetWorld()) return;
	for (TActorIterator<ADomainOrb> It(GetWorld()); It; ++It)
	{
		ADomainOrb* Orb = *It;
		if (Orb->GetOwner() != Fighter.Get() || Orb->GetFeedbackAttackId() != AttackId) continue;
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(Fighter.Get());
		Mesh->SetStaticMesh(OrbTrailMesh);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetReceivesDecals(false);
		Fighter->AddInstanceComponent(Mesh);
		Mesh->RegisterComponent();
		FOrbTrail& Trail = OrbTrails.AddDefaulted_GetRef();
		Trail.AttackId = AttackId;
		Trail.Orb = Orb;
		Trail.Mesh = Mesh;
		Trail.LastSample = Orb->GetActorLocation();
		ActiveOrbTrailCount = OrbTrails.Num();
		return;
	}
}

void UCombatRangedVisualConsumer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld()) return;
	if (Feedback.IsValid() && !Feedback->IsChannelEnabled(ECombatFeedbackChannel::RangedFX))
	{
		StopAll();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	DrainingCueCount = 0;
	for (int32 Index = Cues.Num() - 1; Index >= 0; --Index)
	{
		FCue& Entry = Cues[Index];
		UParticleSystemComponent* Cue = Entry.Component.Get();
		if (Cue && !Entry.bDraining && Now >= Entry.EmitEnd)
		{
			Cue->DeactivateSystem();
			Entry.bDraining = true;
		}
		if (!Cue || Now >= Entry.HardEnd || (Entry.bDraining && Cue->GetNumActiveParticles() == 0))
		{
			if (Cue && Entry.bDraining && Cue->GetNumActiveParticles() == 0) ++NaturalRetiredCueCount;
			else if (Cue) ++ForcedRetiredCueCount;
			if (Cue) Cue->DestroyComponent();
			Cues.RemoveAtSwap(Index);
		}
		else if (Entry.bDraining) ++DrainingCueCount;
	}
	ActiveCueCount = Cues.Num();
	for (int32 Index = OrbTrails.Num() - 1; Index >= 0; --Index)
	{
		FOrbTrail& Trail = OrbTrails[Index];
		if (!Trail.Orb.IsValid() || !Trail.Mesh.IsValid())
		{
			if (UInstancedStaticMeshComponent* Mesh = Trail.Mesh.Get()) Mesh->DestroyComponent();
			OrbTrails.RemoveAtSwap(Index);
			continue;
		}
		const FVector Position = Trail.Orb->GetActorLocation();
		if (FVector::DistSquared(Position, Trail.LastSample) >= FMath::Square(30.f))
		{
			Trail.Samples.Add(Trail.LastSample);
			if (Trail.Samples.Num() > 8) Trail.Samples.RemoveAt(0);
			Trail.LastSample = Position;
		}
		Trail.Mesh->ClearInstances();
		for (int32 Sample = 0; Sample < Trail.Samples.Num(); ++Sample)
		{
			const float Scale = FMath::Lerp(.045f, .11f, float(Sample + 1) / float(Trail.Samples.Num()));
			Trail.Mesh->AddInstance(FTransform(FQuat::Identity, Trail.Samples[Sample], FVector(Scale)), true);
		}
	}
	ActiveOrbTrailCount = OrbTrails.Num();
}

void UCombatRangedVisualConsumer::RemoveTrail(int64 AttackId)
{
	for (int32 Index = OrbTrails.Num() - 1; Index >= 0; --Index)
	{
		if (OrbTrails[Index].AttackId != AttackId) continue;
		if (UInstancedStaticMeshComponent* Mesh = OrbTrails[Index].Mesh.Get()) Mesh->DestroyComponent();
		OrbTrails.RemoveAtSwap(Index);
	}
	ActiveOrbTrailCount = OrbTrails.Num();
}

void UCombatRangedVisualConsumer::StopOwned(int64 AttackId)
{
	for (int32 Index = Cues.Num() - 1; Index >= 0; --Index)
	{
		if (Cues[Index].AttackId != AttackId) continue;
		if (UParticleSystemComponent* Cue = Cues[Index].Component.Get()) Cue->DestroyComponent();
		Cues.RemoveAtSwap(Index);
	}
	RemoveTrail(AttackId);
	ActiveCueCount = Cues.Num();
}

void UCombatRangedVisualConsumer::StopAll()
{
	for (FCue& Entry : Cues) if (UParticleSystemComponent* Cue = Entry.Component.Get()) Cue->DestroyComponent();
	Cues.Reset();
	for (FOrbTrail& Entry : OrbTrails) if (UInstancedStaticMeshComponent* Mesh = Entry.Mesh.Get()) Mesh->DestroyComponent();
	OrbTrails.Reset();
	ActiveCueCount = 0;
	DrainingCueCount = 0;
	ActiveOrbTrailCount = 0;
}
