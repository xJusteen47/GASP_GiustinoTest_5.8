// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_MoveEvents.h"
#include "GameplayTagContainer.h"
#include "GSTN_TimelinePlay.generated.h"

/** How a play of Play Combat Timeline And Wait ended, as the node's pins say it */
UENUM(BlueprintType)
enum class EGSTN_TimelinePlayResult : uint8
{
	/** The timeline played to its last frame */
	Completed,

	/** Something cut it: a hit, a command through a cancel window, a tail cancel, a ricochet recoil, another Play of the same ability */
	Interrupted,

	/** The ability ended or was cancelled while the timeline ran, or the play never started */
	Cancelled,
};

/** Why a request to start a timeline from an ability graph was refused (FGSTN_TimelinePlayRules::CheckStart). Logged, never shown in game */
enum class EGSTN_TimelineStartRefusal : uint8
{
	None,

	/** Called outside the ability system's simulation step (an input event, presentation code, a timer): a write there would not roll back */
	OutsideSimulation,

	/** The ability system is restoring a frame: nothing runs again until the re-simulation */
	Restoring,

	/** A loadout swap closed the gate this frame: no move starts until the new tables are here */
	GateClosed,

	/** The timeline is not in the registry for this ability (not in its Playable Timelines, or the move is not bound or adoptable) */
	NotRegistered,
};

/**
 * The rules of the timeline nodes (move-timeline-decoupling 1A and 1B), split out of UGSTN_GA_BaseCombat and the combat component so they run
 * in a test without a world or an ability system.
 *
 * Both nodes start a timeline IN THE FRAME OF THE CALL. The combat model ticks before the ability system in every frame, so the call runs after
 * the combat tick of its frame: the start is written into that frame's combat state from inside the ability system's simulation step (the
 * same state the ability reads in that step), exactly as the combat tick writes a command's start. It is deterministic because the call itself
 * is: the graph runs in the simulation step, on every machine and again in every re-simulation, after the combat tick of the same frame.
 * Outside the simulation step a write would be an out-of-band change no rollback undoes, so the request is refused there (CheckStart).
 */
struct GSTNCOMBATNP_API FGSTN_TimelinePlayRules
{
	/** Whether an ability may start a timeline now, and if not why */
	static EGSTN_TimelineStartRefusal CheckStart(bool bInSimulationTick, bool bRestoringFrame, bool bGateClosed, bool bEntryRegistered);

	/** The node pin a play ends on: Finished is Completed, an end of the ability is Cancelled, anything else Interrupted */
	static EGSTN_TimelinePlayResult ResolveResult(EGSTN_MoveEndReason Reason, bool bAbilityEnding);

	/**
	 * Whether a host (an ability with Play Timeline On Activation off) ends itself after a play ended with Result. A completed play never ends
	 * it: the graph decides what follows (another timeline, nothing, End Ability). An interrupted one ends it when bEndOnInterrupt is set
	 * (default): a hit or a cancel into another move ends the whole sequence, as it ends a standard move, with no wiring needed
	 */
	static bool ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult Result, bool bEndOnInterrupt);

	/**
	 * The loose tags a start written after the combat tick has to move: OutRemove holds the tags of Before that After lacks, OutAdd those of
	 * After that Before lacks. The combat tick projects the Tags tracks of the frame as such a difference; a later start in the same frame
	 * applies the difference between what it projected and what the new timeline holds, so the next frame's projection finds the right counts
	 */
	static void DiffTags(const FGameplayTagContainer& Before, const FGameplayTagContainer& After, FGameplayTagContainer& OutRemove, FGameplayTagContainer& OutAdd);
};
