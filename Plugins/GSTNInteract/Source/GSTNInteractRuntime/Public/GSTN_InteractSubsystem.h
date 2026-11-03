// Interaction world subsystem: registry owner, frame source, lookup by id, static line of sight (interaction.md §3.2, §2.4, §9).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractInterfaces.h"
#include "GSTN_InteractMath.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_InteractSubsystem.generated.h"

class UGSTN_InteractableComponent;

/**
 * Owns the world's candidate registry and meaning table, turns server time into the frame numbers stamps carry, finds the
 * component behind a candidate id, and answers line-of-sight queries against WorldStatic only (§2.4). Created in every net mode,
 * dedicated servers included: a server validates presses against the same registry. Game thread only.
 */
UCLASS()
class GSTNINTERACTRUNTIME_API UGSTN_InteractSubsystem : public UWorldSubsystem, public IGSTN_InteractVisibilityQuery
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UGSTN_InteractSubsystem* Get(const UWorld* World) { return World ? World->GetSubsystem<UGSTN_InteractSubsystem>() : nullptr; }

	/** Server frame number now: the game state's server world time times FrameRateHz (world time when there is no game state) */
	int32 GetFrame() const;

	/** Replaces GetFrame with a fixed value (tests, replay tools, a paused sandbox); ClearFrameOverride restores the clock */
	void SetFrameOverride(int32 Frame) { FrameOverride = Frame; }
	void ClearFrameOverride() { FrameOverride.Reset(); }

	FGSTN_InteractRegistry& GetRegistry() { return Registry; }
	const FGSTN_InteractRegistry& GetRegistry() const { return Registry; }
	const FGSTN_InteractMeaningTable& GetMeanings() const { return Meanings; }

	/** Requirement queries candidates registered here index through QueryIndex; pass to FGSTN_InteractQuery::Requirements */
	TConstArrayView<FGameplayTagQuery> GetRequirements() const { return Requirements; }

	/** Adds a requirement query and returns its index (-1 for an empty query) */
	int32 AddRequirement(const FGameplayTagQuery& Query);

	/** Adds the record and remembers its component for ResolveComponent. False on a duplicate key or a full registry */
	bool RegisterRecord(const FGSTN_InteractCandidate& Record, UGSTN_InteractableComponent* Component);

	/** Stamps the end of the record at the current frame and purges old tombstones */
	void UnregisterRecord(const FGSTN_InteractCandidateId& Id);

	/** The component of a registry record, or null (unknown, destroyed, a provider's candidate) */
	UGSTN_InteractableComponent* ResolveComponent(const FGSTN_InteractCandidateId& Id) const;

	/** Handle for a record the server spawns: a per-world serial, high bit set so it never meets a stable-key hash. Never reused */
	int32 AllocateSpawnedHandle();
	static int32 MakeStableHandle(const FString& StableKey);

	void RegisterProvider(const IGSTN_InteractCandidateProvider* Provider);
	void UnregisterProvider(const IGSTN_InteractCandidateProvider* Provider);
	TConstArrayView<const IGSTN_InteractCandidateProvider*> GetProviders() const { return Providers; }

	/** The settings' default rules, sanitised once at Initialize */
	const FGSTN_InteractRules& GetDefaultRules() const { return DefaultRules; }

	// IGSTN_InteractVisibilityQuery: one segment, WorldStatic objects only, simple collision. Pawns, doors and physics bodies never
	// block: they differ between machines, or between a frame and its resimulation
	virtual bool IsSegmentBlocked(const FVector& From, const FVector& To) const override;

private:
	FGSTN_InteractRegistry Registry;
	FGSTN_InteractMeaningTable Meanings;
	FGSTN_InteractRules DefaultRules;
	TArray<FGameplayTagQuery> Requirements;
	TMap<FGSTN_InteractCandidateId, TWeakObjectPtr<UGSTN_InteractableComponent>> Components;
	TArray<const IGSTN_InteractCandidateProvider*> Providers;
	uint32 SpawnedSerial = 0;
	TOptional<int32> FrameOverride;
};
