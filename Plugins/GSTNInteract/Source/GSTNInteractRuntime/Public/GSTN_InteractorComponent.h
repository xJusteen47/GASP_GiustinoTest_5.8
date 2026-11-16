// Game thread interactor: the same focus maths with no simulation (interaction.md §3.4).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_InteractMath.h"
#include "GSTN_InteractRuntimeTypes.h"
#include "GSTN_InteractTarget.h"
#include "GSTN_InteractorComponent.generated.h"

class UGSTN_InteractRulesAsset;
class UGSTN_InteractableComponent;
class UGSTN_InteractSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnFocusChanged, FGSTN_InteractCandidateId, NewFocusId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnInteractRefused, FGSTN_InteractCandidateId, TargetId, EGSTN_InteractVerb, Verb);

/**
 * Runs UGSTN_InteractMath::UpdateFocus on the game thread for the pawn that owns it, with a local FGSTN_InteractFocusState (§3.4).
 * It probes only for a locally controlled pawn (or a non-pawn owner outside a dedicated server): other machines never need the
 * focus, because a press names its target and the server validates it. Interact(Verb) raises ServerInteract(Id, Verb), validated
 * by UGSTN_InteractMath::IsStillInteractable and the target's CanInteractServer; a refusal changes nothing but a client event.
 * In a combat project the bridge's focus source replaces this component (a pawn has one focus source).
 */
UCLASS(ClassGroup = (Interact), Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNINTERACTRUNTIME_API UGSTN_InteractorComponent : public UActorComponent, public IGSTN_InteractFocusSource
{
	GENERATED_BODY()

public:
	UGSTN_InteractorComponent();

	/** Rules of this interactor; empty = the project settings' defaults */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	TObjectPtr<UGSTN_InteractRulesAsset> RulesAsset;

	/** Optional pure filter of the best few survivors (§6 item 3) */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Interact")
	TObjectPtr<UGSTN_InteractCandidateFilter> CandidateFilter;

	/** Raised when the focus id changes, on the machine that owns the interactor */
	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FGSTN_OnFocusChanged OnFocusChanged;

	/** Raised on the pressing client when the server refused a press */
	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FGSTN_OnInteractRefused OnInteractRefused;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Interact is held (the hold lock and the local hold progress). Call with true on press and false on release */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void SetInteractHeld(bool bHeld);

	/** The optional cycle command (INT.2): needs CycleSkipFrames > 0 in the rules. No input is bound here */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void RequestCycle() { bCycleRequested = true; }

	/** Acts on the current focus: false when there is nothing actionable to act on (a ghost, a greyed target, no focus) */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	bool Interact(EGSTN_InteractVerb Verb);

	/** One probe now at the subsystem's frame (the tick does this). Exposed for tests and for callers that want to probe on demand */
	FGSTN_InteractFocusResult ProbeAtFrame(int32 Frame);

	/** Server: validates and performs a press on a named target. Returns false when refused (the RPC then tells the client) */
	bool TryInteractServer(const FGSTN_InteractCandidateId& TargetId, EGSTN_InteractVerb Verb);

	const FGSTN_InteractRules& GetRules() const;
	const FGSTN_InteractFocusState& GetFocusState() const { return State; }
	AActor* GetFocusActor() const;

	// IGSTN_InteractFocusSource
	virtual FGSTN_InteractFocusResult GetFocus() const override { return Result; }
	virtual float GetHoldProgress() const override;
	virtual bool IsLocallyPresented() const override;

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerInteract(FGSTN_InteractCandidateId TargetId, EGSTN_InteractVerb Verb);

	UFUNCTION(Client, Reliable)
	void ClientInteractRefused(FGSTN_InteractCandidateId TargetId, EGSTN_InteractVerb Verb);

	bool ShouldProbe() const;
	void GatherPointing(FGSTN_InteractPointingInput& Out) const;
	void GatherTags(FGameplayTagContainer& Out) const;

	FGSTN_InteractFocusState State;
	FGSTN_InteractFocusResult Result;
	bool bHeld = false;
	bool bCycleRequested = false;
	int32 HeldSinceFrame = 0;
	int32 LastHeldFrames = 0;
	int32 ProbeCounter = 0;
};
