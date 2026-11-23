// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutExtensions.h"

namespace
{
	TMap<FName, FGSTN_LoadoutExtension>& GetExtensions()
	{
		static TMap<FName, FGSTN_LoadoutExtension> Extensions;
		return Extensions;
	}

}

/** Priority, then lexical id: the map's own order is insertion order, which follows the module load order of the machine */
void FGSTN_LoadoutExtensionRegistry::GetOrderedIds(TArray<FName>& OutIds)
{
	GetExtensions().GenerateKeyArray(OutIds);
	OutIds.Sort([](const FName& A, const FName& B)
	{
		const int32 PriorityA = GetExtensions()[A].Priority;
		const int32 PriorityB = GetExtensions()[B].Priority;
		return PriorityA != PriorityB ? PriorityA < PriorityB : A.LexicalLess(B);
	});
}

void FGSTN_LoadoutExtensionRegistry::Register(FName Id, FGSTN_LoadoutExtension Extension)
{
	GetExtensions().Add(Id, MoveTemp(Extension));
}

bool FGSTN_LoadoutExtensionRegistry::Unregister(FName Id)
{
	return GetExtensions().Remove(Id) > 0;
}

int32 FGSTN_LoadoutExtensionRegistry::GetCount()
{
	return GetExtensions().Num();
}

bool FGSTN_LoadoutExtensionRegistry::IsRegistered(FName Id)
{
	return GetExtensions().Contains(Id);
}

void FGSTN_LoadoutExtensionRegistry::RunContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	TArray<FName> Ids;
	GetOrderedIds(Ids);
	for (const FName& Id : Ids)
	{
		const FGSTN_LoadoutExtension& Extension = GetExtensions()[Id];
		if (Extension.ContributeCompileInput)
		{
			Extension.ContributeCompileInput(Context, Input);
		}
	}
}

void FGSTN_LoadoutExtensionRegistry::RunContributeRulesView(const FGSTN_LoadoutExtensionContext& Context, FGSTN_LoadoutRulesView& View)
{
	TArray<FName> Ids;
	GetOrderedIds(Ids);
	for (const FName& Id : Ids)
	{
		const FGSTN_LoadoutExtension& Extension = GetExtensions()[Id];
		if (Extension.ContributeRulesView)
		{
			Extension.ContributeRulesView(Context, View);
		}
	}
}

void FGSTN_LoadoutExtensionRegistry::RunOnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem)
{
	TArray<FName> Ids;
	GetOrderedIds(Ids);
	for (const FName& Id : Ids)
	{
		const FGSTN_LoadoutExtension& Extension = GetExtensions()[Id];
		if (Extension.OnGranted)
		{
			Extension.OnGranted(Context, AbilitySystem);
		}
	}
}

void FGSTN_LoadoutExtensionRegistry::RunOnCleared(UAbilitySystemComponent& AbilitySystem)
{
	TArray<FName> Ids;
	GetOrderedIds(Ids);
	for (const FName& Id : Ids)
	{
		const FGSTN_LoadoutExtension& Extension = GetExtensions()[Id];
		if (Extension.OnCleared)
		{
			Extension.OnCleared(AbilitySystem);
		}
	}
}
