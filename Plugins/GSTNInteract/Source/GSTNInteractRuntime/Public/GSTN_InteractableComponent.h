// Interactable component: makes an actor a candidate (interaction.md §6 item 1, §3.2).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractRuntimeTypes.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_InteractableComponent.generated.h"

class UGSTN_InteractSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnInteractedServer, AActor*, Interactor, EGSTN_InteractVerb, Verb);

/**
 * Add to an actor Blueprint to make it something the Interact button can focus (§6 item 1). It registers one immutable record with
 * the world's registry: a level-placed actor on both machines from frame 0 (handle from a hash of its stable name), a spawned actor
 * through a replicated stamp the server sets MinAgeFrames ahead, so a client receives it before it simulates that frame. A moved
 * actor needs UpdateRecord (a removal plus a new record). What an interaction does is the consumer's: bind OnInteracted, implement
 * IGSTN_InteractTarget on the actor, or write an ability bound to the meaning.
 */
UCLASS(ClassGroup = (Interact), Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNINTERACTRUNTIME_API UGSTN_InteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_InteractableComponent();

	/** The Interact.Target leaf projected while this is the focus. It must be listed in the meaning table of the settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (Categories = "Interact.Target"))
	FGameplayTag Meaning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Prompt")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Prompt")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Replaces the meaning's tap verb when not empty */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Prompt")
	FText TapVerbOverride;

	/** Replaces the meaning's hold verb when not empty */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Prompt")
	FText HoldVerbOverride;

	/** 0..255, or -1 for the meaning's default priority */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Focus", meta = (ClampMin = -1, ClampMax = 255))
	int32 Priority = -1;

	/** 0 = the rules' ReachCm; clamped to the rules' HardMaxReachCm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Focus", meta = (ClampMin = 0, ForceUnits = "cm"))
	float ReachOverrideCm = 0.f;

	/** The interactor's tags must satisfy this to act. Empty = no requirement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Focus")
	FGameplayTagQuery InteractorRequires;

	/** Show the prompt greyed when InteractorRequires fails (a locked terminal), instead of ignoring the target (INT.6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Focus")
	bool bFocusWhenUnactionable = false;

	/** StaticGeometry for terminals and anything behind walls: one WorldStatic segment must be clear */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Focus")
	EGSTN_InteractLosPolicy LosPolicy = EGSTN_InteractLosPolicy::None;

	/** Anchor offset in the actor's local space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Anchor")
	FVector AnchorOffset = FVector::ZeroVector;

	/** Radius around the anchor; reach is measured to the surface. Ignored when bUseActorBounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Anchor", meta = (ClampMin = 0, ForceUnits = "cm"))
	float RadiusCm = 30.f;

	/** Take the anchor and radius from the actor's bounds (centre and radius of the bounding sphere) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact|Anchor")
	bool bUseActorBounds = false;

	/** Server only: raised after a press passed every check */
	UPROPERTY(BlueprintAssignable, Category = "Interact")
	FGSTN_OnInteractedServer OnInteracted;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Registers the record now (BeginPlay does this). Authority creates the stamp; a client waits for it unless the actor is level-placed */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void RegisterRecord();

	/** Ends the record at the current frame (EndPlay does this); the registry keeps a tombstone */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void UnregisterRecord();

	/** Ends the record and registers a new one at the actor's current location (a moved item is a removal plus a new record) */
	UFUNCTION(BlueprintCallable, Category = "Interact")
	void UpdateRecord();

	UFUNCTION(BlueprintPure, Category = "Interact")
	FGSTN_InteractCandidateId GetCandidateId() const { return CandidateId; }

	/** The record this component would register for a stamp, from its current data. What a client builds on receiving the stamp */
	FGSTN_InteractCandidate BuildCandidate(const FGSTN_InteractRecordStamp& Stamp, const UGSTN_InteractSubsystem& Subsystem, int32 QueryIndex) const;

	/** Prompt data: the meaning's verbs overridden by this component, then by IGSTN_InteractTarget on the owner */
	FGSTN_InteractDisplayInfo ResolveDisplayInfo(const UGSTN_InteractSubsystem& Subsystem) const;
	FVector ResolveAnchor() const;

	/** Server: the final game rule of the owner. True when the owner implements nothing */
	bool CanInteractServer(AActor* Interactor, EGSTN_InteractVerb Verb) const;

	/** Server: calls the owner's interface event and raises OnInteracted */
	void NotifyInteractedServer(AActor* Interactor, EGSTN_InteractVerb Verb);

	const FGSTN_InteractRecordStamp& GetRecordStamp() const { return RecordStamp; }

private:
	UFUNCTION()
	void OnRep_RecordStamp();

	void RegisterFromStamp();

	/** Server-assigned for spawned actors; empty for level-placed ones, which derive their handle on both machines */
	UPROPERTY(ReplicatedUsing = OnRep_RecordStamp)
	FGSTN_InteractRecordStamp RecordStamp;

	FGSTN_InteractCandidateId CandidateId;
};
