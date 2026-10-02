#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/CombatFeedbackTypes.h"
#include "CombatRangedVisualConsumer.generated.h"

class ADomainOrb;
class AFighterCharacter;
class UCombatFeedbackComponent;
class UCombatRangedVisualProfile;
class UInstancedStaticMeshComponent;
class UParticleSystemComponent;
class UStaticMesh;

/** Visual-only consumer of A's settled ranged feedback v1. One instance belongs to one fighter. */
UCLASS(ClassGroup=(JJK), Blueprintable, meta=(BlueprintSpawnableComponent))
class UCombatRangedVisualConsumer : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatRangedVisualConsumer();

	/** Set on a BP component subclass; A adds that subclass to FeedbackConsumerClasses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Feedback|Visual")
	TObjectPtr<UCombatRangedVisualProfile> VisualProfile;

	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 SpawnedCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 ActiveCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 DrainingCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 NaturalRetiredCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 ForcedRetiredCueCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	int32 ActiveOrbTrailCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	FVector LastCueLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	FRotator LastCueRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Category="Feedback|Visual")
	FName LastCueEffect;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION() void HandleAction(const FCombatActionFeedback& Event);
	UFUNCTION() void HandleContact(const FCombatContactFeedback& Event);
	UFUNCTION() void HandleLifecycle(const FCombatLifecycleFeedback& Event);

	struct FShot { ECombatFeedbackTier Tier = ECombatFeedbackTier::MobileBlast; float PaidQ = 0.f; bool bHadContact = false; };
	struct FCue { int64 AttackId = 0; TWeakObjectPtr<UParticleSystemComponent> Component; double EmitEnd = 0., HardEnd = 0.; bool bDraining = false; };
	struct FOrbTrail
	{
		int64 AttackId = 0;
		TWeakObjectPtr<ADomainOrb> Orb;
		TWeakObjectPtr<UInstancedStaticMeshComponent> Mesh;
		TArray<FVector> Samples;
		FVector LastSample = FVector::ZeroVector;
	};

	TWeakObjectPtr<AFighterCharacter> Fighter;
	TWeakObjectPtr<UCombatFeedbackComponent> Feedback;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> OrbTrailMesh;
	TMap<int64, FShot> Shots;
	TArray<FCue> Cues;
	TArray<FOrbTrail> OrbTrails;

	void SpawnCue(int64 AttackId, const struct FCombatRangedVisualCue& Cue, FVector Location, FVector Facing, float Strength);
	void AttachOrbTrail(int64 AttackId);
	void RemoveTrail(int64 AttackId);
	void StopOwned(int64 AttackId);
	void StopAll();
};
