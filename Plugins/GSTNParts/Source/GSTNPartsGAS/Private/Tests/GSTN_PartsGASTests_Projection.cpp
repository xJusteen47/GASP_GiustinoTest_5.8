// Automation tests of the tag projection (plan P5): the projected Combat.Part.* tags and the removed base tag on a stock ability
// system component, from a broken mask through the part math's derived projection.

#include "AbilitySystemComponent.h"
#include "GSTN_PartsTags.h"
#include "Math/GSTN_PartMath.h"
#include "Projection/GSTN_PartTagProjection.h"
#include "Tests/GSTN_PartsGASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}

	FGSTN_PartsState StateWithMask(uint16 Mask)
	{
		FGSTN_PartsState State;
		State.BrokenMask = Mask;
		return State;
	}

	/** Four parts: 0 torso (breakable), 1 arm with a Tags effect, 2 head, 3 an armour plate that cannot break */
	TArray<FGSTN_PartDefinition> MakeParts()
	{
		TArray<FGSTN_PartDefinition> Parts;
		Parts.Add(GSTNPartsGASTest::MakePart("Torso"));
		Parts.Add(GSTNPartsGASTest::MakePart("Arm"));
		Parts.Add(GSTNPartsGASTest::MakePart("Head"));
		Parts.Add(GSTNPartsGASTest::MakePart("Plate", 0.f));

		FGSTN_PartBreakEffect_Tags ArmLost;
		ArmLost.Granted.AddTag(Tag(TEXT("Part.Type.Arm")));
		ArmLost.Removed.AddTag(Tag(TEXT("Part.Type.Torso")));
		Parts[1].OnBreak.Add(FInstancedStruct::Make(ArmLost));
		return Parts;
	}

	/** Runs the part math's projection for a mask and applies it to the ASC; returns the count of tags that changed */
	int32 Project(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_GrabGate& Gate, uint16 Mask, const FGameplayTagContainer& Base)
	{
		FGSTN_PartsProjection Projection;
		UGSTN_PartMath::ProjectDerived(Parts, StateWithMask(Mask), Gate, Projection);
		return FGSTN_PartTagProjection::Apply(ASC, Parts, Projection, Base);
	}

	bool Has(const UAbilitySystemComponent& ASC, const FGameplayTag& Tag)
	{
		return ASC.GetTagCount(Tag) > 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASBrokenTagTest, "GSTNParts.GAS.Projection.BrokenTagAfterBreak", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASBrokenTagTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TArray<FGSTN_PartDefinition> Parts = MakeParts();
	const FGSTN_GrabGate NoGate;
	const FGameplayTagContainer NoBase;

	Project(ASC, Parts, NoGate, 0u, NoBase);
	TestFalse(TEXT("nothing broken, no Broken tag"), Has(ASC, GSTNPartsTags::Part_Broken));

	Project(ASC, Parts, NoGate, 1u << 2, NoBase);
	TestTrue(TEXT("a break projects the tag"), Has(ASC, GSTNPartsTags::Part_Broken));
	TestFalse(TEXT("but not AllBroken"), Has(ASC, GSTNPartsTags::Part_AllBroken));

	TestEqual(TEXT("projecting the same mask again changes nothing"), Project(ASC, Parts, NoGate, 1u << 2, NoBase), 0);
	TestEqual(TEXT("the tag is set, never stacked"), ASC.GetTagCount(GSTNPartsTags::Part_Broken), 1);

	// A resimulation from before the break: the tag is a function of the mask, so it goes away with no undo step
	Project(ASC, Parts, NoGate, 0u, NoBase);
	TestFalse(TEXT("the mask without the break clears the tag"), Has(ASC, GSTNPartsTags::Part_Broken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASRemovedBaseTagTest, "GSTNParts.GAS.Projection.RemovedBaseTagWhileBroken", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASRemovedBaseTagTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TArray<FGSTN_PartDefinition> Parts = MakeParts();
	const FGSTN_GrabGate NoGate;
	const FGameplayTag BaseTag = Tag(TEXT("Part.Type.Torso"));
	const FGameplayTag GrantedTag = Tag(TEXT("Part.Type.Arm"));
	FGameplayTagContainer Base;
	Base.AddTag(BaseTag);

	Project(ASC, Parts, NoGate, 0u, Base);
	TestTrue(TEXT("the base tag is shown while the arm is whole"), Has(ASC, BaseTag));
	TestFalse(TEXT("the arm's tag is not"), Has(ASC, GrantedTag));

	Project(ASC, Parts, NoGate, 1u << 1, Base);
	TestFalse(TEXT("the broken arm suppresses the base tag"), Has(ASC, BaseTag));
	TestTrue(TEXT("and grants its own"), Has(ASC, GrantedTag));

	Project(ASC, Parts, NoGate, 1u << 2, Base);
	TestTrue(TEXT("another part breaking does not touch the base tag"), Has(ASC, BaseTag));
	TestFalse(TEXT("the arm's grant went with the arm"), Has(ASC, GrantedTag));

	Project(ASC, Parts, NoGate, 0u, Base);
	TestTrue(TEXT("a restore brings the base tag back"), Has(ASC, BaseTag));

	// A removed parent suppresses its children; the pure build shows it without an ability system
	FGSTN_PartsProjection Projection;
	Projection.Removed.AddTag(Tag(TEXT("Part.Type")));
	FGameplayTagContainer Shown;
	FGSTN_PartTagProjection::BuildTags(Projection, Base, Shown);
	TestFalse(TEXT("a removed parent suppresses the child"), Shown.HasTagExact(BaseTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASGrabLockedTest, "GSTNParts.GAS.Projection.GrabLockedGate", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASGrabLockedTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TArray<FGSTN_PartDefinition> Parts = MakeParts();
	const FGameplayTagContainer NoBase;

	FGSTN_GrabGate Empty;
	Project(ASC, Parts, Empty, 0u, NoBase);
	TestFalse(TEXT("an empty gate never locks"), Has(ASC, GSTNPartsTags::Part_GrabLocked));

	FGSTN_GrabGate Count;
	Count.MinBrokenParts = 2;
	Project(ASC, Parts, Count, 0u, NoBase);
	TestTrue(TEXT("none broken, the gate is unmet: locked"), Has(ASC, GSTNPartsTags::Part_GrabLocked));
	Project(ASC, Parts, Count, 1u << 0, NoBase);
	TestTrue(TEXT("one of two broken: still locked"), Has(ASC, GSTNPartsTags::Part_GrabLocked));
	Project(ASC, Parts, Count, (1u << 0) | (1u << 2), NoBase);
	TestFalse(TEXT("two broken: the gate is met, unlocked"), Has(ASC, GSTNPartsTags::Part_GrabLocked));

	FGSTN_GrabGate Required;
	Required.RequiredParts = { 1, 2 };
	Project(ASC, Parts, Required, 1u << 1, NoBase);
	TestTrue(TEXT("a required part missing: locked"), Has(ASC, GSTNPartsTags::Part_GrabLocked));
	Project(ASC, Parts, Required, (1u << 1) | (1u << 2), NoBase);
	TestFalse(TEXT("every required part broken: unlocked"), Has(ASC, GSTNPartsTags::Part_GrabLocked));

	FGSTN_GrabGate AnyOf = Required;
	AnyOf.bAnyOf = true;
	Project(ASC, Parts, AnyOf, 1u << 2, NoBase);
	TestFalse(TEXT("any-of: one required part is enough"), Has(ASC, GSTNPartsTags::Part_GrabLocked));
	Project(ASC, Parts, AnyOf, 1u << 0, NoBase);
	TestTrue(TEXT("any-of: a part outside the list does not count"), Has(ASC, GSTNPartsTags::Part_GrabLocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASAllBrokenTest, "GSTNParts.GAS.Projection.AllBrokenCountsBreakableOnly", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASAllBrokenTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TArray<FGSTN_PartDefinition> Parts = MakeParts();
	const FGSTN_GrabGate NoGate;
	const FGameplayTagContainer NoBase;

	// Parts 0..2 can break, part 3 is a plate that cannot: AllBroken is the three, whether or not the plate is in the mask
	Project(ASC, Parts, NoGate, (1u << 0) | (1u << 1), NoBase);
	TestFalse(TEXT("two of three breakable parts: not all broken"), Has(ASC, GSTNPartsTags::Part_AllBroken));
	Project(ASC, Parts, NoGate, (1u << 0) | (1u << 1) | (1u << 2), NoBase);
	TestTrue(TEXT("all three breakable parts: all broken, the plate does not count"), Has(ASC, GSTNPartsTags::Part_AllBroken));
	TestTrue(TEXT("and the entity has a broken part"), Has(ASC, GSTNPartsTags::Part_Broken));
	Project(ASC, Parts, NoGate, 1u << 3, NoBase);
	TestFalse(TEXT("only the plate in the mask: nothing breakable is broken"), Has(ASC, GSTNPartsTags::Part_AllBroken));

	// A set with nothing breakable is never all broken
	TArray<FGSTN_PartDefinition> Plates;
	Plates.Add(GSTNPartsGASTest::MakePart("PlateA", 0.f));
	Plates.Add(GSTNPartsGASTest::MakePart("PlateB", 0.f));
	Project(ASC, Plates, NoGate, 0u, NoBase);
	TestFalse(TEXT("no breakable part: never all broken"), Has(ASC, GSTNPartsTags::Part_AllBroken));
	Project(ASC, Plates, NoGate, 0x3u, NoBase);
	TestFalse(TEXT("even with every bit set"), Has(ASC, GSTNPartsTags::Part_AllBroken));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
