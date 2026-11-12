// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidationExtensions.h"

#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Validation/GSTN_CombatValidators.h"

namespace
{
	TArray<FGSTN_ValidationExtension>& GetExtensions()
	{
		static TArray<FGSTN_ValidationExtension> Extensions;
		return Extensions;
	}
}

void FGSTN_ValidationExtensions::Register(FGSTN_ValidationExtension Extension)
{
	check(IsInGameThread());
	Unregister(Extension.Name);
	GetExtensions().Add(MoveTemp(Extension));
}

void FGSTN_ValidationExtensions::Unregister(FName Name)
{
	check(IsInGameThread());
	GetExtensions().RemoveAll([Name](const FGSTN_ValidationExtension& Entry) { return Entry.Name == Name; });
}

bool FGSTN_ValidationExtensions::IsRegistered(FName Name)
{
	return GetExtensions().ContainsByPredicate([Name](const FGSTN_ValidationExtension& Entry) { return Entry.Name == Name; });
}

bool FGSTN_ValidationExtensions::AppliesToAny(const UObject* Asset)
{
	if (Asset == nullptr)
	{
		return false;
	}
	for (const FGSTN_ValidationExtension& Extension : GetExtensions())
	{
		if (Extension.AppliesTo && Extension.Rule && Extension.AppliesTo(*Asset))
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_ValidationExtensions::RunRules(UObject& Asset, FGSTN_ValidationReport& Report)
{
	// A copy: a rule may load assets, and a load may start a module that registers an extension
	const TArray<FGSTN_ValidationExtension> Extensions = GetExtensions();
	bool bRan = false;
	for (const FGSTN_ValidationExtension& Extension : Extensions)
	{
		if (Extension.AppliesTo && Extension.Rule && Extension.AppliesTo(Asset))
		{
			Extension.Rule(Asset, Report);
			bRan = true;
		}
	}
	Report.SetAssetWhereMissing(&Asset);
	return bRan;
}

void FGSTN_ValidationExtensions::GetAssetClasses(TArray<UClass*>& OutClasses)
{
	for (const FGSTN_ValidationExtension& Extension : GetExtensions())
	{
		for (const TWeakObjectPtr<UClass>& Class : Extension.AssetClasses)
		{
			if (UClass* Resolved = Class.Get())
			{
				OutClasses.AddUnique(Resolved);
			}
		}
	}
}

void FGSTN_ValidationExtensions::ValidateTimelineData(const FGSTN_CombatTimelineData& Data, UObject* Asset, FGSTN_ValidationReport& Report, bool bLoadMontage)
{
	const FGSTN_TimelineRuleContext Context = FGSTN_CombatValidators::MakeTimelineContext(Data, Asset, UGSTN_CombatEditorSettings::GetSimulationTickRate(), bLoadMontage);
	FGSTN_CombatValidators::ValidateTimeline(Data, Context, Report);
}
