// Automation tests of the content validators of checkpoint H13a: the hurtbox set checks, the matrix printers that IsDataValid prints, and
// the hitstop override check the timeline validation runs. Pure: in-memory structs and transient objects.

#include "GSTN_CombatTestUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatRulesValidation.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNValidationTest
{
	FGSTN_Hurtbox MakeHurtbox(const TCHAR* Name, double Radius = 30.0)
	{
		FGSTN_Hurtbox Box;
		Box.Name = Name;
		Box.Shape.Type = EGSTN_ShapeType::Sphere;
		Box.Shape.Extent = FVector(Radius);
		Box.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Torso"), false));
		return Box;
	}

	int32 CountMessages(const TArray<FGSTN_ReactionMessage>& Messages, bool bError)
	{
		int32 Count = 0;
		for (const FGSTN_ReactionMessage& Message : Messages)
		{
			Count += Message.bError == bError ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationHurtboxSetTest, "GSTNCombat.Core.Validation.HurtboxSet", GSTN_CORE_TEST_FLAGS)

bool FGSTN_ValidationHurtboxSetTest::RunTest(const FString& Parameters)
{
	using namespace GSTNValidationTest;
	const TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	TestEqual(TEXT("an empty set is an error"), CountMessages(Messages, true), 1);

	Set->Hurtboxes = { MakeHurtbox(TEXT("torso")), MakeHurtbox(TEXT("head"), 20.0) };
	Messages.Reset();
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	TestEqual(TEXT("a good set has no finding"), Messages.Num(), 0);

	Set->Hurtboxes.Add(MakeHurtbox(TEXT("torso")));
	Set->Hurtboxes.Add(MakeHurtbox(TEXT("flat"), 0.0));
	FGSTN_Hurtbox Untagged = MakeHurtbox(TEXT("feet"));
	Untagged.Tags.Reset();
	Set->Hurtboxes.Add(Untagged);
	Set->Hurtboxes.Add(MakeHurtbox(TEXT("")));
	Messages.Reset();
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	TestEqual(TEXT("a duplicated name, a flat shape and an empty name are errors"), CountMessages(Messages, true), 3);
	TestEqual(TEXT("a hurtbox with no tag is a warning"), CountMessages(Messages, false), 1);

	Set->Hurtboxes = { MakeHurtbox(TEXT("torso")) };
	Set->WeightCurve.Samples.SetNumZeroed(10);
	Messages.Reset();
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(*Set, Messages);
	TestEqual(TEXT("a curve of 10 samples is an error"), CountMessages(Messages, true), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationSetMatrixTest, "GSTNCombat.Core.Validation.HurtboxSetMatrix", GSTN_CORE_TEST_FLAGS)

bool FGSTN_ValidationSetMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GSTNValidationTest;
	const TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	const TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));

	FGSTN_Hurtbox Head = MakeHurtbox(TEXT("head"));
	FGSTN_Hurtbox Armour = MakeHurtbox(TEXT("armour"));
	Armour.ReactionScale.StaggerThreshold.bInfinite = true;
	FGSTN_Hurtbox Capped = MakeHurtbox(TEXT("capped"));
	Capped.ReactionScale.MaxReaction = EGSTN_HitReaction::StaggerInPlace;
	Set->Hurtboxes = { Head, Armour, Capped };

	const FString Matrix = FGSTN_CombatRulesValidation::PrintHurtboxSetMatrix(*Set, *Rules);
	TArray<FString> Lines;
	Matrix.ParseIntoArrayLines(Lines);
	if (!TestEqual(TEXT("a header and one line per hurtbox"), Lines.Num(), 4))
	{
		return false;
	}
	TestEqual(TEXT("header"), Lines[0], FString(TEXT("Victim | Light | Medium | Heavy")));
	TestTrue(TEXT("an infinite threshold never staggers"), Lines[2].EndsWith(TEXT("| - | - | -")));
	TestFalse(TEXT("a capped hurtbox never leaves the spot"), Lines[3].Contains(TEXT("Push")) || Lines[3].Contains(TEXT("Launch")));
	TestTrue(TEXT("the set threshold applies to a hurtbox that sets none"), Lines[1].Contains(TEXT("head")));

	// The set's own threshold is the default of a hurtbox that sets none
	Set->StaggerThreshold.bInfinite = true;
	TArray<FString> Armoured;
	FGSTN_CombatRulesValidation::PrintHurtboxSetMatrix(*Set, *Rules).ParseIntoArrayLines(Armoured);
	TestTrue(TEXT("a set threshold shows on its hurtboxes"), Armoured[1].EndsWith(TEXT("| - | - | -")));

	const FString Standard = FGSTN_CombatRulesValidation::PrintStandardMatrix(*Rules);
	TestTrue(TEXT("the standard matrix has the three victims"), Standard.StartsWith(TEXT("Victim | Light | Medium | Heavy\nLight (weight 60)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationHitstopTimelineTest, "GSTNCombat.Core.Validation.TimelineHitstopOverride", GSTN_CORE_TEST_FLAGS)

bool FGSTN_ValidationHitstopTimelineTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 20;
	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = 2;
	Track.EndFrame = 6;
	FGSTN_HitboxSpec& Hitbox = Track.Hitboxes.AddDefaulted_GetRef();
	Hitbox.Shape.Type = EGSTN_ShapeType::Sphere;
	Hitbox.Shape.Extent = FVector(40.0);
	Hitbox.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Hitbox.Data.HitstopFramesOverride = FGSTN_ReactionTuning().MaxHitstop + 10;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidation::MakeContext(Data, 60.f), Messages);
	bool bFound = false;
	for (const FGSTN_TimelineMessage& Message : Messages)
	{
		bFound |= !Message.bError && Message.Text.Contains(TEXT("clamped"));
	}
	TestTrue(TEXT("a hitstop override above the model's limit is reported by the timeline validation"), bFound);
	return true;
}

#endif
