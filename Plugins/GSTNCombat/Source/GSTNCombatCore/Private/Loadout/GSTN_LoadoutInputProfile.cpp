// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutInputProfile.h"

#include "Layout/GSTN_ButtonLayout.h"

const FName FGSTN_LoadoutInputProfileExt::WireId(TEXT("GSTN.InputProfile"));

namespace
{
	/** Tags travel as names, like the rest of the loadout: independent of the tag replication mode, and an unknown tag reads as empty */
	void SerializeTagAsName(FArchive& Ar, FGameplayTag& Tag)
	{
		FName Name = Tag.GetTagName();
		Ar << Name;
		if (Ar.IsLoading())
		{
			Tag = FGameplayTag::RequestGameplayTag(Name, false);
		}
	}

	/** A count read from a stream, refused above Max so a hostile stream fails instead of allocating */
	bool SerializeCount(FArchive& Ar, int32& Count, int32 Max)
	{
		Ar << Count;
		if (Ar.IsLoading() && (Count < 0 || Count > Max))
		{
			Ar.SetError();
			return false;
		}
		return true;
	}
}

bool FGSTN_LoadoutInputProfileExt::SerializePayload(FArchive& Ar)
{
	uint8 Version = PayloadVersion;
	Ar << Version;
	if (Ar.IsLoading() && Version != PayloadVersion)
	{
		// A newer layout this build cannot read: the entry is dropped (the player gets the defaults), the loadout survives
		Ar.SetError();
		return true;
	}

	uint8 Device = static_cast<uint8>(Profile.Device);
	Ar << Device;
	Profile.Device = Device == static_cast<uint8>(EGSTN_InputDevice::MouseKeyboard) ? EGSTN_InputDevice::MouseKeyboard : EGSTN_InputDevice::Gamepad;

	int32 OverrideCount = Profile.ButtonOverrides.Num();
	if (!SerializeCount(Ar, OverrideCount, FGSTN_InputProfile::MaxEntries))
	{
		return true;
	}
	if (Ar.IsLoading())
	{
		Profile.ButtonOverrides.SetNum(OverrideCount);
	}
	for (FGSTN_ButtonOverride& Override : Profile.ButtonOverrides)
	{
		Ar << Override.Function;
		SerializeTagAsName(Ar, Override.Button);
	}

	int32 PickCount = Profile.CommandPicks.Num();
	if (!SerializeCount(Ar, PickCount, FGSTN_InputProfile::MaxEntries))
	{
		return true;
	}
	if (Ar.IsLoading())
	{
		Profile.CommandPicks.SetNum(PickCount);
	}
	for (FGSTN_CommandPick& Pick : Profile.CommandPicks)
	{
		SerializeTagAsName(Ar, Pick.VirtualCommand);
		Ar << Pick.Choice;
	}
	return true;
}

FGSTN_InputProfile FGSTN_LoadoutInputProfileExt::Resolve(const TArray<FInstancedStruct>& Extensions)
{
	const FGSTN_LoadoutInputProfileExt* Entry = GSTNLoadoutExtension::Find<FGSTN_LoadoutInputProfileExt>(Extensions);
	return Entry ? Entry->Profile.Sanitized(UGSTN_ButtonLayout::Get()) : FGSTN_InputProfile();
}
