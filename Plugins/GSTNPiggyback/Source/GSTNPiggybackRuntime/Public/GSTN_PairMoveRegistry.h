// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "Actions/GSTN_LinkActionProvider.h"
#include "UObject/Object.h"
#include "GSTN_PairMoveRegistry.generated.h"

class UGSTN_PairMoveDefinition;
class UGSTN_PairMoveSet;
class UGSTN_Placer_Mount;

/**
 * The mount type's action provider (M7 G8, IGSTN_LinkActionProvider of GSTNLink G2): resolves the PartnerMoveId of a pair move's initiator key
 * into the definition's action. The module creates one, registers it for Combat.Link.Type.Mount once the engine exists, and the mount's loadout
 * extension adds the pair-move set of every kit it compiles.
 *
 * Determinism: the answer for an id depends only on the definition that owns it (ids are authored and unique), never on the order kits were
 * compiled, so two machines that compiled different kits still agree on every id both know. An id is known wherever the initiator's kit was
 * compiled, which is everywhere the pair move is simulated. A second definition with a taken id is refused (and logged): first come wins, so
 * fix the content (the set's validation reports the clash in the editor).
 *
 * The registry keeps one mount placer per definition (the definition's exit mode and distance) alive for the action info.
 */
UCLASS()
class GSTNPIGGYBACKRUNTIME_API UGSTN_PairMoveRegistry : public UObject, public IGSTN_LinkActionProvider
{
	GENERATED_BODY()

public:
	/** The module's registry (null before the engine exists or when the plugin is off) */
	static UGSTN_PairMoveRegistry* Get();

	/** Creates the module's registry and registers it as the mount type's action provider. Idempotent */
	static void RegisterGlobal();
	static void UnregisterGlobal();

	/**
	 * Adds a definition. Refused (false, OutError says why): null, id out of 1..32767, a missing ability, SwapRoles, or an id another
	 * definition holds. The same definition twice is accepted and changes nothing. Full timeline checks are the definition's validation
	 */
	bool AddDefinition(const UGSTN_PairMoveDefinition* Definition, FString* OutError = nullptr);

	/** Adds every definition of Set; returns how many are known afterwards from it (refusals logged) */
	int32 AddSet(const UGSTN_PairMoveSet* Set);

	/** Forgets every definition (tests) */
	void Reset();

	/** The definitions, ordered by id */
	TArray<const UGSTN_PairMoveDefinition*> GetDefinitions() const;

	const UGSTN_PairMoveDefinition* FindById(int32 Id) const;
	const UGSTN_PairMoveDefinition* FindByTag(const FGameplayTag& PairTag) const;

	virtual bool ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo& OutInfo) const override;

private:
	/** Sorted by the definition's id. Held, not changed (content) */
	UPROPERTY()
	TArray<TObjectPtr<UGSTN_PairMoveDefinition>> Definitions;

	/** The exit placer of Definitions[i] */
	UPROPERTY()
	TArray<TObjectPtr<UGSTN_Placer_Mount>> Placers;

	int32 IndexOfId(int32 Id) const;
};
