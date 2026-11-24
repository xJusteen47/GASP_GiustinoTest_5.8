// Mode verbs, Network Prediction half (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractableComponent.h"
#include "GSTN_RequestableComponent.generated.h"

/**
 * The arming of a requestable prop as every machine sees it: the record's handle and the frames it is actionable from and until. Replicated, so
 * host and clients register the same record with the same stamps and the prop becomes usable on the same frame everywhere. Frames are the combat
 * simulation's (UGSTN_VerbSubsystem::GetSimFrame), the ones the interact probe queries with.
 */
USTRUCT()
struct GSTNMODEVERBSNP_API FGSTN_RequestableArming
{
	GENERATED_BODY()

	/** The record's handle (0 = not armed by Arm(): a start-armed prop keeps the interactable component's own record) */
	UPROPERTY()
	int32 Handle = 0;

	/** FromFrame and ActionableFrom of the new record; INDEX_NONE until armed */
	UPROPERTY()
	int32 ArmFrame = INDEX_NONE;

	/** Exclusive end of the current record; MAX_int32 while it is not disarmed */
	UPROPERTY()
	int32 DisarmFrame = MAX_int32;

	bool operator==(const FGSTN_RequestableArming& Other) const { return Handle == Other.Handle && ArmFrame == Other.ArmFrame && DisarmFrame == Other.DisarmFrame; }
};

/**
 * A prop a mode verb acts on: a terminal, a charge, a gate, the extraction console (game-modes 2.4). It is a GSTNInteract record with the meaning
 * Interact.Target.Objective plus the three things a mission needs from it:
 *
 *   - arming by frame (Arm): the mission framework arms the prop ahead of time, the record becomes focusable and actionable at ActionableFrom
 *     on every machine at once (interaction.md 3.2: records are stamped and never rolled back), so the simulation never sees a prop that exists on
 *     one machine only. A prop that is not armed has no record at all. Disarm ends the record at a frame; a press after it is one host refusal.
 *   - which verbs it accepts (AcceptedVerbs) and how many uses it has (MaxUses): checked by the host at the verb's confirmation frame.
 *   - identity tags (IdentityTags, e.g. Mission.Objective.Terminal1) copied onto the Message.Mode.Verb event, so an `Activate` objective counts
 *     "this terminal" by tag query and no mission code names a prop.
 *
 * What the prop DOES when a verb lands is the framework's (a mission counts the event) or the Blueprint's (OnInteracted, raised on the host for
 * every accepted verb, and IGSTN_InteractTarget on the owner). Nothing here is simulation state.
 */
UCLASS(ClassGroup = (Interact), Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNMODEVERBSNP_API UGSTN_RequestableComponent : public UGSTN_InteractableComponent
{
	GENERATED_BODY()

public:
	UGSTN_RequestableComponent();

	/** Verbs the prop accepts (Ability.Mode.*). Empty = any */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requestable", meta = (Categories = "Ability.Mode"))
	FGameplayTagContainer AcceptedVerbs;

	/** Tags added to the Message.Mode.Verb event of an accepted verb (Mission.Objective.* leaves from the content's ini) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requestable")
	FGameplayTagContainer IdentityTags;

	/** True: usable from the start (the interactable component's own record, actionable from frame 0). False: unusable until the mission calls Arm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requestable")
	bool bStartArmed = true;

	/** Accepted verbs before the prop disarms itself. -1 = unlimited */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requestable", meta = (ClampMin = -1))
	int32 MaxUses = 1;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Authority only. Registers a new record that is focusable and actionable from now + DelayFrames (negative = the interact rules' MinAgeFrames, so
	 * every client has the stamp before it simulates that frame). Ignored while the prop is armed. Call it ahead of when the player needs it
	 */
	UFUNCTION(BlueprintCallable, Category = "Requestable")
	void Arm(int32 DelayFrames = -1);

	/** Authority only. Ends the record at the next frame; a verb confirmed after it is refused as Removed. Re-armable */
	UFUNCTION(BlueprintCallable, Category = "Requestable")
	void Disarm();

	UFUNCTION(BlueprintPure, Category = "Requestable")
	bool IsArmed() const;

	UFUNCTION(BlueprintPure, Category = "Requestable")
	bool AcceptsVerb(const FGameplayTag& Verb) const;

	/** Uses left, -1 = unlimited */
	UFUNCTION(BlueprintPure, Category = "Requestable")
	int32 GetUsesLeft() const;

	/** Host: one accepted verb used one use; the last use disarms the prop */
	void ConsumeUse();

	/** The id of the record in force (the armed one, else the interactable component's own) */
	FGSTN_InteractCandidateId GetActiveId() const { return ArmedId.IsValid() ? ArmedId : GetCandidateId(); }

private:
	UFUNCTION()
	void OnRep_Arming();

	/** Registers the record a replicated arming names (once per handle) and applies its end frame. Needs BeginPlay to have run */
	void ApplyArming();

	/** The frame a new stamp starts from on this machine: one past the last fixed tick (never negative) */
	int32 NextSimFrame() const;

	/** Runs Call with the interact subsystem's clock pointed at the combat frame, as GSTNHeld's kit records do: stamps and probe queries must share it */
	void WithSimFrame(TFunctionRef<void()> Call);

	UPROPERTY(ReplicatedUsing = OnRep_Arming)
	FGSTN_RequestableArming Arming;

	UPROPERTY(Replicated)
	int32 UsesDone = 0;

	/** The record Arm() registered on this machine; invalid for a prop that only ever had the interactable component's own */
	FGSTN_InteractCandidateId ArmedId;
};
