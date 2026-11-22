// Automation tests of the parts editor module: the validator, the balance report and the Physics Asset draft (plan P13).
// Editor context, no PIE: every asset is transient.

#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PenetrationTable.h"
#include "GSTN_PartBalanceReport.h"
#include "GSTN_PartSetValidator.h"
#include "GSTN_PhysicsAssetToPartSet.h"
#include "Math/GSTN_PartMath.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsEditorTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FGSTN_PartDefinition MakePart(FName Name, FName Hurtbox, int32 Hardness, float Cap, float MainTransfer = 1.f, int32 ParentIndex = INDEX_NONE)
	{
		FGSTN_PartDefinition Part;
		Part.Name = Name;
		Part.Hurtboxes.Add(Hurtbox);
		Part.Hardness = Hardness;
		Part.PartCap = Cap;
		Part.MainTransfer = MainTransfer;
		Part.ParentIndex = ParentIndex;
		return Part;
	}

	/** Torso, an armoured arm with a hand child, a weak point head: the same shape as the Core tests' small enemy */
	TArray<FGSTN_PartDefinition> MakeEnemyParts()
	{
		TArray<FGSTN_PartDefinition> Parts;
		Parts.Add(MakePart("Torso", "TorsoBox", 0, 100.f, 1.f));
		Parts.Add(MakePart("Arm", "ArmBox", 4, 60.f, 0.3f));
		Parts.Add(MakePart("Hand", "HandBox", 2, 30.f, 0.2f, 1));
		Parts.Add(MakePart("Head", "HeadBox", 1, 40.f, 2.f));
		Parts[3].bWeakPoint = true;
		return Parts;
	}

	UGSTN_PartSetDefinition* MakeSet(const TArray<FGSTN_PartDefinition>& Parts)
	{
		UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage());
		Set->Parts = Parts;
		return Set;
	}

	/** Whether some issue of the given severity names the asset path and contains Text */
	bool HasIssue(const FDataValidationContext& Context, EMessageSeverity::Type Severity, const UObject* Asset, const TCHAR* Text)
	{
		const FString Path = Asset->GetPathName();
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			const FString Message = Issue.Message.ToString();
			if (Issue.Severity == Severity && Message.Contains(Path) && Message.Contains(Text))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorDuplicateHurtboxTest, "GSTNParts.Editor.DuplicateHurtbox", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorDuplicateHurtboxTest::RunTest(const FString& Parameters)
{
	UGSTN_PartSetDefinition* Valid = GSTNPartsEditorTest::MakeSet(GSTNPartsEditorTest::MakeEnemyParts());
	FDataValidationContext ValidContext;
	TestEqual(TEXT("A well formed set validates"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Valid, ValidContext)), static_cast<int32>(EDataValidationResult::Valid));
	TestEqual(TEXT("A well formed set has no findings"), ValidContext.GetIssues().Num(), 0);

	TArray<FGSTN_PartDefinition> Parts = GSTNPartsEditorTest::MakeEnemyParts();
	Parts[3].Hurtboxes.Add("ArmBox");
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Parts);
	FDataValidationContext Context;
	TestEqual(TEXT("A hurtbox in two parts is invalid"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Set, Context)), static_cast<int32>(EDataValidationResult::Invalid));
	TestEqual(TEXT("One error"), static_cast<int32>(Context.GetNumErrors()), 1);
	TestTrue(TEXT("The error names the asset path and the hurtbox"), GSTNPartsEditorTest::HasIssue(Context, EMessageSeverity::Error, Set, TEXT("hurtbox ArmBox already belongs")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorParentCycleTest, "GSTNParts.Editor.ParentCycle", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorParentCycleTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsEditorTest::MakeEnemyParts();
	Parts[1].ParentIndex = 2; // the hand is already the arm's child
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Parts);
	FDataValidationContext Context;
	TestEqual(TEXT("A parent cycle is invalid"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Set, Context)), static_cast<int32>(EDataValidationResult::Invalid));
	TestTrue(TEXT("At least one error"), Context.GetNumErrors() > 0);
	TestTrue(TEXT("The error names the asset path"), GSTNPartsEditorTest::HasIssue(Context, EMessageSeverity::Error, Set, TEXT("parent cycle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorMaskOver16Test, "GSTNParts.Editor.MaskOver16", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorMaskOver16Test::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts + 1; ++Index)
	{
		Parts.Add(GSTNPartsEditorTest::MakePart(*FString::Printf(TEXT("Part%d"), Index), *FString::Printf(TEXT("Box%d"), Index), 0, 50.f));
	}
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Parts);
	FDataValidationContext Context;
	TestEqual(TEXT("Seventeen parts are invalid"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Set, Context)), static_cast<int32>(EDataValidationResult::Invalid));
	TestEqual(TEXT("One error"), static_cast<int32>(Context.GetNumErrors()), 1);
	TestTrue(TEXT("The error names the asset path and the limit"), GSTNPartsEditorTest::HasIssue(Context, EMessageSeverity::Error, Set, TEXT("limit is 16")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorMissingBoneTest, "GSTNParts.Editor.MissingBone", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorMissingBoneTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsEditorTest::MakeEnemyParts();
	Parts[1].Bones = { "upperarm_l", "lowerarm_l" };
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Parts);
	const TSet<FName> Bones = { "upperarm_l" };
	FDataValidationContext Context;
	TestEqual(TEXT("A missing bone is a warning, the set stays valid"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Set, Context, &Bones)), static_cast<int32>(EDataValidationResult::Valid));
	TestEqual(TEXT("No errors"), static_cast<int32>(Context.GetNumErrors()), 0);
	TestEqual(TEXT("One warning"), static_cast<int32>(Context.GetNumWarnings()), 1);
	TestTrue(TEXT("The warning names the asset path and the bone"), GSTNPartsEditorTest::HasIssue(Context, EMessageSeverity::Warning, Set, TEXT("bone lowerarm_l")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorEmptyTableTest, "GSTNParts.Editor.EmptyTableRejected", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorEmptyTableTest::RunTest(const FString& Parameters)
{
	UGSTN_PenetrationTable* Table = NewObject<UGSTN_PenetrationTable>(GetTransientPackage());
	FDataValidationContext Valid;
	TestEqual(TEXT("A new table is the default and validates"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Table, Valid)), static_cast<int32>(EDataValidationResult::Valid));

	Table->Bands.Reset();
	FDataValidationContext Context;
	TestEqual(TEXT("An empty table is invalid"), static_cast<int32>(UGSTN_PartSetValidator::ValidateAsset(Table, Context)), static_cast<int32>(EDataValidationResult::Invalid));
	TestTrue(TEXT("The error names the asset path"), GSTNPartsEditorTest::HasIssue(Context, EMessageSeverity::Error, Table, TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorBalanceReportTest, "GSTNParts.Editor.BalanceReportNumbers", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorBalanceReportTest::RunTest(const FString& Parameters)
{
	// Worked by hand from the P3 golden rows (same parts, same quantisation): damage 10, penetration 6 is above every hardness here.
	// Torso (cap 100): bytes 26, 52, ... 234, then 255 on the 10th hit. Arm (cap 60, 10 / 60 x 255 = 42.5): 43, 86, 129, 172, 215,
	// then 255 on the 6th hit; it feeds 3 per hit to the main pool while intact and 1.5 once broken (broken transfer is half)
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsEditorTest::MakeEnemyParts();
	Parts[0].Reaction.StaggerThreshold = 4.f;
	Parts[1].Reaction.StaggerThreshold = 10.f;
	Parts.Add(GSTNPartsEditorTest::MakePart("Rock", "RockBox", 0, 0.f));

	FGSTN_PartHitData HitData;
	HitData.Penetration = 6;
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	// Pool 95: the torso feeds 10 per hit, so 10 hits. Pool 99.5: the arm feeds 6 x 3 = 18, then 1.5 per hit on the stump: 55 more
	TArray<FGSTN_PartBalanceRow> Rows = UGSTN_PartBalanceReport::ComputeFromParts(Parts, Bands, HitData, 10.f, 95.f, 5.f);
	if (!TestEqual(TEXT("One row per part"), Rows.Num(), 5))
	{
		return false;
	}
	TestEqual(TEXT("Torso hits to break"), Rows[0].HitsToBreak, 10);
	TestEqual(TEXT("Torso hits to kill"), Rows[0].HitsToKill, 10);
	TestEqual(TEXT("Torso staggers (5 >= 4)"), Rows[0].HitsToStagger, 1);
	TestEqual(TEXT("Arm hits to break"), Rows[1].HitsToBreak, 6);
	TestEqual(TEXT("Arm never staggers (5 < 10)"), Rows[1].HitsToStagger, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Head has no threshold: never staggers"), Rows[3].HitsToStagger, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("Rock is unbreakable"), Rows[4].bUnbreakable);
	TestEqual(TEXT("Rock never breaks"), Rows[4].HitsToBreak, static_cast<int32>(INDEX_NONE));

	Rows = UGSTN_PartBalanceReport::ComputeFromParts(Parts, Bands, HitData, 10.f, 99.5f, 5.f);
	TestEqual(TEXT("Arm hits to kill through the stump"), Rows[1].HitsToKill, 61);

	// Penetration 3 against the arm's hardness 4 ricochets: no break, no kill, no stagger
	HitData.Penetration = 3;
	Rows = UGSTN_PartBalanceReport::ComputeFromParts(Parts, Bands, HitData, 10.f, 99.5f, 5.f);
	TestEqual(TEXT("A ricochet never breaks"), Rows[1].HitsToBreak, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A ricochet never kills"), Rows[1].HitsToKill, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("A ricochet never staggers"), Rows[1].HitsToStagger, static_cast<int32>(INDEX_NONE));

	// The asset form with the default table and the default pool (sum of the breakable caps: 100 + 60 + 30 + 40 = 230); the torso's broken transfer is half
	HitData.Penetration = 6;
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Parts);
	Rows = UGSTN_PartBalanceReport::Compute(Set, nullptr, HitData, 10.f);
	TestEqual(TEXT("Asset form: torso hits to break"), Rows[0].HitsToBreak, 10);
	TestEqual(TEXT("Asset form: torso hits to kill (10 x 10, then the stump feeds 5: 100 + 26 x 5 >= 230)"), Rows[0].HitsToKill, 36);

	const FString Text = UGSTN_PartBalanceReport::FormatReport(Rows);
	TestTrue(TEXT("The text lists the parts and marks the unbreakable one"), Text.Contains(TEXT("Torso")) && Text.Contains(TEXT("Rock (unbreakable)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEditorPhysicsAssetDraftTest, "GSTNParts.Editor.PhysicsAssetDraftHasUniqueNames", GSTNPartsEditorTest::Flags)
bool FGSTN_PartsEditorPhysicsAssetDraftTest::RunTest(const FString& Parameters)
{
	UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(GetTransientPackage());
	for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_01"), TEXT("spine_01") })
	{
		USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PhysicsAsset);
		Body->BoneName = Bone;
		PhysicsAsset->SkeletalBodySetups.Add(Body);
	}

	const TArray<FGSTN_PartDefinition> Draft = UGSTN_PhysicsAssetToPartSet::MakeDraftParts(PhysicsAsset);
	if (!TestEqual(TEXT("One part per body"), Draft.Num(), 4))
	{
		return false;
	}
	TSet<FName> Names;
	for (const FGSTN_PartDefinition& Part : Draft)
	{
		Names.Add(Part.Name);
	}
	TestEqual(TEXT("Every name is unique"), Names.Num(), 4);
	TestEqual(TEXT("First body keeps the bone name"), Draft[1].Name, FName("spine_01"));
	TestEqual(TEXT("Second body on the bone gets a suffix"), Draft[2].Name, FName("spine_01_1"));
	TestEqual(TEXT("Third body on the bone gets the next suffix"), Draft[3].Name, FName("spine_01_2"));
	TestTrue(TEXT("Bone is the body's bone"), Draft[2].Bones.Num() == 1 && Draft[2].Bones[0] == FName("spine_01"));

	// The draft is a valid starting point: the unique names pass the validator's rules
	UGSTN_PartSetDefinition* Set = GSTNPartsEditorTest::MakeSet(Draft);
	FDataValidationContext Context;
	UGSTN_PartSetValidator::ValidateAsset(Set, Context);
	TestEqual(TEXT("The draft has no duplicate-name error"), static_cast<int32>(Context.GetNumErrors()), 0);
	TestEqual(TEXT("No physics asset gives no parts"), UGSTN_PhysicsAssetToPartSet::MakeDraftParts(nullptr).Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
