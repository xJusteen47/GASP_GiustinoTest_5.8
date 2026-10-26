// Interaction focus: the stamped candidate registry (interaction.md §3.2, §4). No GAS, no Network Prediction, no Mover.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InteractTypes.h"

/**
 * Flat records plus a 3D hash grid (§4). Cells are cubes, gravity-agnostic, so a query reads 27 cells; add and remove are O(1)
 * and nothing runs per frame. The registry is not rolled back: it is append-only and stamp-gated, so the set a reader sees at
 * frame F depends on the records and on F, never on when the machine learned them. A removal stamps ToFrame and keeps a
 * tombstone until Purge. Not thread safe; one registry per world.
 */
class GSTNINTERACTCORE_API FGSTN_InteractRegistry
{
public:
	/** Cells should be at least HardMaxReachCm times the sticky reach scale plus the largest radius, so one ring of cells covers a probe */
	explicit FGSTN_InteractRegistry(float InCellSizeCm = 600.f, int32 InMaxRecords = 4096);

	void Configure(float InCellSizeCm, int32 InMaxRecords);

	/** False when the id is invalid, already present (a duplicate stable key, tombstones included) or the registry is full */
	bool Add(const FGSTN_InteractCandidate& Record);

	/** Stamps ToFrame on a live record (never extends a shorter lifetime). False when unknown */
	bool Remove(const FGSTN_InteractCandidateId& Id, int32 ToFrame);

	/** Drops the record at once (local, non-simulated use) */
	bool RemoveImmediate(const FGSTN_InteractCandidateId& Id);

	/** Drops tombstones whose ToFrame + TombstoneFrames is behind Frame. Returns how many */
	int32 Purge(int32 Frame, int32 TombstoneFrames);

	const FGSTN_InteractCandidate* Find(const FGSTN_InteractCandidateId& Id) const;

	/** Records stored (live and tombstones) */
	int32 Num() const { return Records.Num(); }

	void Empty();

	/**
	 * Calls Visit for every record in the cells around Location (a superset of the records within one cell size), live at
	 * Frame only. Visit order is unspecified: callers must not depend on it (the probe sorts under a total order).
	 */
	void QueryAround(const FVector& Location, int32 Frame, TFunctionRef<void(const FGSTN_InteractCandidate&)> Visit) const;

private:
	FIntVector CellOf(const FIntVector& PositionCm) const;

	float CellSizeCm = 600.f;
	int32 MaxRecords = 4096;
	TMap<FGSTN_InteractCandidateId, FGSTN_InteractCandidate> Records;
	TMap<FIntVector, TArray<FGSTN_InteractCandidateId>> Grid;
};
