// Pure combat logic (no GAS, no Network Prediction).

#include "Sync/GSTN_EntityRef.h"

#include "Sync/GSTN_SyncFragment.h"

bool FGSTN_EntityRef::IsNull() const
{
	return Kind == GSTNEntityKinds::None;
}

bool FGSTN_EntityRef::operator==(const FGSTN_EntityRef& Other) const
{
	// A null reference carries no meaningful Id or Serial, so two of them are equal whatever those hold
	if (IsNull() || Other.IsNull())
	{
		return IsNull() && Other.IsNull();
	}
	return Kind == Other.Kind && Id == Other.Id && Serial == Other.Serial;
}

bool FGSTN_EntityRef::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsSaving() && IsNull())
	{
		*this = Null();
	}
	Ar << Kind;
	Ar << Id;
	Ar << Serial;
	if (Ar.IsLoading() && IsNull())
	{
		*this = Null();
	}
	bOutSuccess = !Ar.IsError();
	return true;
}

FString FGSTN_EntityRef::ToString() const
{
	if (IsNull())
	{
		return TEXT("None");
	}
	switch (Kind)
	{
	case GSTNEntityKinds::Fighter:
		return FString::Printf(TEXT("Fighter:%d#%d"), Id, Serial);
	case GSTNEntityKinds::Minion:
		return FString::Printf(TEXT("Minion:%d#%d"), Id, Serial);
	case GSTNEntityKinds::Object:
		return FString::Printf(TEXT("Object:%d#%d"), Id, Serial);
	default:
		return FString::Printf(TEXT("Kind%d:%d#%d"), Kind, Id, Serial);
	}
}

uint32 GetTypeHash(const FGSTN_EntityRef& Ref)
{
	// Hashes the null reference to one value, as operator== treats every null as the same
	return Ref.IsNull() ? 0u : HashCombineFast(GetTypeHash(Ref.Kind), HashCombineFast(GetTypeHash(Ref.Id), GetTypeHash(Ref.Serial)));
}
