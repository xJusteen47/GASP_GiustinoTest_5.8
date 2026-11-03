// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_CombatRulesComponent.generated.h"

class UGSTN_CombatRules;

/**
 * Tells every machine which rules asset the match plays (combat-m4-hits 3). Put it on the GameState: the server picks the asset
 * (a game mode, a lobby, a test) and the reference replicates, so the clients resolve the same rules the host does.
 *
 * Resolving loads the asset and its hit pipeline once, outside the simulation, and must happen before the first Network
 * Prediction tick on every machine. ApplyRules is therefore called from BeginPlay on every machine (a replicated value is
 * already there when a component of a replicated actor begins play), from OnRep_RulesAsset when the server changes the asset
 * later, and from SetRulesAsset on the authority. The authority never receives the OnRep of a value it set itself (a listen host
 * included), so SetRulesAsset calls ApplyRules directly, as the loadout component does.
 *
 * An empty or unloadable reference resolves to the rules of UGSTN_CombatSettings (its asset, else the built-in standard rules),
 * so a match that sets nothing still has working hits. The asset reference is soft; nothing here names a /Game/ path.
 */
UCLASS(ClassGroup = "GSTN Combat", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_CombatRulesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_CombatRulesComponent();

	/** The rules the match plays. Empty = the rules of the combat settings. Set it in the defaults or with SetRulesAsset on the authority */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_RulesAsset, Category = "Rules")
	TSoftObjectPtr<UGSTN_CombatRules> RulesAsset;

	/** Picks the rules and resolves them at once. Authority only; a client's call is ignored with a warning */
	UFUNCTION(BlueprintCallable, Category = "Combat|Rules")
	void SetRulesAsset(TSoftObjectPtr<UGSTN_CombatRules> NewRules);

	/** Loads RulesAsset (or falls back to the settings) and resolves its pipelines. Safe to call again; it rebuilds the result */
	UFUNCTION(BlueprintCallable, Category = "Combat|Rules")
	void ApplyRules();

	/** The resolved rules, never null once ApplyRules ran (it runs in BeginPlay). Before that it resolves on demand */
	UFUNCTION(BlueprintPure, Category = "Combat|Rules")
	const UGSTN_CombatRules* GetRules();

	/** How many times the rules were resolved on this machine; tests and diagnostics read it */
	int32 GetApplyCount() const { return ApplyCount; }

	/** The rules component of the world's GameState, or null (a world without one plays the settings' rules) */
	static UGSTN_CombatRulesComponent* Find(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_RulesAsset();

	/** Kept alive here: the built-in rules are kept by the settings, a loaded asset only by this reference */
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatRules> Resolved;

	int32 ApplyCount = 0;
};
