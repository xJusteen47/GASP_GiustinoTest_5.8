// Held objects, pure core tests of slice 6 (GSTNHeld.Core.*): the plan's G6 size classes, carry load payload, capacity and command bans.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldLoadPayloadTest, "GSTNHeld.Core.LoadPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldLoadPayloadTest::RunTest(const FString& Parameters)
{
	for (int32 Size = 0; Size < FGSTN_HeldLoad::MaxSizeClasses; ++Size)
	{
		for (const float Ratio : { 0.f, 0.1f, 0.5f, 1.f })
		{
			const uint8 Payload = FGSTN_HeldLoad::EncodePayload(Size, Ratio);
			int32 OutSize = INDEX_NONE;
			float OutRatio = -1.f;
			FGSTN_HeldLoad::DecodePayload(Payload, OutSize, OutRatio);
			TestEqual(TEXT("size round trip"), OutSize, Size);
			TestTrue(TEXT("ratio round trip within a step"), FMath::Abs(OutRatio - Ratio) <= 0.5f / FGSTN_HeldLoad::RatioSteps + KINDA_SMALL_NUMBER);
			// Through the effect's set-by-caller magnitude (payload / 255) and back
			TestEqual(TEXT("magnitude round trip"), FGSTN_HeldLoad::PayloadFromMagnitude(static_cast<float>(Payload) / 255.f), Payload);
		}
	}
	TestEqual(TEXT("out of range clamps"), FGSTN_HeldLoad::EncodePayload(7, 2.f), FGSTN_HeldLoad::EncodePayload(3, 1.f));
	TestEqual(TEXT("payload 0 = class 0, no load"), FGSTN_HeldLoad::EncodePayload(0, 0.f), static_cast<uint8>(0));

	TestEqual(TEXT("own capacity"), FGSTN_HeldLoad::LoadRatio(20.f, 40.f, 50.f), 0.5f);
	TestEqual(TEXT("no own capacity: the default"), FGSTN_HeldLoad::LoadRatio(25.f, 0.f, 50.f), 0.5f);
	TestEqual(TEXT("over capacity clamps to 1"), FGSTN_HeldLoad::LoadRatio(80.f, 40.f, 50.f), 1.f);
	TestEqual(TEXT("no capacity at all: full load"), FGSTN_HeldLoad::LoadRatio(1.f, 0.f, 0.f), 1.f);

	FGSTN_HeldSizeClass Medium;
	Medium.SpeedLossAtFullLoad = 0.4f;
	Medium.JumpLossAtFullLoad = 1.f;
	TestEqual(TEXT("speed at half load"), FGSTN_HeldLoad::SpeedScale(&Medium, 0.5f), 0.8f);
	TestEqual(TEXT("jump at full load"), FGSTN_HeldLoad::JumpScale(&Medium, 1.f), 0.f);
	TestEqual(TEXT("no class, no loss"), FGSTN_HeldLoad::SpeedScale(nullptr, 1.f), 1.f);

	FGSTN_HeldSizeClass Huge;
	Huge.bRequiresCapacity = true;
	TestTrue(TEXT("huge within capacity"), FGSTN_HeldLoad::CanLift(&Huge, 40.f, 40.f));
	TestFalse(TEXT("huge over capacity"), FGSTN_HeldLoad::CanLift(&Huge, 41.f, 40.f));
	TestTrue(TEXT("a class without the rule lifts anything"), FGSTN_HeldLoad::CanLift(&Medium, 400.f, 40.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldPickupCapacityTest, "GSTNHeld.Core.PickupCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldPickupCapacityTest::RunTest(const FString& Parameters)
{
	// A huge object nearest, a light one further: the weak holder takes the light one, the strong one the huge one
	TArray<FGSTN_PickupCandidate> Candidates;
	FGSTN_PickupCandidate& HugeObject = Candidates.Add_GetRef({ 1, FVector(50.0, 0.0, 0.0), 200.f, true });
	HugeObject.WeightKg = 100.f;
	HugeObject.bRequiresCapacity = true;
	FGSTN_PickupCandidate& LightObject = Candidates.Add_GetRef({ 2, FVector(150.0, 0.0, 0.0), 200.f, true });
	LightObject.WeightKg = 100.f;

	TArray<FGSTN_PickupRequest> Requests;
	FGSTN_PickupRequest& Weak = Requests.Add_GetRef({ 10, 5, FVector::ZeroVector });
	Weak.CarryCapacityKg = 50.f;
	Weak.TargetObjectId = 1;
	TArray<FGSTN_PickupAssignment> Assignments;
	FGSTN_HeldRules::ResolvePickups(Requests, Candidates, Assignments);
	TestTrue(TEXT("weak holder skips the focused huge object"), Assignments.Num() == 1 && Assignments[0].ObjectId == 2);

	Requests[0].CarryCapacityKg = 100.f;
	FGSTN_HeldRules::ResolvePickups(Requests, Candidates, Assignments);
	TestTrue(TEXT("strong holder lifts it"), Assignments.Num() == 1 && Assignments[0].ObjectId == 1);

	Candidates.RemoveAt(1);
	Requests[0].CarryCapacityKg = 10.f;
	FGSTN_HeldRules::ResolvePickups(Requests, Candidates, Assignments);
	TestEqual(TEXT("nothing liftable = fumble"), Assignments.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldSizeBansTest, "GSTNHeld.Core.SizeBans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldSizeBansTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHeldTags;
	const FGameplayTag Shield = FGameplayTag::RequestGameplayTag(TEXT("Command.Defence.Shield"), false);
	const FGameplayTag Attack = FGameplayTag::RequestGameplayTag(TEXT("Command.Attack"), false);
	const FGameplayTag AttackNeutral = FGameplayTag::RequestGameplayTag(TEXT("Command.Attack.Neutral"), false);
	const FGameplayTag Airborne = FGameplayTag::RequestGameplayTag(TEXT("Input.Context.Airborne"), false);
	if (!Shield.IsValid() || !AttackNeutral.IsValid() || !Airborne.IsValid())
	{
		AddWarning(TEXT("combat command tags not registered: skipped"));
		return true;
	}

	TArray<FGSTN_HeldSizeClass> Classes;
	FGSTN_HeldSizeClass& Medium = Classes.AddDefaulted_GetRef();
	Medium.SizeTag = Held_Size_Medium;
	Medium.CarryingTag = Held_Carrying_Medium;
	Medium.Bans.AddDefaulted_GetRef().Commands.AddTag(Shield);
	FGSTN_HeldCommandBan& Air = Medium.Bans.AddDefaulted_GetRef();
	Air.Commands.AddTag(Attack);
	Air.Commands.AddTag(Command_Held_Throw.GetTag().RequestDirectParent());
	Air.OnlyBindingsRequiring = Airborne;

	TArray<FGSTN_BindingInput> Bindings;
	Bindings.AddDefaulted_GetRef().Command = Shield;
	Bindings.AddDefaulted_GetRef().Command = AttackNeutral;
	FGSTN_BindingInput& AirAttack = Bindings.AddDefaulted_GetRef();
	AirAttack.Command = AttackNeutral;
	AirAttack.Requirements.RequireTags.AddTag(Airborne);
	FGSTN_BindingInput& AirThrow = Bindings.AddDefaulted_GetRef();
	AirThrow.Command = Command_Held_Throw;
	AirThrow.Requirements.RequireTags.AddTag(Airborne);

	TestEqual(TEXT("shield and the air attack banned"), UGSTN_HeldSlots::ApplySizeBans(Classes, Bindings), 2);
	TestTrue(TEXT("shield ignores carrying medium"), Bindings[0].Requirements.IgnoreTags.HasTagExact(Held_Carrying_Medium));
	TestFalse(TEXT("ground attack kept"), Bindings[1].Requirements.IgnoreTags.HasTagExact(Held_Carrying_Medium));
	TestTrue(TEXT("air attack banned (hierarchical)"), Bindings[2].Requirements.IgnoreTags.HasTagExact(Held_Carrying_Medium));
	TestFalse(TEXT("a held command is never banned"), Bindings[3].Requirements.IgnoreTags.HasTagExact(Held_Carrying_Medium));
	TestEqual(TEXT("idempotent"), UGSTN_HeldSlots::ApplySizeBans(Classes, Bindings), 0);

	// The project defaults: four classes, packable, each with its carrying tag; only Huge needs capacity
	const UGSTN_HeldSettings& Settings = UGSTN_HeldSettings::Get();
	TestEqual(TEXT("four default classes"), Settings.SizeClasses.Num(), 4);
	int32 Index = INDEX_NONE;
	const FGSTN_HeldSizeClass* HugeClass = Settings.FindSizeClass(Held_Size_Huge, &Index);
	TestTrue(TEXT("huge found at index 3 and needs capacity"), HugeClass && Index == 3 && HugeClass->bRequiresCapacity);
	const FGSTN_HeldSizeClass* LightClass = Settings.FindSizeClass(Held_Size_Light, &Index);
	TestTrue(TEXT("light found at 0, no bans"), LightClass && Index == 0 && LightClass->Bans.Num() == 0);
	TestEqual(TEXT("object default is light"), FGSTN_HeldObjectTuning().SizeTag, Held_Size_Light.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldImpactTest, "GSTNHeld.Core.Impact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldImpactTest::RunTest(const FString& Parameters)
{
	FGSTN_HeldObjectTuning Tuning;
	FVector Out;
	const FVector Down(600.0, 0.0, -800.0);
	const FVector Up(0.0, 0.0, 1.0);
	TestEqual(TEXT("default lands"), FGSTN_HeldRules::ResolveImpact(Tuning, true, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Land);

	Tuning.ImpactBehaviour = EGSTN_HeldImpactBehaviour::Break;
	TestEqual(TEXT("break on the world"), FGSTN_HeldRules::ResolveImpact(Tuning, true, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Break);
	TestEqual(TEXT("break on a fighter"), FGSTN_HeldRules::ResolveImpact(Tuning, false, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Break);

	Tuning.ImpactBehaviour = EGSTN_HeldImpactBehaviour::Stick;
	TestEqual(TEXT("stick on the world"), FGSTN_HeldRules::ResolveImpact(Tuning, true, Down, FVector(-1.0, 0.0, 0.0), 0, Out), EGSTN_HeldImpactBehaviour::Stick);
	TestEqual(TEXT("a fighter hit lands"), FGSTN_HeldRules::ResolveImpact(Tuning, false, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Land);

	Tuning.ImpactBehaviour = EGSTN_HeldImpactBehaviour::Bounce;
	Tuning.BounceRestitution = 0.5f;
	Tuning.MaxBounces = 2;
	Tuning.MinBounceSpeed = 200.f;
	TestEqual(TEXT("bounce off the floor"), FGSTN_HeldRules::ResolveImpact(Tuning, true, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Bounce);
	TestTrue(TEXT("reflected and damped"), Out.Equals(FVector(300.0, 0.0, 400.0), 0.01));
	TestEqual(TEXT("too many bounces land"), FGSTN_HeldRules::ResolveImpact(Tuning, true, Down, Up, 2, Out), EGSTN_HeldImpactBehaviour::Land);
	TestEqual(TEXT("too slow lands"), FGSTN_HeldRules::ResolveImpact(Tuning, true, FVector(0.0, 0.0, -300.0), Up, 0, Out), EGSTN_HeldImpactBehaviour::Land);
	TestEqual(TEXT("a fighter hit lands"), FGSTN_HeldRules::ResolveImpact(Tuning, false, Down, Up, 0, Out), EGSTN_HeldImpactBehaviour::Land);
	return true;
}

#endif
