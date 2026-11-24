// Automation tests of the movement mixer that keeps a velocity override visible to the post-generation hook (vertical-only launches,
// 2026-11-24). No world: the mixer is a plain object fed hand built proposed moves, the same calls the Mover state machine makes.

#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "Movement/GSTN_MovementMixer.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** What the falling mode proposes: its own (additive) move, a little horizontal drift and the gravity step */
	FProposedMove FallingModeMove()
	{
		FProposedMove Mode;
		Mode.MixMode = EMoveMixMode::AdditiveVelocity;
		Mode.LinearVelocity = FVector(12.0, 0.0, -63.0);
		return Mode;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MixerKeepsVelocityOverrideTest, "GSTNCombat.NP.Hits.Launch.MixerKeepsVelocityOverride", GSTN_NP_TEST_FLAGS)
bool FGSTN_MixerKeepsVelocityOverrideTest::RunTest(const FString& Parameters)
{
	// The knockback's launch (or relaunch) frame: the combined layered move overrides the velocity
	FProposedMove Launch;
	Launch.MixMode = EMoveMixMode::OverrideVelocity;
	Launch.LinearVelocity = FVector(2868.0, 0.0, 1593.0);

	// The stock mixer loses the mix mode: this is what hid the launch from the air momentum
	{
		UMovementMixer* Stock = NewObject<UMovementMixer>();
		FProposedMove Combined = FallingModeMove();
		Stock->MixProposedMoves(Launch, FVector::UpVector, Combined);
		TestTrue(TEXT("stock: velocity replaced"), Combined.LinearVelocity.Equals(Launch.LinearVelocity));
		TestEqual(TEXT("stock: the mode's mix mode stays (the cause)"), Combined.MixMode, EMoveMixMode::AdditiveVelocity);
	}

	UGSTN_MovementMixer* Mixer = NewObject<UGSTN_MovementMixer>();
	FProposedMove Combined = FallingModeMove();
	Mixer->MixProposedMoves(Launch, FVector::UpVector, Combined);
	TestTrue(TEXT("velocity replaced, horizontal kept"), Combined.LinearVelocity.Equals(Launch.LinearVelocity));
	TestEqual(TEXT("the combined move says the velocity is overridden"), Combined.MixMode, EMoveMixMode::OverrideVelocity);

	// Additive layered moves (a knockback's later frames, its drag and gravity scale) leave the frame to the mode and the hook
	FProposedMove Drag;
	Drag.MixMode = EMoveMixMode::AdditiveVelocity;
	Drag.LinearVelocity = FVector(-40.0, 0.0, -20.0);
	FProposedMove Additive = FallingModeMove();
	Mixer->MixProposedMoves(Drag, FVector::UpVector, Additive);
	TestEqual(TEXT("additive stays additive"), Additive.MixMode, EMoveMixMode::AdditiveVelocity);
	TestTrue(TEXT("additive adds"), Additive.LinearVelocity.Equals(FVector(-28.0, 0.0, -83.0)));

	// The whole-move overrides were already carried by the stock mixer; unchanged
	FProposedMove Freeze;
	Freeze.MixMode = EMoveMixMode::OverrideAll;
	FProposedMove Frozen = FallingModeMove();
	Mixer->MixProposedMoves(Freeze, FVector::UpVector, Frozen);
	TestEqual(TEXT("override all carried"), Frozen.MixMode, EMoveMixMode::OverrideAll);
	TestTrue(TEXT("override all: no velocity"), Frozen.LinearVelocity.IsZero());
	return true;
}

#endif
