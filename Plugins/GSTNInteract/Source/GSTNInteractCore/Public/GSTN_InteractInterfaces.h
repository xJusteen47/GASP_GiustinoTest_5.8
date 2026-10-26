// Interaction focus: Core interfaces (interaction.md §2.4, §6). No GAS, no Network Prediction, no Mover.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InteractTypes.h"
#include "UObject/Object.h"
#include "GSTN_InteractInterfaces.generated.h"

struct FGSTN_InteractQuery;

/**
 * Line-of-sight test injected into the probe, like the exit collision query of M6 §7.7 (§2.4). The Runtime module implements it
 * with an object-type query for WorldStatic only, simple collision: pawns, doors and physics bodies never block, because they
 * differ between machines or between a frame and its resimulation.
 */
class GSTNINTERACTCORE_API IGSTN_InteractVisibilityQuery
{
public:
	virtual ~IGSTN_InteractVisibilityQuery() = default;

	/** True when the segment is blocked by static geometry */
	virtual bool IsSegmentBlocked(const FVector& From, const FVector& To) const = 0;
};

/**
 * A source of candidates that move in the simulation (§6 item 2): enemies to grab, allies to mount. Deterministic, no component
 * reads, reading only the query (and the opaque host context). Registered with a provider id the candidates' ids carry. Static
 * records do not need a provider: they live in the FGSTN_InteractRegistry.
 */
class GSTNINTERACTCORE_API IGSTN_InteractCandidateProvider
{
public:
	virtual ~IGSTN_InteractCandidateProvider() = default;

	/** The provider byte of the ids this provider produces (>= GSTNInteractProviders::FirstConsumer) */
	virtual uint8 GetProviderId() const = 0;

	/** Appends candidates near Query.Origin. The probe filters them by stamp, reach, arc and requirements */
	virtual void GatherCandidates(const FGSTN_InteractQuery& Query, TArray<FGSTN_InteractCandidate>& OutCandidates) const = 0;
};

/**
 * Opt-in Blueprint filter (§6 item 3, Mod.17): sees the best few survivors only, may reject one or bias its cost. Must be pure:
 * the same view gives the same answer, no world reads. The probe calls it at most FGSTN_InteractRules::MaxFilterCalls times.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class GSTNINTERACTCORE_API UGSTN_InteractCandidateFilter : public UObject
{
	GENERATED_BODY()

public:
	/** bAccept false drops the candidate; ScoreBias is added to its cost (positive = less likely) */
	UFUNCTION(BlueprintNativeEvent, Category = "Interact")
	void Evaluate(const FGSTN_InteractCandidateView& View, bool& bAccept, float& ScoreBias) const;
	virtual void Evaluate_Implementation(const FGSTN_InteractCandidateView& View, bool& bAccept, float& ScoreBias) const
	{
		bAccept = true;
		ScoreBias = 0.f;
	}
};
