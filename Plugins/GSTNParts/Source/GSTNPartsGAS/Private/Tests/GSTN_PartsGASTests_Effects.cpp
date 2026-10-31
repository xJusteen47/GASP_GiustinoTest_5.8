// Automation tests of the one-shot break effects (plan P5): the Stats effect per part, its magnitudes, the launch request and the
// stagger floor. On a stock ability system component: no Network Prediction, no Mover.

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Effects/GSTN_PartBreakEffectApplier.h"
#include "GSTN_CombatTags.h"
#include "Tests/GSTN_PartsGASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	float WalkSpeed(const UAbilitySystemComponent& ASC)
	{
		return ASC.GetNumericAttribute(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute());
	}

	float FastFall(const UAbilitySystemComponent& ASC)
	{
		return ASC.GetNumericAttribute(UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute());
	}

	FInstancedStruct MakeLaunch(float Speed, float UpFraction)
	{
		FGSTN_PartBreakEffect_Launch Launch;
		Launch.Speed = Speed;
		Launch.UpFraction = UpFraction;
		return FInstancedStruct::Make(Launch);
	}

	FInstancedStruct MakeStagger(int32 Frames)
	{
		FGSTN_PartBreakEffect_Stagger Stagger;
		Stagger.Frames = Frames;
		return FInstancedStruct::Make(Stagger);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASStatsApplyRemoveTest, "GSTNParts.GAS.Effects.StatsApplyRemoveByBreak", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASStatsApplyRemoveTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TSubclassOf<UGSTN_GE_PartStats> Class = UGSTN_TestPartStats::StaticClass();

	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Parts.Add(GSTNPartsGASTest::MakePart(*FString::Printf(TEXT("P%d"), Index)));
	}
	// Both keys the test effect reads are set (an unset key is an error the effect logs), the fast fall one with no magnitude
	Parts[1].OnBreak.Add(GSTNPartsGASTest::MakeStats({ { GSTNPartsGASTest::Stat_PartsTestSpeed, 100.f }, { GSTNPartsGASTest::Stat_PartsTestFastFall, 0.f } }));
	Parts[3].OnBreak.Add(GSTNPartsGASTest::MakeStats({ { GSTNPartsGASTest::Stat_PartsTestSpeed, 50.f }, { GSTNPartsGASTest::Stat_PartsTestFastFall, 0.f } }));

	TestEqual(TEXT("the effect is infinite"), static_cast<int32>(GetDefault<UGSTN_GE_PartStats>()->DurationPolicy), static_cast<int32>(EGameplayEffectDurationType::Infinite));
	const float Base = WalkSpeed(ASC);

	TestEqual(TEXT("a part without a Stats effect applies nothing"), FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 0), 0);
	TestEqual(TEXT("part 1 applies one effect"), FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 1), 1);
	TestEqual(TEXT("part 1's stats are on"), WalkSpeed(ASC), Base + 100.f);
	TestTrue(TEXT("part 1's effect is found by its part"), UGSTN_GE_PartStats::IsApplied(ASC, Class, 1));
	TestFalse(TEXT("part 3's is not applied yet"), UGSTN_GE_PartStats::IsApplied(ASC, Class, 3));

	TestEqual(TEXT("the same break again applies nothing (a resimulated frame)"), FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 1), 0);
	TestEqual(TEXT("and does not stack"), WalkSpeed(ASC), Base + 100.f);

	TestEqual(TEXT("part 3 is a second, independent effect of the same class"), FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 3), 1);
	TestEqual(TEXT("both on"), WalkSpeed(ASC), Base + 150.f);

	TestEqual(TEXT("removing part 1 removes one effect"), FGSTN_PartBreakEffectApplier::RemoveStats(ASC, Class, 1u << 1), 1);
	TestEqual(TEXT("only part 3's stats remain"), WalkSpeed(ASC), Base + 50.f);
	TestFalse(TEXT("part 1's effect is gone"), UGSTN_GE_PartStats::IsApplied(ASC, Class, 1));
	TestTrue(TEXT("part 3's effect stays"), UGSTN_GE_PartStats::IsApplied(ASC, Class, 3));
	TestEqual(TEXT("removing it twice removes nothing"), FGSTN_PartBreakEffectApplier::RemoveStats(ASC, Class, 1u << 1), 0);

	FGSTN_PartBreakEffectApplier::RemoveStats(ASC, Class, 0xFFFFu);
	TestEqual(TEXT("everything removed restores the base"), WalkSpeed(ASC), Base);

	TestFalse(TEXT("a null class applies nothing"), UGSTN_GE_PartStats::Apply(ASC, nullptr, 1, TConstArrayView<FGSTN_StatValue>()).IsValid());
	TestEqual(TEXT("an out of range part in the mask is ignored"), FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 9), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASStatsMagnitudesTest, "GSTNParts.GAS.Effects.StatsMagnitudesBySetByCaller", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASStatsMagnitudesTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const TSubclassOf<UGSTN_GE_PartStats> Class = UGSTN_TestPartStats::StaticClass();

	FGSTN_PartDefinition Part = GSTNPartsGASTest::MakePart("Leg");
	// Two Stats entries: the same key adds up, a second key is its own magnitude, an invalid key is dropped, a key no modifier reads is harmless
	Part.OnBreak.Add(GSTNPartsGASTest::MakeStats({ { GSTNPartsGASTest::Stat_PartsTestSpeed, -30.f }, { FGameplayTag(), 9.f } }));
	Part.OnBreak.Add(GSTNPartsGASTest::MakeStats({ { GSTNPartsGASTest::Stat_PartsTestSpeed, -20.f }, { GSTNPartsGASTest::Stat_PartsTestFastFall, 1.5f }, { GSTNPartsGASTest::Stat_PartsTestOther, 4.f } }));

	TArray<FGSTN_StatValue> Values;
	FGSTN_PartBreakEffectApplier::CollectStatValues(Part, Values);
	TestEqual(TEXT("three distinct valid keys"), Values.Num(), 3);
	const FGSTN_StatValue* Speed = Values.FindByPredicate([](const FGSTN_StatValue& Value) { return Value.Stat == GSTNPartsGASTest::Stat_PartsTestSpeed; });
	TestTrue(TEXT("the speed key is there"), Speed != nullptr);
	if (Speed)
	{
		TestEqual(TEXT("equal keys add up in data order"), Speed->Value, -50.f);
	}

	FGSTN_PartDefinition Plain = GSTNPartsGASTest::MakePart("Torso");
	FGSTN_PartBreakEffectApplier::CollectStatValues(Plain, Values);
	TestTrue(TEXT("a part without Stats effects has no magnitudes"), Values.IsEmpty());

	const float BaseSpeed = WalkSpeed(ASC);
	const float BaseFall = FastFall(ASC);
	TArray<FGSTN_PartDefinition> Parts;
	Parts.Add(Part);
	FGSTN_PartBreakEffectApplier::ApplyStats(ASC, Class, Parts, 1u << 0);
	TestEqual(TEXT("the speed modifier read its set-by-caller magnitude"), WalkSpeed(ASC), FMath::Max(0.f, BaseSpeed - 50.f));
	TestEqual(TEXT("the second modifier read its own key"), FastFall(ASC), BaseFall + 1.5f);

	FGSTN_PartBreakEffectApplier::RemoveStats(ASC, Class, 1u << 0);
	TestEqual(TEXT("speed back"), WalkSpeed(ASC), BaseSpeed);
	TestEqual(TEXT("fast fall back"), FastFall(ASC), BaseFall);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASLaunchRequestTest, "GSTNParts.GAS.Effects.LaunchRequestShape", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASLaunchRequestTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	FGSTN_PartBreakEffect_Launch Effect;
	Effect.Speed = 600.f;
	Effect.UpFraction = 0.25f;

	// World up: horizontal 450 along the hit's flat direction, vertical 150; the hit's own height is ignored
	FGSTN_PartLaunchRequest Request = FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector::UpVector, FVector(2.f, 0.f, 5.f), 12);
	TestTrue(TEXT("the request is set"), Request.IsSet());
	TestTrue(TEXT("velocity is the horizontal share along the hit plus the up share"), Request.Velocity.Equals(FVector(450.f, 0.f, 150.f), 0.01f));
	TestEqual(TEXT("the lifetime is the one asked for"), Request.Frames, 12);
	TestEqual(TEXT("no decay of its own"), Request.DecayPerFrame, 0.f);
	TestEqual(TEXT("normal gravity"), Request.GravityScale, 1.f);

	// A tilted up (a wall): horizontal and up are relative to it, never world Z
	Request = FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector(0.f, 1.f, 0.f), FVector(1.f, 3.f, 0.f), 6);
	TestTrue(TEXT("up is the entity's up"), Request.Velocity.Equals(FVector(450.f, 150.f, 0.f), 0.01f));

	// A hit along up has no horizontal direction: only the vertical share is left
	Request = FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector::UpVector, FVector(0.f, 0.f, 1.f), 6);
	TestTrue(TEXT("vertical only"), Request.Velocity.Equals(FVector(0.f, 0.f, 150.f), 0.01f));

	// Pure fractions
	Effect.UpFraction = 0.f;
	Request = FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector::UpVector, FVector::ForwardVector, 6);
	TestTrue(TEXT("0 = horizontal"), Request.Velocity.Equals(FVector(600.f, 0.f, 0.f), 0.01f));
	Effect.UpFraction = 1.f;
	Request = FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector::UpVector, FVector::ForwardVector, 6);
	TestTrue(TEXT("1 = along up"), Request.Velocity.Equals(FVector(0.f, 0.f, 600.f), 0.01f));

	Effect.Speed = 0.f;
	TestFalse(TEXT("no speed, no launch"), FGSTN_PartBreakEffectApplier::MakeLaunchRequest(Effect, FVector::UpVector, FVector::ForwardVector, 6).IsSet());

	// Collect: the strongest launch of the newly broken parts, ties to the lowest index
	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Parts.Add(GSTNPartsGASTest::MakePart(*FString::Printf(TEXT("P%d"), Index)));
	}
	Parts[0].OnBreak.Add(MakeLaunch(300.f, 0.f));
	Parts[1].OnBreak.Add(MakeLaunch(400.f, 0.f));
	Parts[2].OnBreak.Add(MakeLaunch(400.f, 1.f));
	Parts[3].OnBreak.Add(MakeLaunch(500.f, 0.5f));

	FGSTN_PartBreakRequests Requests;
	FGSTN_PartBreakEffectApplier::Collect(Parts, (1u << 0) | (1u << 1) | (1u << 2), FVector::UpVector, FVector::ForwardVector, 9, Requests);
	TestTrue(TEXT("a tie goes to the lower part: horizontal"), Requests.Launch.Velocity.Equals(FVector(400.f, 0.f, 0.f), 0.01f));
	FGSTN_PartBreakEffectApplier::Collect(Parts, 0xFu, FVector::UpVector, FVector::ForwardVector, 9, Requests);
	TestTrue(TEXT("the strongest wins"), Requests.Launch.Velocity.Equals(FVector(250.f, 0.f, 250.f), 0.01f));
	FGSTN_PartBreakEffectApplier::Collect(Parts, 0u, FVector::UpVector, FVector::ForwardVector, 9, Requests);
	TestFalse(TEXT("nothing newly broken, nothing launched"), Requests.Launch.IsSet());

	// The spec: the same set-by-caller shape as the hit reaction's knockback, through the keys the caller owns
	FGSTN_PartLaunchRequest Fixed;
	Fixed.Velocity = FVector(450.f, -20.f, 150.f);
	Fixed.DecayPerFrame = 0.05f;
	Fixed.GravityScale = 0.5f;
	Fixed.Frames = 10;
	FGSTN_PartLaunchKeys Keys;
	Keys.VelocityX = GSTNPartsGASTest::Stat_PartsTestSpeed;
	Keys.VelocityY = GSTNPartsGASTest::Stat_PartsTestFastFall;
	Keys.VelocityZ = GSTNPartsGASTest::Stat_PartsTestOther;
	Keys.DecayPerFrame = GSTNCombatTags::SetByCaller_Damage;
	Keys.GravityScale = GSTNCombatTags::SetByCaller_Meter;
	Keys.Seconds = GSTNCombatTags::SetByCaller_MeterCost;
	const FGameplayEffectSpecHandle Spec = FGSTN_PartBreakEffectApplier::MakeLaunchSpec(*Rig.ASC, UGameplayEffect::StaticClass(), Fixed, 16.f, Keys);
	TestTrue(TEXT("the spec exists"), Spec.IsValid());
	if (Spec.IsValid())
	{
		TestEqual(TEXT("velocity X"), Spec.Data->GetSetByCallerMagnitude(Keys.VelocityX, false, -1.f), 450.f);
		TestEqual(TEXT("velocity Y"), Spec.Data->GetSetByCallerMagnitude(Keys.VelocityY, false, -1.f), -20.f);
		TestEqual(TEXT("velocity Z"), Spec.Data->GetSetByCallerMagnitude(Keys.VelocityZ, false, -1.f), 150.f);
		TestEqual(TEXT("decay"), Spec.Data->GetSetByCallerMagnitude(Keys.DecayPerFrame, false, -1.f), 0.05f);
		TestEqual(TEXT("gravity scale"), Spec.Data->GetSetByCallerMagnitude(Keys.GravityScale, false, -1.f), 0.5f);
		TestEqual(TEXT("lifetime in seconds"), Spec.Data->GetSetByCallerMagnitude(Keys.Seconds, false, -1.f), 0.16f);
	}
	TestFalse(TEXT("an empty request makes no spec"), FGSTN_PartBreakEffectApplier::MakeLaunchSpec(*Rig.ASC, UGameplayEffect::StaticClass(), FGSTN_PartLaunchRequest(), 16.f, Keys).IsValid());
	TestFalse(TEXT("a null class makes no spec"), FGSTN_PartBreakEffectApplier::MakeLaunchSpec(*Rig.ASC, nullptr, Fixed, 16.f, Keys).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASStaggerMinimumTest, "GSTNParts.GAS.Effects.StaggerMinimumFrames", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASStaggerMinimumTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Parts.Add(GSTNPartsGASTest::MakePart(*FString::Printf(TEXT("P%d"), Index)));
	}
	Parts[0].OnBreak.Add(MakeStagger(10));
	Parts[1].OnBreak.Add(MakeStagger(-5));
	Parts[2].OnBreak.Add(MakeStagger(24));
	Parts[2].OnBreak.Add(MakeStagger(8));

	TestEqual(TEXT("one part"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, 1u << 0), 10);
	TestEqual(TEXT("the longest of two parts"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, (1u << 0) | (1u << 2)), 24);
	TestEqual(TEXT("the longest entry of one part"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, 1u << 2), 24);
	TestEqual(TEXT("a negative count is 0"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, 1u << 1), 0);
	TestEqual(TEXT("a part without the effect"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, 1u << 3), 0);
	TestEqual(TEXT("nothing broken"), FGSTN_PartBreakEffectApplier::GetMinimumStaggerFrames(Parts, 0u), 0);

	TestEqual(TEXT("a short hitstun is raised to the floor"), FGSTN_PartBreakEffectApplier::ApplyMinimumHitstun(6, 24), 24);
	TestEqual(TEXT("a longer hitstun is kept"), FGSTN_PartBreakEffectApplier::ApplyMinimumHitstun(30, 24), 30);
	TestEqual(TEXT("no floor changes nothing"), FGSTN_PartBreakEffectApplier::ApplyMinimumHitstun(6, 0), 6);

	FGSTN_PartBreakRequests Requests;
	FGSTN_PartBreakEffectApplier::Collect(Parts, (1u << 0) | (1u << 2), FVector::UpVector, FVector::ForwardVector, 6, Requests);
	TestEqual(TEXT("Collect carries the floor"), Requests.MinimumHitstunFrames, 24);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
