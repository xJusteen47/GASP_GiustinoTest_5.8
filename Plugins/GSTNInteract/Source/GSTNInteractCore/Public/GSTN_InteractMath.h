// Interaction focus: the pure maths (interaction.md §2, §3.3, §9). No GAS, no Network Prediction, no Mover.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractInterfaces.h"
#include "GSTN_InteractRegistry.h"
#include "GSTN_InteractTypes.h"
#include "UObject/Object.h"
#include "GSTN_InteractMath.generated.h"

/** Opaque host data a provider may need (a combat snapshot): a pointer and a type name the provider checks; the core knows no combat type (§6 item 2) */
struct FGSTN_InteractHostContext
{
	const void* Data = nullptr;
	FName Type;

	template <typename T>
	const T* As(FName ExpectedType) const { return (Data && Type == ExpectedType) ? static_cast<const T*>(Data) : nullptr; }
};

/**
 * Inputs of one probe (§9). Plain struct, not reflected: it carries pointers to the caller's data, valid for the call only.
 * Everything here must come from simulated inputs (or, in the game thread interactor, from the same values the pawn really has).
 */
struct GSTNINTERACTCORE_API FGSTN_InteractQuery
{
	/** The frame the stamps are compared with (server frame numbers) */
	int32 Frame = 0;

	/** Interactor position in centimetres; quantised to whole centimetres before use */
	FVector Origin = FVector::ZeroVector;

	/** Simulated up. Planar angles and the height band are measured against it, never world Z (gravity can change) */
	FVector Up = FVector::UpVector;

	FVector Facing = FVector::ForwardVector;

	/** World-plane stick direction (any length) and magnitude 0..1; the stick points once the magnitude reaches the rules' StickThreshold */
	FVector StickDirection = FVector::ZeroVector;
	float StickMagnitude = 0.f;

	/** 3D aim direction from AimYaw/AimPitch for CameraAim mode; ignored unless bHasAim */
	FVector AimDirection = FVector::ForwardVector;
	bool bHasAim = false;

	/** Interact is pressed or held (the hold lock) */
	bool bInteractHeld = false;

	/** The optional cycle command was issued this frame (needs CycleSkipFrames > 0) */
	bool bCycleRequested = false;

	/** The interactor's tags (end of the previous frame in a simulation); null = none */
	const FGameplayTagContainer* Tags = nullptr;

	const FGSTN_InteractRules* Rules = nullptr;
	const FGSTN_InteractRegistry* Registry = nullptr;
	const FGSTN_InteractMeaningTable* Meanings = nullptr;

	/** Requirement queries candidates index through QueryIndex */
	TConstArrayView<FGameplayTagQuery> Requirements;

	/** Providers of moving candidates */
	TConstArrayView<const IGSTN_InteractCandidateProvider*> Providers;

	const IGSTN_InteractVisibilityQuery* Visibility = nullptr;
	const UGSTN_InteractCandidateFilter* Filter = nullptr;
	FGSTN_InteractHostContext HostContext;
};

/** The resolved pointing direction */
struct FGSTN_InteractPointing
{
	FVector Direction = FVector::ForwardVector;
	/** True for a 3D direction (camera aim); false for a direction in the plane perpendicular to Up */
	bool b3D = false;
	EGSTN_InteractPointingSource Source = EGSTN_InteractPointingSource::Facing;
};

/** One scored candidate (kept by the probe, exposed for tests and debug draw) */
struct FGSTN_InteractScored
{
	FGSTN_InteractCandidate Candidate;
	/** Cost in 1/4096 units, the only value ever compared */
	int32 CostFixed = 0;
	int32 TieBucket = 0;
	int32 Priority = 0;
	float DistanceCm = 0.f;
	float AngleDeg = 0.f;
	bool bUnactionable = false;
};

/**
 * Pointing, candidate selection, scoring and the sticky focus (§2). All functions are pure: the result is a function of the
 * inputs and the candidate set, never of gather order. Nothing here reads a world, a clock or a component.
 */
UCLASS()
class GSTNINTERACTCORE_API UGSTN_InteractMath : public UObject
{
	GENERATED_BODY()

public:
	/** 1/4096: the fixed-point scale of costs */
	static constexpr int32 CostScale = 4096;

	/** The pointing direction of the query: stick else facing, or the 3D aim (§2.1), yaw quantised to 16 bits */
	static FGSTN_InteractPointing ComputePointing(const FGSTN_InteractQuery& Query);

	/**
	 * Scores one candidate against the pointing under the normal limits scaled by ReachScale and ArcScale (1 = acquire, the
	 * sticky scales = keep). False when it is not focusable (stamps, reach, height band, arc, requirements).
	 */
	static bool ScoreCandidate(const FGSTN_InteractQuery& Query, const FGSTN_InteractPointing& Pointing, const FGSTN_InteractCandidate& Candidate,
		float ReachScale, float ArcScale, FGSTN_InteractScored& OutScored);

	/** Gathers (registry cells plus providers), caps and scores: the focusable candidates sorted best first under the total order of §2.3 */
	static void GatherAndScore(const FGSTN_InteractQuery& Query, const FGSTN_InteractPointing& Pointing, TArray<FGSTN_InteractScored>& OutSorted);

	/** True when A sorts before B: tie bucket, then priority, then lower id (priority first in Lexicographic mode) */
	static bool SortsBefore(const FGSTN_InteractScored& A, const FGSTN_InteractScored& B, EGSTN_InteractPriorityMode Mode);

	/** One probe: updates the state and returns the focus (§2.5). Needs Query.Rules */
	static FGSTN_InteractFocusResult UpdateFocus(const FGSTN_InteractQuery& Query, FGSTN_InteractFocusState& State);

	/**
	 * Server-side check of a press (§3.3): stamps, the requirement and the reach with ServerReachSlackCm of lag slack. No arc:
	 * the consumer validates its own rules at its confirmation frame.
	 */
	static bool IsStillInteractable(const FGSTN_InteractCandidate& Candidate, const FVector& InteractorLocation, const FGameplayTagContainer* Tags,
		TConstArrayView<FGameplayTagQuery> Requirements, int32 Frame, const FGSTN_InteractRules& Rules);

	/** Rounds a direction into the plane perpendicular to Up and its yaw to 16 bits. Exposed for tests and bridges */
	static FVector QuantiseYaw(const FVector& PlanarDirection, const FVector& Up);

	static FGSTN_InteractCandidateId MakeCandidateId(int32 Provider, int32 Handle);

	/** 32-bit hash of a stable per-actor key, for level-placed candidates */
	static int32 HashStableKey(const FString& StableKey);
};
