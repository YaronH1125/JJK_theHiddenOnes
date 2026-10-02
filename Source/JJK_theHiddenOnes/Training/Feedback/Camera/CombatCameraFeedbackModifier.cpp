#include "Training/Feedback/Camera/CombatCameraFeedbackModifier.h"
#include "Training/ArenaPlayerController.h"
#include "Training/CombatFeedbackComponent.h"
#include "Training/FighterCharacter.h"
#include "Training/TrainingGameMode.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
 TAutoConsoleVariable<int32> CVarCameraStrength(TEXT("JJK.Feedback.CameraStrength"), 2,
  TEXT("Combat camera: 0=off, 1=low (0.5), 2=standard (1). Runtime only."));
}

UCombatCameraFeedbackModifier::UCombatCameraFeedbackModifier()
{
 Priority = 200;
 bExclusive = false;
 AlphaInTime = AlphaOutTime = 0.f;
}

int32 UCombatCameraFeedbackModifier::GetStrength() { return FMath::Clamp(CVarCameraStrength.GetValueOnGameThread(), 0, 2); }
void UCombatCameraFeedbackModifier::SetStrength(int32 Value) { CVarCameraStrength.AsVariable()->Set(FMath::Clamp(Value, 0, 2), ECVF_SetByConsole); }

void UCombatCameraFeedbackModifier::ClearPulse()
{
 PulseStart = PulseEnd = BurstStart = LastStart = -100.;
 PeakRoll = PulseSeconds = 0.f;
 PulsePriority = LastPriority = 0;
 PulseSource.Reset();
 // AppliedRoll describes the camera manager's last rendered cache until ModifyCamera runs again.
}

void UCombatCameraFeedbackModifier::ClearForSource(AFighterCharacter* Source)
{
 if (PulseSource.Get() == Source) ClearPulse();
}

void UCombatCameraFeedbackModifier::ConsumeContact(const FCombatContactFeedback& E, AFighterCharacter* Player)
{
 if (!IsValid(Player) || Player->IsDead() || Player->IsAimingEffective() || GetStrength() == 0) return;
 if (E.Result != ECombatFeedbackResult::Hit && E.Result != ECombatFeedbackResult::Guard) return;
 const bool bVictim = E.Target.Get() == Player;
 if (!bVictim && E.Source.Get() != Player) return;
 const auto* Source = E.Source.Get();
 if (!Source || !Source->GetCombatFeedback()->IsChannelEnabled(ECombatFeedbackChannel::Camera)) return;

 float Roll = 0.f, Seconds = .10f;
 int32 PriorityValue = bVictim ? 3 : 1;
 if (E.Result == ECombatFeedbackResult::Guard)
 {
  Roll = bVictim ? .09f : .04f; Seconds = .10f; PriorityValue = bVictim ? 2 : 1;
 }
 else if (bVictim)
 {
  Roll = E.Tier == ECombatFeedbackTier::Heavy ? .55f : E.Tier == ECombatFeedbackTier::SuperBlast ? .42f : .14f;
  Seconds = .12f;
  if (E.Tier == ECombatFeedbackTier::DomainOrb) { Roll = .09f; Seconds = .10f; }
 }
 else
 {
  switch (E.Tier)
  {
  case ECombatFeedbackTier::Finisher: Roll = .15f; Seconds = .08f; break;
  case ECombatFeedbackTier::Heavy: Roll = .60f; Seconds = .11f; break;
  case ECombatFeedbackTier::SuperBlast: Roll = .32f; break;
  // A1/A2/A3, ordinary kicks, mobile blasts and outgoing domain balls have no roll.
  default: return;
  }
 }
 const double Now = GetWorld()->GetTimeSeconds();
 const double MergeWindow = E.Tier == ECombatFeedbackTier::DomainOrb ? .30 : .10;
 if (LastPriority == PriorityValue && LastTier == E.Tier && LastResult == E.Result && Now - LastStart < MergeWindow)
 {
  if (Now < PulseEnd) { PeakRoll = FMath::Min(.60f, FMath::Max(PeakRoll, Roll)); ++MergedCount; }
  else ++DroppedCount;
  return; // No restart or duration extension for repeated contacts.
 }
 if (Now < PulseEnd)
 {
  if (PriorityValue <= PulsePriority) { ++DroppedCount; return; }
  // A higher priority may replace the envelope, but the first event owns a 150ms hard end.
  PulseEnd = FMath::Min(BurstStart + .15, Now + Seconds);
 }
 else { BurstStart = Now; PulseEnd = Now + Seconds; }
 PulseStart = Now;
 PulseSeconds = static_cast<float>(PulseEnd - Now);
 PeakRoll = FMath::Min(Roll, .60f);
 PulseSource = E.Source;
 PulsePriority = PriorityValue;
 LastPriority = PriorityValue; LastTier = E.Tier; LastResult = E.Result; LastStart = Now;
 ++StartedCount;
}

bool UCombatCameraFeedbackModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& POV)
{
 auto* PC = CameraOwner ? Cast<AArenaPlayerController>(CameraOwner->PCOwner) : nullptr;
 auto* Player = PC ? PC->GetPlayerFighter() : nullptr;
 auto* GM = PC ? PC->GetTrainingGameMode() : nullptr;
 if (!PC || !Player || !PC->CanPlayCombatFeedback() || Player->IsAimingEffective()
  || !Player->GetCombatFeedback()->IsChannelEnabled(ECombatFeedbackChannel::Camera) || GetStrength() == 0
  || (GM && (GM->IsMatchResolved() || GM->IsTrainingMenuOpen()))) ClearPulse();
 const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
 const float T = PulseSeconds > 0.f ? FMath::Clamp(static_cast<float>((Now - PulseStart) / PulseSeconds), 0.f, 1.f) : 1.f;
 AppliedRoll = Now < PulseEnd ? PeakRoll * FMath::Sin(PI * T) * (GetStrength() == 1 ? .5f : 1.f) : 0.f;
 POV.Rotation.Roll += AppliedRoll;
 // Roll leaves Location and Rotation.Vector() exactly unchanged, including during transitions.
 return false;
}
