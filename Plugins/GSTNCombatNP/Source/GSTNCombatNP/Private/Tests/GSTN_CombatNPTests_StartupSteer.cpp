// Automation tests of the startup steering turn (plan precisione-weakpoint 5, slice 3): the per-frame decision of the combat tick (a SetFacing
// request while the aim stick turns a move in its startup) and the Mover instant effect that applies it. Pure: a sync core and a Mover sync
// state built by hand, no world.

#include "Misc/AutomationTest.h"
#include "Movement/GSTN_SetFacingEffect.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatHitState.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNSteerTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A 30-frame move whose only hitbox window is [10, 14): startup 0..9 */
	FGSTN_CombatTimelineData MakeMove(float MaxSteer)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 26;
		Data.MaxSteerDegrees = MaxSteer;
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 10;
		Track.EndFrame = 14;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		return Data;
	}

	/** A core running timeline 0 from StartFrame, its move origin facing StartYaw */
	FGSTN_CombatSyncCore MakeCore(int32 StartFrame, float StartYaw)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 0, EGSTN_ActivationSource::Command);
		Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FRotator(0.0, StartYaw, 0.0), FVector::ZeroVector));
		return Core;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StartupSteerDecisionTest, "GSTNCombat.NP.Weakpoint.StartupSteerTurn", GSTNSteerTest::Flags)
bool FGSTN_StartupSteerDecisionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSteerTest;
	using S = FGSTN_StartupSteer;
	const FGSTN_CombatTimelineData Move = MakeMove(20.f);
	const FVector2D Right(1.0, 0.0);
	int16 Arg = 0;

	// Start frame 100, facing 30: stick right with the camera at 0 aims at 90, clamped to 30 + 20
	FGSTN_CombatSyncCore Core = MakeCore(100, 30.f);
	TestTrue(TEXT("first frame steers"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 100, &Move, Right, 0.f, 30.f, true, Arg));
	TestEqual(TEXT("clamped to start + MaxSteer"), S::DequantizeYaw(Arg), 50.f, 0.01f);

	// Same command later in the startup, pose already turned: nothing to ask. Same command from a pose that is elsewhere: the same target
	TestFalse(TEXT("already facing the target"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 105, &Move, Right, 0.f, 50.f, true, Arg));
	TestTrue(TEXT("not accumulated"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 105, &Move, Right, 0.f, 40.f, true, Arg) && FMath::IsNearlyEqual(S::DequantizeYaw(Arg), 50.f, 0.01f));

	// The window: timeline frame 9 is the last; paused frames (hitstop) push it later
	TestTrue(TEXT("last startup frame"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 109, &Move, Right, 0.f, 30.f, true, Arg));
	TestFalse(TEXT("first active frame does not steer"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 110, &Move, Right, 0.f, 30.f, true, Arg));
	Core.TimelinePausedFrames = 3;
	TestTrue(TEXT("paused frames move the window"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 112, &Move, Right, 0.f, 30.f, true, Arg));
	Core.TimelinePausedFrames = 0;

	// Nothing steers: centred stick, MaxSteerDegrees 0, a recoil, no move origin, no timeline
	TestFalse(TEXT("dead zone"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 101, &Move, FVector2D(0.1, 0.1), 0.f, 30.f, true, Arg));
	const FGSTN_CombatTimelineData Fixed = MakeMove(0.f);
	TestFalse(TEXT("MaxSteerDegrees 0"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 101, &Fixed, Right, 0.f, 30.f, true, Arg));
	FGSTN_CombatSyncCore Recoil = Core;
	Recoil.RecoilStartFrame = 101;
	TestFalse(TEXT("recoil"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Recoil, 101, &Move, Right, 0.f, 30.f, true, Arg));
	FGSTN_CombatSyncCore NoOrigin = Core;
	NoOrigin.Hit.MoveOrigin = FGSTN_MoveOrigin();
	TestFalse(TEXT("no move origin"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(NoOrigin, 101, &Move, Right, 0.f, 30.f, true, Arg));
	TestFalse(TEXT("idle"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(FGSTN_CombatSyncCore(), 101, &Move, Right, 0.f, 30.f, true, Arg));

	// Across the seam: facing 175, stick right aims at -95 (through 180): 175 + 20 = 195 = -165
	FGSTN_CombatSyncCore Seam = MakeCore(100, 175.f);
	TestTrue(TEXT("seam steers"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Seam, 100, &Move, Right, 175.f, 175.f, true, Arg));
	TestEqual(TEXT("seam target"), FMath::FindDeltaAngleDegrees(S::DequantizeYaw(Arg), -165.f), 0.f, 0.01f);

	// The request carries the yaw over the wire with its sign
	FGSTN_CombatSyncCore Wire = MakeCore(100, 30.f);
	Wire.Requests.Add(EGSTN_RequestType::SetFacing, 100, S::QuantizeYaw(-120.f));
	FBitWriter Writer(0, true);
	Wire.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_CombatSyncCore Loaded;
	Loaded.NetSerialize(Reader);
	const FGSTN_CombatRequest* Found = Loaded.Requests.Find(EGSTN_RequestType::SetFacing, 100);
	TestTrue(TEXT("request on the wire"), !Reader.IsError() && Found != nullptr && FMath::IsNearlyEqual(S::DequantizeYaw(Found->Arg), -120.f, 0.01f));
	TestFalse(TEXT("loaded state reconciles"), Loaded.ShouldReconcile(Wire));
	return true;
}

// Slice 3b (owner decision 2026-11-10): the right stick turns the character only while the reticle is on, and then it wins over the left stick
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReticleGatesSteerTest, "GSTNCombat.NP.Weakpoint.ReticleGatesSteer", GSTNSteerTest::Flags)
bool FGSTN_ReticleGatesSteerTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSteerTest;
	using L = FGSTN_CombatTimelineLogic;
	const FGSTN_CombatTimelineData Move = MakeMove(20.f);
	const FGSTN_CombatSyncCore Core = MakeCore(100, 30.f);
	const FVector2D Right(1.0, 0.0);
	int16 Arg = 0;
	float Yaw = 0.f;

	// Reticle off: no turn request and nothing held, so the left stick keeps turning the character as before
	TestFalse(TEXT("off: no steer"), L::ComputeStartupSteer(Core, 101, &Move, Right, 0.f, 30.f, false, Arg));
	TestFalse(TEXT("off: no hold"), L::ResolveSteerHoldYaw(Core, 101, &Move, Right, 0.f, false, Yaw));

	// Reticle on: the steer turns, and the held facing is the same clamped yaw
	TestTrue(TEXT("on: steers"), L::ComputeStartupSteer(Core, 101, &Move, Right, 0.f, 30.f, true, Arg));
	TestTrue(TEXT("on: holds the steered yaw"), L::ResolveSteerHoldYaw(Core, 101, &Move, Right, 0.f, true, Yaw) && FMath::IsNearlyEqual(Yaw, 50.f, 0.01f));
	TestTrue(TEXT("on, stick centred: holds the start yaw"), L::ResolveSteerHoldYaw(Core, 101, &Move, FVector2D::ZeroVector, 0.f, true, Yaw)
		&& FMath::IsNearlyEqual(Yaw, 30.f, 0.01f));
	TestFalse(TEXT("on, stick centred: no turn request"), L::ComputeStartupSteer(Core, 101, &Move, FVector2D::ZeroVector, 0.f, 0.f, true, Arg));
	TestFalse(TEXT("on, active frames: nothing held"), L::ResolveSteerHoldYaw(Core, 110, &Move, Right, 0.f, true, Yaw));
	TestFalse(TEXT("on, idle: nothing held"), L::ResolveSteerHoldYaw(FGSTN_CombatSyncCore(), 101, &Move, Right, 0.f, true, Yaw));

	// The held yaw replaces the left stick's orientation intent and keeps its move input
	FMoverInputCmdContext MoverCmd;
	FCharacterDefaultInputs& Inputs = MoverCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector(0.0, -1.0, 0.0));
	Inputs.OrientationIntent = FVector(0.0, -1.0, 0.0);
	TestTrue(TEXT("intent replaced"), UGSTN_CombatSimComponent::HoldOrientationIntent(MoverCmd, 50.f));
	const FCharacterDefaultInputs* After = MoverCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	TestTrue(TEXT("intent faces the held yaw"), After && After->OrientationIntent.Equals(FRotator(0.0, 50.0, 0.0).Vector(), 1e-4));
	TestTrue(TEXT("move input kept"), After && After->GetMoveInput().Equals(FVector(0.0, -1.0, 0.0), 1e-4));
	FMoverInputCmdContext NoInputs;
	TestFalse(TEXT("no character inputs: nothing to replace"), UGSTN_CombatSimComponent::HoldOrientationIntent(NoInputs, 50.f));

	// The reticle bit rides in the predicted command
	for (const bool bOn : { true, false })
	{
		FGSTN_InputCmd Cmd;
		Cmd.AimStickX = 90;
		Cmd.bPrecisionAim = bOn;
		FBitWriter Writer(0, true);
		FGSTN_InputCmd(Cmd).NetSerialize(FNetSerializeParams(Writer));
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_InputCmd Loaded;
		Loaded.bPrecisionAim = !bOn;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		TestTrue(FString::Printf(TEXT("reticle bit %d on the wire"), bOn ? 1 : 0), !Reader.IsError() && Loaded.bPrecisionAim == bOn && Loaded.AimStickX == 90);
	}
	return true;
}

// M11 L9 (owner 2026-11-12): aim handling. The cone stays the move's MaxSteerDegrees; SteerRateDegPerSecond says how fast the facing follows
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SteerAimHandlingTest, "GSTNCombat.NP.Weakpoint.SteerAimHandling", GSTNSteerTest::Flags)
bool FGSTN_SteerAimHandlingTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSteerTest;
	using S = FGSTN_StartupSteer;
	using L = FGSTN_CombatTimelineLogic;
	constexpr float Step = 0.016f;

	TestEqual(TEXT("rate 0: at once"), S::RateLimitYaw(0.f, 40.f, 0.f, Step), 40.f, 0.001f);
	TestEqual(TEXT("step 0: at once"), S::RateLimitYaw(0.f, 40.f, 250.f, 0.f), 40.f, 0.001f);
	TestEqual(TEXT("250 deg/s for 16 ms is 4 degrees"), S::RateLimitYaw(0.f, 40.f, 250.f, Step), 4.f, 0.001f);
	TestEqual(TEXT("the other way"), S::RateLimitYaw(0.f, -40.f, 250.f, Step), -4.f, 0.001f);
	TestEqual(TEXT("the short way across the seam"), S::RateLimitYaw(178.f, -178.f, 250.f, Step), -178.f, 0.001f);
	TestEqual(TEXT("close enough: lands on the target"), S::RateLimitYaw(38.f, 40.f, 250.f, Step), 40.f, 0.001f);

	FGSTN_CombatTimelineData Heavy = MakeMove(20.f);
	Heavy.SteerRateDegPerSecond = 250.f;
	const FGSTN_CombatSyncCore Core = MakeCore(100, 30.f);
	const FVector2D Right(1.0, 0.0);
	int16 Arg = 0;
	TestTrue(TEXT("heavy move steers"), L::ComputeStartupSteer(Core, 100, &Heavy, Right, 0.f, 30.f, true, Arg, Step));
	TestEqual(TEXT("one step toward the cone edge"), S::DequantizeYaw(Arg), 34.f, 0.01f);
	TestTrue(TEXT("from the pose it reached"), L::ComputeStartupSteer(Core, 101, &Heavy, Right, 0.f, 34.f, true, Arg, Step));
	TestEqual(TEXT("next step"), S::DequantizeYaw(Arg), 38.f, 0.01f);
	TestTrue(TEXT("never past the cone"), L::ComputeStartupSteer(Core, 102, &Heavy, Right, 0.f, 48.f, true, Arg, Step));
	TestEqual(TEXT("the edge"), S::DequantizeYaw(Arg), 50.f, 0.01f);
	TestTrue(TEXT("no step given: at once, as before L9"), L::ComputeStartupSteer(Core, 100, &Heavy, Right, 0.f, 30.f, true, Arg));
	TestEqual(TEXT("the edge at once"), S::DequantizeYaw(Arg), 50.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SetFacingEffectTest, "GSTNCombat.NP.Weakpoint.SetFacingEffect", GSTNSteerTest::Flags)
bool FGSTN_SetFacingEffectTest::RunTest(const FString& Parameters)
{
	const FRotator Turned = FGSTN_SetFacingEffect::MakeOrientation(FRotator(5.0, 10.0, -3.0), 200.f);
	TestEqual(TEXT("pitch kept"), Turned.Pitch, 5.0);
	TestEqual(TEXT("roll kept"), Turned.Roll, -3.0);
	TestEqual(TEXT("yaw normalized"), Turned.Yaw, -160.0, 1e-4);

	FMoverSyncState State;
	FMoverDefaultSyncState& Default = State.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	Default.SetTransforms_WorldSpace(FVector(100.0, 200.0, 90.0), FRotator(0.0, 30.0, 0.0), FVector(300.0, 0.0, 0.0), FVector::ZeroVector);
	FGSTN_SetFacingEffect Effect;
	Effect.YawDeg = 50.f;
	FApplyMovementEffectParams Params;
	TestTrue(TEXT("applied"), Effect.ApplyMovementEffect(Params, State));
	const FMoverDefaultSyncState* After = State.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	TestTrue(TEXT("yaw set"), After && FMath::IsNearlyEqual(After->GetOrientation_WorldSpace().Yaw, 50.0, 1e-3));
	TestTrue(TEXT("location kept"), After && After->GetLocation_WorldSpace().Equals(FVector(100.0, 200.0, 90.0), 1e-3));
	TestTrue(TEXT("velocity kept"), After && After->GetVelocity_WorldSpace().Equals(FVector(300.0, 0.0, 0.0), 1e-3));

	FMoverSyncState Empty;
	TestFalse(TEXT("no default sync state: nothing applied"), Effect.ApplyMovementEffect(Params, Empty));
	return true;
}

#endif
