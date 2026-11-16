// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PairMoveTestContent.generated.h"

/**
 * TEST CONTENT of the two reference pair moves (combat-m7-piggyback.md 6.6, M7 G9 part B): the four timelines are built in the constructors, as
 * the mount moves, so the network harness (Tools/GASNPPTest/mount_scenarios.py) plays them without an ability asset. They are not designed game
 * abilities: no animation, no cue, round numbers. The definitions that pair them are assets of the plugin's Content/Test folder, filled by
 * UGSTN_PiggybackContentLibrary::FillReferencePairMove. Geometry is relative to the mover's root facing +X.
 */

/**
 * PM_ThrowRider, the carrier's part: 32 frames (startup 12, release 2, recovery 18), actionable at 32. Its PartnerMove key on frame 1 starts the
 * rider's thrown part 12 frames later (frame 13, the thrown pose one frame before the release); the definition's Break at frame 14 releases the
 * rider with the definition's exit launch
 */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Test_CarrierThrowRider : public UGSTN_GA_PairMove
{
	GENERATED_BODY()

public:
	/** The frame of the PartnerMove key and the release (the definition's OutcomeFrame) */
	static constexpr int32 KeyFrame = 1;
	static constexpr int32 ReleaseFrame = 14;

	UGSTN_GA_Test_CarrierThrowRider(const FObjectInitializer& ObjectInitializer);
};

/**
 * PM_ThrowRider, the rider's part (forced by the link tick): 40 frames, actionable from ThrownActionableFrame (air actions allowed). A capsule
 * hitbox around the body in Root space from frame 2 to the end, swept between frames, its launch yaw along the hitbox's velocity (the throw
 * direction), one hit per target. The flight itself is the exit launch: the timeline has no movement track
 */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Test_RiderThrown : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	static constexpr int32 ThrownActionableFrame = 24;

	UGSTN_GA_Test_RiderThrown(const FObjectInitializer& ObjectInitializer);
};

/** PM_BoostJump, the rider's part: 14 frames, actionable at 10; the PartnerMove key on frame 0 (the carrier braces from the next frame), Break at 6 */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Test_RiderBoostJump : public UGSTN_GA_PairMove
{
	GENERATED_BODY()

public:
	static constexpr int32 KeyFrame = 0;
	static constexpr int32 LaunchFrame = 6;

	UGSTN_GA_Test_RiderBoostJump(const FObjectInitializer& ObjectInitializer);
};

/** PM_BoostJump, the carrier's part (forced): 12 frames, actionable at 10, Combat.Lock.Movement for its first 10 frames, no hitbox */
UCLASS(Blueprintable)
class GSTNPIGGYBACKRUNTIME_API UGSTN_GA_Test_CarrierBraced : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	static constexpr int32 LockFrames = 10;

	UGSTN_GA_Test_CarrierBraced(const FObjectInitializer& ObjectInitializer);
};
