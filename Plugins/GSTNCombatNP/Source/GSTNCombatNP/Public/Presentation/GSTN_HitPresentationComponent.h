// Combat simulation (GAS-NPP fork): the attacker's own hit presentation (combat-m4-hits 9b, M4 H10).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayCueInterface.h"
#include "Presentation/GSTN_HitPresentation.h"
#include "GSTN_HitPresentationComponent.generated.h"

class USkeletalMeshComponent;
struct FGSTN_CombatSyncCore;

/**
 * Shows a hit the moment the attacker's machine predicts it, instead of one round trip plus the interpolation buffer later when the
 * authority's confirmation and the victim's reaction arrive. Add it to a pawn that carries a UGSTN_CombatSimComponent; it works on the
 * locally controlled pawn only (the host's own pawn included: nothing rolls back there, so it simply never cancels).
 *
 * On a new entry in the attacker's hit target ring (seen by the combat component's FinalizeFrame) it:
 *  - runs the hit's gameplay cue on the victim locally (no replication) and remembers the hit id, so the real cue of the same hit is
 *    played once: a cue handler on this machine asks ShouldPlayAuthoritativeCue before it plays;
 *  - flashes the victim's meshes through a scalar material parameter;
 *  - pauses the attacker's mesh for its hitstop and the victim's for the victim's hitstop (the animation rate, never the simulation);
 *  - for a victim that is an interpolated proxy here, extrapolates its reaction (the launch or push back direction, without the victim's
 *    DI) as a short offset of its mesh, fading out as the authoritative motion arrives.
 * When the entry disappears while the same move still runs (the authority did not confirm the hit and the state was corrected), every
 * effect of the hit is cancelled: the mesh resumes, the flash and the offset go, the cue's record is forgotten. A cue already played cannot be
 * recalled; it is one shot.
 *
 * Presentation only: nothing here is read by the simulation, and it holds no sync state.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_HitPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_HitPresentationComponent();

	/** Master switch of the whole component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation")
	bool bEnabled = true;

	/** Run the hit's cue locally on the victim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation")
	bool bExecuteCueLocal = true;

	/** Scalar material parameter set to 1 on the victim's meshes at the hit and faded to 0. None = no flash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Flash")
	FName FlashParameterName = TEXT("HitFlash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Flash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FlashSeconds = 0.12f;

	/** Pause the attacker's and the victim's meshes for their hitstop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Hitstop")
	bool bPauseMeshes = true;

	/** Move the mesh of an interpolated victim along its predicted reaction until the authoritative motion takes over */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Reaction")
	bool bPredictVictimReaction = true;

	/** How long the extrapolation lasts after the victim's hitstop; it fades out over its second half */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Reaction", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ReactionSeconds = 0.25f;

	/** The most the mesh is moved off the actor by the extrapolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit Presentation|Reaction", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxReactionOffset = 60.f;

	/**
	 * Called by the combat component of this pawn from its FinalizeFrame, once per presented frame, with the attacker's latest state and the
	 * hits its tick predicted. Core is the state of Frame
	 */
	void OnAttackerFrame(const FGSTN_CombatSyncCore& Core, int32 Frame, int32 AttackerId, TConstArrayView<FGSTN_PredictedHitView> Views, float StepMs);

	/**
	 * For a cue handler on the machine of the attacker: false when this hit's cue was already played locally by the prediction (and uses the
	 * record up), true otherwise. Params.EffectContext must be the combat hit context the victim's simulation makes
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Presentation")
	static bool ShouldPlayAuthoritativeCue(const AActor* Attacker, const AActor* Victim, const FGameplayCueParameters& Params);

	/** The id a hit has for the dedupe: both actors, the attacker's timeline slot and the hitbox */
	static uint32 MakeHitId(const AActor* Attacker, const AActor* Victim, int32 Timeline, int32 HitboxIndex);

	/** "started,cancelled,cuesPlayed,cuesSuppressed,active,lastHitstopMs" for the harness. Not state */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	FString GetPresentationInfo() const;

	/** Number of meshes the component holds paused right now */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	int32 GetNumPausedMeshes() const { return PausedMeshes.Num(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPauseState
	{
		float OldRate = 1.f;
		int32 Holders = 0;
	};

	struct FEffect
	{
		FGSTN_HitPresentationKey Key;
		uint32 HitId = 0;
		TWeakObjectPtr<AActor> Victim;
		double StartTime = 0.0;
		bool bFlashing = false;
		bool bAttackerPaused = false;
		bool bVictimPaused = false;
		double AttackerPauseEnd = 0.0;
		double VictimPauseEnd = 0.0;
		TWeakObjectPtr<USkeletalMeshComponent> VictimMesh;
		FVector ReactionVelocity = FVector::ZeroVector;
		double ReactionStart = 0.0;
		FVector AppliedOffset = FVector::ZeroVector;
		bool bReacting = false;
	};

	void StartEffect(const FGSTN_HitPresentationKey& Key, const FGSTN_PredictedHitView& View, int32 Frame, int32 AttackerId, float StepMs);
	void EndEffect(FEffect& Effect);
	void CancelEffect(const FGSTN_HitPresentationKey& Key);
	void PauseMesh(USkeletalMeshComponent* Mesh);
	void ReleaseMesh(USkeletalMeshComponent* Mesh);
	void SetFlash(AActor* Victim, float Value);
	USkeletalMeshComponent* FindMesh(const AActor* Actor) const;
	void UpdateTickEnabled();

	FGSTN_HitPresentationTracker Tracker;
	FGSTN_HitCueDedup Dedup;
	TArray<FEffect> Effects;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FPauseState> PausedMeshes;

	int32 NumStarted = 0;
	int32 NumCancelled = 0;
	int32 NumCuesPlayed = 0;
	int32 NumCuesSuppressed = 0;
	float LastHitstopMs = 0.f;
};
