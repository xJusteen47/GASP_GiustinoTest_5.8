// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UClass;
class UGSTN_LinkRegistry;
struct FGSTN_CombatTimelineData;
struct FGSTN_LinkFragment;

/**
 * The partner window of one pair move (FP G1, shared with G4): the frames in which the PARTNER's authored part of a move plays, derived from the
 * INITIATOR's fragment alone. When the initiator's move crosses a PartnerMove key at frame S, its fragment records the active move (Id, S, side
 * Initiator); the partner's part starts at S + max(PartnerStartOffsetFrames, 1) (the same clamp as StartPartnerMove) and lasts the partner
 * timeline's TotalFrames.
 *
 * Why it exists: the partner's real part is started on the server one network hop later and shown on the initiator's screen another hop after
 * that (G0b: ~350 ms after the initiator's own action). The initiator already knows everything needed to show it now: G1 plays the partner's
 * authored reaction from this window as presentation only; G4 will promote the partner to forward prediction inside the same window, with the
 * same frames, so neither redoes the other.
 *
 * Pure and deterministic: it reads the fragment, the registry's content and the timeline data the caller hands over, never a world, a clock or a
 * component, so the simulation can call it too (G4: promote the partner during [PartnerStartFrame - lead, PartnerEndFrame)). The same inputs give
 * the same window on every machine and on every resimulation.
 */
struct GSTNLINKNP_API FGSTN_LinkPartnerWindow
{
	/** The link serial of the initiator's Body link; a window is about that very link */
	uint8 Serial = 0;

	/** The action provider's move id (1..32767) */
	int16 MoveId = 0;

	/** The initiator's frame S at which its move crossed the PartnerMove key */
	int32 InitiatorFrame = 0;

	/** First frame the partner's part plays (S + max(offset, 1)) */
	int32 PartnerStartFrame = 0;

	/** First frame the partner's part no longer plays (exclusive): start + the partner timeline's TotalFrames, else start + MaxDurationFrames */
	int32 PartnerEndFrame = 0;

	/** The partner's ability (a combat timeline ability of its kit), null when the action plays nothing on the partner */
	const UClass* PartnerAbility = nullptr;

	/** PartnerEndFrame comes from an authored length (the timeline's TotalFrames or the action's MaxDurationFrames). When false the window lasts as long as the initiator's record */
	bool bLengthKnown = false;

	bool bValid = false;

	/** Frame of the partner's timeline at LocalFrame (0 at PartnerStartFrame; negative before it) */
	int32 TimelineFrameAt(int32 LocalFrame) const { return LocalFrame - PartnerStartFrame; }

	/** The partner's part plays at LocalFrame */
	bool IsInside(int32 LocalFrame) const { return bValid && LocalFrame >= PartnerStartFrame && (!bLengthKnown || LocalFrame < PartnerEndFrame); }

	/** The same move of the same link (a record that changed Id or start frame is another window) */
	bool IsSameMove(const FGSTN_LinkPartnerWindow& Other) const
	{
		return bValid && Other.bValid && Serial == Other.Serial && MoveId == Other.MoveId && InitiatorFrame == Other.InitiatorFrame;
	}

	/** Hands over the frame data of a partner ability (null = unknown). A function, not a table, so the simulation, the presentation and the tests each bring their own */
	using FTimelineResolver = TFunctionRef<const FGSTN_CombatTimelineData*(const UClass* PartnerAbility)>;

	/**
	 * Builds the window of the move the INITIATOR's fragment records (Actions.Active with side Initiator, on an Active Body link). False, Out left
	 * invalid, when the fragment records no initiator move, the link is not Active, or the registry does not know the move's id.
	 * ResolveTimeline may return null (then the end frame falls back to start + MaxDurationFrames, and with neither the length is unknown)
	 */
	static bool BuildFromInitiator(const FGSTN_LinkFragment& InitiatorFragment, const UGSTN_LinkRegistry* Registry, FTimelineResolver ResolveTimeline, FGSTN_LinkPartnerWindow& Out);
};

/**
 * The pure rules of the predicted partner presentation (FP G1): positions, the lead rule, the end rule and the drop rule, split from the
 * component so they can be tested without a world, a mesh or a montage.
 */
struct GSTNLINKNP_API FGSTN_LinkPartnerPlayback
{
	/** The montage position (seconds) the partner shows at TimelineFrame: the section's start + frames x step x rate. Frames before the start show the start */
	static float PredictedPosition(int32 TimelineFrame, float StepSeconds, float PlayRate, float SectionStartSeconds)
	{
		return SectionStartSeconds + static_cast<float>(FMath::Max(TimelineFrame, 0)) * StepSeconds * PlayRate;
	}

	/** The "presentation lead" rule: the instance shows the later of its own position and the predicted one */
	static float LeadPosition(float InstancePosition, float PredictedPosition) { return FMath::Max(InstancePosition, PredictedPosition); }

	/** The instance is behind the prediction by more than Epsilon: write the predicted position. Inside it the instance is left alone (no notify churn) */
	static bool ShouldWriteLead(float InstancePosition, float PredictedPosition, float Epsilon) { return PredictedPosition - InstancePosition > Epsilon; }

	/** The prediction has reached the end of the montage (or of its section): the instance blends out */
	static bool HasReachedEnd(float PredictedPosition, float EndSeconds) { return PredictedPosition >= EndSeconds; }

	/**
	 * The initiator's record of the move vanished (refusal, abort, rollback, link ended). The partner's part is dropped (stop at once) when it had
	 * not begun at LocalFrame, or when the vanishing was an abort (a Forced pending break was seen). After the partner's start a vanished record
	 * is a normal outcome (KeepLink, Break at the outcome frame, an interrupted initiator): the partner's part plays on alone, so the window runs
	 * to its end
	 */
	static bool IsDroppedOnRecordLoss(const FGSTN_LinkPartnerWindow& Window, int32 LocalFrame, bool bAbortSeen)
	{
		return bAbortSeen || LocalFrame < Window.PartnerStartFrame;
	}

	/** Frames after the predicted end during which the synced copy of the montage is kept stopped at least (the proxy lags the prediction by ~14 frames, G0b, plus jitter) */
	static constexpr int32 MinSuppressFrames = 30;

	/** Frames after the predicted end at which the suppression gives up whatever the partner's shown state says (~2 s) */
	static constexpr int32 MaxSuppressFrames = 125;

	/**
	 * The synced copy may play again: the partner's shown state has been seen playing the move and is not any more, and the minimum hold has
	 * passed; or the maximum hold has passed (the partner never showed it: refused on the server, or the proxy is far behind)
	 */
	static bool IsSuppressionOver(int32 FramesSincePredictedEnd, bool bPartnerSeenPlaying, bool bPartnerPlayingNow)
	{
		if (FramesSincePredictedEnd >= MaxSuppressFrames)
		{
			return true;
		}
		return FramesSincePredictedEnd >= MinSuppressFrames && bPartnerSeenPlaying && !bPartnerPlayingNow;
	}
};
