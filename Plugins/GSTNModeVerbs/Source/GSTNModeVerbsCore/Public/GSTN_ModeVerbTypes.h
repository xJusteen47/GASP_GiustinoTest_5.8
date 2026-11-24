// Mode verbs, pure core (no GAS, no Network Prediction, no Mover, no world).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEvent.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_ModeVerbTypes.generated.h"

/** Where a verb gets its target (game-modes 2.4, the "Target source" column) */
UENUM(BlueprintType)
enum class EGSTN_VerbTargetSource : uint8
{
	/** No target: the verb is about the player (call extraction from anywhere, a taunt) */
	None,
	/** The interact probe's focus: a requestable prop (Interact.Target.Objective). The host resolves and validates it */
	InteractFocus,
	/** The camera aim of the confirming command: the host turns it into a world point from the pawn's position */
	AimPoint
};

/** Why the host refused a confirmed verb. Published as Message.Mode.VerbRefused (Magnitude = the value); changes no state anywhere */
UENUM(BlueprintType)
enum class EGSTN_VerbRefusal : uint8
{
	None = 0,
	/** The verb needs a target and the host found none (nothing in reach, or an unknown id) */
	NoTarget,
	/** The prop does not accept this verb */
	WrongVerb,
	/** The prop's record ended before the confirmation frame (a late remove: another player won, the mission moved on) */
	Removed,
	/** The confirmation frame is before the prop's ActionableFrom (the mission has not armed it yet) */
	NotActionable,
	/** Out of reach even with the lag slack */
	OutOfRange,
	/** The prop has no uses left */
	Exhausted
};

/**
 * What one mode verb is, as data on the ability (game-modes 2.4): the identity tag the frameworks subscribe by, how long the predicted hold
 * lasts after the ability starts, where the target comes from and how far it may be. All of it is final before the first NP tick of any pawn
 * (a Sim-class toggle, game-modes 2.5): never changed while a pawn exists.
 */
USTRUCT(BlueprintType)
struct GSTNMODEVERBSCORE_API FGSTN_VerbSpec
{
	GENERATED_BODY()

	/** Identity of the verb (Ability.Mode.<Name>); copied onto every event the verb produces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb", meta = (Categories = "Ability.Mode"))
	FGameplayTag VerbTag;

	/**
	 * Frames the ability holds, after it starts, before it confirms. 0 = confirm in the activation frame, for a verb whose command already is the
	 * hold (Command.Mode.Interact = Hold(Interact)); more = a channel the player keeps up (a charge), cancelled by releasing the button
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 HoldFrames = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	EGSTN_VerbTargetSource TargetSource = EGSTN_VerbTargetSource::InteractFocus;

	/** Reach of the target, or of the aim point. 0 = the interact rules' reach (and the record's own override) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb", meta = (ClampMin = 0, ForceUnits = "cm"))
	float MaxRange = 0.f;

	bool IsValid() const { return VerbTag.IsValid(); }
};

/**
 * One confirmed verb as the host publishes it (game-modes 2.4: verb tag, instigator, target, quantised aim point, magnitude). The event on the
 * stream (FGSTN_SimEvent) carries everything except the aim point, which has no field there: the aim point is delivered to C++ listeners by
 * UGSTN_VerbSubsystem::OnModeVerb. Round trips through ToEvent / FromEvent except for that point.
 */
USTRUCT(BlueprintType)
struct GSTNMODEVERBSCORE_API FGSTN_ModeVerbPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb", meta = (Categories = "Ability.Mode"))
	FGameplayTag VerbTag;

	/** The pawn that did it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	TWeakObjectPtr<AActor> Instigator;

	/** The prop it acted on, if any */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	TWeakObjectPtr<AActor> Target;

	/** Simulation frame the verb was confirmed on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	int32 ConfirmFrame = INDEX_NONE;

	/** Aim target in whole centimetres (the same quantisation as the interact records). Zero when the verb has no aim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	FIntVector AimPointCm = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verb")
	float Magnitude = 0.f;

	static FIntVector QuantiseAimCm(const FVector& Point);

	/** The stream form: EventType Message.Mode.Verb, Tags = VerbTag + IdentityTags, plus instigator, target, frame, magnitude */
	FGSTN_SimEvent ToEvent(const FGameplayTagContainer& IdentityTags = FGameplayTagContainer()) const;

	/** The payload of a Message.Mode.Verb event (the verb is the event's Ability.Mode.* tag). The aim point is zero */
	static FGSTN_ModeVerbPayload FromEvent(const FGSTN_SimEvent& Event);
};

/**
 * What an ability writes at its confirmation frame, in the simulation, on every machine and in every re-simulation: plain numbers, no decision
 * (the host decides outside the simulation). TargetKey is the interact focus id when the activation context carries it (FGSTN_InteractCandidateId::ToKey;
 * 0 = the host resolves the nearest prop in reach, the fallback until the TargetKey seam of interaction.md 3.3 lands).
 */
struct GSTNMODEVERBSCORE_API FGSTN_VerbConfirmRequest
{
	FGSTN_VerbSpec Spec;
	FGSTN_ModeVerbPayload Payload;
	/** UObject id of the instigator: part of the once-only key, so two players confirming on one frame are two verbs */
	uint32 InstigatorId = 0;
	uint64 TargetKey = 0;
	/** The command's camera aim, when the verb aims */
	FRotator Aim = FRotator::ZeroRotator;
	bool bHasAim = false;

	/** (instigator, verb, frame): the identity of one confirmation */
	bool SameConfirmation(const FGSTN_VerbConfirmRequest& Other) const
	{
		return InstigatorId == Other.InstigatorId && Payload.VerbTag == Other.Payload.VerbTag && Payload.ConfirmFrame == Other.Payload.ConfirmFrame;
	}
};

/**
 * The seam between a simulation that may run a frame many times and an event that must exist once (the same shape as FGSTN_SimEventOutbox, which
 * it complements for abilities that have no timeline to carry an outbox key). The ability calls Add unconditionally, in every tick on every
 * machine, with no role or re-simulation branch (CLAUDE.md rule 1). The subsystem calls Flush after each fixed tick: only the authority publishes,
 * outside a re-simulation, and a confirmation published once is never published again, whatever a rollback appends later.
 * Not thread safe; one queue per world.
 */
class GSTNMODEVERBSCORE_API FGSTN_VerbConfirmQueue
{
public:
	static constexpr int32 MaxPending = 32;
	/** Confirmations remembered after publishing, to refuse a duplicate a late re-simulation appends */
	static constexpr int32 MaxHistory = 64;

	/** Appends a confirmation unless the same one is already pending or was published. Capped: the oldest pending one is dropped first */
	void Add(const FGSTN_VerbConfirmRequest& Request);

	/**
	 * bAuthority false: everything pending is thrown away unseen (a client's predictions) and 0 is returned.
	 * bResimulating: nothing is published and nothing is dropped; the next real flush decides (the history refuses duplicates).
	 * Otherwise publishes, in order, every pending confirmation whose frame is at or before ServerFrame and remembers it.
	 * @return how many were published
	 */
	int32 Flush(bool bAuthority, bool bResimulating, int32 ServerFrame, TFunctionRef<void(const FGSTN_VerbConfirmRequest&)> Publish);

	int32 NumPending() const { return Pending.Num(); }
	void Reset() { Pending.Reset(); History.Reset(); }

private:
	bool WasPublished(const FGSTN_VerbConfirmRequest& Request) const;

	TArray<FGSTN_VerbConfirmRequest> Pending;
	TArray<FGSTN_VerbConfirmRequest> History;
};

/**
 * Whether a requestable prop may accept a verb at a frame: stamps, verb, uses and reach, as plain numbers so the host's decision is tested with no
 * world. The order is the order of the answers: an ended record is "Removed" even when it was also out of reach.
 */
struct GSTNMODEVERBSCORE_API FGSTN_RequestableGate
{
	struct FInput
	{
		/** The frame the verb confirmed on */
		int32 Frame = 0;
		/** The record's stamps (FGSTN_InteractCandidate::ActionableFrom and ToFrame) */
		int32 ActionableFrom = 0;
		int32 ToFrame = MAX_int32;
		/** Distance from the instigator to the prop's surface (DistanceToSurfaceCm) */
		float DistanceCm = 0.f;
		float ReachCm = 0.f;
		/** The server's lag slack on top of the reach (FGSTN_InteractRules::ServerReachSlackCm) */
		float SlackCm = 0.f;
		bool bVerbAccepted = true;
		/** Uses left, -1 = unlimited */
		int32 UsesLeft = -1;
	};

	static EGSTN_VerbRefusal Evaluate(const FInput& Input);

	/** max(0, distance from From to the record's anchor - its radius): how reach is measured (interaction.md 2.2) */
	static float DistanceToSurfaceCm(const FVector& From, const FIntVector& PositionCm, int32 RadiusCm);
};
