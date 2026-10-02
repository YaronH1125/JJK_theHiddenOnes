#include "Training/CombatFeedbackProfile.h"

UAnimMontage* UCombatReactionLibrary::Resolve(ECombatFeedbackTier Tier, bool bGuard, const FVector& D) const
{
 if (bGuard) return Guard;
 if (D.X > .5f && Back) return Back;
 if (D.Y > .5f && Left) return Left;
 if (D.Y < -.5f && Right) return Right;
 return (Tier == ECombatFeedbackTier::Heavy || Tier == ECombatFeedbackTier::Finisher
  || Tier == ECombatFeedbackTier::SuperBlast || Tier == ECombatFeedbackTier::DomainOrb) && Heavy ? Heavy : Light;
}

float UCombatFeedbackProfile::StopFor(const FCombatContactFeedback& C) const
{
 if (C.bLethal || (C.Result != ECombatFeedbackResult::Hit && C.Result != ECombatFeedbackResult::Guard)) return 0.f;
 if (C.Result == ECombatFeedbackResult::Guard) return FMath::Clamp(GuardStop, 0.f, .030f);
 switch (C.Tier)
 {
 case ECombatFeedbackTier::Medium: return MediumStop;
 case ECombatFeedbackTier::Finisher: return FinisherStop;
 case ECombatFeedbackTier::Heavy: return HeavyStop;
 case ECombatFeedbackTier::MobileBlast: return MobileStop;
 case ECombatFeedbackTier::SuperBlast: return SuperStop;
 case ECombatFeedbackTier::DomainOrb: return DomainStop;
 default: return LightStop;
 }
}
