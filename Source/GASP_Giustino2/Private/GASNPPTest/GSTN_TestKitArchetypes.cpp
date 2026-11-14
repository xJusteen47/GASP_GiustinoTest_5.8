// GAS + NPP + Mover network test harness: the three test kit archetypes of the hub (M11 K3).

#include "GASNPPTest/GSTN_TestKitArchetypes.h"
#include "Animation/AnimMontage.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Timeline/GSTN_TimelineTypes.h"

UGSTN_GA_TestKitStrike::UGSTN_GA_TestKitStrike(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_GA_TestKitStrike::FillStrikeTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_TestStrikeSpec& Spec)
{
	const int32 Total = FMath::Max(Spec.TotalFrames, 1);
	const int32 Start = FMath::Clamp(Spec.ActiveStart, 0, Total - 1);
	const int32 End = FMath::Clamp(Spec.ActiveEnd, Start, Total - 1);

	OutTimeline.Tracks.Reset();
	OutTimeline.TotalFrames = Total;
	OutTimeline.FirstActionableFrame = FMath::Clamp(Spec.FirstActionableFrame, End + 1, Total);
	OutTimeline.OverrideFrames = 0;
	OutTimeline.Montage = Spec.Montage.Get();
	OutTimeline.MontagePlayRate = FMath::Max(Spec.MontagePlayRate, 0.01f);

	FGSTN_HitData Hit;
	Hit.Damage = Spec.Damage;
	Hit.BaseKnockback = Spec.BaseKnockback;
	Hit.KnockbackGrowth = Spec.KnockbackGrowth;
	Hit.AngleDeg = Spec.AngleDeg;
	Hit.HitstopMode = EGSTN_HitstopMode::Override;
	Hit.HitstopFramesOverride = Spec.HitstopFrames;
	Hit.AttackerHitstopFramesOverride = Spec.AttackerHitstopFrames;
	Hit.Impact = Spec.Impact;

	FGSTN_HitboxSpec Box;
	Box.Shape.Type = EGSTN_ShapeType::Sphere;
	Box.Shape.Extent = FVector(Spec.Radius);
	Box.HitGroup = 0;
	Box.Space = EGSTN_HitboxSpace::Root;
	FGSTN_HitboxKey From;
	From.Frame = 0;
	From.Offset = FVector(Spec.ReachFrom, 0.0, Spec.Height);
	FGSTN_HitboxKey To;
	To.Frame = End - Start;
	To.Offset = FVector(Spec.ReachTo, 0.0, Spec.Height);
	Box.Keys = { From, To };
	Box.bSweepBetweenFrames = true;
	Box.Data = Hit;

	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = Start;
	Track.EndFrame = End + 1;
	Track.Hitboxes.Add(Box);
	OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

	if (!Spec.Tags.IsEmpty())
	{
		FGSTN_TimelineTrack_Tags TagsTrack;
		TagsTrack.StartFrame = FMath::Clamp(Spec.TagsFromFrame, 0, Total - 1);
		TagsTrack.EndFrame = Total;
		TagsTrack.Tags = Spec.Tags;
		OutTimeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>(TagsTrack));
	}
}

bool UGSTN_TestKitContentLibrary::WriteStrikeMove(TSubclassOf<UGameplayAbility> StrikeClass, const FGSTN_TestStrikeSpec& Spec)
{
	if (!StrikeClass || !StrikeClass->IsChildOf(UGSTN_GA_TestKitStrike::StaticClass()) || StrikeClass->HasAnyClassFlags(CLASS_Native))
	{
		return false;
	}
	UGSTN_GA_TestKitStrike* Defaults = GetMutableDefault<UGSTN_GA_TestKitStrike>(StrikeClass.Get());
	FGSTN_CombatTimelineData Data = Defaults->Timeline;
	UGSTN_GA_TestKitStrike::FillStrikeTimeline(Data, Spec);
	if (FGSTN_CombatTimelineData::StaticStruct()->CompareScriptStruct(&Data, &Defaults->Timeline, PPF_None))
	{
		return false;
	}
	Defaults->Timeline = MoveTemp(Data);
	Defaults->MarkPackageDirty();
	return true;
}

bool UGSTN_TestKitContentLibrary::WriteStartupSteer(TSubclassOf<UGameplayAbility> StrikeClass, float MaxSteerDegrees, float SteerRateDegPerSecond,
	const FGSTN_SteerWindow& Steer)
{
	if (!StrikeClass || !StrikeClass->IsChildOf(UGSTN_GA_TestKitStrike::StaticClass()) || StrikeClass->HasAnyClassFlags(CLASS_Native))
	{
		return false;
	}
	UGSTN_GA_TestKitStrike* Defaults = GetMutableDefault<UGSTN_GA_TestKitStrike>(StrikeClass.Get());
	FGSTN_CombatTimelineData Data = Defaults->Timeline;
	Data.MaxSteerDegrees = FMath::Clamp(MaxSteerDegrees, 0.f, 180.f);
	Data.SteerRateDegPerSecond = FMath::Max(SteerRateDegPerSecond, 0.f);
	Data.Steer = Steer;
	if (FGSTN_CombatTimelineData::StaticStruct()->CompareScriptStruct(&Data, &Defaults->Timeline, PPF_None))
	{
		return false;
	}
	Defaults->Timeline = MoveTemp(Data);
	Defaults->MarkPackageDirty();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"

namespace GSTNTestKitArchetypes
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	float StatOf(const UGSTN_PrimaryKitDefinition& Kit, const FGameplayTag& Stat)
	{
		const FGSTN_StatValue* Found = Kit.Stats.Values.FindByPredicate([&Stat](const FGSTN_StatValue& Value) { return Value.Stat == Stat; });
		return Found ? Found->Value : -1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TestKitStrikeTimelineTest, "GSTN.Test.KitArchetypes.StrikeTimeline", GSTNTestKitArchetypes::TestFlags)
bool FGSTN_TestKitStrikeTimelineTest::RunTest(const FString& Parameters)
{
	FGSTN_TestStrikeSpec Spec;
	Spec.TotalFrames = 20;
	Spec.FirstActionableFrame = 4;   // before the active frames end: raised to the frame after them
	Spec.ActiveStart = 3;
	Spec.ActiveEnd = 6;
	FGSTN_CombatTimelineData Data;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>());
	UGSTN_GA_TestKitStrike::FillStrikeTimeline(Data, Spec);
	TestEqual(TEXT("total frames"), Data.TotalFrames, 20);
	TestEqual(TEXT("actionable after the active frames"), Data.FirstActionableFrame, 7);
	TestEqual(TEXT("one track (the old one replaced)"), Data.Tracks.Num(), 1);
	const FGSTN_TimelineTrack_Hitbox* Track = Data.Tracks.Num() ? Data.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>() : nullptr;
	if (TestNotNull(TEXT("hitbox track"), Track))
	{
		TestEqual(TEXT("track start"), Track->StartFrame, 3);
		TestEqual(TEXT("track end (half open)"), Track->EndFrame, 7);
		TestEqual(TEXT("last key on the last active frame"), Track->Hitboxes[0].Keys.Last().Frame, 3);
	}
	return true;
}

/**
 * The saved archetype kits (Tools/GAS/setup_test_kit_archetypes.py): every one carries the eight stats of its init effect (a missing one
 * would read as zero), and they are really different: light < medium < heavy in weight and carry capacity, the other way round in speed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TestKitArchetypesContentTest, "GSTN.Test.KitArchetypes.Content", GSTNTestKitArchetypes::TestFlags)
bool FGSTN_TestKitArchetypesContentTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTestKitArchetypes;
	const TCHAR* Names[] = { TEXT("KIT_Test_Light"), TEXT("KIT_Test_Medium"), TEXT("KIT_Test_Heavy") };
	const FGameplayTag Keys[] = { GSTNTestTags::Stat_WalkSpeed, GSTNTestTags::Stat_JumpVelocity, GSTNTestTags::Stat_Weight, GSTNTestTags::Stat_MaxCarryWeight,
		GSTNTestTags::Stat_Acceleration, GSTNTestTags::Stat_AirControl, GSTNTestTags::Stat_FallGravityScale, GSTNTestTags::Stat_AirJumpCount };
	TArray<const UGSTN_PrimaryKitDefinition*> Kits;
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/GASNPPTest/Combat/%s.%s"), Name, Name);
		const UGSTN_PrimaryKitDefinition* Kit = LoadObject<UGSTN_PrimaryKitDefinition>(nullptr, *Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), Name), Kit))
		{
			return false;
		}
		TestTrue(*FString::Printf(TEXT("%s uses the archetype init effect"), Name),
			Kit->InitStatsEffect.ToSoftObjectPath() == FSoftObjectPath(UGSTN_GE_TestKitArchetypeInit::StaticClass()));
		for (const FGameplayTag& Key : Keys)
		{
			TestTrue(*FString::Printf(TEXT("%s has %s"), Name, *Key.ToString()), StatOf(*Kit, Key) >= 0.f);
		}
		Kits.Add(Kit);
	}
	for (int32 Index = 1; Index < Kits.Num(); ++Index)
	{
		TestTrue(TEXT("heavier down the list"), StatOf(*Kits[Index], GSTNTestTags::Stat_Weight) > StatOf(*Kits[Index - 1], GSTNTestTags::Stat_Weight));
		TestTrue(TEXT("carries more down the list"), StatOf(*Kits[Index], GSTNTestTags::Stat_MaxCarryWeight) > StatOf(*Kits[Index - 1], GSTNTestTags::Stat_MaxCarryWeight));
		TestTrue(TEXT("slower down the list"), StatOf(*Kits[Index], GSTNTestTags::Stat_WalkSpeed) < StatOf(*Kits[Index - 1], GSTNTestTags::Stat_WalkSpeed));
	}
	return true;
}

#endif
