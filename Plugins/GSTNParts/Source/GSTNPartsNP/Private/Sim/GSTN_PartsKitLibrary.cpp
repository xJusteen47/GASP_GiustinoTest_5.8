// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsKitLibrary.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"

bool UGSTN_PartsKitLibrary::SetKitPartSet(UGSTN_PrimaryKitDefinition* Kit, UGSTN_PartSetDefinition* Set)
{
	if (!Kit)
	{
		return false;
	}
	TArray<FInstancedStruct>& Data = Kit->Extensions.ExtensionData;
	const int32 Existing = Data.IndexOfByPredicate([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_PartsKitExtension>() != nullptr; });
	if (!Set)
	{
		if (Existing != INDEX_NONE)
		{
			Data.RemoveAt(Existing);
			Kit->MarkPackageDirty();
		}
		return true;
	}
	FGSTN_PartsKitExtension Extension;
	Extension.PartSet = Set;
	if (Existing != INDEX_NONE)
	{
		Data[Existing] = FInstancedStruct::Make(Extension);
	}
	else
	{
		Data.Add(FInstancedStruct::Make(Extension));
	}
	Kit->MarkPackageDirty();
	return true;
}
