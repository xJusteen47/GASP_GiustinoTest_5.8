// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "Timeline/GSTN_Pushbox.h"
#include "Timeline/GSTN_Landing.h"
#include "GSTN_TimelineUtils.generated.h"

/**
 * Stateless questions about a timeline's data for Blueprint (details-panel helpers, editor utilities, UI). Per-frame
 * evaluation inside the simulation stays in FGSTN_TimelinePlayer, in C++.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_TimelineUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Is Frame inside the cancellable tail of the timeline? False without a tail */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	static bool IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame);

	/** First frame of the cancellable tail, -1 if the timeline has none */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	static int32 GetTailStartFrame(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper for editor scripts (Python cannot build a TInstancedStruct): a copy of Data with one hitbox track appended, active for
	 * [StartFrame, EndFrame) and addressing Part. Setup scripts build the test timeline assets with it; it is not for the simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData AddHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, const FGSTN_PartSelector& Part, const TArray<FGSTN_HitboxSpec>& Hitboxes);

	/**
	 * Authoring helper: a copy of Data without its attack hitbox tracks (Hurtbox tracks, hitbox tracks by inheritance, stay). Lets a setup
	 * script that rebuilds a move's hitboxes from its clips run again instead of stacking tracks
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveAttackHitboxTracks(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper (as AddHitboxTrack): a copy of Data whose Hold Until Landed track (FGSTN_TimelineTrack_HoldUntilLanded) is the one given,
	 * held in the air on [StartFrame, EndFrame) and landing on LandFrame (-1 = EndFrame). A track of that type already there is replaced, so a
	 * setup script can run again. Sections None = the timeline frames mapped onto the montage. MaxAirFrames: the air timeout (0 = none)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData SetHoldUntilLandedTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, int32 LandFrame = -1,
		int32 LandFromFrame = 0, FName LoopSection = NAME_None, FName LandSection = NAME_None, int32 MaxAirFrames = 0);

	/** Authoring helper: a copy of Data without its Hold Until Landed track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveHoldUntilLandedTrack(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper (as SetHoldUntilLandedTrack): a copy of Data whose Charge Hold track (FGSTN_TimelineTrack_ChargeHold) stands on HoldFrame while
	 * HoldButton (or LayoutFunction's button) is held, at most MaxHoldFrames, scaling the damage up to FullChargeDamageScale. A track of that type
	 * already there is replaced, so a setup script can run again. LoopSection None = the pose is paused on the hold frame
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData SetChargeHoldTrack(const FGSTN_CombatTimelineData& Data, int32 HoldFrame, FGameplayTag HoldButton, FName LayoutFunction,
		int32 MaxHoldFrames = 60, float FullChargeDamageScale = 1.f, bool bLockDirection = true, FName LoopSection = NAME_None);

	/** Authoring helper: a copy of Data without its Charge Hold track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveChargeHoldTrack(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper: a copy of Data with one more Body Capsule track (FGSTN_TimelineTrack_BodyCapsule) on [StartFrame, EndFrame). Setup scripts
	 * call RemoveBodyCapsuleTracks first, so they can run again. Offset: the capsule centre from the mesh root, X forward, Y right, Z up
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData AddBodyCapsuleTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, float HalfHeight, float Radius,
		FVector Offset, EGSTN_BodyCapsuleAnchor Anchor = EGSTN_BodyCapsuleAnchor::Body, float MaxShiftPerFrame = 20.f);

	/** Authoring helper: a copy of Data without any Body Capsule track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveBodyCapsuleTracks(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper: a copy of Data with one more Pushbox track (FGSTN_TimelineTrack_Pushbox) on [StartFrame, EndFrame). bEnabled false = no pushbox
	 * on those frames. Radius/HalfHeight 0 = from the movement capsule; bUseOffset false = the capsule's centre. Run RemovePushboxTracks first to re-run
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData AddPushboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, bool bEnabled = true, float Radius = 0.f,
		float HalfHeight = 0.f, bool bUseOffset = false, FVector Offset = FVector::ZeroVector);

	/** Authoring helper: a copy of Data without any Pushbox track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemovePushboxTracks(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper: a copy of Data with one more Landing track (FGSTN_TimelineTrack_Landing) on [StartFrame, EndFrame), so a kit script can
	 * author an aerial's landing windows (Python cannot build the instanced track itself): Action, the LandingLag response (frames, montage, cue)
	 * and the ContinueWithMove ability. The first window holding a frame wins. Run RemoveLandingTracks first to re-run
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData AddLandingTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, EGSTN_LandingAction Action,
		const FGSTN_RicochetResponse& Lag, TSoftClassPtr<UObject> ContinueMove);

	/** Authoring helper: a copy of Data without any Landing track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveLandingTracks(const FGSTN_CombatTimelineData& Data);

	/**
	 * Authoring helper: a copy of Data whose Hit Bounce track (FGSTN_TimelineTrack_HitBounce, owner 2026-11-23) is the one given: hits on
	 * [StartFrame, EndFrame) bounce the user. A track of that type already there is replaced, so a setup script can run again. BounceMove may be
	 * null (the move just ends on the bounce)
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData SetHitBounceTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, int32 MaxConsecutiveBounces = 2,
		float ApexHeightCm = 150.f, int32 RiseFrames = 18, float BackDistanceCm = 40.f, int32 ActionableFrames = 24, bool bBounceOnBlock = false,
		TSoftClassPtr<UObject> BounceMove = nullptr);

	/** Authoring helper: a copy of Data without its Hit Bounce track */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData RemoveHitBounceTracks(const FGSTN_CombatTimelineData& Data);
};
