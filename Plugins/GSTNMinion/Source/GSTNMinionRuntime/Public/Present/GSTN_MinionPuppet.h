// Minion runtime (client presentation, M8 G7).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_MinionPuppet.generated.h"

class UGSTN_MinionDefinition;
class USkeletalMeshComponent;

/**
 * What a minion looks like on a machine that renders (combat-m8-minions.md 8.4): a skeletal mesh and its animation, nothing else. No
 * capsule, no movement component, no collision, not replicated, actor tick off (only the mesh's animation ticks). Pooled by
 * UGSTN_MinionPresentationSubsystem: acquired for a minion that is in the visible set, released when it leaves it, never destroyed
 * while the world lives, so 40 minions coming and going cause no spawn churn.
 *
 * Animation cost stack (8.4): update-rate optimization on, the pose only ticks while rendered, and the mesh is hidden (not just
 * culled) while pooled. The Animation Budget Allocator is not wired here: its component type lives in an engine plugin this project
 * does not enable, and this plugin may not depend on it (the presentation subsystem logs what it found once).
 *
 * The hit flash goes through custom primitive data (one float at FlashDataIndex), never a dynamic material instance per puppet: the
 * puppet's material reads it as a per-primitive value, so 60 puppets share their materials. The hitstop look is the mesh's global
 * animation rate at 0. The subsystem drives both; the puppet holds no state of its own that matters to anything.
 */
UCLASS(NotPlaceable, Transient)
class GSTNMINIONRUNTIME_API AGSTN_MinionPuppet : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_MinionPuppet();

	/** Index of the custom primitive data float the material reads as the hit flash (0 = none, 1 = full) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minion|Presentation", meta = (ClampMin = "0"))
	int32 FlashDataIndex = 0;

	/**
	 * Takes the puppet out of the pool for a minion: mesh and animation of Definition (soft, loaded on first use of the type; a null mesh
	 * is fine, the puppet then shows nothing), placed at Transform, shown
	 */
	void ActivatePuppet(const UGSTN_MinionDefinition* Definition, int32 InPresentKey, const FTransform& Transform);

	/** Back to the pool: hidden, animation stopped, flash and pause cleared */
	void DeactivatePuppet();

	void SetPose(const FVector& Location, float YawDegrees);
	void SetFlash(float Value);
	void SetPaused(bool bPaused);

	/** The visibility cut of the subsystem: a puppet over MaxVisiblePuppets is not drawn and its pose does not tick */
	void SetShown(bool bShown);

	bool IsInUse() const { return bInUse; }
	int32 GetPresentKey() const { return PresentKey; }
	const UGSTN_MinionDefinition* GetDefinition() const { return Definition.Get(); }
	float GetFlash() const { return Flash; }
	bool IsPaused() const { return bPaused; }
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Minion|Presentation")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	TWeakObjectPtr<const UGSTN_MinionDefinition> Definition;
	int32 PresentKey = INDEX_NONE;
	float Flash = 0.f;
	bool bPaused = false;
	bool bInUse = false;
};

/**
 * Placeholder of the crystal shatter of 4d.4: a pooled actor that marks where a minion died, for ShatterSeconds, and shows nothing. It
 * exists so the death path (the puppet goes, something takes its place, both are pooled) is in place and measured before the art of
 * combat-parts 6.3 arrives (no aesthetics before the mechanics are done, owner 2026-10-24).
 */
UCLASS(NotPlaceable, Transient)
class GSTNMINIONRUNTIME_API AGSTN_MinionShatter : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_MinionShatter();

	void Play(const FVector& Location, double EndSeconds);
	void Stop();

	bool IsPlaying() const { return bPlaying; }
	double GetEndSeconds() const { return EndSeconds; }

private:
	double EndSeconds = 0.0;
	bool bPlaying = false;
};
