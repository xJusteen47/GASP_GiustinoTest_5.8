// Leaf pose node: plays the clip a Chooser Table picks for its anim instance (greybox simple locomotion, greybox-capsula 2026-11-24).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "GSTN_AnimNode_ChooserPlayer.generated.h"

class UAnimSequenceBase;
class UChooserTable;

/** How the chosen clip loops */
UENUM(BlueprintType)
enum class EGSTN_ChooserPlayerLoop : uint8
{
	/** The clip asset's own setting (looping sequences loop) */
	Asset,
	Loop,
	Once
};

/**
 * Written by the chooser's output columns for the row it picks (chooser parameter 1, Write): how to play the clip. The row's clip is the
 * chooser's result. Same idea as GASP's experimental state machine OUT struct (StartTime, BlendTime), plus the play rate and the loop.
 */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_ChooserPlayerOutput
{
	GENERATED_BODY()

	/** Seconds into the clip where it starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chooser Player", meta = (ClampMin = "0", ForceUnits = "s"))
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chooser Player", meta = (ClampMin = "0.01", ClampMax = "10"))
	float PlayRate = 1.f;

	/** Inertial blend from the previous pose when this clip starts (needs an Inertialization node after the player) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chooser Player", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float BlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chooser Player")
	EGSTN_ChooserPlayerLoop Loop = EGSTN_ChooserPlayerLoop::Asset;

	/** Ask the chooser again every frame while this clip plays; a different clip starts with its own blend (e.g. air up -> air down) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chooser Player")
	bool bReselectWhileActive = false;
};

/**
 * Asks Chooser (context: parameter 0 = the anim instance, ReadWrite; parameter 1 = FGSTN_ChooserPlayerOutput, Write) for a clip each time the
 * node becomes relevant (a state machine state entered), then plays it with the row's start time, rate, loop and blend. No root motion is
 * extracted: Mover moves the character, the clip only poses it.
 *
 * Why a node and not GASP's Blend Stack + On State Entry functions: those need Blueprint function bindings per state, which our scripts
 * cannot author reliably; one node per state with the same chooser keeps the graph script-built and every clip a chooser row (owner
 * 2026-11-24: "put things in the choosers, easy and scalable"). Which state's rows apply is a chooser column on the instance's state enum.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_ChooserPlayer : public FAnimNode_Base
{
	GENERATED_BODY()

	/** Root chooser; its first column is usually the instance's state enum */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<UChooserTable> Chooser;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override {}
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	/** Pure: next time of a clip of Length played at Rate for DeltaSeconds (wraps when looping, holds the last frame otherwise) */
	static float AdvanceTime(float Time, float DeltaSeconds, float Rate, float Length, bool bLooping);

	/** Pure: whether the clip loops under a loop mode */
	static bool ResolveLooping(EGSTN_ChooserPlayerLoop Mode, bool bAssetLooping);

private:
	/** Runs the chooser; true when it gave a clip (Clip/Output updated) */
	bool Choose(const FAnimationBaseContext& Context);

	void StartClip(const FAnimationUpdateContext& Context);

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Clip;

	FGSTN_ChooserPlayerOutput Output;
	float Time = 0.f;
	bool bLooping = false;

	/** Detects re-entry: the node was not updated last frame */
	FGraphTraversalCounter UpdateCounter;
};
