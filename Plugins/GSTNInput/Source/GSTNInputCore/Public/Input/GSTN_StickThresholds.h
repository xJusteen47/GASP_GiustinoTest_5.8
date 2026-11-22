// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_StickThresholds.generated.h"

/**
 * How the attack stick reading classifies a tilt (owner 2026-11-16: "a player option, not a Project Setting"): under NeutralZone the stick
 * is Neutral (neutral attack), under AimZone it is Aim (neutral attack turned to the stick), from AimZone on it is Tilt (directional attack).
 * A flick is a stick that reaches Tilt at most FlickFrames after it was last Neutral.
 *
 * The classification runs inside the simulation on every machine, so the values travel with the player's loadout (a loadout extension in
 * GSTN Combat) and every machine classifies with the same numbers; a change mid-match is a new loadout revision. The defaults here are the
 * game's defaults (they were UGSTN_InputSettings' StickDeadZone / AimZone / FlickFrames before 2026-11-17). The stick read here is the one
 * after the player's own move-stick dead zone (UGSTN_InputOptionsSubsystem), remapped to 0..1.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_StickThresholds
{
	GENERATED_BODY()

	/** Stick magnitude (0..1) below which the stick is Neutral */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float NeutralZone = 0.2f;

	/** Stick magnitude below which the stick is only Aim (a light tilt); from here on it is Tilt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float AimZone = 0.5f;

	/** Flick window, frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "30"))
	int32 FlickFrames = 3;

	/** The values brought into range (a hostile or old stream cannot make Aim smaller than Neutral) */
	FGSTN_StickThresholds Sanitized() const
	{
		FGSTN_StickThresholds Out;
		Out.NeutralZone = FMath::Clamp(NeutralZone, 0.f, 0.95f);
		Out.AimZone = FMath::Clamp(AimZone, Out.NeutralZone, 1.f);
		Out.FlickFrames = FMath::Clamp(FlickFrames, 0, 30);
		return Out;
	}

	bool operator==(const FGSTN_StickThresholds& Other) const
	{
		return NeutralZone == Other.NeutralZone && AimZone == Other.AimZone && FlickFrames == Other.FlickFrames;
	}
	bool operator!=(const FGSTN_StickThresholds& Other) const { return !(*this == Other); }
};
