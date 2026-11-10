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
	TestTrue(TEXT("first frame steers"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 100, &Move, Right, 0.f, 30.f, Arg));
	TestEqual(TEXT("clamped to start + MaxSteer"), S::DequantizeYaw(Arg), 50.f, 0.01f);

	// Same command later in the startup, pose already turned: nothing to ask. Same command from a pose that is elsewhere: the same target
	TestFalse(TEXT("already facing the target"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 105, &Move, Right, 0.f, 50.f, Arg));
	TestTrue(TEXT("not accumulated"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 105, &Move, Right, 0.f, 40.f, Arg) && FMath::IsNearlyEqual(S::DequantizeYaw(Arg), 50.f, 0.01f));

	// The window: timeline frame 9 is the last; paused frames (hitstop) push it later
	TestTrue(TEXT("last startup frame"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 109, &Move, Right, 0.f, 30.f, Arg));
	TestFalse(TEXT("first active frame does not steer"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 110, &Move, Right, 0.f, 30.f, Arg));
	Core.TimelinePausedFrames = 3;
	TestTrue(TEXT("paused frames move the window"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 112, &Move, Right, 0.f, 30.f, Arg));
	Core.TimelinePausedFrames = 0;

	// Nothing steers: centred stick, MaxSteerDegrees 0, a recoil, no move origin, no timeline
	TestFalse(TEXT("dead zone"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 101, &Move, FVector2D(0.1, 0.1), 0.f, 30.f, Arg));
	const FGSTN_CombatTimelineData Fixed = MakeMove(0.f);
	TestFalse(TEXT("MaxSteerDegrees 0"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Core, 101, &Fixed, Right, 0.f, 30.f, Arg));
	FGSTN_CombatSyncCore Recoil = Core;
	Recoil.RecoilStartFrame = 101;
	TestFalse(TEXT("recoil"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Recoil, 101, &Move, Right, 0.f, 30.f, Arg));
	FGSTN_CombatSyncCore NoOrigin = Core;
	NoOrigin.Hit.MoveOrigin = FGSTN_MoveOrigin();
	TestFalse(TEXT("no move origin"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(NoOrigin, 101, &Move, Right, 0.f, 30.f, Arg));
	TestFalse(TEXT("idle"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(FGSTN_CombatSyncCore(), 101, &Move, Right, 0.f, 30.f, Arg));

	// Across the seam: facing 175, stick right aims at -95 (through 180): 175 + 20 = 195 = -165
	FGSTN_CombatSyncCore Seam = MakeCore(100, 175.f);
	TestTrue(TEXT("seam steers"), FGSTN_CombatTimelineLogic::ComputeStartupSteer(Seam, 100, &Move, Right, 175.f, 175.f, Arg));
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
