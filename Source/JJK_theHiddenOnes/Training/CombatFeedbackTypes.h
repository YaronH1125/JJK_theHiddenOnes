#pragma once

#include "CoreMinimal.h"
#include "CombatFeedbackTypes.generated.h"

class AFighterCharacter;
class UAttackDefinition;

UENUM(BlueprintType)
enum class ECombatFeedbackResult : uint8 { None, Hit, Guard, Immune, WorldImpact };
UENUM(BlueprintType)
enum class ECombatFeedbackTier : uint8 { Light, Medium, Finisher, Heavy, MobileBlast, SuperBlast, DomainOrb };
UENUM(BlueprintType)
enum class ECombatActionStage : uint8 { Start, Update, Full, Fire, End };
UENUM(BlueprintType)
enum class ECombatFeedbackEnd : uint8 { None, Completed, Expire, Cancel, Reset, Death, Destroyed, Rejected, Interrupted, Suppressed };
UENUM(BlueprintType)
enum class ECombatFeedbackChannel : uint8 { HitStop, Reaction, Audio, RangedFX, Camera, HUD };

/** Interface v1. References are weak: consumers must validate them and the generation before delayed work. */
USTRUCT(BlueprintType)
struct FCombatContactFeedback
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 InterfaceVersion = 1;
 UPROPERTY(BlueprintReadOnly) int32 RoundId = 0;
 UPROPERTY(BlueprintReadOnly) int32 SourceGeneration = 0;
 UPROPERTY(BlueprintReadOnly) int32 TargetGeneration = 0;
 UPROPERTY(BlueprintReadOnly) int64 AttackInstanceId = 0;
 UPROPERTY(BlueprintReadOnly) int32 SegmentId = 0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AFighterCharacter> Source;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AFighterCharacter> Target;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<UAttackDefinition> IncomingAttack;
 UPROPERTY(BlueprintReadOnly) FName MoveId;
 UPROPERTY(BlueprintReadOnly) ECombatFeedbackTier Tier = ECombatFeedbackTier::Light;
 UPROPERTY(BlueprintReadOnly) ECombatFeedbackResult Result = ECombatFeedbackResult::None;
 UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector Normal = FVector::UpVector;
 /** Travel direction towards the victim, in world space. */
 UPROPERTY(BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
 UPROPERTY(BlueprintReadOnly) float PaidQ = 0.f;
 UPROPERTY(BlueprintReadOnly) float ActualDamage = 0.f;
 UPROPERTY(BlueprintReadOnly) bool bArmored = false;
 UPROPERTY(BlueprintReadOnly) bool bLethal = false;
 UPROPERTY(BlueprintReadOnly) bool bLocationFallback = false;
 UPROPERTY(BlueprintReadOnly) bool bRanged = false;
};

USTRUCT(BlueprintType)
struct FCombatActionFeedback
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 InterfaceVersion = 1;
 UPROPERTY(BlueprintReadOnly) int32 RoundId = 0;
 UPROPERTY(BlueprintReadOnly) int32 Generation = 0;
 UPROPERTY(BlueprintReadOnly) int64 SessionId = 0;
 UPROPERTY(BlueprintReadOnly) int64 AttackInstanceId = 0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AFighterCharacter> Source;
 UPROPERTY(BlueprintReadOnly) FName MoveId;
 UPROPERTY(BlueprintReadOnly) ECombatFeedbackTier Tier = ECombatFeedbackTier::Light;
 UPROPERTY(BlueprintReadOnly) ECombatActionStage Stage = ECombatActionStage::Start;
 UPROPERTY(BlueprintReadOnly) ECombatFeedbackEnd EndReason = ECombatFeedbackEnd::None;
 UPROPERTY(BlueprintReadOnly) float PaidQ = 0.f;
 UPROPERTY(BlueprintReadOnly) FTransform Muzzle;
};

/** Expire/Cancel/Reset are lifecycle events, never contact results. */
USTRUCT(BlueprintType)
struct FCombatLifecycleFeedback
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 RoundId = 0;
 UPROPERTY(BlueprintReadOnly) int32 Generation = 0;
 UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AFighterCharacter> Source;
 /** Zero means all owned feedback (character cleanup). */
 UPROPERTY(BlueprintReadOnly) int64 AttackInstanceId = 0;
 UPROPERTY(BlueprintReadOnly) ECombatFeedbackEnd Reason = ECombatFeedbackEnd::Cancel;
 UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
};
