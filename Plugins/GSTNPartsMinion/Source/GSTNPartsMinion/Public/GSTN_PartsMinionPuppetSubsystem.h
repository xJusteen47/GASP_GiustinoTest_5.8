// Bridge between parts and minions (plan P16 sub-gate B).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "GSTN_PartsMinionPuppetSubsystem.generated.h"

class AGSTN_MinionPuppet;
class UGSTN_PartSetDefinition;
class USkeletalMeshComponent;

/** Counters of the puppet hide pass, for the bench report and the tests. Not state */
struct FGSTN_PartsMinionPuppetStats
{
	/** Puppets the pass has applied a mask to at least once (pooled ones included) */
	int32 Tracked = 0;
	/** Of those, puppets that currently hide at least one part */
	int32 Hiding = 0;
	/** Mask changes applied to a puppet since the world started (a new break, or a pooled puppet reused by another minion) */
	int32 Changes = 0;
	/** Bones hidden by the last changes that did not exist on the puppet's mesh (a part set naming bones of another skeleton) */
	int32 MissingBones = 0;
};

/**
 * Broken parts on the minion puppets (plan P16 sub-gate B; combat-m8-minions.md §4d.1, §8.4). Presentation only: after the minion
 * presentation subsystem placed its puppets, this pass reads each visible puppet's broken mask and hides the bones of the broken parts
 * (FGSTN_PartDefinition::Bones; snap, no transition). Where the mask comes from follows the presentation's own source:
 *  - listen host or standalone (Store): the server row of UGSTN_PartsMinionSubsystem, through the record's slot and generation (the host
 *    reads the store, M8 §4d.1, there is no pack item to read);
 *  - remote client (Registry): the extension byte the server sent, UGSTN_MinionClientRegistry::GetExtByte(record key).
 * The part set is the one this machine attaches to the record's definition (UGSTN_PartsMinionSubsystem::GetPresentationSet), so the bit
 * order is the server's. A puppet keeps what was applied to it (minion key, generation, mask): nothing is done until one of them changes,
 * and a pooled puppet reused by another minion is shown whole again before the new mask goes on.
 *
 * Nothing here is read by the simulation, nothing is replicated. Not created on a dedicated server (nothing renders). Per frame: two map
 * lookups and a byte read per visible puppet; bone visibility calls only on a change.
 */
UCLASS()
class GSTNPARTSMINION_API UGSTN_PartsMinionPuppetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** One pass over the presented puppets. Tick calls it every frame; tests call it right after the presentation's TickPresentation */
	void ApplyToPuppets();

	/** The mask last applied to a puppet, 0 when none (tests) */
	uint8 GetAppliedMask(const AGSTN_MinionPuppet* Puppet) const;

	const FGSTN_PartsMinionPuppetStats& GetStats() const { return Stats; }

private:
	struct FApplied
	{
		int32 Key = INDEX_NONE;
		uint16 Generation = 0;
		uint8 Mask = 0;
		TWeakObjectPtr<const UGSTN_PartSetDefinition> Set;
	};

	/** Hides (or shows again) the bones of the parts in Mask; returns how many named bones the mesh did not have */
	static int32 SetPartBonesHidden(USkeletalMeshComponent& Mesh, const UGSTN_PartSetDefinition& Set, uint8 Mask, bool bHidden);

	TMap<TObjectKey<AGSTN_MinionPuppet>, FApplied> Applied;
	FGSTN_PartsMinionPuppetStats Stats;
};
