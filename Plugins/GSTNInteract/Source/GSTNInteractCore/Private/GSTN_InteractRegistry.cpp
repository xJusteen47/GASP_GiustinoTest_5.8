// Interaction focus: the stamped candidate registry (interaction.md §3.2, §4). No GAS, no Network Prediction, no Mover.

#include "GSTN_InteractRegistry.h"

FGSTN_InteractRegistry::FGSTN_InteractRegistry(float InCellSizeCm, int32 InMaxRecords)
{
	Configure(InCellSizeCm, InMaxRecords);
}

void FGSTN_InteractRegistry::Configure(float InCellSizeCm, int32 InMaxRecords)
{
	// Existing records keep the cell they were filed under, so changing the size means starting over
	check(Records.Num() == 0);
	CellSizeCm = FMath::Max(InCellSizeCm, 1.f);
	MaxRecords = FMath::Max(InMaxRecords, 1);
}

FIntVector FGSTN_InteractRegistry::CellOf(const FIntVector& PositionCm) const
{
	// Floor division so negative coordinates fall in the right cell
	return FIntVector(
		FMath::FloorToInt(PositionCm.X / CellSizeCm),
		FMath::FloorToInt(PositionCm.Y / CellSizeCm),
		FMath::FloorToInt(PositionCm.Z / CellSizeCm));
}

bool FGSTN_InteractRegistry::Add(const FGSTN_InteractCandidate& Record)
{
	if (!Record.Id.IsValid() || Records.Contains(Record.Id) || Records.Num() >= MaxRecords)
	{
		return false;
	}
	Records.Add(Record.Id, Record);
	Grid.FindOrAdd(CellOf(Record.PositionCm)).Add(Record.Id);
	return true;
}

bool FGSTN_InteractRegistry::Remove(const FGSTN_InteractCandidateId& Id, int32 ToFrame)
{
	FGSTN_InteractCandidate* Record = Records.Find(Id);
	if (!Record)
	{
		return false;
	}
	Record->ToFrame = FMath::Min(Record->ToFrame, ToFrame);
	return true;
}

bool FGSTN_InteractRegistry::RemoveImmediate(const FGSTN_InteractCandidateId& Id)
{
	const FGSTN_InteractCandidate* Record = Records.Find(Id);
	if (!Record)
	{
		return false;
	}
	const FIntVector Cell = CellOf(Record->PositionCm);
	if (TArray<FGSTN_InteractCandidateId>* Bucket = Grid.Find(Cell))
	{
		Bucket->RemoveSingleSwap(Id);
		if (Bucket->IsEmpty())
		{
			Grid.Remove(Cell);
		}
	}
	Records.Remove(Id);
	return true;
}

int32 FGSTN_InteractRegistry::Purge(int32 Frame, int32 TombstoneFrames)
{
	TArray<FGSTN_InteractCandidateId> Dead;
	for (const TPair<FGSTN_InteractCandidateId, FGSTN_InteractCandidate>& Pair : Records)
	{
		// int64: ToFrame is MAX_int32 for a record with no end
		if (Pair.Value.ToFrame != MAX_int32 && static_cast<int64>(Pair.Value.ToFrame) + TombstoneFrames < Frame)
		{
			Dead.Add(Pair.Key);
		}
	}
	for (const FGSTN_InteractCandidateId& Id : Dead)
	{
		RemoveImmediate(Id);
	}
	return Dead.Num();
}

const FGSTN_InteractCandidate* FGSTN_InteractRegistry::Find(const FGSTN_InteractCandidateId& Id) const
{
	return Records.Find(Id);
}

void FGSTN_InteractRegistry::Empty()
{
	Records.Empty();
	Grid.Empty();
}

void FGSTN_InteractRegistry::QueryAround(const FVector& Location, int32 Frame, TFunctionRef<void(const FGSTN_InteractCandidate&)> Visit) const
{
	const FIntVector Centre = CellOf(FGSTN_InteractCandidate::QuantiseCm(Location));
	for (int32 Z = -1; Z <= 1; ++Z)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			for (int32 X = -1; X <= 1; ++X)
			{
				const TArray<FGSTN_InteractCandidateId>* Bucket = Grid.Find(Centre + FIntVector(X, Y, Z));
				if (!Bucket)
				{
					continue;
				}
				for (const FGSTN_InteractCandidateId& Id : *Bucket)
				{
					const FGSTN_InteractCandidate& Record = Records.FindChecked(Id);
					if (Record.IsLiveAt(Frame))
					{
						Visit(Record);
					}
				}
			}
		}
	}
}
