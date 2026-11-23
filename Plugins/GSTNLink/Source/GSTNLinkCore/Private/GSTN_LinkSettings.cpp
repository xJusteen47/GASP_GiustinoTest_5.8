// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_LinkSettings.h"

#include "HAL/IConsoleManager.h"
#include "Input/GSTN_InputSettings.h"

namespace
{
	const FName ContributionSource(TEXT("GSTNLink"));
}

void UGSTN_LinkSettings::PushToInputSettings() const
{
	GetMutableDefault<UGSTN_InputSettings>()->SetContribution(ContributionSource, CommandTiers, TArray<FGameplayTag>());
}

void UGSTN_LinkSettings::RemoveFromInputSettings()
{
	if (UGSTN_InputSettings* InputSettings = GetMutableDefault<UGSTN_InputSettings>())
	{
		InputSettings->RemoveContribution(ContributionSource);
	}
}

#if WITH_EDITOR
void UGSTN_LinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsTemplate())
	{
		PushToInputSettings();
	}
}
#endif

// ---- grab-link playtest overrides (GSTN Debug window, gstn.Grab.*) ----

namespace
{
	// The ints with a "2 =" in their help get a number in the GSTN Debug window, the 0/1 ones a check box (one control per option)
	TAutoConsoleVariable<int32> CVarGrabTurnMode(TEXT("gstn.Grab.TurnMode"), 0,
		TEXT("Grab turn ergonomics in the Notation throw mode: 0 = project setting, 1 = instant (F. throw in every direction), 2 = with a B. throw. ")
		TEXT("Read when a kit is compiled: takes effect at the next kit swap or Play"));
	TAutoConsoleVariable<float> CVarGrabWalkScale(TEXT("gstn.Grab.WalkScale"), -1.f,
		TEXT("Walk speed multiplier while holding a grabbed enemy; < 0 = project setting (HoldWalkScale). Read when the hold starts"));
	TAutoConsoleVariable<float> CVarGrabJumpScale(TEXT("gstn.Grab.JumpScale"), -1.f,
		TEXT("Jump speed multiplier while holding a grabbed enemy; < 0 = project setting (HoldJumpScale). Read when the hold starts"));
	TAutoConsoleVariable<int32> CVarGrabHoldByNeck(TEXT("gstn.Grab.HoldByNeck"), 0,
		TEXT("Hold the grabbed enemy by the neck: 0 = project or kit data, 1 = on, 2 = off (the grab type's offset). Read when the hold starts"));
	TAutoConsoleVariable<int32> CVarGrabMaxHoldFrames(TEXT("gstn.Grab.MaxHoldFrames"), 0,
		TEXT("Longest hold before the grabbed enemy frees itself, in 62.5 Hz frames (e.g. 300 = 4.8 s); 0 = the kit's mash rule. Read when the grab forms"));
	TAutoConsoleVariable<int32> CVarGrabEscapeHitForAll(TEXT("gstn.Grab.EscapeHitForAll"), 0,
		TEXT("1 = every grabbed entity hits its grabber when it frees itself (test on mannequins); 0 = only kits whose EscapeHit is on (PVE enemies)"));
}

EGSTN_GrabTurnErgonomics UGSTN_LinkSettings::GetEffectiveGrabTurn() const
{
	const int32 Value = CVarGrabTurnMode.GetValueOnAnyThread();
	return Value == 1 ? EGSTN_GrabTurnErgonomics::Instant : Value == 2 ? EGSTN_GrabTurnErgonomics::BackThrow : GrabTurnErgonomics;
}

float UGSTN_LinkSettings::GetEffectiveHoldWalkScale() const
{
	const float Value = CVarGrabWalkScale.GetValueOnAnyThread();
	return Value >= 0.f ? Value : HoldWalkScale;
}

float UGSTN_LinkSettings::GetEffectiveHoldJumpScale() const
{
	const float Value = CVarGrabJumpScale.GetValueOnAnyThread();
	return Value >= 0.f ? Value : HoldJumpScale;
}

FGSTN_GrabHoldPose UGSTN_LinkSettings::GetEffectiveHoldPose(const FGSTN_LinkKitExtension* GrabberKit) const
{
	FGSTN_GrabHoldPose Pose = GrabberKit && GrabberKit->bOverrideHoldPose ? GrabberKit->HoldPose : HoldPose;
	const int32 Neck = CVarGrabHoldByNeck.GetValueOnAnyThread();
	if (Neck == 1 || Neck == 2)
	{
		Pose.bHoldByNeck = Neck == 1;
	}
	return Pose;
}

int32 UGSTN_LinkSettings::GetMaxHoldFramesOverride()
{
	return FMath::Max(CVarGrabMaxHoldFrames.GetValueOnAnyThread(), 0);
}

bool UGSTN_LinkSettings::GetEscapeHitForAll()
{
	return CVarGrabEscapeHitForAll.GetValueOnAnyThread() != 0;
}
