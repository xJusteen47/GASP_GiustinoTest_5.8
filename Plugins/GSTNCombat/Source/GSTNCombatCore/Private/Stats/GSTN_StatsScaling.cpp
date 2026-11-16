// Pure combat logic (no GAS, no Network Prediction).

#include "Stats/GSTN_StatsScaling.h"

#include "UObject/UnrealType.h"

float FGSTN_StatsScaling::ComputeFactor(float Stat, float Reference)
{
	if (Reference <= 0.f)
	{
		return 1.f;
	}
	return FMath::Max(Stat / Reference, 0.f);
}

namespace
{
	const FNumericProperty* FindFloatingProperty(const UObject& Object, FName Name)
	{
		const FNumericProperty* Property = CastField<FNumericProperty>(Object.GetClass()->FindPropertyByName(Name));
		return Property && Property->IsFloatingPoint() ? Property : nullptr;
	}
}

bool FGSTN_StatsScaling::ReadFloat(const UObject& Object, FName Name, double& OutValue)
{
	const FNumericProperty* Property = FindFloatingProperty(Object, Name);
	if (!Property)
	{
		return false;
	}
	OutValue = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Object));
	return true;
}

int32 FGSTN_StatsScaling::ApplyScaledFloats(UObject& Target, const UObject& Base, TConstArrayView<FName> Names, float Factor)
{
	int32 Written = 0;
	for (const FName Name : Names)
	{
		const FNumericProperty* TargetProperty = FindFloatingProperty(Target, Name);
		const FNumericProperty* BaseProperty = FindFloatingProperty(Base, Name);
		if (!TargetProperty || !BaseProperty)
		{
			continue;
		}
		const double BaseValue = BaseProperty->GetFloatingPointPropertyValue(BaseProperty->ContainerPtrToValuePtr<void>(&Base));
		TargetProperty->SetFloatingPointPropertyValue(TargetProperty->ContainerPtrToValuePtr<void>(&Target), BaseValue * Factor);
		++Written;
	}
	return Written;
}
