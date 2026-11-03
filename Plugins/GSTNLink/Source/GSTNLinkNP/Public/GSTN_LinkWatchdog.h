// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Placement/GSTN_ExitCollisionQuery.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "State/GSTN_LinkState.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LinkWatchdog.generated.h"

class UGSTN_LinkTypeDefinition;

/** Exit flags of a LinkCommand (FGSTN_LinkCommandEntry::ExitFlags, copied to FGSTN_LinkActions::FExit::Flags) */
namespace GSTNLinkExitFlags
{
	/** The side is moved to the exit transform on the command's frame (the parked role of a broken link) */
	constexpr uint8 Teleport = 1 << 0;

	/** The placer found no free place: the exit is the fallback next to the partner (logged by the watchdog) */
	constexpr uint8 Fallback = 1 << 1;

	/**
	 * The parked initiator placed this exit itself from its view (M7 G6, UGSTN_LinkTypeDefinition::bInitiatorPlacesExit) and ends its link on
	 * the exit's frame by itself; a watchdog command that copies it keeps the flag, so both paths leave the same record
	 */
	constexpr uint8 InitiatorPlaced = 1 << 2;
}

/**
 * One combat entity as the watchdog reads it at the end of a frame: values only, taken from the authority's finalized state (the combat
 * component's sync state, the pawn's transform and capsule, its ability system tags). The pure evaluator reads nothing else.
 */
struct GSTNLINKNP_API FGSTN_LinkWatchdogEntity
{
	int32 EntityId = INDEX_NONE;

	/** The frame the entity's state is the end of */
	int32 Frame = INDEX_NONE;

	FGSTN_LinkStates Links;

	/** The root (capsule centre) and the capsule, for the distance rule and the exit placer */
	FTransform Transform = FTransform::Identity;
	/** World velocity (the pawn's on the authority, the snapshot's in an initiator-placed exit): the exit query's PartnerVelocity (M7 G9) */
	FVector Velocity = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	float CapsuleRadius = 34.f;
	float CapsuleHalfHeight = 88.f;

	/** Knocked out (EGSTN_ActionState::KO): OwnDeath / PartnerDeath */
	bool bDead = false;

	/** The state tags the entity projects (hitstun, tumble, knockdown...), for the partner's PartnerMustNot */
	FGameplayTagContainer StateTags;

	/** The partner move the entity plays (M7 G2: its link actions' Active), for the exit of a move's outcome break */
	FGSTN_LinkActions::FActive ActiveMove;

	/** The exit record of its link actions (M7 G6: an initiator-placed exit the break copies) */
	FGSTN_LinkActions::FExit Exit;
};

/** One inbox entry the watchdog posts: a LinkCommand stamped StampFrame for TargetId, sent by SenderId (the partner) */
struct GSTNLINKNP_API FGSTN_LinkWatchdogPost
{
	int32 TargetId = INDEX_NONE;
	int32 SenderId = INDEX_NONE;
	int32 StampFrame = INDEX_NONE;
	FGSTN_LinkCommandEntry Command;
};

/** A break the watchdog decided (one per link, or one per orphaned side), for events and logs */
struct GSTNLINKNP_API FGSTN_LinkWatchdogBreak
{
	int32 EntityId = INDEX_NONE;
	int32 PartnerId = INDEX_NONE;
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;
	uint8 Serial = 0;
	uint8 LinkTypeIndex = 0;
	EGSTN_LinkBreakReason Reason = EGSTN_LinkBreakReason::None;
	bool bOrphan = false;
	bool bExitFallback = false;
};

/**
 * The watchdog's rules as a pure function (m6 spec 7.5, 7.6, 7.7), for the subsystem and the tests. For every link of the entities:
 *
 *  - a Serial-matched pair (both sides hold the other end of the same serial, Active) is evaluated once, from each side's point of view, by
 *    UGSTN_LinkMath::EvaluateBreak with the definition's break rules and MaxFrames: a PendingBreak (the LinkBreakOnHit step, a Release or
 *    an Escape of the link tick) wins with its own reason, then death, partner state, distance (the parked capsule against where the link
 *    expects it: the F5b rule, a leash from where the parked role stands), the mash end and the timeout. One reason per link: the first side
 *    in entity id order that breaks.
 *  - a side whose partner does not hold the same link is an ORPHAN once the formation grace (OrphanGraceFrames after its LinkFrame) is over;
 *    it breaks alone (Forced).
 *  - a break posts LinkCommand{Break, Serial, Reason} stamped Frame to BOTH inboxes (to the orphan's only), consumed at Frame + 1 on both, so
 *    both sides clear the link the same frame. The definition's parked role gets its exit from the definition's exit placer (Teleport flag,
 *    the placer's transform quantised to floats as the command carries it, the placer's velocity); a type whose request rules hold a grab rule gives
 *    its target the grab immunity end (that rule's ImmunityFramesAfterRelease, or AfterThrow for a throw: the type's data, not the kit's).
 *  - M7 G6, a type with bInitiatorPlacesExit: a parked side that carries its own exit for the next frame (InitiatorPlaced, its PendingBreak
 *    set) gets that exit copied into its command when the place is free in the world (the pair ignored), else the placer's own (one
 *    correction on its machine). A side whose partner already ended this very serial by itself (an idle channel, same serial, a reason: the
 *    frame the watchdog did not see) is broken at once with the partner's reason, not as an orphan.
 *
 * Deterministic for equal inputs: entities are walked in entity id order, the placer's candidates come in its fixed order. Everything per link
 * type comes from its definition (break rules, placer, parked role), so a new type (a drill latch) needs only its own definition.
 */
struct GSTNLINKNP_API FGSTN_LinkWatchdogLogic
{
	struct FParams
	{
		/** Frames after a LinkFrame before a one-sided link counts as an orphan (the settings' OrphanGraceFrames) */
		int32 OrphanGraceFrames = 3;

		/** The world for the exit placer, or null (the placer then takes its first candidate). Used when WithPairCollision is unset */
		const IGSTN_ExitCollisionQuery* Collision = nullptr;

		/**
		 * The world for the exit of ONE pair (F15, HANDOVER 58 OPEN 2): calls Use with a query that ignores only the exiting and the staying
		 * entity, so a third pawn (a bystander) blocks a place like any wall. Unset = Collision for every pair
		 */
		TFunction<void(int32 ExitingId, int32 StayingId, TFunctionRef<void(const IGSTN_ExitCollisionQuery*)> Use)> WithPairCollision;

		/** The definition of a link type index; null = unknown (the link is left alone) */
		TFunction<const UGSTN_LinkTypeDefinition*(uint8)> Definitions;

		/**
		 * The exit placer of a partner move (M7 G2: the type's action provider's ExitPlacer of the move id), or null for the type's own. Asked for
		 * a Body break while either side still plays a move: an outcome Break keeps the move until the break, an abort clears it first, so an
		 * aborted move leaves through the default exit. Unset = the type's placer always
		 */
		TFunction<const UGSTN_LinkExitPlacer*(uint8 TypeIndex, int16 ActionId)> ActionExitPlacer;
	};

	/** Entities must be sorted by EntityId. Frame is the frame all of them are the end of (the stamp of the posts) */
	static void Evaluate(TConstArrayView<FGSTN_LinkWatchdogEntity> Entities, int32 Frame, const FParams& Params, TArray<FGSTN_LinkWatchdogPost>& OutPosts,
		TArray<FGSTN_LinkWatchdogBreak>& OutBreaks);

	/** The exit of the parked side of a link: the definition's placer around the staying entity, quantised as the command carries it */
	static FGSTN_ExitResult PlaceExit(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_LinkWatchdogEntity& Exiting, const FGSTN_LinkWatchdogEntity& Staying,
		const IGSTN_ExitCollisionQuery* Collision, const UGSTN_LinkExitPlacer* Placer = nullptr);

	/** How far the parked entity's capsule stands from the placement the link expects for it (0 for a type that parks nobody) */
	static float ParkedDistance(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_LinkWatchdogEntity& Initiator, const FGSTN_LinkWatchdogEntity& Target);
};

/** The real world asked by the exit placer: a capsule overlap against blocking geometry, the actors it is given ignored (the pair). Server only */
class GSTNLINKNP_API FGSTN_WorldExitCollisionQuery : public IGSTN_ExitCollisionQuery
{
public:
	FGSTN_WorldExitCollisionQuery(const UWorld* InWorld, TArray<const AActor*, TInlineAllocator<2>> InIgnored) : World(InWorld), Ignored(MoveTemp(InIgnored)) {}

	virtual bool IsCapsuleFree(const FVector& Location, const FQuat& Rotation, float Radius, float HalfHeight) const override;

private:
	const UWorld* World = nullptr;
	TArray<const AActor*, TInlineAllocator<2>> Ignored;
};

/**
 * The only break authority of links (m6 spec 7.6). A world subsystem that exists in every game world but works only where the combat state
 * is authoritative, the server (a dedicated server or the listen host): THE documented exception to "no net mode in link code", because it
 * is not simulation code. It runs once per engine frame after the actor tick, which is after Network Prediction finalized the frame
 * (FinalizeFrame, combat.md 5.6: the publication point of authoritative results), reads every registered combat entity's finalized state of
 * that frame F, runs FGSTN_LinkWatchdogLogic and posts the commands stamped F to the inboxes (the authority path every inbox entry takes),
 * consumed at F + 1 on both sides: two frames after the cause, one correction on a client. A command already posted for a serial is not
 * posted again while it waits for its frame. Events LinkBroken (Magnitude = reason index) and GrabEscaped go through UGSTN_SimEventSubsystem.
 *
 * With several fixed ticks per engine frame (a slow host) the watchdog sees only the latest of them: every break condition is state that
 * holds until the break (a pending flag, a death, a distance, a timeout), so a break is never missed, it lands on the frame after the latest.
 */
UCLASS()
class GSTNLINKNP_API UGSTN_LinkWatchdog : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_LinkWatchdog* Get(const UWorld* World);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** One evaluation now (the post-actor-tick hook calls it; tests and tools may too). Returns the number of breaks posted */
	int32 Evaluate();

	/** Breaks posted since the world began (diagnostic and harness) */
	int32 GetNumBreaks() const { return NumBreaks; }
	int32 GetNumOrphans() const { return NumOrphans; }
	const FGSTN_LinkWatchdogBreak& GetLastBreak() const { return LastBreak; }
	int32 GetLastBreakFrame() const { return LastBreakFrame; }

private:
	void OnPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	bool IsAuthority() const;

	FDelegateHandle PostActorTickHandle;

	/** (entity id << 8 | channel << 7 ... ) -> frame a command for that serial was posted; see Evaluate */
	TMap<uint64, int32> PostedFrames;

	int32 NumBreaks = 0;
	int32 NumOrphans = 0;
	FGSTN_LinkWatchdogBreak LastBreak;
	int32 LastBreakFrame = INDEX_NONE;
};
