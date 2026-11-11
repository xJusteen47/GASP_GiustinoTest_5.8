// Held objects, world side (needs the GAS-NPP fork). M11 K1: the kit on the ground as an interact target.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_InteractableComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_KitInteract.generated.h"

class AGSTN_KitPickup;
class UTextRenderComponent;
struct FGSTN_SimEvent;

/*
 * M11 K1, the kit swap on the grab button (owner decisions 2026-11-11, TEST VALUES in UGSTN_HeldSettings::KitSwap). It lives here, beside the
 * object pickup it shares the button with, because this plugin already joins the three sides it needs (GSTNCombatGAS's kit pickup, GSTNInteract's
 * records, GSTNLink's interact probe); the plan's L2 moves the kit pickup and this bridge into a GSTNPickups plugin.
 *
 *   Focus      every kit on the ground carries a UGSTN_KitInteractableComponent: one GSTNInteract record with the meaning Interact.Target.Kit.
 *              GSTNLink's probe frames one target per frame among fighters, objects at rest and kits, so the framed thing wins (decision 6).
 *   Command    every compiled kit gets Command.Kit.Swap = Release(Interact) after HoldSeconds (0.5 s), requiring Interact.Target.Kit (loadout
 *              extension of the runtime module). The press of B stays the grab's and the pickup's; with no kit framed the release falls through.
 *   Swap       UGSTN_GA_KitSwap only asks the host; after the frame the host swaps with the kit the probe framed for that player on the
 *              request's frame (UGSTN_InteractSubsystem::FindProbeFocus), else the nearest kit in reach. The swap is GSTNCombatGAS's out-of-band
 *              loadout write: the new kit reaches the simulation as a new LoadoutRevision and FGSTN_LoadoutSwapLogic swaps it at one frame
 *              boundary on every machine, ending a running move there. Nothing here is simulation state.
 *   Death      on an authoritative Combat.Event.KO of a human player, the kit drops where the player fell (DeathDrop: drop and lose it, drop a
 *              copy, or nothing); a dropped kit expires after DroppedKitLifetimeSeconds.
 */

/**
 * The GSTNInteract record of a kit on the ground (M11 K1). Added by UGSTN_KitInteractSubsystem to every AGSTN_KitPickup: the server adds it
 * replicated to a spawned kit (the record stamp replicates as for any interactable), every machine adds it to a level-placed one. Its frames
 * are the combat simulation's (the held subsystem's last fixed-tick frame), the ones GSTNLink's probe queries with, not the interact
 * subsystem's server-time clock. Also draws the placeholder prompt over the framed kit for the local player.
 */
UCLASS(ClassGroup = (Interact))
class GSTNHELDRUNTIME_API UGSTN_KitInteractableComponent : public UGSTN_InteractableComponent
{
	GENERATED_BODY()

public:
	UGSTN_KitInteractableComponent();

	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	AGSTN_KitPickup* GetPickup() const;

private:
	/** Points GSTNInteract's clock at the combat frame for one register or unregister call */
	void WithSimFrame(TFunctionRef<void()> Call);
	void UpdatePrompt();

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> Prompt;
};

/**
 * Command.Kit.Swap (M11 K1). Like the held pickup it ends at once and its only effect is a host request: the swap happens after the frame
 * (UGSTN_KitInteractSubsystem), and the client gets the new kit by the loadout's replication and the revision boundary.
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_KitSwap : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The kit records, the host's swap requests and the kit drop on a KO (M11 K1). See the comment at the top of this file */
UCLASS()
class GSTNHELDRUNTIME_API UGSTN_KitInteractSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_KitInteractSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Host only: Holder asked for a swap on the frame being simulated now; resolved after that frame */
	void RequestSwap(AActor* Holder);

	/**
	 * The kit a swap of Holder on Frame takes, per the settings: the kit the probe framed for it (FindProbeFocus), else (or when
	 * bTakesFocusedKit is off) the nearest kit in reach when allowed. Null when none. Any machine (the prompt uses it); the host's answer decides
	 */
	AGSTN_KitPickup* FindSwapTarget(const AActor* Holder, int32 Frame, bool* bOutFocused = nullptr) const;

	/** Host only: the KO drop of Victim's kit per DeathDrop. Returns the dropped kit or null. Public for tests and scripted flows */
	AGSTN_KitPickup* DropKitOnKO(AActor* Victim);

	/** The combat frame of the last fixed tick on this machine (the held subsystem's), INDEX_NONE before the first */
	int32 GetSimFrame() const;

	/** Adds the record component to Pickup if it has none (server or level-placed kits; a client gets a spawned kit's by replication) */
	void EnsureRecord(AGSTN_KitPickup* Pickup) const;

private:
	void HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating);
	void HandleSimEvent(const FGSTN_SimEvent& Event);
	void HandleActorSpawned(AActor* Actor);
	bool HasAuthority() const;

	struct FQueuedSwap
	{
		TWeakObjectPtr<AActor> Holder;
		int32 Frame = INDEX_NONE;
	};
	TArray<FQueuedSwap> Pending;

	FDelegateHandle PostFixedTickHandle;
	FDelegateHandle ActorSpawnedHandle;
	FGSTN_SimEventListenerHandle EventListener;
};
