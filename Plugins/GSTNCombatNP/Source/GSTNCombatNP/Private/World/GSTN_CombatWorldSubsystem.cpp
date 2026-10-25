// Combat simulation (GAS-NPP fork).

#include "World/GSTN_CombatWorldSubsystem.h"

#include "Algo/BinarySearch.h"
#include "Algo/StableSort.h"
#include "Engine/World.h"
#include "Hits/GSTN_HurtboxTypes.h"

const FGSTN_SnapshotEntry* FGSTN_StepSnapshot::Find(int32 EntityId) const
{
	const int32 Index = Algo::LowerBoundBy(Entries, EntityId, &FGSTN_SnapshotEntry::EntityId);
	return Entries.IsValidIndex(Index) && Entries[Index].EntityId == EntityId ? &Entries[Index] : nullptr;
}

FIntVector FGSTN_StepSnapshot::CellOf(const FVector& Point) const
{
	return FIntVector(FMath::FloorToInt32(Point.X / CellSize), FMath::FloorToInt32(Point.Y / CellSize), FMath::FloorToInt32(Point.Z / CellSize));
}

void FGSTN_StepSnapshot::BuildGrid(double DefaultCellSize)
{
	Cells.Reset();
	double LargestRadius = 0.0;
	for (const FGSTN_SnapshotEntry& Entry : Entries)
	{
		LargestRadius = FMath::Max(LargestRadius, static_cast<double>(Entry.BoundRadius));
	}
	// Twice the largest extent: no entity spans more than two cells on an axis
	CellSize = LargestRadius > 0.0 ? FMath::Max(2.0 * LargestRadius, 1.0) : DefaultCellSize;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGSTN_SnapshotEntry& Entry = Entries[Index];
		const FVector Center = Entry.Transform.GetLocation();
		const FVector Extent(Entry.BoundRadius);
		const FIntVector Min = CellOf(Center - Extent);
		const FIntVector Max = CellOf(Center + Extent);
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			{
				for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
				{
					Cells.FindOrAdd(FIntVector(X, Y, Z)).Add(Index);
				}
			}
		}
	}
}

void FGSTN_StepSnapshot::QueryCandidates(const FBox& Box, TArray<int32>& OutEntryIndices) const
{
	OutEntryIndices.Reset();
	const FIntVector Min = CellOf(Box.Min);
	const FIntVector Max = CellOf(Box.Max);
	const int64 CellCount = static_cast<int64>(Max.X - Min.X + 1) * (Max.Y - Min.Y + 1) * (Max.Z - Min.Z + 1);

	if (CellCount > Cells.Num() * 4 + 64)
	{
		// A box much larger than the populated grid: walking its cells costs more than looking at every entry
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			OutEntryIndices.Add(Index);
		}
	}
	else
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
			{
				for (int32 Z = Min.Z; Z <= Max.Z; ++Z)
				{
					if (const TArray<int32>* Cell = Cells.Find(FIntVector(X, Y, Z)))
					{
						OutEntryIndices.Append(*Cell);
					}
				}
			}
		}
		OutEntryIndices.Sort();
		for (int32 Index = OutEntryIndices.Num() - 1; Index > 0; --Index)
		{
			if (OutEntryIndices[Index] == OutEntryIndices[Index - 1])
			{
				OutEntryIndices.RemoveAt(Index, EAllowShrinking::No);
			}
		}
	}

	// The cells are a coarse filter: keep the entries whose bounding sphere touches the box
	OutEntryIndices.RemoveAll([this, &Box](int32 Index)
	{
		const FGSTN_SnapshotEntry& Entry = Entries[Index];
		const FVector Center = Entry.Transform.GetLocation();
		return FVector::DistSquared(Center, Box.GetClosestPointTo(Center)) > FMath::Square(static_cast<double>(Entry.BoundRadius));
	});
}

UGSTN_CombatWorldSubsystem* UGSTN_CombatWorldSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_CombatWorldSubsystem>() : nullptr;
}

bool UGSTN_CombatWorldSubsystem::RegisterEntity(int32 EntityId, IGSTN_SnapshotSource& Source, UObject* Owner, FGenericTeamId Team, const UGSTN_HurtboxSetDefinition* Hurtboxes)
{
	if (EntityId < 0)
	{
		return false;
	}
	const int32 Index = Algo::LowerBoundBy(Registry, EntityId, &FRegistryEntry::EntityId);
	if (Registry.IsValidIndex(Index) && Registry[Index].EntityId == EntityId)
	{
		return false;
	}
	FRegistryEntry Entry;
	Entry.EntityId = EntityId;
	Entry.Source = &Source;
	Entry.Owner = Owner;
	Entry.bHasOwner = Owner != nullptr;
	Entry.Team = Team;
	Entry.Hurtboxes = Hurtboxes;
	Registry.Insert(Entry, Index);
	InvalidateSnapshot();
	return true;
}

void UGSTN_CombatWorldSubsystem::UnregisterEntity(int32 EntityId)
{
	const int32 Index = Algo::LowerBoundBy(Registry, EntityId, &FRegistryEntry::EntityId);
	if (Registry.IsValidIndex(Index) && Registry[Index].EntityId == EntityId)
	{
		Registry.RemoveAt(Index);
		InvalidateSnapshot();
	}
}

bool UGSTN_CombatWorldSubsystem::RegisterProvider(IGSTN_HittableProvider* Provider)
{
	if (!Provider)
	{
		return false;
	}
	const int32 Id = Provider->GetProviderId();
	const int32 Index = Algo::LowerBoundBy(Providers, Id, [](const IGSTN_HittableProvider* Other) { return Other->GetProviderId(); });
	if (Providers.IsValidIndex(Index) && Providers[Index]->GetProviderId() == Id)
	{
		return false;
	}
	Providers.Insert(Provider, Index);
	InvalidateSnapshot();
	return true;
}

void UGSTN_CombatWorldSubsystem::UnregisterProvider(IGSTN_HittableProvider* Provider)
{
	if (Providers.Remove(Provider) > 0)
	{
		InvalidateSnapshot();
	}
}

bool UGSTN_CombatWorldSubsystem::PostHit(int32 EntityId, const FGSTN_HitInboxEntry& Entry)
{
	const int32 Index = Algo::LowerBoundBy(Registry, EntityId, &FRegistryEntry::EntityId);
	if (!Registry.IsValidIndex(Index) || Registry[Index].EntityId != EntityId)
	{
		return false;
	}
	const FRegistryEntry& Registered = Registry[Index];
	if (Registered.bHasOwner && !Registered.Owner.IsValid())
	{
		return false;
	}
	return Registered.Source && Registered.Source->PostHit(Entry);
}

void UGSTN_CombatWorldSubsystem::InvalidateSnapshot()
{
	bSnapshotInvalid = true;
}

double UGSTN_CombatWorldSubsystem::ComputeBoundRadius(const UGSTN_HurtboxSetDefinition* Hurtboxes)
{
	double Radius = 0.0;
	if (Hurtboxes)
	{
		for (const FGSTN_Hurtbox& Hurtbox : Hurtboxes->Hurtboxes)
		{
			const FTransform& Local = Hurtbox.Shape.Local;
			Radius = FMath::Max(Radius, Local.GetLocation().Size() + Hurtbox.Shape.Extent.GetMax() * Local.GetScale3D().GetAbsMax());
		}
	}
	return Radius;
}

const FGSTN_StepSnapshot& UGSTN_CombatWorldSubsystem::EnsureSnapshot(int32 Frame)
{
	if (!bSnapshotInvalid && Snapshot.Frame == Frame)
	{
		return Snapshot;
	}

	// A frame behind the held one with no invalidation in between is a rollback that did not reach a RestoreFrame: some combat instance
	// would read a snapshot built from the state it no longer has. Frames only move forward otherwise
	ensureMsgf(bSnapshotInvalid || Snapshot.Frame == INDEX_NONE || Frame > Snapshot.Frame,
		TEXT("Combat snapshot asked for frame %d after one built for frame %d without an invalidation: RestoreFrame must reach every combat instance"), Frame, Snapshot.Frame);

	Snapshot.Frame = Frame;
	Snapshot.Entries.Reset();

	for (const FRegistryEntry& Registered : Registry)
	{
		if (Registered.bHasOwner && !Registered.Owner.IsValid())
		{
			continue;
		}
		const UGSTN_HurtboxSetDefinition* Hurtboxes = Registered.Hurtboxes.Get();
		FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
		Entry.EntityId = Registered.EntityId;
		Entry.Team = Registered.Team;
		Entry.Hurtboxes = Hurtboxes;
		if (!Registered.Source->ReadSnapshotEntry(Frame, Entry))
		{
			Snapshot.Entries.Pop(EAllowShrinking::No);
			continue;
		}
		// The registry owns the identity; the source only reports state
		Entry.EntityId = Registered.EntityId;
		Entry.Hurtboxes = Hurtboxes;
		if (Entry.BoundRadius <= 0.f)
		{
			Entry.BoundRadius = static_cast<float>(ComputeBoundRadius(Hurtboxes));
		}
	}

	for (IGSTN_HittableProvider* Provider : Providers)
	{
		Provider->AppendSnapshotEntities(Frame, Snapshot.Entries);
	}

	// Registry entries are sorted already and providers append after them: one stable sort gives every machine the same order
	Algo::StableSortBy(Snapshot.Entries, &FGSTN_SnapshotEntry::EntityId);
	Snapshot.BuildGrid(DefaultCellSize);

	bSnapshotInvalid = false;
	++SnapshotBuildCount;
	return Snapshot;
}

AActor* UGSTN_CombatWorldSubsystem::FindOwnerActor(int32 EntityId) const
{
	for (const FRegistryEntry& Entry : Registry)
	{
		if (Entry.EntityId == EntityId)
		{
			if (const UActorComponent* Component = Cast<UActorComponent>(Entry.Owner.Get()))
			{
				return Component->GetOwner();
			}
			return Cast<AActor>(Entry.Owner.Get());
		}
	}
	return nullptr;
}
