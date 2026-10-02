#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatAudioProfile.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

UENUM(BlueprintType)
enum class ECombatAudioCueKind : uint8
{
	PunchSwing, KickSwing, PunchHit, KickHit, HeavyHit, Guard, Immune,
	ChargeStart, ChargeLoop, ChargeFull, ChargeCancel,
	MobileFire, SuperFire, RangedHit, WorldImpact, Expire, DomainStart, DomainEnd
};

USTRUCT(BlueprintType)
struct FCombatAudioCue
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ECombatAudioCueKind Kind = ECombatAudioCueKind::PunchSwing;
	/** Hard references: loaded before combat and followed by Cook. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<USoundBase>> Variants;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Gain = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float PitchVariation = .02f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxLife = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Priority = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundConcurrency> Concurrency;
};

/** D-owned presentation settings; contains no gameplay values. */
UCLASS(BlueprintType)
class UCombatAudioProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FCombatAudioCue> Cues;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MasterGain = .8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeMinGain = .30f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeMaxGain = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeMinPitch = .85f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ChargeMaxPitch = 1.10f;
	/** Swing starts 40 ms before the existing, B-reviewed effective window. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SwingLeadSeconds = .04f;
	/** Duck only D-owned secondary tails/charge. No assumed project music bus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SuperDuckGain = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SuperDuckSeconds = .18f;
	const FCombatAudioCue* Find(ECombatAudioCueKind Kind) const
	{
		return Cues.FindByPredicate([Kind](const FCombatAudioCue& Cue) { return Cue.Kind == Kind; });
	}
};
