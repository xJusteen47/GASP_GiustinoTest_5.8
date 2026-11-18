// Combat ability-system layer: bakes the run skid's clip root track (owner choice A part 2, 2026-11-18).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "GSTN_RunSkidTrackLibrary.generated.h"

class UAnimSequence;
class UGSTN_LocomotionProfile;

/**
 * Turns a skid clip's root motion into the plain samples the simulation reads (FGSTN_RunSkidRootTrack). Baking happens in the editor
 * (the profile's SkidRootMotionClip, its Bake button, a Python tool or an editor utility), never in the simulation: the samples are saved
 * in the profile, so every machine moves the skid from the same numbers without loading the clip.
 *
 * The clip's entry direction is the direction its root travels over its first centimetres; Speed is the root's ground speed along it
 * (central differences), Turn the root yaw done so far over the clip's total, kept non-decreasing so the capsule never turns back.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_RunSkidTrackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fills Track's baked arrays (Speed, Turn, TurnSign, TotalYawDeg, SourceClip, SampleRate) from Clip's root bone track and turns it on.
	 * The tuning fields (PlayRate, EntryBlendSeconds, ReverseSpeedScale, EndTime) are kept. False (Track untouched) without a clip
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Run Skid")
	static bool BakeRunSkidRootTrack(const UAnimSequence* Clip, float SampleRate, UPARAM(ref) FGSTN_RunSkidRootTrack& Track);

	/** Bakes Profile's SkidRootMotionClip into Profile->RunStages.Skid.RootTrack and marks the profile dirty (save it afterwards) */
	UFUNCTION(BlueprintCallable, Category = "Combat|Run Skid")
	static bool BakeProfileSkidRootTrack(UGSTN_LocomotionProfile* Profile);

	/** Debug / tooling: the signed ground speed (cm/s, > 0 = braking along the old run) and turn fraction of a skid live frame */
	UFUNCTION(BlueprintPure, Category = "Combat|Run Skid")
	static void SampleRunSkidRootTrack(const FGSTN_RunSkidRootTrack& Track, int32 LiveFrame, float DeltaSeconds, float EntrySpeed, float& GroundSpeed, float& TurnFraction);
};
