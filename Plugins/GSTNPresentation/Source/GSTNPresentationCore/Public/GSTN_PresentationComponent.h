// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GSTN_AdditiveReactionSource.h"
#include "GSTN_PresentationLogic.h"
#include "GSTN_PresentationComponent.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;
class UStaticMeshComponent;

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
class UNiagaraComponent;

UCLASS(ClassGroup = (GSTN), Config = Game, meta = (BlueprintSpawnableComponent))
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

	/** Asked after every layer; null = ProjectDefaultTable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	TObjectPtr<UGSTN_PresentationTable> DefaultTable;

	/**
	 * The project's last fallback, from DefaultGame.ini ([/Script/GSTNPresentationCore.GSTN_PresentationComponent] ProjectDefaultTable=...):
	 * a component added at run time (UGSTN_CombatPresentationEvents) has no DefaultTable of its own. Data, not a gameplay setting.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Presentation")
	TSoftObjectPtr<UGSTN_PresentationTable> ProjectDefaultTable;

	/**
	 * VFX v2 (owner decision 2A, 2026-11-17): while this character is in hitstop (Combat.State.Hitstop in its snapshot), the one-shot
	 * effects it spawned run at this time scale, then go on at full speed with the character. 0 = frozen, 1 = free. A player setting: the
	 * settings menu writes it on every presentation component the local player sees (no project setting). Cosmetic only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Hitstop", meta = (ClampMin = "0", ClampMax = "1"))
	float HitstopVfxTimeScale = 0.15f;

	/** Frames an effect runs at full speed before the hitstop slows it: the hit's flash (frames 1-2) shows whatever the scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Hitstop", meta = (ClampMin = "0", ClampMax = "8"))
	int32 HitstopVfxFreeFrames = 2;

	/**
	 * VFX v3 (docs/notes/stile/vfx-stile-v3-design-sheet.md, owner ref astro_2): a crisp dark disc straight under the character, the
	 * depth cue of a platformer camera. A flat mesh placed by one line trace per presented frame (SubmitSnapshot), never on a dedicated
	 * server; it shrinks and fades with the height (custom primitive data 0 = opacity). Cosmetic only, a graphics setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|ContactShadow")
	bool bContactShadow = true;

	/** Unlit translucent disc material (reads custom primitive data 0 as opacity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|ContactShadow")
	TSoftObjectPtr<UMaterialInterface> ContactShadowMaterial;

	/** Disc radius on the ground; 0 = the owner's collision radius x 1.15 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|ContactShadow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ContactShadowRadius = 0.f;

	/** Above this height over the floor there is no shadow; in between it shrinks to 60% and fades */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|ContactShadow", meta = (ClampMin = "50", ForceUnits = "cm"))
	float ContactShadowMaxHeight = 700.f;

	/**
	 * VFX v3 dust and snow take the colour of the floor they rise from (owner ref astro_3): a system exposing User.SurfaceColor gets the
	 * colour of the surface type of the floor under the event (Physical Material), or DefaultSurfaceColor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Surface")
	TMap<TEnumAsByte<EPhysicalSurface>, FLinearColor> SurfaceColors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Surface")
	FLinearColor DefaultSurfaceColor = FLinearColor(0.78f, 0.74f, 0.68f, 1.f);

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

	/** A sustained cue's system still playing: deactivated by a StopOn event or at its end time */
	struct FSustained
	{
		TWeakObjectPtr<UNiagaraComponent> System;
		FGameplayTagContainer StopOn;
		/** World time (s); <= 0 = no cap */
		double EndTime = 0.0;
		/** FGSTN_PresentationCue::SustainMinSpeed */
		float MinSpeed = 0.f;
	};
	TArray<FSustained> Sustained;

	/** A one-shot effect of this character, slowed while the character is in hitstop (HitstopVfxTimeScale) */
	struct FOneShot
	{
		TWeakObjectPtr<UNiagaraComponent> System;
		/** World time (s) of the spawn */
		double SpawnTime = 0.0;
	};
	TArray<FOneShot> OneShots;

	/** Applies HitstopVfxTimeScale to the live one-shots; drops the finished ones */
	void UpdateHitstopTime(double Now);

	/**
	 * VFX v2 bounces and shadows (owner decision 3A): a system exposing User.GroundPoint / User.GroundNormal (its local space) and
	 * User.GroundHeight (world) gets the floor under the event: one line trace per event on the game thread, the feet as fallback.
	 * The particles bounce on that analytical plane (no Niagara trace, no depth buffer: the toaster target) and their shadows lie on it
	 */
	void SetGroundPlane(UNiagaraComponent* System, const FGSTN_PresentationEvent& Event) const;

	/** Places the contact shadow under the owner (SubmitSnapshot, every presented frame) */
	void UpdateContactShadow(const FGSTN_PresentationSnapshot& Snapshot);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ContactShadow;

	/** A cue waiting for the drawn body (FGSTN_PresentationCue::GroundedBone) */
	struct FPendingGrounded
	{
		FGSTN_PresentationCue Cue;
		FGSTN_PresentationEvent Event;
		float Intensity = 1.f;
		/** World time (s) after which it plays anyway */
		double Deadline = 0.0;
	};
	TArray<FPendingGrounded> PendingGrounded;

	/** True (and Event.Location moved under the bone) when the cue may play now */
	bool IsBodyGrounded(const FGSTN_PresentationCue& Cue, FGSTN_PresentationEvent& Event) const;

	/** Stops the sustained systems that EventTag ends; drops the finished ones */
	void StopSustained(const FGameplayTag& EventTag);
	void UpdateTickEnabled();

	UPROPERTY(Transient)
	TArray<FGSTN_PresentationTableLayer> Layers;

	FGSTN_PresentationDeriver Deriver;
	FGSTN_PresentationDedupe Dedupe;
	TArray<FGSTN_PresentationEvent> Scratch;
};
