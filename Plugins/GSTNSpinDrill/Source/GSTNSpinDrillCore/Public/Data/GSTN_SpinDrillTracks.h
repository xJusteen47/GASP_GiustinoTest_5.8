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
