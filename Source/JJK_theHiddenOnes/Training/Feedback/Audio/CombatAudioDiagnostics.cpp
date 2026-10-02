#include "Training/Feedback/Audio/CombatAudioDiagnostics.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "Misc/App.h"

float UCombatAudioDiagnostics::GetBackgroundVolume() { return FApp::GetUnfocusedVolumeMultiplier(); }
void UCombatAudioDiagnostics::SetRecordingBackgroundVolume(float Gain)
{
#if !UE_BUILD_SHIPPING
	FApp::SetUnfocusedVolumeMultiplier(FMath::Clamp(Gain, 0.f, 1.f));
#endif
}
TArray<FString> UCombatAudioDiagnostics::DescribeAudioDevice(const UObject* Context)
{
	TArray<FString> Result;
	Result.Add(FString::Printf(TEXT("app_volume=%.4f background_volume=%.4f"), FApp::GetVolumeMultiplier(), FApp::GetUnfocusedVolumeMultiplier()));
	if (const UWorld* World = Context ? Context->GetWorld() : nullptr)
	{
		const FAudioDeviceHandle Device = World->GetAudioDevice();
		if (Device.IsValid()) Result.Add(FString::Printf(TEXT("world_device=%u muted=%d primary_volume=%.4f"), Device.GetDeviceID(), Device->IsAudioDeviceMuted(), Device->GetPrimaryVolume()));
	}
	return Result;
}
