// Automation tests of the part data: part sets, validation, penetration table, baked curve, extensions (plan P2).

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsAssetTypes.h"
#include "Data/GSTN_PartsBakedCurve.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "Data/GSTN_PartsRulesExtension.h"
#include "Data/GSTN_PartsValidation.h"
#include "GSTN_PartsSettings.h"
#include "GSTN_PartsTags.h"
#include "GSTN_PartsTestUtils.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsTest
{
	FGSTN_PartsValidationReport Validate(const TArray<FGSTN_PartDefinition>& Parts, const TSet<FName>* Bones = nullptr,
		int32 Limit = GSTN_PartsLimits::MaxParts, const FGSTN_GrabGate& Gate = FGSTN_GrabGate())
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateParts(Parts, Gate, FGSTN_PartReaction(), Bones, Limit, Report);
		return Report;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataSetValidTest, "GSTNParts.Core.Data.PartSetValid", GSTNPartsTest::Flags)
bool FGSTN_PartsDataSetValidTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts);
	TestEqual(TEXT("A well formed set has no errors"), Report.NumErrors(), 0);
	TestEqual(TEXT("A well formed set has no warnings"), Report.NumWarnings(), 0);

	// The asset form goes through the same rules
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage());
	Set->Parts = Parts;
	FDataValidationContext Context;
	TestEqual(TEXT("The asset validates"), static_cast<int32>(Set->IsDataValid(Context)), static_cast<int32>(EDataValidationResult::Valid));

	// A single part is a valid set: a player's whole body
	TArray<FGSTN_PartDefinition> Player;
	Player.Add(GSTNPartsTest::MakePart("Body", { "Torso", "Head", "Arm" }, 0, 300.f));
	TestEqual(TEXT("A single part set has no errors"), GSTNPartsTest::Validate(Player).NumErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataDuplicateHurtboxTest, "GSTNParts.Core.Data.DuplicateHurtbox", GSTNPartsTest::Flags)
bool FGSTN_PartsDataDuplicateHurtboxTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[3].Hurtboxes.Add("ArmBox"); // also owned by part 1
	FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts);
	TestTrue(TEXT("A hurtbox in two parts is an error"), Report.Contains(true, TEXT("hurtbox ArmBox already belongs")));

	// Twice in the same part is the same mistake
	Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[0].Hurtboxes.Add("TorsoBox");
	Report = GSTNPartsTest::Validate(Parts);
	TestTrue(TEXT("A hurtbox listed twice is an error"), Report.Contains(true, TEXT("hurtbox TorsoBox already belongs")));

	// Two parts with one name are ambiguous for the bridges that key by name
	Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[2].Name = "Arm";
	Report = GSTNPartsTest::Validate(Parts);
	TestTrue(TEXT("A duplicate part name is an error"), Report.Contains(true, TEXT("name is used by another part")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataParentCycleTest, "GSTNParts.Core.Data.ParentCycle", GSTNPartsTest::Flags)
bool FGSTN_PartsDataParentCycleTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[1].ParentIndex = 2; // arm under hand, hand under arm
	FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts);
	TestTrue(TEXT("A two part cycle is an error"), Report.Contains(true, TEXT("parent cycle")));
	int32 CycleReports = 0;
	for (const FGSTN_PartsIssue& Issue : Report.Issues)
	{
		CycleReports += Issue.Message.Contains(TEXT("parent cycle")) ? 1 : 0;
	}
	TestEqual(TEXT("A cycle is reported once"), CycleReports, 1);

	// A branch hanging off a cycle is not part of it
	Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[1].ParentIndex = 2;
	Parts[3].ParentIndex = 1;
	Report = GSTNPartsTest::Validate(Parts);
	CycleReports = 0;
	for (const FGSTN_PartsIssue& Issue : Report.Issues)
	{
		CycleReports += Issue.Message.Contains(TEXT("parent cycle")) ? 1 : 0;
	}
	TestEqual(TEXT("A part below a cycle adds no report"), CycleReports, 1);

	Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[3].ParentIndex = 3;
	TestTrue(TEXT("A part that is its own parent is an error"), GSTNPartsTest::Validate(Parts).Contains(true, TEXT("parent index 3")));
	Parts[3].ParentIndex = 9;
	TestTrue(TEXT("A parent index out of range is an error"), GSTNPartsTest::Validate(Parts).Contains(true, TEXT("parent index 9")));

	TestEqual(TEXT("The valid tree has no errors"), GSTNPartsTest::Validate(GSTNPartsTest::MakeEnemyParts()).NumErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataMaskOver16Test, "GSTNParts.Core.Data.MaskOver16", GSTNPartsTest::Flags)
bool FGSTN_PartsDataMaskOver16Test::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		Parts.Add(GSTNPartsTest::MakePart(*FString::Printf(TEXT("Part%d"), Index), { *FString::Printf(TEXT("Box%d"), Index) }, 0, 10.f));
	}
	TestEqual(TEXT("Sixteen parts fill the mask exactly and are valid"), GSTNPartsTest::Validate(Parts).NumErrors(), 0);

	Parts.Add(GSTNPartsTest::MakePart("Part16", { "Box16" }, 0, 10.f));
	TestTrue(TEXT("Seventeen parts are an error"), GSTNPartsTest::Validate(Parts).Contains(true, TEXT("limit is 16")));

	// The minion bridge planned for M8 will validate with its own limit
	Parts.SetNum(GSTN_PartsLimits::MaxMinionParts + 1);
	TestTrue(TEXT("Nine parts exceed a minion set"), GSTNPartsTest::Validate(Parts, nullptr, GSTN_PartsLimits::MaxMinionParts).Contains(true, TEXT("limit is 8")));
	TestEqual(TEXT("Nine parts are fine for a fighter"), GSTNPartsTest::Validate(Parts).NumErrors(), 0);

	// The math addresses sixteen at most, whatever the asset holds
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage());
	Set->Parts.SetNum(20);
	TestEqual(TEXT("The addressable parts stop at the mask width"), Set->GetParts().Num(), GSTN_PartsLimits::MaxParts);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataMissingBoneTest, "GSTNParts.Core.Data.MissingBoneWarning", GSTNPartsTest::Flags)
bool FGSTN_PartsDataMissingBoneTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[1].Bones = { "upperarm_l", "lowerarm_l" };
	const TSet<FName> Bones = { "upperarm_l" };
	const FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts, &Bones);
	TestTrue(TEXT("A bone the mesh lacks is a warning"), Report.Contains(false, TEXT("bone lowerarm_l")));
	TestFalse(TEXT("A bone the mesh has is not reported"), Report.Contains(false, TEXT("bone upperarm_l")));
	TestEqual(TEXT("A missing bone is never an error: it is presentation data"), Report.NumErrors(), 0);

	TestEqual(TEXT("Without a preview mesh the check is skipped"), GSTNPartsTest::Validate(Parts, nullptr).NumWarnings(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataNumbersTest, "GSTNParts.Core.Data.NumbersValidated", GSTNPartsTest::Flags)
bool FGSTN_PartsDataNumbersTest::RunTest(const FString& Parameters)
{
	auto Mutated = [](TFunctionRef<void(FGSTN_PartDefinition&)> Edit)
	{
		TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
		Edit(Parts[1]);
		return GSTNPartsTest::Validate(Parts);
	};

	TestTrue(TEXT("Hardness above 15 is an error"), Mutated([](FGSTN_PartDefinition& P) { P.Hardness = 16; }).HasErrors());
	TestTrue(TEXT("Negative hardness is an error"), Mutated([](FGSTN_PartDefinition& P) { P.Hardness = -1; }).HasErrors());
	TestTrue(TEXT("An infinite cap is an error"), Mutated([](FGSTN_PartDefinition& P) { P.PartCap = std::numeric_limits<float>::infinity(); }).HasErrors());
	TestTrue(TEXT("A NaN transfer is an error"), Mutated([](FGSTN_PartDefinition& P) { P.MainTransfer = std::numeric_limits<float>::quiet_NaN(); }).HasErrors());
	TestTrue(TEXT("A negative damage scale is an error"), Mutated([](FGSTN_PartDefinition& P) { P.PartDamageScale = -1.f; }).HasErrors());
	TestTrue(TEXT("A lost mass above 1 is an error"), Mutated([](FGSTN_PartDefinition& P) { P.LostMassFraction = 1.5f; }).HasErrors());
	TestFalse(TEXT("Cap 0 is the unbreakable armour, not an error"), Mutated([](FGSTN_PartDefinition& P) { P.PartCap = 0.f; }).HasErrors());
	TestFalse(TEXT("Hardness 15 is the highest valid tier"), Mutated([](FGSTN_PartDefinition& P) { P.Hardness = 15; }).HasErrors());
	TestTrue(TEXT("Replace warns until the hit shape type exists"), Mutated([](FGSTN_PartDefinition& P) { P.BrokenPolicy = EGSTN_BrokenHurtboxPolicy::Replace; }).Contains(false, TEXT("Replace policy")));
	TestEqual(TEXT("BrokenMainTransfer defaults to half the transfer"), GSTNPartsTest::MakeEnemyParts()[1].GetBrokenMainTransfer(), 0.15f);

	FGSTN_GrabGate Gate;
	Gate.RequiredParts = { 7 };
	TestTrue(TEXT("A grab gate on a missing part is an error"), GSTNPartsTest::Validate(GSTNPartsTest::MakeEnemyParts(), nullptr, 16, Gate).HasErrors());
	Gate.RequiredParts.Reset();
	Gate.MinBrokenParts = 5;
	TestTrue(TEXT("A grab gate needing more parts than exist is an error"), GSTNPartsTest::Validate(GSTNPartsTest::MakeEnemyParts(), nullptr, 16, Gate).HasErrors());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataUnknownEffectTest, "GSTNParts.Core.Data.UnknownEffectIgnored", GSTNPartsTest::Flags)
bool FGSTN_PartsDataUnknownEffectTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();

	// An entry whose struct is gone (a disabled plugin) loads as an empty instanced struct
	Parts[1].OnBreak.Add(FInstancedStruct());
	// A struct that is not a part break effect at all
	Parts[1].OnBreak.Add(FInstancedStruct::Make(FGSTN_PartHitData()));
	// And a real one after them, which must still be found
	FGSTN_PartBreakEffect_MainDamage Bonus;
	Bonus.AddPercent = 30.f;
	Parts[1].OnBreak.Add(FInstancedStruct::Make(Bonus));

	const FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts);
	TestEqual(TEXT("Unknown effects are never errors"), Report.NumErrors(), 0);
	TestTrue(TEXT("An empty entry is reported as ignored"), Report.Contains(false, TEXT("break effect 0 is empty")));
	TestTrue(TEXT("A foreign struct is reported as ignored"), Report.Contains(false, TEXT("not a part break effect")));

	const FGSTN_PartBreakEffect_MainDamage* Found = Parts[1].FindBreakEffect<FGSTN_PartBreakEffect_MainDamage>();
	TestNotNull(TEXT("Lookup skips unknown entries and finds the real one"), Found);
	if (Found)
	{
		TestEqual(TEXT("It is the right effect"), Found->AddPercent, 30.f);
	}
	TestNull(TEXT("A kind that is absent is null"), Parts[1].FindBreakEffect<FGSTN_PartBreakEffect_Stagger>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataAssetTest, "GSTNParts.Core.Data.PartSetAsset", GSTNPartsTest::Flags)
bool FGSTN_PartsDataAssetTest::RunTest(const FString& Parameters)
{
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage(), TEXT("TestPartSet"));
	Set->Parts = GSTNPartsTest::MakeEnemyParts();
	Set->Parts[1].PartCap = 0.f; // the arm plate is unbreakable

	TestTrue(TEXT("Primary asset type"), Set->GetPrimaryAssetId().PrimaryAssetType == GSTNPartsAssetTypes::PartSet());
	TestEqual(TEXT("Part by name"), Set->FindPartByName("Head"), 3);
	TestEqual(TEXT("Unknown part name"), Set->FindPartByName("Tail"), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Part by hurtbox"), Set->FindPartByHurtbox("HandBox"), 2);
	TestEqual(TEXT("An unmapped hurtbox belongs to the body"), Set->FindPartByHurtbox("Belly"), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Only capped parts are breakable"), static_cast<int32>(Set->GetBreakableMask()), 0b1101);

	// The default weight curve is the settings' shape, baked to integers
	Set->RebakeWeightCurve();
	TestEqual(TEXT("No damage, full weight"), Set->GetWeightCurve().Sample(0.f), 1.f);
	TestEqual(TEXT("At the cap, the settings' scale"), Set->GetWeightCurve().Sample(1.f), UGSTN_PartsSettings::Get().DefaultWeightScaleAtCap, 0.0001f);

	// A set's own curve wins and is baked at load too
	Set->bOverrideWeightCurve = true;
	if (FRichCurve* Curve = Set->WeightCurve.GetRichCurve())
	{
		Curve->Reset();
		Curve->AddKey(0.f, 0.8f);
		Curve->AddKey(1.f, 0.8f);
	}
	Set->RebakeWeightCurve();
	TestEqual(TEXT("A flat override never gets lighter"), Set->GetWeightCurve().Sample(0.f), Set->GetWeightCurve().Sample(1.f));
	TestEqual(TEXT("It is the override's value"), Set->GetWeightCurve().Sample(0.5f), 0.8f, 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataExtensionsTest, "GSTNParts.Core.Data.ExtensionsAndHitData", GSTNPartsTest::Flags)
bool FGSTN_PartsDataExtensionsTest::RunTest(const FString& Parameters)
{
	// The kit extension is an entry of the combat kit's extension array, found by type
	FGSTN_KitExtensionList List;
	FGSTN_PartsKitExtension Extension;
	Extension.PartSet = TSoftObjectPtr<UGSTN_PartSetDefinition>(FSoftObjectPath(TEXT("/Test/PartSet.PartSet")));
	List.ExtensionData.Add(FInstancedStruct::Make(Extension));
	const FGSTN_PartsKitExtension* Found = List.Find<FGSTN_PartsKitExtension>();
	TestNotNull(TEXT("The kit extension is found by type"), Found);
	if (Found)
	{
		TestEqual(TEXT("It keeps the soft reference"), Found->PartSet.ToSoftObjectPath().ToString(), FString(TEXT("/Test/PartSet.PartSet")));
	}
	TestTrue(TEXT("The kit extension derives from the combat base"), FGSTN_PartsKitExtension::StaticStruct()->IsChildOf(FGSTN_KitExtension::StaticStruct()));

	// A hit without part data is penetration 0 with scales of 1
	const FGSTN_PartHitData Hit;
	TestEqual(TEXT("Default penetration"), static_cast<int32>(Hit.Penetration), 0);
	TestEqual(TEXT("Default part damage scale"), Hit.PartDamageScale, 1.f);
	TestFalse(TEXT("No flags"), Hit.HasFlag(EGSTN_PartHitFlags::IgnoreHardness) || Hit.HasFlag(EGSTN_PartHitFlags::MainOnly));
	FGSTN_PartHitData Piercing;
	Piercing.Flags = static_cast<uint8>(EGSTN_PartHitFlags::IgnoreHardness);
	TestTrue(TEXT("The flag reads back"), Piercing.HasFlag(EGSTN_PartHitFlags::IgnoreHardness) && !Piercing.HasFlag(EGSTN_PartHitFlags::MainOnly));

	// The rules extension inherits everything by default
	const FGSTN_PartsRulesExtension Rules;
	TestTrue(TEXT("No table"), Rules.DefaultPenetrationTable.IsNull());
	TestTrue(TEXT("Weights inherit"), Rules.DefaultWeightScaleAtCap < 0.f && Rules.MinWeightScale < 0.f);
	TestFalse(TEXT("The budget is not overridden"), Rules.bOverridePresentationBudget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataBlueprintTypesTest, "GSTNParts.Core.Data.StructsAreBlueprintTypes", GSTNPartsTest::Flags)
bool FGSTN_PartsDataBlueprintTypesTest::RunTest(const FString& Parameters)
{
	const UScriptStruct* Structs[] = {
		FGSTN_PartDefinition::StaticStruct(), FGSTN_PartReaction::StaticStruct(), FGSTN_GrabGate::StaticStruct(),
		FGSTN_PenetrationBand::StaticStruct(), FGSTN_PartHitData::StaticStruct(), FGSTN_PartsRulesExtension::StaticStruct(),
		FGSTN_PartsKitExtension::StaticStruct(), FGSTN_PartsPresentationBudget::StaticStruct(),
		FGSTN_PartBreakEffect::StaticStruct(), FGSTN_PartBreakEffect_Tags::StaticStruct(),
		FGSTN_PartBreakEffect_DisableHurtbox::StaticStruct(), FGSTN_PartBreakEffect_DisableHitGroups::StaticStruct(),
		FGSTN_PartBreakEffect_Stats::StaticStruct(), FGSTN_PartBreakEffect_MainDamage::StaticStruct(),
		FGSTN_PartBreakEffect_Launch::StaticStruct(), FGSTN_PartBreakEffect_Stagger::StaticStruct() };
	for (const UScriptStruct* Struct : Structs)
	{
		TestTrue(FString::Printf(TEXT("%s is a Blueprint type"), *Struct->GetName()), GSTNPartsTest::IsBlueprintType(Struct));
	}

	// Authored data is read-only from a graph: a hook cannot write what the simulation reads
	const UStruct* Authored[] = { FGSTN_PartDefinition::StaticStruct(), FGSTN_PenetrationBand::StaticStruct(), FGSTN_PartReaction::StaticStruct() };
	for (const UStruct* Struct : Authored)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			TestFalse(FString::Printf(TEXT("%s.%s is not Blueprint writable"), *Struct->GetName(), *It->GetName()), It->HasAnyPropertyFlags(CPF_BlueprintVisible) && !It->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataNoHardContentTest, "GSTNParts.Core.Data.NoHardContentReferences", GSTNPartsTest::Flags)
bool FGSTN_PartsDataNoHardContentTest::RunTest(const FString& Parameters)
{
	// Replaceable content is reached through soft references (modularity-and-tags 3.9): no authored property of the data types
	// is a hard object reference, so loading a set never drags another plugin's content along
	const UStruct* Types[] = { UGSTN_PartSetDefinition::StaticClass(), UGSTN_PenetrationTable::StaticClass(),
		FGSTN_PartDefinition::StaticStruct(), FGSTN_PenetrationBand::StaticStruct(), FGSTN_PartsKitExtension::StaticStruct(),
		FGSTN_PartsRulesExtension::StaticStruct(), UGSTN_PartsSettings::StaticClass() };
	for (const UStruct* Type : Types)
	{
		for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const bool bHardObject = CastField<FObjectProperty>(*It) != nullptr;
			TestFalse(FString::Printf(TEXT("%s.%s is not a hard object reference"), *Type->GetName(), *It->GetName()), bHardObject);
		}
	}
	return true;
}

// Review 2026-10-23 B.3: a grab gate that waits for parts no break can reach locked the entity for good and the validator said valid
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataGateReachableTest, "GSTNParts.Core.Data.GrabGateMustBeReachable", GSTNPartsTest::Flags)
bool FGSTN_PartsDataGateReachableTest::RunTest(const FString& Parameters)
{
	// Torso 0, arm 1, hand 2 under the arm, head 3: the head is an armour plate that never breaks
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[3].PartCap = 0.f;

	FGSTN_GrabGate Gate;
	Gate.RequiredParts = { 3 };
	TestTrue(TEXT("Requiring an unbreakable part with no breakable ancestor is an error"), GSTNPartsTest::Validate(Parts, nullptr, 16, Gate).Contains(true, TEXT("can never break")));

	Gate.bAnyOf = true;
	TestTrue(TEXT("Any of a set where nothing can break is an error"), GSTNPartsTest::Validate(Parts, nullptr, 16, Gate).Contains(true, TEXT("none of them can ever break")));
	Gate.RequiredParts = { 3, 1 };
	TestFalse(TEXT("Any of a set where one can break is fine"), GSTNPartsTest::Validate(Parts, nullptr, 16, Gate).HasErrors());

	// An unbreakable part under a breakable parent breaks with it, so it is reachable
	TArray<FGSTN_PartDefinition> Plated = GSTNPartsTest::MakeEnemyParts();
	Plated[2].PartCap = 0.f; // the hand plate, child of the arm
	Gate = FGSTN_GrabGate();
	Gate.RequiredParts = { 2 };
	TestFalse(TEXT("A plate that goes with a breakable limb is reachable"), GSTNPartsTest::Validate(Plated, nullptr, 16, Gate).HasErrors());

	// The count: torso, arm and hand can break (3 of 4)
	Gate = FGSTN_GrabGate();
	Gate.MinBrokenParts = 3;
	TestFalse(TEXT("A count the breakable parts can reach is fine"), GSTNPartsTest::Validate(Parts, nullptr, 16, Gate).HasErrors());
	Gate.MinBrokenParts = 4;
	const FGSTN_PartsValidationReport Report = GSTNPartsTest::Validate(Parts, nullptr, 16, Gate);
	TestTrue(TEXT("A count above the reachable parts is an error"), Report.Contains(true, TEXT("only 3 can ever break")));
	TestFalse(TEXT("... which is not the error for more than the set holds"), Report.Contains(true, TEXT("but the set has")));
	return true;
}

// Review 2026-10-23 B.4: only MainDamage was checked; the other effects and the band table had holes
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataEffectFieldsTest, "GSTNParts.Core.Data.BreakEffectFieldsValidated", GSTNPartsTest::Flags)
bool FGSTN_PartsDataEffectFieldsTest::RunTest(const FString& Parameters)
{
	auto WithEffect = [](const FInstancedStruct& Effect)
	{
		TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
		Parts[1].OnBreak.Add(Effect);
		return GSTNPartsTest::Validate(Parts);
	};
	constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

	FGSTN_PartBreakEffect_Launch Launch;
	Launch.Speed = 600.f;
	Launch.UpFraction = 0.5f;
	TestFalse(TEXT("A sane launch is valid"), WithEffect(FInstancedStruct::Make(Launch)).HasErrors());
	Launch.Speed = NaN;
	TestTrue(TEXT("A NaN launch speed is an error"), WithEffect(FInstancedStruct::Make(Launch)).Contains(true, TEXT("launch speed")));
	Launch.Speed = -1.f;
	TestTrue(TEXT("A negative launch speed is an error"), WithEffect(FInstancedStruct::Make(Launch)).Contains(true, TEXT("launch speed")));
	Launch.Speed = 600.f;
	Launch.UpFraction = 1.5f;
	TestTrue(TEXT("An up fraction above 1 is an error"), WithEffect(FInstancedStruct::Make(Launch)).Contains(true, TEXT("up fraction")));
	Launch.UpFraction = -0.1f;
	TestTrue(TEXT("An up fraction below 0 is an error"), WithEffect(FInstancedStruct::Make(Launch)).Contains(true, TEXT("up fraction")));

	FGSTN_PartBreakEffect_Stagger Stagger;
	Stagger.Frames = 12;
	TestFalse(TEXT("A stagger of 12 frames is valid"), WithEffect(FInstancedStruct::Make(Stagger)).HasErrors());
	Stagger.Frames = -3;
	TestTrue(TEXT("Negative stagger frames are an error"), WithEffect(FInstancedStruct::Make(Stagger)).Contains(true, TEXT("stagger frames")));

	FGSTN_PartBreakEffect_Stats Stats;
	TestTrue(TEXT("A stats effect with no magnitude warns"), WithEffect(FInstancedStruct::Make(Stats)).Contains(false, TEXT("no stat magnitude")));
	FGSTN_PartStatMagnitude& Magnitude = Stats.Magnitudes.AddDefaulted_GetRef();
	TestTrue(TEXT("A magnitude with no stat tag is an error"), WithEffect(FInstancedStruct::Make(Stats)).Contains(true, TEXT("no stat tag")));
	Magnitude.Stat = GSTNPartsTags::Part_Broken; // a registered tag, but not under Combat.SetByCaller.Stat
	TestTrue(TEXT("A stat outside the SetByCaller stat root warns"), WithEffect(FInstancedStruct::Make(Stats)).Contains(false, TEXT("not under Combat.SetByCaller.Stat")));
	Magnitude.Magnitude = NaN;
	TestTrue(TEXT("A NaN magnitude is an error"), WithEffect(FInstancedStruct::Make(Stats)).Contains(true, TEXT("not a finite number")));

	FGSTN_PartBreakEffect_DisableHurtbox Disable;
	Disable.Hurtboxes = { "ArmBox", NAME_None };
	TestTrue(TEXT("An empty hurtbox name in a disable effect is an error"), WithEffect(FInstancedStruct::Make(Disable)).Contains(true, TEXT("empty hurtbox name")));
	Disable.Hurtboxes = { "ArmBox" };
	TestFalse(TEXT("A named hurtbox is valid"), WithEffect(FInstancedStruct::Make(Disable)).HasErrors());

	// Part types are Part.Type.* leaves: anything else is not seen by the rules that look for a type
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[3].Types.AddTag(GSTNPartsTags::Part_Broken);
	TestTrue(TEXT("A type outside Part.Type warns"), GSTNPartsTest::Validate(Parts).Contains(false, TEXT("not a Part.Type.* tag")));
	Parts = GSTNPartsTest::MakeEnemyParts();
	const FGameplayTag Head = FGameplayTag::RequestGameplayTag(FName(TEXT("Part.Type.Head")), false);
	if (Head.IsValid())
	{
		Parts[3].Types.AddTag(Head);
		TestEqual(TEXT("A Part.Type leaf passes"), GSTNPartsTest::Validate(Parts).NumWarnings(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataDegenerateBandsTest, "GSTNParts.Core.Data.DegenerateBandTablesWarn", GSTNPartsTest::Flags)
bool FGSTN_PartsDataDegenerateBandsTest::RunTest(const FString& Parameters)
{
	auto Validate = [](const TArray<FGSTN_PenetrationBand>& Bands)
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateBands(Bands, Report);
		return Report;
	};

	TestEqual(TEXT("The default table has no finding"), Validate(UGSTN_PenetrationTable::MakeDefaultBands()).Issues.Num(), 0);

	// A ricochet on top: a hit that ignores hardness takes band 0 and would bounce off everything
	TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	Bands[0].bRicochet = true;
	Bands[0].PartScale = Bands[0].MainScale = Bands[0].StaggerScale = Bands[0].HitstopScale = 0.f;
	TestTrue(TEXT("A top band that ricochets warns"), Validate(Bands).Contains(false, TEXT("top band and ricochets")));

	// Nothing deals damage
	FGSTN_PenetrationBand Wall;
	Wall.MinDelta = GSTN_PartsLimits::MinPossibleDelta;
	Wall.bRicochet = true;
	Wall.PartScale = Wall.MainScale = Wall.StaggerScale = Wall.HitstopScale = 0.f;
	TestTrue(TEXT("A table where every band ricochets warns"), Validate({ Wall }).Contains(false, TEXT("No band deals damage")));
	FGSTN_PenetrationBand Zero;
	Zero.MinDelta = GSTN_PartsLimits::MinPossibleDelta;
	Zero.PartScale = Zero.MainScale = 0.f;
	TestTrue(TEXT("A table where every band has scales of 0 warns"), Validate({ Zero }).Contains(false, TEXT("No band deals damage")));

	// A ricochet band whose stagger or hitstop scale is set: ApplyHit ignores it
	Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	Bands[2].StaggerScale = 1.f;
	TestTrue(TEXT("Stagger on a ricochet band warns"), Validate(Bands).Contains(false, TEXT("stagger and hitstop scales are ignored")));
	Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	Bands[2].HitstopScale = 1.f;
	TestTrue(TEXT("Hitstop on a ricochet band warns"), Validate(Bands).Contains(false, TEXT("stagger and hitstop scales are ignored")));
	return true;
}

// Review 2026-10-23 B.6: a duplicated set kept the constructor baked curve until the next load
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataDuplicateCurveTest, "GSTNParts.Core.Data.DuplicatedSetRebakesItsCurve", GSTNPartsTest::Flags)
bool FGSTN_PartsDataDuplicateCurveTest::RunTest(const FString& Parameters)
{
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage(), TEXT("CurveSource"));
	Set->bOverrideWeightCurve = true;
	if (FRichCurve* Curve = Set->WeightCurve.GetRichCurve())
	{
		Curve->Reset();
		Curve->AddKey(0.f, 0.5f);
		Curve->AddKey(1.f, 0.5f);
	}
	Set->RebakeWeightCurve();
	TestEqual(TEXT("The source is baked from its curve"), Set->GetWeightCurve().Sample(0.f), 0.5f, 0.0001f);

	UGSTN_PartSetDefinition* Copy = DuplicateObject<UGSTN_PartSetDefinition>(Set, GetTransientPackage(), TEXT("CurveCopy"));
	if (TestNotNull(TEXT("The set duplicates"), Copy))
	{
		TestTrue(TEXT("The copy keeps the override flag"), Copy->bOverrideWeightCurve);
		TestEqual(TEXT("The copy is baked from the copied curve, not the constructor one (1.0)"), Copy->GetWeightCurve().Sample(0.f), 0.5f, 0.0001f);
		TestEqual(TEXT("... and flat like its source"), Copy->GetWeightCurve().Sample(1.f), 0.5f, 0.0001f);
	}
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDataTableSortOnEditTest, "GSTNParts.Core.Data.TableSortsWhenEditSettles", GSTNPartsTest::Flags)
bool FGSTN_PartsDataTableSortOnEditTest::RunTest(const FString& Parameters)
{
	UGSTN_PenetrationTable* Table = NewObject<UGSTN_PenetrationTable>(GetTransientPackage());
	FGSTN_PenetrationBand Low;
	Low.MinDelta = -3;
	FGSTN_PenetrationBand High;
	High.MinDelta = 2;
	Table->Bands = { Low, High }; // typed in the wrong order

	FPropertyChangedEvent Interactive(nullptr, EPropertyChangeType::Interactive);
	Table->PostEditChangeProperty(Interactive);
	TestEqual(TEXT("A value being dragged does not reorder the rows"), Table->Bands[0].MinDelta, -3);

	FPropertyChangedEvent Settled(nullptr, EPropertyChangeType::ValueSet);
	Table->PostEditChangeProperty(Settled);
	TestEqual(TEXT("The final edit sorts them"), Table->Bands[0].MinDelta, 2);
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS
