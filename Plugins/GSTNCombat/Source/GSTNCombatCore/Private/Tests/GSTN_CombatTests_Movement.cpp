#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_MovementSlots.h"
#include "GSTN_CombatCoreTags.h"
#include "Movement/GSTN_DashTypes.h"
#include "Movement/GSTN_MoveTimelines.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNCombatTest;

namespace
{
	/** The standard six buttons, then dodge, sprint and interact (letters G, H, I of the scripts) */
	TArray<FGameplayTag> MovementButtonTags()
	{
		TArray<FGameplayTag> Tags = ButtonTags();
		Tags.Add(GSTNCombatButtonTags::Button_Dodge.GetTag());
		Tags.Add(GSTNCombatButtonTags::Button_Sprint.GetTag());
		Tags.Add(GSTNCombatButtonTags::Button_Interact.GetTag());
		return Tags;
	}

	/** The command the slots trigger for a notation played with the movement buttons, empty if none */
	FString CommandFor(FAutomationTestBase& Test, const TArray<FGSTN_InputSlotSpec>& Slots, const TCHAR* Notation, const UGSTN_InputSettings& Settings)
	{
		FGSTN_InputScript Script(MovementButtonTags());
		FString Error;
		FGSTN_InputHistory History;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);

		const FGSTN_ButtonMap Map(MovementButtonTags());
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		FGSTN_ResolveParams Params;
		Params.Buttons = &Map;
		Params.Now = History.GetLatestFrame();
		const FGSTN_SlotResolution Resolution = FGSTN_SlotResolver::Resolve(Pointers, History, Params, Settings, [](int32) { return true; });
		return Resolution.SlotIndex == INDEX_NONE ? FString() : Slots[Resolution.SlotIndex].Command.ToString();
	}

	/** Up of a frame whose gravity is tilted by TiltDegrees around X from world -Z */
	FVector TiltedUp(float TiltDegrees)
	{
		return FRotator(0.f, 0.f, TiltDegrees).RotateVector(FVector::UpVector);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DashTiltedGravityTest, "GSTNCombat.Core.Dash.TiltedGravityDirection", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DashTiltedGravityTest::RunTest(const FString& Parameters)
{
	FGSTN_DashMoveData Move;
	Move.ForwardSpeed = 1000.f;
	Move.UpSpeed = 200.f;

	const float Tilts[] = { 0.f, 30.f, 90.f, 180.f };
	for (const float Tilt : Tilts)
	{
		const FVector Up = TiltedUp(Tilt);
		// A facing with a component along up: the dash direction must ignore it
		const FVector Facing = (FVector::ForwardVector + Up * 0.5f).GetSafeNormal();
		const FVector Forward = FGSTN_DashMath::HorizontalForward(Up, Facing, FVector::RightVector);
		const FString What = FString::Printf(TEXT("tilt %.0f"), Tilt);

		TestTrue(*(What + TEXT(": forward is a unit vector")), FMath::IsNearlyEqual(Forward.Size(), 1.0, 1e-4));
		TestTrue(*(What + TEXT(": forward is perpendicular to up")), FMath::Abs(Forward.Dot(Up)) < 1e-4);

		const FVector Impulse = FGSTN_DashMath::ImpulseVelocity(Up, Forward, Move);
		TestTrue(*(What + TEXT(": the up part is the up speed")), FMath::IsNearlyEqual(Impulse.Dot(Up), 200.0, 1e-2));
		TestTrue(*(What + TEXT(": the horizontal part is the forward speed")), FMath::IsNearlyEqual(Impulse.Dot(Forward), 1000.0, 1e-2));
		TestTrue(*(What + TEXT(": nothing sideways")), (Impulse - Forward * 1000.0 - Up * 200.0).IsNearlyZero(1e-2));
	}

	// Facing along up has no horizontal part: the fallback is projected instead, and with no fallback either there is no direction
	const FVector Up = TiltedUp(30.f);
	const FVector Fallback = FGSTN_DashMath::HorizontalForward(Up, Up, FVector::ForwardVector);
	TestTrue(TEXT("a facing along up uses the fallback"), Fallback.Size() > 0.99 && FMath::Abs(Fallback.Dot(Up)) < 1e-4);
	TestTrue(TEXT("no facing and no fallback gives no direction"), FGSTN_DashMath::HorizontalForward(Up, Up, Up).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeArcTest, "GSTNCombat.Core.Dodge.ArcInTiltedFrame", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeArcTest::RunTest(const FString& Parameters)
{
	const FGSTN_DashMoveData Move = FGSTN_MoveTimelines::GroundDodgeMove();
	const float Gravity = FGSTN_MoveTimelines::NominalGravity;
	const float TickRate = 60.f;
	const FGSTN_ArcPrediction Arc = FGSTN_DashMath::PredictArc(Move, Gravity, TickRate);

	TestTrue(TEXT("a low arc: under half a second of air above a metre"), Arc.AirTimeSeconds > 0.4f && Arc.AirTimeSeconds < 0.8f && Arc.ApexHeight < 100.f);
	TestTrue(TEXT("range is forward speed times air time"), FMath::IsNearlyEqual(Arc.Range, Move.ForwardSpeed * Arc.AirTimeSeconds, 1e-2f));
	TestEqual(TEXT("air frames are the air time rounded up"), Arc.AirFrames, FMath::CeilToInt32(Arc.AirTimeSeconds * TickRate));

	// Integrate the arc with the fixed tick in a tilted frame and in the world frame: both land where the prediction says, in frame coordinates
	const float Tilts[] = { 0.f, 30.f };
	for (const float Tilt : Tilts)
	{
		const FVector Up = TiltedUp(Tilt);
		const FVector Forward = FGSTN_DashMath::HorizontalForward(Up, FVector::ForwardVector, FVector::RightVector);
		FVector Velocity = FGSTN_DashMath::ImpulseVelocity(Up, Forward, Move);
		FVector Position = FVector::ZeroVector;
		const float Dt = 1.f / TickRate;
		int32 Frames = 0;
		double Apex = 0.0;
		do
		{
			Velocity += -Up * Gravity * Dt;
			Position += Velocity * Dt;
			Apex = FMath::Max(Apex, Position.Dot(Up));
			++Frames;
		} while (Position.Dot(Up) > 0.0 && Frames < 600);

		const FString What = FString::Printf(TEXT("tilt %.0f"), Tilt);
		TestTrue(*(What + TEXT(": lands within a frame of the prediction")), FMath::Abs(Frames - Arc.AirFrames) <= 1);
		TestTrue(*(What + TEXT(": the apex follows the local up")), FMath::Abs(Apex - Arc.ApexHeight) < 3.0);
		TestTrue(*(What + TEXT(": the range along forward")), FMath::Abs(Position.Dot(Forward) - Arc.Range) < Move.ForwardSpeed * Dt * 1.5f);
		TestTrue(*(What + TEXT(": nothing sideways")), FMath::Abs(Position.Dot(FVector::CrossProduct(Up, Forward))) < 1e-2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeMomentumTest, "GSTNCombat.Core.Dodge.MomentumModes", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeMomentumTest::RunTest(const FString& Parameters)
{
	const FVector Prior(300.0, 0.0, -100.0);
	const FVector Impulse(0.0, 500.0, 50.0);
	using EMix = EGSTN_MoveMixMode;
	using EMom = EGSTN_MomentumMode;
	TestEqual(TEXT("discard drops the prior velocity"), FGSTN_DashMath::ResolveVelocity(Prior, Impulse, EMix::Additive, EMom::Discard), Impulse);
	TestEqual(TEXT("preserve + additive adds"), FGSTN_DashMath::ResolveVelocity(Prior, Impulse, EMix::Additive, EMom::Preserve), Prior + Impulse);
	TestEqual(TEXT("preserve + override replaces"), FGSTN_DashMath::ResolveVelocity(Prior, Impulse, EMix::Override, EMom::Preserve), Impulse);
	TestEqual(TEXT("clear at end + additive adds"), FGSTN_DashMath::ResolveVelocity(Prior, Impulse, EMix::Additive, EMom::ClearAtEnd), Prior + Impulse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveTimelinesTest, "GSTNCombat.Core.Dodge.DefaultTimelines", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MoveTimelinesTest::RunTest(const FString& Parameters)
{
	const float TickRate = 60.f;
	const FGSTN_DashMoveData GroundMove = FGSTN_MoveTimelines::GroundDodgeMove();
	const FGSTN_MoveTimelineParams Ground = FGSTN_MoveTimelines::GroundDodgeParams(GroundMove, TickRate);
	const FGSTN_ArcPrediction Arc = FGSTN_DashMath::PredictArc(GroundMove, FGSTN_MoveTimelines::NominalGravity, TickRate);

	const FGSTN_CombatTimelineData Timelines[] = {
		FGSTN_MoveTimelines::Make(FGSTN_MoveTimelines::DashParams()),
		FGSTN_MoveTimelines::Make(Ground),
		FGSTN_MoveTimelines::Make(FGSTN_MoveTimelines::AirDodgeParams()),
	};
	const TCHAR* Names[] = { TEXT("dash"), TEXT("ground dodge"), TEXT("air dodge") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TArray<FGSTN_TimelineMessage> Messages;
		FGSTN_TimelineValidation::Validate(Timelines[Index], FGSTN_TimelineValidationContext(), Messages);
		for (const FGSTN_TimelineMessage& Message : Messages)
		{
			if (Message.bError)
			{
				AddError(FString::Printf(TEXT("%s: %s"), Names[Index], *Message.Text));
			}
		}
	}

	// The feet window of the ground dodge: from take-off to 40 % of the airborne frames, as data
	const FGSTN_CombatTimelineData& GroundData = Timelines[1];
	const FGSTN_TimelineTrack_HurtboxModifier* Hurtbox = nullptr;
	const FGSTN_TimelineTrack_Movement* Movement = nullptr;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : GroundData.Tracks)
	{
		if (const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>())
		{
			if (Base->GetKind() == EGSTN_TimelineTrackKind::HurtboxModifier)
			{
				Hurtbox = static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(Base);
			}
			else if (Base->GetKind() == EGSTN_TimelineTrackKind::Movement)
			{
				Movement = static_cast<const FGSTN_TimelineTrack_Movement*>(Base);
			}
		}
	}
	if (TestNotNull(TEXT("the ground dodge has a hurtbox window"), Hurtbox))
	{
		TestEqual(TEXT("it starts at take-off"), Hurtbox->StartFrame, 0);
		TestEqual(TEXT("it ends at 40 % of the airborne frames"), Hurtbox->EndFrame, FMath::RoundToInt32(Arc.AirFrames * 0.4f));
		TestEqual(TEXT("it addresses the feet group, not the whole body"), Hurtbox->Part.GroupName, FName(TEXT("Feet")));
		TestTrue(TEXT("it ends before the landing"), Hurtbox->EndFrame < Arc.AirFrames);
	}
	if (TestNotNull(TEXT("the ground dodge has a movement track"), Movement))
	{
		TestTrue(TEXT("it ends on landing"), Movement->bCancelOnLand);
		TestEqual(TEXT("and lands straight into actionable: no landing lag"), Movement->LandingLagFrames, 0);
	}
	TestTrue(TEXT("the timeline has room for a heavier gravity"), GroundData.TotalFrames > Arc.AirFrames);

	// The air dodge is the dive roll: cancelled by landing with a short lag
	const FGSTN_MoveTimelineParams Air = FGSTN_MoveTimelines::AirDodgeParams();
	TestTrue(TEXT("the air dodge ends on landing"), Air.bCancelOnLand);
	TestTrue(TEXT("with a landing lag"), Air.LandingLagFrames > 0);
	TestTrue(TEXT("the dash has no invincibility"), FGSTN_MoveTimelines::DashParams().InvincibleFrames == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MovementSlotsCommandsTest, "GSTNCombat.Core.MovementSlots.PressCommands", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MovementSlotsCommandsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_MovementSlots::Build(false, 12);
	TestEqual(TEXT("dash, dodge, tap and hold"), Slots.Num(), 4);
	TestEqual(TEXT("a press of the dodge button is the dodge"), CommandFor(*this, Slots, TEXT("5:+Dodge"), *Settings), GSTNCommandTags::Defence_Dodge.GetTag().ToString());
	TestEqual(TEXT("a press of the sprint button is the dash"), CommandFor(*this, Slots, TEXT("5:+Sprint"), *Settings), GSTNCommandTags::Movement_Dash.GetTag().ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MovementSlotsTapHoldTest, "GSTNCombat.Core.MovementSlots.InteractTapHold", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MovementSlotsTapHoldTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FString Grab = GSTNCommandTags::Interact_Grab.GetTag().ToString();
	const FString PickUp = GSTNCommandTags::Interact_PickUp.GetTag().ToString();

	const TArray<FGSTN_InputSlotSpec> Default = UGSTN_MovementSlots::Build(false, 12);
	TestEqual(TEXT("default: a short press is a grab"), CommandFor(*this, Default, TEXT("5:+Interact 8:-Interact"), *Settings), Grab);
	TestEqual(TEXT("default: the longest tap is still a grab"), CommandFor(*this, Default, TEXT("5:+Interact 16:-Interact"), *Settings), Grab);
	TestEqual(TEXT("default: a press of the hold length is a pick up"), CommandFor(*this, Default, TEXT("5:+Interact 17:-Interact"), *Settings), PickUp);
	TestEqual(TEXT("default: a long press is a pick up"), CommandFor(*this, Default, TEXT("5:+Interact 40:-Interact"), *Settings), PickUp);
	TestEqual(TEXT("default: a button still down fires nothing yet, so the tap is not swallowed by a pending hold"), CommandFor(*this, Default, TEXT("5:+Interact"), *Settings), FString());
	TestEqual(TEXT("default: a tap right after a held-down press of another run is a grab"), CommandFor(*this, Default, TEXT("5:+Interact 30:-Interact 33:+Interact 36:-Interact"), *Settings), Grab);

	// The swapped data set flips the two with no other change
	const TArray<FGSTN_InputSlotSpec> Swapped = UGSTN_MovementSlots::Build(true, 12);
	TestEqual(TEXT("swapped: a short press is a pick up"), CommandFor(*this, Swapped, TEXT("5:+Interact 8:-Interact"), *Settings), PickUp);
	TestEqual(TEXT("swapped: a long press is a grab"), CommandFor(*this, Swapped, TEXT("5:+Interact 40:-Interact"), *Settings), Grab);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MovementSlotsValidTest, "GSTNCombat.Core.MovementSlots.NoConflicts", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MovementSlotsValidTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	for (const bool bSwap : { false, true })
	{
		const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_MovementSlots::Build(bSwap, 12);
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			TestTrue(TEXT("valid pattern"), Spec.Pattern.IsStructurallyValid());
			Pointers.Add(&Spec);
		}
		for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(Pointers, *Settings))
		{
			AddError(Conflict.Message);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
