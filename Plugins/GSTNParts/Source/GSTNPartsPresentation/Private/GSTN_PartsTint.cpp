#include "GSTN_PartsTint.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

void UGSTN_PartsTint::ComputeTint(const FGSTN_PartsState& State, int32 PartCount, TArray<float>& Out)
{
	const int32 Count = FMath::Clamp(PartCount, 0, GSTN_PartsLimits::MaxParts);
	Out.SetNumUninitialized(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Out[i] = GetPartTint(State, i);
	}
}

float UGSTN_PartsTint::GetPartTint(const FGSTN_PartsState& State, int32 PartIndex)
{
	if (PartIndex < 0 || PartIndex >= GSTN_PartsLimits::MaxParts)
	{
		return 0.f;
	}
	if (State.IsBroken(PartIndex))
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>(State.PartDamage[PartIndex]) / static_cast<float>(GSTN_PartsLimits::DamageAtCap), 0.f, 1.f);
}

int32 UGSTN_PartsTintComponent::ApplyState(const FGSTN_PartsState& State, int32 PartCount)
{
	UGSTN_PartsTint::ComputeTint(State, PartCount, LastTint);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}

	int32 Written = 0;
	TArray<UPrimitiveComponent*> Prims;
	Owner->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		for (int32 i = 0; i < LastTint.Num(); ++i)
		{
			// Compared with what the primitive already holds, so a fresh mesh still gets its values and a repeat writes nothing
			const TArray<float>& Data = Prim->GetCustomPrimitiveData().Data;
			const int32 Index = BaseIndex + i;
			if (!Data.IsValidIndex(Index) || Data[Index] != LastTint[i])
			{
				Prim->SetCustomPrimitiveDataFloat(Index, LastTint[i]);
				++Written;
			}
		}
	}
	return Written;
}
