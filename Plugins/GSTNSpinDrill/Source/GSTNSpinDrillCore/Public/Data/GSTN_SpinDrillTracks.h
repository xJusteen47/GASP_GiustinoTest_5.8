// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_SpinDrillTracks.generated.h"

/** Sense of a spin, as seen from above along the character's up axis (spin-drill.md 13c convention: clockwise = +1) */
UENUM(BlueprintType)
enum class EGSTN_SpinSense : uint8
{
	Clockwise,
	CounterClockwise,
};

/**
 * Marks a timeline as a spin (plan D4). The drill tick extension finds it on the timeline that started this frame and turns it into the
 * Spinning phase of the drill fragment, with the sense and the air flag written here. It is a plugin track: the combat core's kind enum is
 * closed, so the track reports no known kind, the timeline player ignores it, and no hook is registered for it (it never stalls and never
 * stamps). The track's range is the spin: EndFrame is where the low gravity of an air spin ends (plus LowGravityExtraFrames).
 *
 * The sense is data of the timeline and not of the command because the drill fragment is written inside the combat tick, before the
 * ability system knows which command fired; a kit therefore binds one ability class per sense (UGSTN_GA_SpinClockwise and its siblings).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spin"))
struct GSTNSPINDRILLCORE_API FGSTN_SpinTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spin")
	EGSTN_SpinSense Sense = EGSTN_SpinSense::Clockwise;

	/** The air spin: it counts against the air spins of the airtime and lowers gravity until EndFrame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spin")
	bool bAirSpin = false;

	bool IsClockwise() const { return Sense == EGSTN_SpinSense::Clockwise; }

	/** The first spin track of a timeline, null when it has none (a scan of the track array, done only on the frame a timeline starts) */
	static const FGSTN_SpinTrack* Find(const FGSTN_CombatTimelineData& Data);
};

/**
 * Marks a timeline as the opposite-spin cancel (plan D5, spin-drill.md 7.3, 14 SD.2): when it starts, the drill tick extension asks the
 * phase machine to stop the running spin, which it does only for the opposite sense and from CancelFromFrame on. Sense is the sense of the
 * rotation that cancels (a clockwise rotation cancels a counter-clockwise spin). A plugin track like FGSTN_SpinTrack: no kind, no hook.
 * The cancel timeline itself is the recovery: no hitbox, no movement, the momentum is kept.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spin Cancel"))
struct GSTNSPINDRILLCORE_API FGSTN_SpinCancelTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spin")
	EGSTN_SpinSense Sense = EGSTN_SpinSense::Clockwise;

	bool IsClockwise() const { return Sense == EGSTN_SpinSense::Clockwise; }

	/** The first spin cancel track of a timeline, null when it has none */
	static const FGSTN_SpinCancelTrack* Find(const FGSTN_CombatTimelineData& Data);
};

/**
 * Marks a timeline as the drill's dive (plan D8, spin-drill.md 7.4): when it starts during a spin, the drill tick extension enters the Diving
 * phase; while it runs, the fragment's phase turns a hit of the timeline into the bounce (D9 adds the latch first) and the ground into the
 * landing lag; the dive gravity (none) holds for the phase. A plugin track like the others: no kind, no hook.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Drill Dive"))
struct GSTNSPINDRILLCORE_API FGSTN_DiveTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The first dive track of a timeline, null when it has none */
	static const FGSTN_DiveTrack* Find(const FGSTN_CombatTimelineData& Data);
};
