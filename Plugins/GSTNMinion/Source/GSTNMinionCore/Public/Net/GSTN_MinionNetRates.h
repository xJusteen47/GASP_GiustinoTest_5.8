// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "LOD/GSTN_MinionLOD.h"

struct FGSTN_MinionNetWireItem;

/**
 * Per-item replication rates (combat-m8-minions.md 7 "Replication rate is per item", 8.1): an item is re-sent at most at its own
 * tier's rate (T0 15 Hz, T1 5 Hz, T2 1 Hz, T3 never) and the pack's NetUpdateFrequency is only a ceiling, the best member's rate.
 *
 * Time is the fixed simulation frame (62.5 Hz, 16 ms), never the wall clock, so the server, a replay and a test agree. The frame does
 * not divide the periods (66.7 ms is 4.17 frames), so the limiter keeps time in thousandths of a frame and is a GCRA (the "virtual
 * scheduling" form of a leaky bucket) with a tolerance just under one frame: the tolerance absorbs only the rounding of a period to
 * whole frames, never a burst. Guarantee, tested: in any span of S frames between two sends an item (or a pack) sends at most
 * 1 + floor((S * 1000 + Tolerance) / Period) times, so the long-run rate never exceeds the nominal one and a continuously dirty T0
 * item still gets about 15 Hz instead of the 12.5 Hz a whole-frame gap of 5 would give.
 */
namespace GSTNMinionNetRate
{
	/** Fixed simulation rate in milli-hertz: 62.5 Hz */
	constexpr int32 FixedTickMilliHz = 62500;

	/** Thousandths of a frame the limiter tolerates early: the rounding of a period to whole frames, never a whole frame */
	constexpr int32 ToleranceMilliFrames = 999;

	/** Bytes one item update is planned at (8.2 "planning size") for every byte budget, until R1 measures it */
	constexpr int32 PlanningBytesPerItem = 20;

	/** Nominal rate of a tier in Hz (T0 15, T1 5, T2 1, T3 0); the T0 value is the one set by SetCombatRateHz (G11 fallback 2) */
	GSTNMINIONCORE_API int32 RateHz(EGSTN_MinionLODTier Tier);

	constexpr int32 DefaultCombatRateHz = 15;
	/** Ceiling of the T0 rate (G12b): above the table, for hosts that can afford smoother near minions; at most half the 62.5 Hz fixed tick */
	constexpr int32 MaxCombatRateHz = 30;

	/**
	 * Host fallback 2 of 4d.1: the T0 send rate, clamped to 5..MaxCombatRateHz (15 = the table). Process-wide, set by the Runtime from the
	 * gstn.Minion.T0RateHz cvar (Core reads no cvar); the default leaves every byte as it was. Game thread only
	 */
	GSTNMINIONCORE_API void SetCombatRateHz(int32 Hz);
	GSTNMINIONCORE_API int32 GetCombatRateHz();

	/** Period of a rate in thousandths of a frame, rounded up so the rate is never exceeded; MAX_int32 for a rate of 0 */
	GSTNMINIONCORE_API int32 PeriodMilliFrames(int32 RateHz);

	/** Pack ceiling: the best (highest) rate of the member tiers, 0 when every member is T3 (the pack may go dormant) */
	GSTNMINIONCORE_API int32 PackCeilingHz(TConstArrayView<uint8> MemberTiers);

	/** Planned bytes per second of one item of a tier (rate x PlanningBytesPerItem) */
	GSTNMINIONCORE_API int32 PlannedBytesPerSecond(EGSTN_MinionLODTier Tier);

	/**
	 * The dirty rule of 8.2 on two quantized items: position moved more than 3 cm (euclidean, in 1 cm steps), yaw more than 4 degrees
	 * (wrapped), or any other field changed. FrameStampLow is ignored (it changes every frame); Id, type and global id are identity,
	 * compared too so a re-used item id is never skipped
	 */
	GSTNMINIONCORE_API bool IsSignificantChange(const FGSTN_MinionNetWireItem& Sent, const FGSTN_MinionNetWireItem& Candidate);
}

/**
 * The GCRA limiter of one item or one pack: 8 bytes of authority state, no clock. CanSend then OnSent in the frame the item really
 * goes out; a frame where the item is clean consumes nothing, so an idle minion costs zero bytes and loses no credit beyond one period.
 *
 * It keeps the virtual time of the last send rather than the next allowed time, and applies the CURRENT rate's period at the check:
 * a minion that wakes from T2 (1 s period) into combat gets its first T0 update within a T0 period, not a second later. For a constant
 * rate this is exactly the GCRA above (virtual last send = max(previous + period, now)).
 */
struct GSTNMINIONCORE_API FGSTN_MinionNetRateLimiter
{
	/** True when a send at Frame keeps the rate: always false for a rate of 0 (T3), always true before the first send */
	bool CanSend(int32 Frame, int32 RateHz) const;

	/** Records a send at Frame (any rate, 0 included: an add of a T3 minion is still a send) */
	void OnSent(int32 Frame, int32 RateHz);

	void Reset() { VirtualLastSend = MIN_int64; }

private:
	/** Virtual time of the last send in thousandths of a frame; MIN_int64 = never sent */
	int64 VirtualLastSend = MIN_int64;
};

namespace GSTNMinionNetRate
{
	/**
	 * The sends of one pack at Frame, the whole tier scheduler in one call (the Runtime publish and the tests run this same code).
	 * The pack's own limiter at CeilingHz gates the frame first, so the actor never has items marked faster than its
	 * NetUpdateFrequency; then each dirty member whose own limiter allows goes out. Tiers, Dirty and ItemLimiters are parallel
	 * (one entry per item slot of the pack). Appends the sent indices in ascending order and returns their count; the pack limiter
	 * is consumed only when something was sent
	 */
	GSTNMINIONCORE_API int32 SelectPackSends(int32 Frame, FGSTN_MinionNetRateLimiter& PackLimiter, int32 CeilingHz, TConstArrayView<uint8> Tiers,
		TConstArrayView<bool> Dirty, TArrayView<FGSTN_MinionNetRateLimiter> ItemLimiters, TArray<int32>& OutSent);
}
