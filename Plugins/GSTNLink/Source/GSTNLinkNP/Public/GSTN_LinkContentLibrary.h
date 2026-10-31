// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LinkContentLibrary.generated.h"

class AActor;
class UGSTN_CombatRules;
class UGSTN_PrimaryKitDefinition;

/**
 * Content and harness helpers of the link plugin (Tools/GASNPPTest/link_scenarios.py, the editor): the pipeline steps in a rules asset, a grab
 * hit type on a kit's move (in memory), the server-side test formation of the S1 spike, and readers of the link state a machine shows.
 * Nothing here runs inside the simulation: the formation command is posted to the inboxes like any entry, and the simulation's handlers apply it.
 */
UCLASS()
class GSTNLINKNP_API UGSTN_LinkContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Inserts GrabImmunity then LinkRequest after HurtState (m6 spec 9 order: before the defence's Parry and Shield). Idempotent; never saves */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool AddLinkStepsToRules(UGSTN_CombatRules* Rules);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool RemoveLinkStepsFromRules(UGSTN_CombatRules* Rules);

	/**
	 * Sets the hit type and link type of every hitbox of the timeline the kit (or its primary weapon) binds to Command (the ability's inline timeline, or its timeline
	 * asset), in memory: a test move becomes a grab. The loadout must be compiled again to take it (the harness re-applies it). Returns the
	 * number of hitboxes changed
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static int32 SetKitMoveHitType(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, FGameplayTag HitType, FGameplayTag LinkType);

	/** The hit type and link type of the first hitbox of that timeline (to restore them); false when the kit binds no such timeline */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool GetKitMoveHitType(UGSTN_PrimaryKitDefinition* Kit, FGameplayTag Command, FGameplayTag& OutHitType, FGameplayTag& OutLinkType);

	/**
	 * The S1 spike's test command (no pipeline): posts LinkCommand{Form} to both entities' inboxes in Initiator's world, stamped with the
	 * world's current snapshot frame, so both take their roles on the next frame; the serial is one after both sides'. Call it on the
	 * AUTHORITY's world: the clients learn the link by correction, like a link formed by a hit they could not predict. False when an entity
	 * or the type is unknown or a channel is busy
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool FormTestLink(AActor* Initiator, AActor* Target, FGameplayTag LinkType);

	/** Posts LinkCommand{Break, Forced} for the Body link of Entity to both sides (its partner's too), stamped with the current frame */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool BreakTestLink(AActor* Entity);

	/** The Body link this machine shows for Actor: role, phase, serial, link frame, partner entity id; false without a fragment */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool GetShownBodyLink(const AActor* Actor, int32& OutRole, int32& OutPhase, int32& OutSerial, int32& OutLinkFrame, int32& OutPartnerId);

	/** The shown fragment as text (FGSTN_LinkFragment::ToString), "" without one */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static FString GetShownLinkText(const AActor* Actor);

	/** The bytes the shown fragment serialises to (its payload; 0 without one): the wire cost of the link state per sync state */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static int32 GetShownLinkPayloadBytes(const AActor* Actor);

	/** The combat entity id of Actor in its world's step snapshot, INDEX_NONE when it is not there */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static int32 GetEntityId(const AActor* Actor);

	/** The hurtbox anchor of Actor in its world's latest step snapshot (its entry's transform); false when it has no entry */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool GetSnapshotTransform(const AActor* Actor, FTransform& OutTransform);

	/** Rebuilds the link registry from the asset manager and re-attaches the link content of every pawn of every world (after creating a test type) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static int32 RebuildRegistryAndReattach();

	/** The link registry's wire index of a type (0 = unknown) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static int32 GetLinkTypeIndex(FGameplayTag LinkType);

	/** The link watchdog of the actor's world (F11): breaks and orphans posted so far, and the last break's reason index and frame. False without one */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool GetWatchdogStats(const AActor* WorldActor, int32& OutBreaks, int32& OutOrphans, int32& OutLastReason, int32& OutLastFrame);

	/** What the last break left on the Body channel this world shows for the actor: the reason index (0 = none), the serial and the grab immunity end */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool GetShownBodyBreak(const AActor* Actor, int32& OutReason, int32& OutSerial, int32& OutGrabImmunityEndFrame);

	/**
	 * One line of what this world shows for the actor, for the harness's separate-process mirror (a client process has no Python: its test component
	 * sends this to the server each frame): "role,phase,serial,linkFrame,partner;x,y,z" with the parked shown location, or "-" after the ';' while it
	 * shows no parked role; "" without a fragment.
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static FString GetMirrorText(const AActor* Actor);

	/**
	 * In memory, never saved (F12 harness): the kit's command set gets the standard link slots (UGSTN_LinkSlots), its bindings the harness link moves
	 * (UGSTN_GA_LinkTest*: grab, pummel, the four throws, release as grabber, latcher and latched, the latched one being the shake-off, the latch
	 * attack) with the slots' default requirements, and a link extension with default rules when it has none. The kit's previous command set,
	 * bindings and extensions are kept for RemoveLinkTestBindings. Re-apply the pawns' loadout afterwards. Returns the bindings added
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static int32 AddLinkTestBindings(UGSTN_PrimaryKitDefinition* Kit);

	/** Puts back what AddLinkTestBindings changed. False when it changed nothing on this kit */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool RemoveLinkTestBindings(UGSTN_PrimaryKitDefinition* Kit);

	/** In memory: the kit link extension's MaxGrabWeight and MinLatchWeight (a negative value keeps the current one). False without an extension */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool SetKitLinkWeights(UGSTN_PrimaryKitDefinition* Kit, float MaxGrabWeight, float MinLatchWeight);

	/** In memory: the definition's Alternatives (the grab's latch, F12b). False for an unknown type */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Content")
	static bool SetTypeAlternatives(FGameplayTag LinkType, const TArray<FGameplayTag>& Alternatives);

	/** What this world shows for the actor's Body link besides the summary: the mash end, the mash's last counted frame, the cooldown, the grab immunity, the reason */
	/**
	 * The grab probe as this actor's world runs it (harness and debug): per entity of the world's step snapshot, "id dist=<cm> offer=<index>|refused
	 * <EGSTN_LinkRefusal>" with the interactor's own side first. The same request checks the link tick's probe makes, without the probe's arc and tie
	 * rules; "" without a snapshot or a link extension
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static FString DescribeGrabProbe(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Link|Harness")
	static bool GetShownLinkNumbers(const AActor* Actor, int32& OutEndFrame, int32& OutMashProcessedFrame, int32& OutCooldownEndFrame, int32& OutGrabImmunityEndFrame,
		int32& OutBreakReason);
};
