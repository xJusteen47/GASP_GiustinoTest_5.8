// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Rules/GSTN_KitExtension.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"
#include "GSTN_PartsAttach.generated.h"

class UAbilitySystemComponent;
class UGSTN_LoadoutComponent;
class UGSTN_PartSetDefinition;
struct FGSTN_PartsFragment;

/**
 * Which part set an entity has (combat-parts.md §2, §5.1): the content side of attaching parts, kept apart from the simulation state.
 * The set is data every machine resolves the same way (a kit's FGSTN_PartsKitExtension), never sync state: the fragment carries the
 * numbers, this registry carries the meaning of the part indices. The simulation reads it as it reads any asset (the snapshot
 * contributor, the tick extension), so an actor without an entry is simply an entity without parts and nothing is paid for it.
 *
 * ApplyPartSet is one function for every machine: the host and the owning client call it from the same place (the loadout applied
 * event of the pawn), so there is no OnRep and no IsServer branch. The registry keeps its sets alive for as long as an entity uses
 * them, which is what lets the lag compensation history hold a copy of the parts extension with a bare set pointer.
 *
 * The part count N is not on the wire, so two machines with different sets would read the same bytes as different parts. A set that
 * disagrees with a fragment already present (more parts written than the set has, or another recorded count) is refused.
 * Game thread only.
 */
class GSTNPARTSNP_API FGSTN_PartsAttach
{
public:
	/** The set attached to Owner, or null (no parts) */
	static const UGSTN_PartSetDefinition* FindSet(const AActor* Owner);

	/** The set a kit's extension list names (the first FGSTN_PartsKitExtension), loaded now; null when the kit names none. Never null for an entry that loads */
	static UGSTN_PartSetDefinition* ResolveKitSet(const FGSTN_KitExtensionList& Extensions);

	/**
	 * Attaches Set to Owner on this machine: registers it, and grants the marker tag Combat.Entity.Destructible on ASC when given.
	 * Existing is the entity's parts fragment when one is already present (a client whose state arrived before the kit): the set is
	 * refused, with an error, when its part count cannot hold the fragment. A null Set detaches (ClearPartSet). Returns whether the
	 * set is attached after the call.
	 */
	static bool ApplyPartSet(AActor* Owner, UGSTN_PartSetDefinition* Set, UAbilitySystemComponent* ASC, const FGSTN_PartsFragment* Existing = nullptr);

	/** Detaches the set of Owner and removes the marker tag */
	static void ClearPartSet(AActor* Owner, UAbilitySystemComponent* ASC);

	/** Whether the set's part count can hold the fragment: no damage or break beyond the set, and the fragment's recorded count, if any, equal to it */
	static bool IsPartCountCompatible(const FGSTN_PartsFragment& Fragment, const UGSTN_PartSetDefinition& Set);

	/** Number of attached entities (tests) */
	static int32 Num();

	/** Drops every attachment (tests) */
	static void Reset();
};

/**
 * Puts a part set on a pawn from its loadout: when the loadout of the owner is applied (on every machine that applies it, the host
 * included) the kit's FGSTN_PartsKitExtension is read and ApplyPartSet runs; a kit without one detaches. DefaultPartSet covers a pawn
 * that has parts without a kit (a training dummy); the kit wins when it names one. Add it to a pawn that has a combat component.
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent))
class GSTNPARTSNP_API UGSTN_PartsAttachComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_PartsAttachComponent();

	/** The set used when the kit names none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UGSTN_PartSetDefinition> DefaultPartSet;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision);

	void Attach(UGSTN_PartSetDefinition* Set);
};
