// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_AdditiveReactionSource.h"
#include "GSTN_PresentationLogic.h"
#include "GSTN_PresentationComponent.generated.h"

class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnPresentationEvent, const FGSTN_PresentationEvent&, Event, float, Intensity);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FGSTN_OnPresentationEventSingle, const FGSTN_PresentationEvent&, Event, float, Intensity);

/** One table in the lookup stack: a source (Kit, Skin...) and its priority (higher is asked first) */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationTableLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FName Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	TObjectPtr<UGSTN_PresentationTable> Table;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	int32 Priority = 0;
};

/**
 * The presentation of one character: it turns presented state into events (SubmitSnapshot, called by an adapter every frame) or takes
 * events from anywhere (ReportEvent), drops the ones a resimulation repeats (dedupe per event and frame), plays the effects the tables
 * give, and fires the Blueprint events. It runs on every machine that shows the character, from that machine's own state: nothing is
 * replicated, so the events reach clients without any RPC (online), and a dedicated server plays no effect but still fires the events.
 *
 * Tables: the layers are asked from the highest priority down (a skin over its kit), then DefaultTable; each table falls back on its own
 * Parent. The first table with a row for the event (or for an ancestor tag) answers.
 *
 * Presentation only: nothing here is read by a simulation, and it holds no sync state.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNPRESENTATIONCORE_API UGSTN_PresentationComponent : public UActorComponent, public IGSTN_AdditiveReactionSource
{
	GENERATED_BODY()

public:
	UGSTN_PresentationComponent();

	/** Adds the component to Owner once (game thread, outside the simulation) */
	static UGSTN_PresentationComponent* FindOrAdd(AActor* Owner);

	/** Off = events still fire, no effect plays */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	bool bPlayEffects = true;

	/** Asked after every layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	TObjectPtr<UGSTN_PresentationTable> DefaultTable;

	/** Thresholds and tag rules of SubmitSnapshot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FGSTN_PresentationDeriveRules DeriveRules;

	/** Rollback dedupe: frames either side that still count as the same event (events stamped by the simulation always use 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Rollback", meta = (ClampMin = "0", ClampMax = "8"))
	int32 DedupeFrameWindow = 2;

	/** Every event that played (after the dedupe), with its intensity (1 when no row answers it) */
	UPROPERTY(BlueprintAssignable, Category = "Presentation")
	FGSTN_OnPresentationEvent OnPresentationEvent;

	/** Calls Delegate for every event whose tag is Tag or a child of it (one Blueprint event per type) */
	UFUNCTION(BlueprintCallable, Category = "Presentation")
	void BindEventByTag(UPARAM(meta = (Categories = "Presentation.Event")) FGameplayTag Tag, FGSTN_OnPresentationEventSingle Delegate);

	UFUNCTION(BlueprintCallable, Category = "Presentation")
	void UnbindEventByTag(FGameplayTag Tag, FGSTN_OnPresentationEventSingle Delegate);

	/** Derives this frame's events from the presented state and reports them */
	UFUNCTION(BlueprintCallable, Category = "Presentation")
	void SubmitSnapshot(const FGSTN_PresentationSnapshot& Snapshot);

	/** Plays one event (unless the dedupe already saw it). True if it played */
	UFUNCTION(BlueprintCallable, Category = "Presentation")
	bool ReportEvent(const FGSTN_PresentationEvent& Event);

	/** Puts Table in the lookup stack under Source (replacing that source's table); null removes it */
	UFUNCTION(BlueprintCallable, Category = "Presentation")
	void SetTableLayer(FName Source, UGSTN_PresentationTable* Table, int32 Priority = 0);

	UFUNCTION(BlueprintPure, Category = "Presentation")
	UGSTN_PresentationTable* GetTableLayer(FName Source) const;

	/** The row that answers EventTag through the whole stack; false = none */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	bool FindCue(FGameplayTag EventTag, FGSTN_PresentationCue& OutCue) const;

	/** Pure resolution over the stack (tests) */
	const FGSTN_PresentationCue* ResolveCue(const FGameplayTag& EventTag) const;

	/** Intensity of Event under Cue, or -1 when the cue does not play it (below its MinMagnitude). Pure */
	static float CueIntensity(const FGSTN_PresentationCue* Cue, const FGSTN_PresentationEvent& Event);

	/** Last presented snapshot (debug) */
	UFUNCTION(BlueprintPure, Category = "Presentation")
	const FGSTN_PresentationSnapshot& GetLastSnapshot() const { return Deriver.GetPrevious(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const override;

private:
	void PlayCue(const FGSTN_PresentationCue& Cue, const FGSTN_PresentationEvent& Event, float Intensity);

	struct FBound
	{
		FGameplayTag Tag;
		FGSTN_OnPresentationEventSingle Delegate;
	};
	TArray<FBound> Bound;

	struct FActiveAdditive
	{
		TWeakObjectPtr<const UAnimSequenceBase> Clip;
		float Time = 0.f;
		float Weight = 0.f;
		FVector Direction = FVector::ZeroVector;
	};
	/** Newest first, at most FGSTN_AdditiveReactionRequest::MaxLayers */
	TArray<FActiveAdditive> Additives;

	UPROPERTY(Transient)
	TArray<FGSTN_PresentationTableLayer> Layers;

	FGSTN_PresentationDeriver Deriver;
	FGSTN_PresentationDedupe Dedupe;
	TArray<FGSTN_PresentationEvent> Scratch;
};
