// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Settings/GSTN_CombatGASSettings.h"
#include "AttributeSet.h"

UGSTN_CombatGASSettings::UGSTN_CombatGASSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("GSTN Combat GAS");
}

void UGSTN_CombatGASSettings::ApplyAttributeDefaults(UAttributeSet& Set) const
{
	for (const FGSTN_AttributeDefault& Default : AttributeDefaults)
	{
		const FStructProperty* Property = CastField<FStructProperty>(Set.GetClass()->FindPropertyByName(Default.Attribute));
		if (Property == nullptr || Property->Struct != FGameplayAttributeData::StaticStruct())
		{
			// Skipped, not fatal: one set of this plugin reads the whole list, so a name that belongs to the other set is expected
			continue;
		}
		FGameplayAttributeData* Data = Property->ContainerPtrToValuePtr<FGameplayAttributeData>(&Set);
		Data->SetBaseValue(Default.Value);
		Data->SetCurrentValue(Default.Value);
	}
}
