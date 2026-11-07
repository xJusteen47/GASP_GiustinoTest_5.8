// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Store/GSTN_MinionStore.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarMinionMax(
		TEXT("gstn.Minion.Max"),
		128,
		TEXT("Slots of the minion store (the most minions alive at once). Read when a store is reset, clamped to [1, 65536]."),
		ECVF_Default);

	/** The generation after Current: wraps from 65535 to 1, because 0 is the null generation */
	uint16 NextGeneration(uint16 Current)
	{
		return Current == MAX_uint16 ? static_cast<uint16>(1) : static_cast<uint16>(Current + 1);
	}
}

FString FGSTN_MinionHandle::ToString() const
{
	return IsNull() ? FString(TEXT("Minion:None")) : FString::Printf(TEXT("Minion:%u#%u"), Index, Generation);
}

int32 GSTNMinion::GetConfiguredCapacity()
{
	return FMath::Clamp(CVarMinionMax.GetValueOnAnyThread(), 1, static_cast<int32>(GSTNEntityKinds::MaxMinionIndex) + 1);
}

void FGSTN_MinionStore::Reset(int32 InCapacity)
{
	const int32 Slots = FMath::Clamp(InCapacity, 1, static_cast<int32>(GSTNEntityKinds::MaxMinionIndex) + 1);

	Generation.Init(1, Slots);
	Position.Init(FVector3f::ZeroVector, Slots);
	YawDegrees.Init(0.f, Slots);
	Speed.Init(0.f, Slots);
	Health.Init(0.f, Slots);
	MaxHealth.Init(0.f, Slots);
	TypeIndex.Init(0, Slots);
	ActionState.Init(static_cast<uint8>(EGSTN_MinionActionState::Idle), Slots);
	MoveId.Init(0, Slots);
	MoveStartFrame.Init(INDEX_NONE, Slots);
	HitCounter.Init(0, Slots);
	Flags.Init(0, Slots);
	LODTier.Init(0, Slots);
	FramesInTier.Init(0, Slots);
	Intent.Init(FGSTN_MinionIntent(), Slots);
	Alive.Init(false, Slots);
	NumAlive = 0;
}

void FGSTN_MinionStore::ResetToConfiguredCapacity()
{
	Reset(GSTNMinion::GetConfiguredCapacity());
}

void FGSTN_MinionStore::ResetSlot(int32 Index)
{
	Position[Index] = FVector3f::ZeroVector;
	YawDegrees[Index] = 0.f;
	Speed[Index] = 0.f;
	Health[Index] = 0.f;
	MaxHealth[Index] = 0.f;
	TypeIndex[Index] = 0;
	ActionState[Index] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	MoveId[Index] = 0;
	MoveStartFrame[Index] = INDEX_NONE;
	HitCounter[Index] = 0;
	Flags[Index] = 0;
	LODTier[Index] = 0;
	FramesInTier[Index] = 0;
	Intent[Index] = FGSTN_MinionIntent();
}

FGSTN_MinionHandle FGSTN_MinionStore::Allocate()
{
	if (IsFull())
	{
		return FGSTN_MinionHandle();
	}

	// Lowest free index first: a bit scan, not a free list, so the order does not depend on the order slots were freed in
	const int32 Index = Alive.Find(false);
	if (Index == INDEX_NONE)
	{
		return FGSTN_MinionHandle();
	}

	Alive[Index] = true;
	++NumAlive;
	ResetSlot(Index);
	return FGSTN_MinionHandle(static_cast<uint16>(Index), Generation[Index]);
}

bool FGSTN_MinionStore::Free(const FGSTN_MinionHandle& Handle)
{
	if (!IsValid(Handle))
	{
		return false;
	}

	Alive[Handle.Index] = false;
	--NumAlive;
	ResetSlot(Handle.Index);
	Generation[Handle.Index] = NextGeneration(Generation[Handle.Index]);
	return true;
}

bool FGSTN_MinionStore::IsValid(const FGSTN_MinionHandle& Handle) const
{
	return !Handle.IsNull() && IsSlotAlive(Handle.Index) && Generation[Handle.Index] == Handle.Generation;
}

FGSTN_MinionHandle FGSTN_MinionStore::HandleOfSlot(int32 Index) const
{
	return IsSlotAlive(Index) ? FGSTN_MinionHandle(static_cast<uint16>(Index), Generation[Index]) : FGSTN_MinionHandle();
}

FGSTN_MinionHandle FGSTN_MinionStore::HandleFromEntityId(int32 EntityId) const
{
	return GSTNEntityKinds::IsMinionEntityId(EntityId) ? HandleOfSlot(GSTNEntityKinds::MinionIndexOf(EntityId)) : FGSTN_MinionHandle();
}

int32 FGSTN_MinionStore::NextAliveSlot(int32 FromIndex) const
{
	for (int32 Index = FMath::Max(FromIndex, 0); Index < Alive.Num(); ++Index)
	{
		if (Alive[Index])
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
