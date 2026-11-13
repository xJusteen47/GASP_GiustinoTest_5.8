// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "LOD/GSTN_MinionLOD.h"
#include "GSTN_MinionInterpolator.generated.h"

/**
 * Tunables of the client smoothing of a minion (combat-m8-minions.md 8.4 "Per-tier smoothing"). Time is in milliseconds of the fixed
 * simulation (16 ms frames) for the sample side and in local seconds for the dead-reckoning side, so the struct stays pure and testable.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionSmoothingParams
{
	GENERATED_BODY()

	/** Length of one server frame (NP fixed step) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "1.0", ForceUnits = "ms"))
	float FixedStepMs = 16.f;

	/** T0: past the newest sample the puppet goes on along Speed and yaw for at most this long, then holds (bridges one lost 15 Hz update) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float ExtrapolationCapMs = 150.f;

	/** T1/T2, and any switch of smoothing mode: the jump between the shown pose and the new target is blended out over this time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float CorrectionBlendMs = 200.f;

	/** T1/T2: dead reckoning goes on from the newest sample for at most this long (a T2 item is re-sent every second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float DeadReckoningCapMs = 1100.f;

	/**
	 * M8 G9b ("no pop" at Far/Dormant): dead reckoning never runs longer than the gap between the two newest samples (the sender's own
	 * interval), and for a sender slower than T1 (sample gap above T1MaxFrameGap: a Far minion, or one about to go Dormant, whose last
	 * Speed is often stale because the server stops moving it) at most this long. 0 = such a minion holds its newest sample and the
	 * correction blend carries it to the next one: behind the server by up to one interval, never ahead of it (the overshoot was 1-2 m)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float FarDeadReckoningCapMs = 0.f;

	/**
	 * The wire does not carry the tier, so a client infers it from the gap between two samples: up to this many frames is T0 (15 Hz is
	 * about 4.2 frames), up to T1MaxFrameGap is T1 (5 Hz is 12.5 frames), anything longer is T2. An idle T0 minion sends nothing and reads
	 * as T2: harmless, dead reckoning of a still minion is a hold
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 T0MaxFrameGap = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 T1MaxFrameGap = 25;
};

/** One dequantized sample of a minion as the client keeps it: the wire state plus the full server frame it was stamped with */
struct GSTNMINIONCORE_API FGSTN_MinionPresentSample
{
	/** Full (reconstructed) server frame of the sample */
	int32 Frame = 0;
	FVector3f Position = FVector3f::ZeroVector;
	float YawDegrees = 0.f;
	float Speed = 0.f;
	uint8 ActionState = 0;
	uint8 MoveId = 0;
	int32 MoveStartFrame = INDEX_NONE;
	uint8 HitCounter = 0;
	/** GSTNMinion::NetFlags (Teleport snaps) */
	uint8 Flags = 0;
};

/** How a displayed pose was made */
enum class EGSTN_MinionDisplayMode : uint8
{
	/** No sample yet */
	None,
	/** Between two samples on the render time (T0) */
	Interpolated,
	/** Past the newest sample, inside the extrapolation cap (T0) */
	Extrapolated,
	/** Past the cap, or before the oldest sample: a held sample */
	Held,
	/** Newest sample plus velocity on local time (T1/T2) */
	DeadReckoned,
	/** Written straight from the authority store (listen host, no smoothing) */
	Direct
};

/** What a puppet shows this render frame */
struct GSTNMINIONCORE_API FGSTN_MinionDisplayPose
{
	FVector3f Position = FVector3f::ZeroVector;
	float YawDegrees = 0.f;
	/** Server frame the pose stands for (fractional between samples). The harness compares the puppet with the server at this frame */
	double Frame = 0.0;
	/** State of the sample at or before Frame */
	uint8 ActionState = 0;
	uint8 MoveId = 0;
	int32 MoveStartFrame = INDEX_NONE;
	float Speed = 0.f;
	EGSTN_MinionDisplayMode Mode = EGSTN_MinionDisplayMode::None;
	/** True while a correction offset is being blended out */
	bool bBlending = false;
};

/**
 * The client smoothing of one minion (combat-m8-minions.md 8.4): a ring of the last 8 samples keyed by their full server frame, and the
 * two ways to show them.
 *  - T0, Interpolate(RenderFrame): the render time is the NP interpolation time of the client, the clock interpolated players use, so a
 *    minion and a remote player hit on the same server frame are shown together and the lag compensation time is one number (9.2).
 *    Linear position and shortest-arc yaw between the two samples around it; past the newest one it extrapolates along Speed and yaw for
 *    at most ExtrapolationCapMs, then holds.
 *  - T1/T2, DeadReckon(Now): the NP buffer (100 ms) cannot interpolate 5 Hz or 1 Hz items, so the puppet runs ahead of the newest sample
 *    with the velocity Speed and yaw imply, on local time since the sample arrived. A new sample moves the target; the jump is kept as
 *    an offset and blended out linearly over CorrectionBlendMs, so a correction never pops.
 * A sample with the Teleport flag empties the ring and snaps: no blend from wherever the puppet was. Switching between the two modes is
 * blended like a correction (the interpolated pose is about 100 ms behind the dead-reckoned one).
 *
 * Pure (no world, no clock of its own: the caller passes the times), so the tests drive it without an engine tick. Not deterministic
 * simulation either: presentation only, nothing reads it back.
 */
class GSTNMINIONCORE_API FGSTN_MinionInterpolator
{
public:
	static constexpr int32 RingSize = 8;

	void Reset();

	/**
	 * Adds a sample that arrived at local time NowSeconds. A sample not newer than the newest one is ignored (false), except a Teleport,
	 * which always restarts the ring. A sample that is not a teleport and arrives while dead reckoning starts a correction blend
	 */
	bool AddSample(const FGSTN_MinionPresentSample& Sample, double NowSeconds);

	/** T0 pose at RenderFrame (a fractional server frame). NowSeconds only times a mode-switch blend */
	FGSTN_MinionDisplayPose Interpolate(double RenderFrame, double NowSeconds, const FGSTN_MinionSmoothingParams& Params);

	/** T1/T2 pose at local time NowSeconds */
	FGSTN_MinionDisplayPose DeadReckon(double NowSeconds, const FGSTN_MinionSmoothingParams& Params);

	/** T0 for an inferred T0, dead reckoning otherwise */
	FGSTN_MinionDisplayPose Evaluate(double RenderFrame, double NowSeconds, const FGSTN_MinionSmoothingParams& Params);

	/**
	 * Tier inferred from the gap between the two newest samples; Far with fewer than two samples. With NowSeconds >= 0 the time since the
	 * newest sample arrived counts as a gap too: a T0 minion that went still sends nothing, and its last two (close) samples must not keep
	 * it T0 forever (G7a: idle minions read T0 with no updates, moving ones read slower)
	 */
	EGSTN_MinionLODTier GetInferredTier(const FGSTN_MinionSmoothingParams& Params, double NowSeconds = -1.0) const;

	int32 Num() const { return Count; }
	/** Newest sample, null when empty */
	const FGSTN_MinionPresentSample* GetNewest() const { return Count > 0 ? &At(Count - 1) : nullptr; }
	/** Oldest-first access, Index in [0, Num()) */
	const FGSTN_MinionPresentSample& At(int32 Index) const { return Ring[(Head + Index) % RingSize]; }
	const FGSTN_MinionDisplayPose& GetLastPose() const { return LastPose; }

	/** Position of a sample moved along its own Speed and yaw for Seconds (no ground snap: the client has no ground query here) */
	static FVector3f Advance(const FGSTN_MinionPresentSample& Sample, double Seconds);

	/** Shortest-arc blend of two yaws in degrees, result in [0, 360) */
	static float LerpYaw(float From, float To, float Alpha);

private:
	/** The raw pose of a mode, then the active correction offset on top; remembers the result for the next blend */
	FGSTN_MinionDisplayPose Finish(FGSTN_MinionDisplayPose Raw, double NowSeconds, const FGSTN_MinionSmoothingParams& Params);
	void StartCorrection(const FGSTN_MinionDisplayPose& Target, double NowSeconds);

	FGSTN_MinionPresentSample Ring[RingSize];
	int32 Head = 0;
	int32 Count = 0;

	/** Local time the newest sample arrived */
	double NewestArrivalSeconds = 0.0;

	/** The jump being blended out: shown pose minus the raw target at CorrectionStartSeconds */
	FVector3f CorrectionOffset = FVector3f::ZeroVector;
	float CorrectionYaw = 0.f;
	double CorrectionStartSeconds = -1.0;

	/** The newest sample arrived while dead reckoning: the next DeadReckon starts a blend from the last shown pose */
	bool bPendingCorrection = false;
	/** The next pose is taken as is (first sample, teleport) */
	bool bSnapNext = true;

	FGSTN_MinionDisplayPose LastPose;
};

/** Flash and mesh pause state of one minion's puppet (combat-m8-minions.md 8.4 "Hit presentation") */
enum class EGSTN_MinionFlashState : uint8
{
	None,
	/** This machine predicted a hit; waiting for the authoritative HitCounter */
	Predicted,
	/** HitCounter moved inside the window of a prediction */
	Confirmed,
	/** HitCounter moved with no prediction pending (another player's hit, or a hit seen only by the authority) */
	Remote,
	/** The attacker's prediction vanished (a correction): effects ended at once */
	Cancelled,
	/** No HitCounter change in the confirm window: effects ended */
	Expired
};

USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionFlashParams
{
	GENERATED_BODY()

	/** Flash fades from 1 to 0 over this time (custom primitive data, no material instance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FlashSeconds = 0.12f;

	/** Puppet animation paused this long at a hit (the hitstop look; the wire does not carry the hitstop length) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PauseSeconds = 0.1f;

	/** A predicted hit with no HitCounter change within this time ends (8.4: 500 ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ConfirmWindowSeconds = 0.5f;

	/**
	 * Adaptive window on a client (G7b part 2: at 2 % loss 11 of 27 lagcomp confirms landed after the 500 ms window and counted expired, then
	 * remote). The confirm rides the host's next snapshot, so under loss it is late by about a round trip plus a retransmitted snapshot, and
	 * no loss figure reaches presentation code: a client with a measured ping (> 0) waits max(ConfirmWindowSeconds, RemoteConfirmFloorSeconds,
	 * RttFactor x RTT + JitterMarginSeconds), capped at MaxConfirmWindowSeconds. No ping (the host, tests, no player state) keeps
	 * ConfirmWindowSeconds, and a confirm inside 500 ms never depended on the window, so the 0 % behaviour is unchanged.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RemoteConfirmFloorSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0"))
	float RttFactor = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float JitterMarginSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxConfirmWindowSeconds = 1.5f;

	/** The window to use for a prediction made now; RttSeconds <= 0 means unknown */
	float ResolveConfirmWindow(float RttSeconds) const
	{
		if (RttSeconds <= 0.f)
		{
			return ConfirmWindowSeconds;
		}
		const float Window = FMath::Max3(ConfirmWindowSeconds, RemoteConfirmFloorSeconds, RttFactor * RttSeconds + JitterMarginSeconds);
		return FMath::Max(ConfirmWindowSeconds, FMath::Min(Window, MaxConfirmWindowSeconds));
	}
};

/**
 * Follows one minion's hit presentation: a predicted hit starts flash and pause immediately; the authoritative HitCounter (3 bits on the
 * wire, compared modulo 8) confirms it if it moves within the confirm window, otherwise the effect ends there; a cancelled prediction
 * ends it at once. A counter change with no prediction pending plays the same effects (other clients, the host's view of a remote hit).
 * The first counter seen is a baseline only, so a late joiner does not flash every minion. Pure; the caller passes the times.
 */
class GSTNMINIONCORE_API FGSTN_MinionHitFlash
{
public:
	/** The authoritative counter as this machine sees it now */
	void ObserveCounter(uint8 Counter, double NowSeconds, const FGSTN_MinionFlashParams& Params);

	void OnPredictedHit(double NowSeconds, const FGSTN_MinionFlashParams& Params);

	void OnPredictionCancelled(double NowSeconds);

	/** Expires an unconfirmed prediction */
	void Tick(double NowSeconds);

	/** Flash value in [0, 1] for the custom primitive data */
	float GetFlash(double NowSeconds, const FGSTN_MinionFlashParams& Params) const;

	bool IsPaused(double NowSeconds) const;

	EGSTN_MinionFlashState GetState() const { return State; }
	int32 GetPendingPredictions() const { return Pending; }

	static const TCHAR* StateName(EGSTN_MinionFlashState Value);

	int32 NumPredicted = 0;
	int32 NumConfirmed = 0;
	int32 NumCancelled = 0;
	int32 NumExpired = 0;
	int32 NumRemote = 0;

private:
	void StartEffect(double NowSeconds, const FGSTN_MinionFlashParams& Params);
	void EndEffect();

	EGSTN_MinionFlashState State = EGSTN_MinionFlashState::None;
	bool bHasCounter = false;
	uint8 LastCounter = 0;
	int32 Pending = 0;
	double ConfirmDeadline = 0.0;
	double FlashStart = -1.0;
	double PauseEnd = -1.0;
};

namespace GSTNMinionPresent
{
	/**
	 * The MaxVisible nearest entries of DistancesSq (nearest first, ties by lower index), written as indices into OutVisible. Entries with
	 * bHasPuppet set count as (1 - StickyFraction) times nearer, so a puppet at the cut does not flip between two minions every frame.
	 * Pure; the subsystem hides (and pools) the rest
	 */
	GSTNMINIONCORE_API void SelectNearest(TConstArrayView<float> DistancesSq, TConstArrayView<bool> bHasPuppet, int32 MaxVisible, float StickyFraction,
		TArray<int32>& OutVisible);
}
