#include "Training/Feedback/Visual/CombatRangedVisualInspection.h"

#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystem.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Color/ParticleModuleColorScaleOverLife.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Distributions/DistributionFloatParticleParameter.h"
#include "Distributions/DistributionVectorParticleParameter.h"

TArray<FString> UCombatRangedVisualInspection::DescribeCascade(const UParticleSystem* Effect)
{
	TArray<FString> Rows;
	if (!Effect) return Rows;
	Rows.Add(FString::Printf(TEXT("system=%s looping=%d emitters=%d"),
		*Effect->GetPathName(), Effect->IsLooping() ? 1 : 0, Effect->Emitters.Num()));
	for (const UParticleEmitter* Emitter : Effect->Emitters)
	{
		if (!Emitter) continue;
		const UParticleLODLevel* LOD = Emitter->LODLevels.IsEmpty() ? nullptr : Emitter->LODLevels[0];
		const UParticleModuleRequired* Required = LOD ? LOD->RequiredModule : nullptr;
		if (!Required) continue;
		Rows.Add(FString::Printf(TEXT("emitter=%s loops=%d duration=%.3f durationLow=%.3f range=%d local=%d"),
			*Emitter->EmitterName.ToString(), Required->EmitterLoops, Required->EmitterDuration,
			Required->EmitterDurationLow, Required->bEmitterDurationUseRange ? 1 : 0,
			Required->bUseLocalSpace ? 1 : 0));
	}
	return Rows;
}

bool UCombatRangedVisualInspection::PrepareNaturalFade(UParticleSystem* Effect, float ParticleLife, bool bBeam)
{
#if WITH_EDITOR
	if (!Effect || !Effect->GetPathName().StartsWith(TEXT("/Game/CombatFeedback/VFX/"))) return false;
	Effect->Modify();
	for (UParticleEmitter* Emitter : Effect->Emitters)
	{
		if (!Emitter) continue;
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (!LOD || !LOD->RequiredModule) continue;
			LOD->RequiredModule->Modify();
			LOD->RequiredModule->bKillOnDeactivate = false;
			LOD->RequiredModule->bKillOnCompleted = false;
			UParticleModuleColorScaleOverLife* Fade = nullptr;
			for (UParticleModule* Module : LOD->Modules)
			{
				if (auto* Scale = Cast<UParticleModuleColorScaleOverLife>(Module)) Fade = Scale;
				if (!bBeam)
					if (auto* Lifetime = Cast<UParticleModuleLifetime>(Module))
					{
						Lifetime->Modify();
						auto* Value = NewObject<UDistributionFloatConstant>(Lifetime);
						Value->Constant = FMath::Clamp(ParticleLife, .08f, .5f);
						Lifetime->Lifetime.Distribution = Value;
						Lifetime->Lifetime.Initialize();
					}
			}
			if (!Fade)
			{
				// Particle modules declare Within=ParticleSystem. An emitter outer
				// can look valid in PIE but is rejected by the cooked serializer.
				Fade = NewObject<UParticleModuleColorScaleOverLife>(Effect);
				LOD->Modules.Add(Fade);
			}
			else if (Fade->GetOuter() != Effect)
			{
				const FName SafeName = MakeUniqueObjectName(Effect, Fade->GetClass(), Fade->GetFName());
				if (!Fade->Rename(*SafeName.ToString(), Effect, REN_DontCreateRedirectors | REN_NonTransactional)) return false;
			}
			Fade->Modify(); Fade->bEmitterTime = false;
			if (bBeam)
			{
				auto* Color = NewObject<UDistributionVectorParticleParameter>(Fade);
				Color->ParameterName = TEXT("FeedbackBeamFade");
				Color->Constant = FVector::OneVector;
				for (int32 Axis = 0; Axis < 3; ++Axis) Color->ParamModes[Axis] = DPM_Direct;
				auto* Alpha = NewObject<UDistributionFloatParticleParameter>(Fade);
				Alpha->ParameterName = TEXT("FeedbackBeamFadeAlpha");
				Alpha->Constant = 1.f; Alpha->ParamMode = DPM_Direct;
				Fade->ColorScaleOverLife.Distribution = Color;
				Fade->AlphaScaleOverLife.Distribution = Alpha;
			}
			else
			{
				auto* Color = NewObject<UDistributionVectorConstantCurve>(Fade);
				Color->ConstantCurve.AddPoint(0.f, FVector::OneVector);
				Color->ConstantCurve.AddPoint(.45f, FVector::OneVector);
				Color->ConstantCurve.AddPoint(1.f, FVector::ZeroVector);
				auto* Alpha = NewObject<UDistributionFloatConstantCurve>(Fade);
				Alpha->ConstantCurve.AddPoint(0.f, 1.f);
				Alpha->ConstantCurve.AddPoint(.45f, 1.f);
				Alpha->ConstantCurve.AddPoint(1.f, 0.f);
				Fade->ColorScaleOverLife.Distribution = Color;
				Fade->AlphaScaleOverLife.Distribution = Alpha;
			}
			Fade->ColorScaleOverLife.Initialize();
			Fade->AlphaScaleOverLife.Initialize();
			LOD->UpdateModuleLists();
		}
	}
	Effect->PostEditChange();
	Effect->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
