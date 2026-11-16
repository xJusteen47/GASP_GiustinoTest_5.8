// Combat presentation (CP4 of the Blueprint seams): plays the cosmetic windows of the moves a character runs.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Presentation/GSTN_MoveCosmetics.h"
#include "GSTN_MoveCosmeticsComponent.generated.h"

class UAudioComponent;
class UGSTN_CombatSimComponent;
class UGSTN_CombatTableSet;
class UGSTN_GA_BaseCombat;
class UNiagaraComponent;
class USkeletalMeshComponent;

/** What a cosmetic window event tells a Blueprint */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_MoveCosmeticEvent
{
	GENERATED_BODY()

	/** The window's tag (Cosmetic.*): what to do, for a Blueprint that does more than the window's own effect */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	FGameplayTag CosmeticTag;

	/** The move the window belongs to */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	TSubclassOf<UGSTN_GA_BaseCombat> Move;

	/** Index in the move's Cosmetic Windows */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	int32 WindowIndex = INDEX_NONE;

	/** The simulation frame the move started on (two runs of the same move are two different events) */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	int32 MoveStartFrame = INDEX_NONE;

	/** The Niagara component the window spawned (None without an Effect or with Play Window Effects off). Do not keep it: it returns to a pool */
	UPROPERTY(BlueprintReadOnly, Category = "Cosmetic")
	TObjectPtr<UNiagaraComponent> SpawnedEffect = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnMoveCosmeticBegin, const FGSTN_MoveCosmeticEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnMoveCosmeticEnd, const FGSTN_MoveCosmeticEvent&, Event, bool, bInterrupted);

/**
 * Plays the Cosmetic Windows of the moves its owner runs (UGSTN_GA_BaseCombat::CosmeticWindows): a trail on the fist for the active frames, a
 * whoosh on the startup, a charge glow. Add it to a character that has a UGSTN_CombatSimComponent (a script adds it, never by hand); a
 * Blueprint child can override On Move Cosmetic Begin / End, or the character Blueprint can bind the two events, for anything the window's
 * data does not cover (a material glow, a decal, a UI flash).
 *
 * Every drawn frame it reads the latest presented combat state (the prediction on the player's own machine, the interpolated state on a remote
 * one) and diffs the live windows with FGSTN_MoveCosmeticTracker. It is never an event of the simulation, so a rollback, a re-simulation or a
 * correction cannot fire anything twice: after a correction the state simply has other live windows, and the windows of a move that did not
 * stand end as interrupted (their effect is killed). A one-shot that already played is never replayed for the same run.
 *
 * Presentation only, on machines that draw (it switches itself off on a dedicated server): the simulation never reads it, it holds no sync
 * state, and its events must not call the move actions (they do nothing outside the simulation step anyway).
 */
UCLASS(Blueprintable, ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_MoveCosmeticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_MoveCosmeticsComponent();

	/** Master switch */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Cosmetics")
	bool bEnabled = true;

	/** Spawn each window's Effect and Sound. Off = only the Blueprint events run (a Blueprint that plays everything itself) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Cosmetics")
	bool bPlayWindowEffects = true;

	/** The skeletal mesh the effects attach to; None = the character's mesh, else the first skeletal mesh of the owner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Cosmetics")
	FName MeshComponentName;

	/** How long a window that began is remembered, so a correction that drops its move for a moment never plays it twice */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Cosmetics", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MemorySeconds = static_cast<float>(FGSTN_MoveCosmeticTracker::DefaultMemorySeconds);

	/** A window began (after its effect spawned) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Cosmetics")
	FGSTN_OnMoveCosmeticBegin OnMoveCosmeticBegin;

	/** A window ended: naturally (its frames are over) or interrupted (the move was cut short, or a correction removed it) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Cosmetics")
	FGSTN_OnMoveCosmeticEnd OnMoveCosmeticEnd;

	/** Windows playing now */
	UFUNCTION(BlueprintPure, Category = "Combat|Cosmetics")
	int32 GetNumActiveWindows() const { return Playing.Num(); }

	/** "began,ended,interrupted,active" since begin play, for the harness. Not state */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetCosmeticsInfo() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Combat|Cosmetics", meta = (DisplayName = "On Move Cosmetic Begin"))
	void K2_OnMoveCosmeticBegin(const FGSTN_MoveCosmeticEvent& Event);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Combat|Cosmetics", meta = (DisplayName = "On Move Cosmetic End"))
	void K2_OnMoveCosmeticEnd(const FGSTN_MoveCosmeticEvent& Event, bool bInterrupted);

private:
	struct FPlaying
	{
		FGSTN_MoveCosmeticKey Key;
		FGSTN_MoveCosmeticWindow Window;
		FGSTN_MoveCosmeticEvent Event;
		TWeakObjectPtr<UNiagaraComponent> Effect;
		TWeakObjectPtr<UAudioComponent> Audio;
		bool bPaused = false;
	};

	/** The move class and its windows behind a registry entry of the table set the state is on; null when unknown */
	const UGSTN_GA_BaseCombat* FindMove(const UGSTN_CombatTableSet* Tables, int32 Entry) const;

	/** Loads the effects and sounds of every move of a table set once (presentation: a synchronous load the first time a loadout is seen) */
	void PreloadTables(const UGSTN_CombatTableSet* Tables);

	void Begin(const FGSTN_MoveCosmeticKey& Key, const UGSTN_GA_BaseCombat* Move);
	void End(const FGSTN_MoveCosmeticTracker::FEnded& Ended);
	void SetHitstopPause(bool bHitstop);
	USkeletalMeshComponent* ResolveMesh();

	FGSTN_MoveCosmeticTracker Tracker;
	TArray<FPlaying> Playing;
	TWeakObjectPtr<UGSTN_CombatSimComponent> Combat;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TWeakObjectPtr<const UGSTN_CombatTableSet> PreloadedTables;
	bool bWasInHitstop = false;

	/** Keeps the loaded effects and sounds alive */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedContent;

	int32 NumBegan = 0;
	int32 NumEnded = 0;
	int32 NumInterrupted = 0;
};
