// Shared helpers of the parts automation tests. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Math/GSTN_PartMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A part with the fields the tests care about; everything else is the data default */
	inline FGSTN_PartDefinition MakePart(FName Name, std::initializer_list<FName> Hurtboxes, int32 Hardness, float Cap,
		float MainTransfer = 1.f, int32 ParentIndex = INDEX_NONE)
	{
		FGSTN_PartDefinition Part;
		Part.Name = Name;
		Part.Hurtboxes = TArray<FName>(Hurtboxes);
		Part.Hardness = Hardness;
		Part.PartCap = Cap;
		Part.MainTransfer = MainTransfer;
		Part.ParentIndex = ParentIndex;
		return Part;
	}

	/** Torso (body-like, hardness 0), an armoured arm with a hand child, a weak point head: the shape of a small enemy */
	inline TArray<FGSTN_PartDefinition> MakeEnemyParts()
	{
		TArray<FGSTN_PartDefinition> Parts;
		Parts.Add(MakePart("Torso", { "TorsoBox" }, 0, 100.f, 1.f));                 // 0
		Parts.Add(MakePart("Arm", { "ArmBox" }, 4, 60.f, 0.3f));                      // 1
		Parts.Add(MakePart("Hand", { "HandBox" }, 2, 30.f, 0.2f, 1));                // 2, child of the arm
		Parts.Add(MakePart("Head", { "HeadBox" }, 1, 40.f, 2.f));                     // 3, weak point
		Parts[3].bWeakPoint = true;
		return Parts;
	}

	inline FGSTN_PartHitInput Hit(float Damage, int32 PartIndex, int32 Penetration)
	{
		FGSTN_PartHitInput Input;
		Input.Damage = Damage;
		Input.PartIndex = PartIndex;
		Input.Penetration = Penetration;
		return Input;
	}

	inline void AddMainDamage(FGSTN_PartDefinition& Part, float AddPercent, bool bKill = false)
	{
		FGSTN_PartBreakEffect_MainDamage Bonus;
		Bonus.AddPercent = AddPercent;
		Bonus.bKill = bKill;
		Part.OnBreak.Add(FInstancedStruct::Make(Bonus));
	}

	/** The enemy of MakeEnemyParts with bonus main damage on the arm (10) and on the hand (5), the hand being the arm's child */
	inline TArray<FGSTN_PartDefinition> MakeGoldenParts()
	{
		TArray<FGSTN_PartDefinition> Parts = MakeEnemyParts();
		AddMainDamage(Parts[1], 10.f);
		AddMainDamage(Parts[2], 5.f);
		return Parts;
	}

	/** Whether a struct is a Blueprint type (the editor's flag, set by USTRUCT(BlueprintType)) */
	inline bool IsBlueprintType(const UScriptStruct* Struct)
	{
		return Struct && Struct->GetBoolMetaData(TEXT("BlueprintType"));
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
