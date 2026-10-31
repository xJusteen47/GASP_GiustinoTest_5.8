// Interaction runtime: interfaces of targets, pointing sources and focus sources (interaction.md §3.4, §5, §6).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InteractMath.h"
#include "GSTN_InteractRuntimeTypes.h"
#include "UObject/Interface.h"
#include "GSTN_InteractTarget.generated.h"

class UPrimitiveComponent;

/**
 * Optional on the actor that owns an interactable component: overrides what the component would answer from its own data.
 * Blueprint native events, so a designer can implement it on the actor Blueprint. The server events run on authority only.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UGSTN_InteractTarget : public UInterface
{
	GENERATED_BODY()
};

class GSTNINTERACTRUNTIME_API IGSTN_InteractTarget
{
	GENERATED_BODY()

public:
	/** Name, icon and verbs of the prompt; Display arrives filled from the component, change what you need */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void GetDisplayInfo(FGSTN_InteractDisplayInfo& Display) const;

	/** Meshes the highlight should light up; empty = every primitive component of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void GetHighlightComponents(TArray<UPrimitiveComponent*>& Components) const;

	/** World anchor of the prompt; Anchor arrives filled from the component */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void GetAnchor(FVector& Anchor) const;

	/** Server: a final game rule on top of stamps, requirement and reach (a locked door, an empty crate). False refuses the press */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	bool CanInteractServer(AActor* Interactor, EGSTN_InteractVerb Verb) const;

	/** Server: the press was accepted. The place to open the chest or start the dialogue */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void OnInteractedServer(AActor* Interactor, EGSTN_InteractVerb Verb);
};

/** What a pointing source reports; any field may be left at its default */
USTRUCT(BlueprintType)
struct GSTNINTERACTRUNTIME_API FGSTN_InteractPointingInput
{
	GENERATED_BODY()

	/** Interactor position (the chest, if line of sight matters) */
	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	FVector Origin = FVector::ZeroVector;

	/** Up of the interactor; never assume world Z */
	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	FVector Facing = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	FVector StickDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ClampMax = 1))
	float StickMagnitude = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	FVector AimDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interact")
	bool bHasAim = false;
};

/** Implemented by the pawn (or a component-owner) to say what it points with. Without it the interactor uses location, forward and velocity */
UINTERFACE(MinimalAPI, Blueprintable)
class UGSTN_InteractPointingSource : public UInterface
{
	GENERATED_BODY()
};

class GSTNINTERACTRUNTIME_API IGSTN_InteractPointingSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact")
	void GetPointingInput(FGSTN_InteractPointingInput& Input) const;
};

/**
 * Anything that decides a focus for a pawn (§3.4): the game thread interactor here, and later a simulation bridge. A pawn has one
 * focus source, so a validator can reject two. Presentation reads the focus only through this.
 */
UINTERFACE(MinimalAPI)
class UGSTN_InteractFocusSource : public UInterface
{
	GENERATED_BODY()
};

class GSTNINTERACTRUNTIME_API IGSTN_InteractFocusSource
{
	GENERATED_BODY()

public:
	/** The latest predicted focus: what a press now would act on */
	virtual FGSTN_InteractFocusResult GetFocus() const = 0;

	/** 0..1 progress of the local hold, for the prompt bar */
	virtual float GetHoldProgress() const { return 0.f; }

	/** True when this source belongs to a locally controlled player, the only ones presented (INT.4) */
	virtual bool IsLocallyPresented() const = 0;
};
