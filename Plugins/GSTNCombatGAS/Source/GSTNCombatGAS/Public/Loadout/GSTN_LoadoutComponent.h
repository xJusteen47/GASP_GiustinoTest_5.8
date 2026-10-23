// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/StrongObjectPtr.h"
#include "GSTN_LoadoutComponent.generated.h"

class APawn;
class APlayerState;
class UAbilitySystemComponent;
class UGSTN_GE_WeaponStats;
class UGSTN_LoadoutComponent;
class UGSTN_LoadoutRules;
class UGSTN_PrimaryKitDefinition;
class UGSTN_SkinDefinition;

/** The applied loadout changed on this machine (server: it was granted; client: it replicated). The table is compiled by then. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnLoadoutApplied, UGSTN_LoadoutComponent*, Component, int32, Revision);

/** The server refused a request this machine's player sent. Each error is one rule finding, ready to show. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnLoadoutRejected, UGSTN_LoadoutComponent*, Component, const TArray<FString>&, Errors);

/**
 * The loadout of a player (combat-m3 10). Put it on the PlayerState: it survives respawn, and every machine that has the
 * PlayerState learns the loadout, which is what the binding table and the skins of remote fighters need.
 *
 * Flow. The owning client calls ServerRequestLoadout. The server resolves the ids, compiles the binding table and runs the
 * loadout rules (GSTNCombatCore); a rejection goes back as OnLoadoutRejected and changes nothing. An accepted loadout is stored
 * in Applied, which replicates, and the revision goes up. Then ApplyAppliedLoadout runs: on the server called directly by the
 * accept path (a listen host never gets the OnRep of a value it set), on a client by OnRep_Applied. It resolves every asset ONCE,
 * keeps them, compiles the table on this machine (the same inputs give the same bytes everywhere) and tells OnLoadoutApplied.
 * Nothing here runs per tick.
 *
 * Granting happens on the server only, OUTSIDE the simulation, on the ability system of the player's pawn: kit stats effect,
 * the primary weapon's stats effect, then the abilities of the table (and the primary weapon's passives). With the GAS-NPP fork
 * that costs the owning client one reconcile, because abilities and effects reach it through the simulation's sync state: a client
 * must not assume the ability exists right after the loadout changed (FindAbilitySpecFromClass is valid only after that
 * reconcile), and must never grant anything itself. A pawn that is not there yet is picked up when the PlayerState gets it.
 * With stock GAS the grants replicate the usual way.
 *
 * Independent of the combat simulation: the revision is a plain counter here. Writing it into the combat sync state, and handing
 * the table to the combat model, is the NP adapter's job.
 *
 * Everything the loadout names is an asset id, resolved through the Asset Manager; a game replaces content by data and the
 * rules asset (Rules) by pointing the component at another one.
 */
UCLASS(ClassGroup = "GSTN Combat", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_LoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_LoadoutComponent();

	/** The rules a request must pass. Empty = the built-in set (the class default of UGSTN_LoadoutRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TSoftObjectPtr<UGSTN_LoadoutRules> Rules;

	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FGSTN_OnLoadoutApplied OnLoadoutApplied;

	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FGSTN_OnLoadoutRejected OnLoadoutRejected;

	/** The owning client asks for a loadout. Validated on the server; see AcceptLoadout */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Combat|Loadout")
	void ServerRequestLoadout(const FGSTN_LoadoutData& Requested);

	/** Sent by the server to the requesting client when the rules refused its loadout; raises OnLoadoutRejected there */
	UFUNCTION(Client, Reliable)
	void ClientLoadoutRejected(const TArray<FString>& Errors);

	/**
	 * Server only. Validates Requested with the loadout rules and, when it passes, stores it as the applied loadout, raises the
	 * revision and applies it. Returns false with the rule findings in OutErrors otherwise (nothing changes). A request equal
	 * to the applied loadout is accepted and does nothing. The RPC calls this; a server-side game flow (a lobby, a bot) can too.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat|Loadout")
	bool AcceptLoadout(const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors);

	/**
	 * Judges a loadout exactly as AcceptLoadout would (same resolution, same table, the revision it would get, the rules and the
	 * registered validators) without changing anything. Server or client; the findings are ready to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool ValidateLoadout(const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors) const;

	/**
	 * Resolves the applied loadout's assets, compiles the table, grants on the server and broadcasts. Called by the accept path
	 * on the server and by OnRep_Applied on a client; call it again to retry a grant that was waiting for a pawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	void ApplyAppliedLoadout();

	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	const FGSTN_LoadoutData& GetApplied() const { return Applied; }

	/** Counts accepted loadouts, from 1. Same value on every machine that has the loadout. 0 = none yet */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	int32 GetRevision() const { return Revision; }

	/** True once a loadout with a kit was applied */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	bool HasLoadout() const { return Applied.PrimaryKit.IsValid(); }

	/** The server accepted a loadout but had no pawn ability system to grant on yet */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	bool IsGrantPending() const { return bGrantPending; }

	/** The kit of the applied loadout, null if none or unresolved on this machine. Valid until the next apply */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	UGSTN_PrimaryKitDefinition* GetResolvedKit() const;

	/** The skin of the applied loadout, null for the character's own look. Valid until the next apply */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	UGSTN_SkinDefinition* GetResolvedSkin() const;

	/** The assets of the applied loadout, resolved once at apply. Kept alive by this component */
	const FGSTN_ResolvedLoadout& GetResolvedLoadout() const { return Resolved; }

	/** The binding table compiled from the applied loadout on this machine (empty when there is none or it did not validate) */
	const FGSTN_CompiledBindingTable& GetCompiledTable() const { return CompiledTable; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Loads the asset a primary asset id names (through the Asset Manager). Synchronous, called once per id per apply. Virtual so
	 * a test or a game with its own content source can answer without the Asset Manager.
	 */
	virtual UObject* ResolveAsset(const FPrimaryAssetId& Id) const;

	/**
	 * The ability system the loadout is granted on: the pawn's of the PlayerState this component sits on, or the owner's own if
	 * the owner is not a PlayerState. Null while there is no pawn yet.
	 */
	virtual UAbilitySystemComponent* GetTargetAbilitySystem() const;

private:
	UFUNCTION()
	void OnRep_Applied();

	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	bool IsServerAuthority() const;

	/** The rules asset, or the built-in default */
	const UGSTN_LoadoutRules& GetRules() const;

	/** Looks every id of Data up. Unresolved ids leave a null; the loaded objects are put in OutHold so they stay alive */
	void ResolveLoadout(const FGSTN_LoadoutData& Data, FGSTN_ResolvedLoadout& OutResolved, TArray<TStrongObjectPtr<UObject>>& OutHold) const;

	/** Removes what the previous apply granted (abilities, the primary weapon's stats effect). Server only */
	void ClearGranted();

	/** Applies the kit's stats, the primary weapon's stats and grants the table's abilities on ASC. Server only */
	void GrantToAbilitySystem(UAbilitySystemComponent& ASC);

	/** The loadout the server accepted. Replicated with the revision in one update, so the OnRep sees both */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loadout", meta = (AllowPrivateAccess = "true"))
	int32 Revision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Applied, BlueprintReadOnly, Category = "Loadout", meta = (AllowPrivateAccess = "true"))
	FGSTN_LoadoutData Applied;

	/** The assets resolved by the last apply: the objects Resolved points into, kept so garbage collection cannot take them */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeptAssets;

	/** The weapon stats effect class applied for the primary weapon, to remove it by class on the next apply */
	UPROPERTY(Transient)
	TSubclassOf<UGSTN_GE_WeaponStats> AppliedWeaponStatsClass;

	FGSTN_ResolvedLoadout Resolved;
	FGSTN_CompiledBindingTable CompiledTable;

	/** Ability system the last grant went to and the abilities this component gave it (server only, never replicated) */
	TWeakObjectPtr<UAbilitySystemComponent> GrantedOn;
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;

	bool bGrantPending = false;
};
