// Effects, tags and a minimal world used only by the Automation tests of the parts GAS half. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartDefinition.h"
#include "Effects/GSTN_GE_PartStats.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "GSTN_PartsGASTestTypes.generated.h"

class UAbilitySystemComponent;
class UWorld;

namespace GSTNPartsGASTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Test leaves under the stat set-by-caller root, as GSTNCombatGAS does for its own tests; production code never names a leaf. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_PartsTestSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_PartsTestFastFall);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_PartsTestOther);

	/** A part with the fields the tests care about; everything else is the data default */
	inline FGSTN_PartDefinition MakePart(FName Name, float Cap = 100.f, int32 ParentIndex = INDEX_NONE)
	{
		FGSTN_PartDefinition Part;
		Part.Name = Name;
		Part.PartCap = Cap;
		Part.ParentIndex = ParentIndex;
		return Part;
	}

	/** A Stats break effect with one magnitude per (stat, value) pair */
	inline FInstancedStruct MakeStats(std::initializer_list<TPair<FGameplayTag, float>> Magnitudes)
	{
		FGSTN_PartBreakEffect_Stats Stats;
		for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
		{
			FGSTN_PartStatMagnitude& Magnitude = Stats.Magnitudes.AddDefaulted_GetRef();
			Magnitude.Stat = Pair.Key;
			Magnitude.Magnitude = Pair.Value;
		}
		return FInstancedStruct::Make(Stats);
	}

	/**
	 * A transient game world with one actor owning a stock ability system component with the combat attribute set. Effects need
	 * a world for their start time; no Network Prediction or Mover is involved, so the test runs on stock GAS.
	 */
	struct FRig
	{
		FRig();
		~FRig();
		UWorld* World = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}

/** Infinite: adds the test speed stat to the walk speed and the test fast fall stat to the fast fall gravity scale. */
UCLASS(Hidden)
class UGSTN_TestPartStats : public UGSTN_GE_PartStats
{
	GENERATED_BODY()

public:
	UGSTN_TestPartStats();
};
