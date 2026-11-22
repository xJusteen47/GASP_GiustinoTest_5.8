// Interaction presentation: presenters and the local presentation subsystem (interaction.md §5). Never simulated.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractRuntimeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Object.h"
#include "GSTN_InteractPresenter.generated.h"

class UGSTN_InteractorComponent;

/**
 * Draws the local focus. Abstract and Blueprintable: a project replaces the built-in look by listing its own subclasses in the
 * settings. One instance per local interactor, so a presenter may keep per-player state. Called on the game thread each frame
 * from the presentation subsystem; it must never write simulation state.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class GSTNINTERACTRUNTIME_API UGSTN_InteractPresenter : public UObject
{
	GENERATED_BODY()

public:
	/** A focus appeared (or changed to another target) */
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void OnFocusEntered(const FGSTN_InteractPresentationView& View);
	virtual void OnFocusEntered_Implementation(const FGSTN_InteractPresentationView& View) {}

	/** Every frame the same focus stays */
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void OnFocusUpdated(const FGSTN_InteractPresentationView& View, float DeltaTime);
	virtual void OnFocusUpdated_Implementation(const FGSTN_InteractPresentationView& View, float DeltaTime) {}

	/** The focus ended (View is the last one shown) */
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void OnFocusLeft(const FGSTN_InteractPresentationView& View);
	virtual void OnFocusLeft_Implementation(const FGSTN_InteractPresentationView& View) {}
};

/**
 * Built-in, functional only: a ring at the target's anchor and its name and verbs as text, through the debug draw helpers (compiled
 * out of Shipping). A project that wants a real look writes its own presenter.
 */
UCLASS()
class GSTNINTERACTRUNTIME_API UGSTN_InteractPresenter_Debug : public UGSTN_InteractPresenter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FColor Tint = FColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FColor UnactionableTint = FColor(120, 120, 120);

	virtual void OnFocusUpdated_Implementation(const FGSTN_InteractPresentationView& View, float DeltaTime) override;
};

/** Raised when the presented focus of a local interactor changes: the Message.Interact.FocusChanged event for audio and UI */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGSTN_OnPresentedFocusChanged, const FGameplayTag& /*MessageTag*/, const UGSTN_InteractorComponent* /*Interactor*/, const FGSTN_InteractPresentationView& /*View*/);

/**
 * Feeds presenters from the latest predicted focus of each local interactor (§5). Created unless this is a dedicated server (the
 * host renders). Only locally controlled interactors are presented, one presenter set each; other players' focus is not drawn (INT.4).
 */
UCLASS()
class GSTNINTERACTRUNTIME_API UGSTN_InteractPresentationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_InteractPresentationSubsystem, STATGROUP_Tickables); }

	static UGSTN_InteractPresentationSubsystem* Get(const UWorld* World) { return World ? World->GetSubsystem<UGSTN_InteractPresentationSubsystem>() : nullptr; }

	/** Nothing is presented on a dedicated server */
	static bool ShouldPresentInNetMode(ENetMode NetMode) { return NetMode != NM_DedicatedServer; }
	/** The one rule of who is drawn: a local player's interactor, on a machine that renders */
	static bool ShouldPresentInteractor(bool bLocallyPresented, ENetMode NetMode) { return bLocallyPresented && ShouldPresentInNetMode(NetMode); }

	void RegisterInteractor(UGSTN_InteractorComponent* Interactor);
	void UnregisterInteractor(UGSTN_InteractorComponent* Interactor);

	/** Interactors registered that are presented right now */
	int32 GetPresentedInteractorCount() const;

	/** Builds the view of an interactor's focus; false for no focus, a ghost, or a target that cannot be resolved */
	bool BuildView(const UGSTN_InteractorComponent& Interactor, FGSTN_InteractPresentationView& OutView) const;

	FGSTN_OnPresentedFocusChanged OnPresentedFocusChanged;

private:
	struct FEntry
	{
		TWeakObjectPtr<UGSTN_InteractorComponent> Interactor;
		TArray<UGSTN_InteractPresenter*> Presenters;
		FGSTN_InteractCandidateId Shown;
		FGSTN_InteractPresentationView LastView;
	};

	void CreatePresenters(FEntry& Entry);
	void DrawDebugRecords(const UGSTN_InteractorComponent& Interactor) const;

	TArray<FEntry> Entries;

	/** Keeps the presenters of Entries alive (Entries is not reflected) */
	UPROPERTY()
	TArray<TObjectPtr<UGSTN_InteractPresenter>> PresenterStore;
};
