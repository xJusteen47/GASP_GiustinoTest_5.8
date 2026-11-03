// Automation tests of the platform shapes with behaviours (traversal-surfaces.md 4.1, 4.2, 4.4; plan T3).

#include "GSTN_PlatformTestRig.h"

#include "Engine/HitResult.h"
#include "GSTN_PlatformCollision.h"
#include "GSTN_PlatformColliders.h"
#include "GSTN_PlatformSettings.h"
#include "GSTN_PlatformShapeStates.h"
#include "GSTN_PlatformTags.h"
#include "GSTN_ScriptedPlatformComponent.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPlatformTest;

namespace GSTNPlatformShapesTest
{
	FGSTN_PlatformShape MakeSurfaceShape(uint8 Id, std::initializer_list<FGameplayTag> Behaviours, const FVector& LocalLocation = FVector::ZeroVector,
		EGSTN_PlatformShapeType Type = EGSTN_PlatformShapeType::Box)
	{
		FGSTN_PlatformShape Shape = MakeShape(Id, Type, Type == EGSTN_PlatformShapeType::Capsule ? FVector(30.0, 0.0, 80.0) : FVector(50.0, 50.0, 10.0), LocalLocation);
		for (const FGameplayTag& Tag : Behaviours)
		{
			Shape.Behaviour.Behaviours.AddTag(Tag);
		}
		return Shape;
	}

	bool IsSolid(const UPrimitiveComponent* Collider)
	{
		return Collider && Collider->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	/** A small sphere swept down onto a point from 100 cm above, on the Mover capsule's channel */
	bool SweepHits(UWorld* World, const FVector& Point, const UPrimitiveComponent* Expected)
	{
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Point + FVector(0.0, 0.0, 100.0), Point - FVector(0.0, 0.0, 100.0), FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(5.f));
		return bHit && Hit.GetComponent() == Expected;
	}

	FHitResult MakeHit(UPrimitiveComponent* Component, const FVector& Point, const FVector& Normal)
	{
		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.Component = Component;
		Hit.HitObjectHandle = FActorInstanceHandle(Component->GetOwner());
		Hit.Location = Hit.ImpactPoint = Point;
		Hit.Normal = Hit.ImpactNormal = Normal;
		return Hit;
	}

	bool ContainsText(const TArray<FText>& Texts, const TCHAR* Needle)
	{
		return Texts.ContainsByPredicate([Needle](const FText& Text) { return Text.ToString().Contains(Needle); });
	}
}

using namespace GSTNPlatformShapesTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformMaskDisablesShapesTest, "GSTNPlatform.Shapes.MaskDisablesShapes", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformMaskDisablesShapesTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = {
		MakeSurfaceShape(0, { GSTNSurfaceTags::Behaviour_Stand }, FVector(0.0, 0.0, 0.0)),
		MakeSurfaceShape(1, { GSTNSurfaceTags::Behaviour_Stand }, FVector(300.0, 0.0, 0.0)),
		MakeSurfaceShape(2, { GSTNSurfaceTags::Behaviour_Stand }, FVector(600.0, 0.0, 0.0)) };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec, FVector(0.0, 0.0, 1000.0));
	UGSTN_PlatformSubsystem* Subsystem = Rig.Subsystem();
	UPrimitiveComponent* Shape1 = Platform->GetShapeComponent(1);
	const FVector Shape1Top = FVector(300.0, 0.0, 1010.0);

	Subsystem->ApplyPosePoint(MakeContext(10));
	TestTrue(TEXT("All shapes solid at first"), IsSolid(Platform->GetShapeComponent(0)) && IsSolid(Shape1) && IsSolid(Platform->GetShapeComponent(2)));
	TestTrue(TEXT("A sweep finds shape 1"), SweepHits(Rig.World, Shape1Top, Shape1));

	// Off at once: applied at the pose point, not when asked
	Platform->SetShapeMask(0b101, EGSTN_PlatformShapeState::Inactive);
	TestTrue(TEXT("Unchanged until the pose point"), IsSolid(Shape1));
	Subsystem->ApplyPosePoint(MakeContext(11));
	TestFalse(TEXT("Shape 1 off at the next frame"), IsSolid(Shape1));
	TestTrue(TEXT("Shapes 0 and 2 still solid"), IsSolid(Platform->GetShapeComponent(0)) && IsSolid(Platform->GetShapeComponent(2)));
	TestTrue(TEXT("State Inactive"), Platform->GetShapeState(1) == EGSTN_PlatformShapeState::Inactive);
	TestTrue(TEXT("Tag Platform.State.Inactive"), Platform->GetShapeStateTag(1) == GSTNPlatformTags::State_Inactive.GetTag());
	TestFalse(TEXT("The sweep no longer finds shape 1 (nothing else reads the mask)"), SweepHits(Rig.World, Shape1Top, Shape1));

	// A re-simulation of frame 10 finds the shape solid, as the first simulation of frame 10 did
	Subsystem->ApplyPosePoint(MakeContext(10));
	TestTrue(TEXT("Replayed frame 10: solid again"), IsSolid(Shape1));
	Subsystem->ApplyPosePoint(MakeContext(11));
	TestFalse(TEXT("Replayed frame 11: off again"), IsSolid(Shape1));

	// Back on: Active at once from the next frame (gaining a surface never hurts)
	Platform->SetShapeMask(0xFF);
	Subsystem->ApplyPosePoint(MakeContext(12));
	TestTrue(TEXT("Shape 1 back at frame 12"), IsSolid(Shape1));
	TestTrue(TEXT("State Active"), Platform->GetShapeState(1) == EGSTN_PlatformShapeState::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformCollisionDerivedTest, "GSTNPlatform.Shapes.CollisionDerivedFromBehaviour", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformCollisionDerivedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSurfaceTags;

	// The table itself, with distinct channels so each rule shows
	const TArray<TEnumAsByte<ECollisionChannel>> Mover = { ECC_Pawn };
	const TArray<TEnumAsByte<ECollisionChannel>> Probe = { ECC_GameTraceChannel1 };
	const TArray<TEnumAsByte<ECollisionChannel>> Extra = { ECC_GameTraceChannel2 };
	const TArray<FGSTN_PlatformCollisionRule> Rules = GSTNPlatformCollision::MakeDefaultRules(Mover, Probe, Extra);
	auto Responses = [&Rules](std::initializer_list<FGameplayTag> Tags, bool& bOutAny)
	{
		FGSTN_SurfaceBehaviour Behaviour;
		for (const FGameplayTag& Tag : Tags)
		{
			Behaviour.Behaviours.AddTag(Tag);
		}
		FCollisionResponseContainer Out;
		bOutAny = GSTNPlatformCollision::DeriveResponses(Behaviour, Rules, Out);
		return Out;
	};
	bool bAny = false;
	FCollisionResponseContainer R = Responses({ Behaviour_Stand }, bAny);
	TestTrue(TEXT("Stand blocks the capsule"), R.GetResponse(ECC_Pawn) == ECR_Block);
	TestTrue(TEXT("Stand does not block the wall probes"), R.GetResponse(ECC_GameTraceChannel1) == ECR_Ignore);
	TestTrue(TEXT("Stand blocks the extra (depth marker) channel"), R.GetResponse(ECC_GameTraceChannel2) == ECR_Block);
	TestTrue(TEXT("Stand ignores Visibility"), R.GetResponse(ECC_Visibility) == ECR_Ignore);
	R = Responses({ Behaviour_WallSlide }, bAny);
	TestTrue(TEXT("WallSlide blocks the capsule and the wall probes"), R.GetResponse(ECC_Pawn) == ECR_Block && R.GetResponse(ECC_GameTraceChannel1) == ECR_Block);
	TestTrue(TEXT("WallSlide does not block the extra channel"), R.GetResponse(ECC_GameTraceChannel2) == ECR_Ignore);
	R = Responses({ Behaviour_WallClimb }, bAny);
	TestTrue(TEXT("WallClimb blocks the wall probes"), R.GetResponse(ECC_GameTraceChannel1) == ECR_Block);
	R = Responses({ Behaviour_Launch }, bAny);
	TestTrue(TEXT("Launch blocks the capsule (landing contact) and the extra channel"), R.GetResponse(ECC_Pawn) == ECR_Block && R.GetResponse(ECC_GameTraceChannel2) == ECR_Block);
	R = Responses({}, bAny);
	TestFalse(TEXT("No traversal behaviour: nothing blocks"), bAny);
	TestTrue(TEXT("No traversal behaviour: the capsule passes"), R.GetResponse(ECC_Pawn) == ECR_Ignore);

	// The colliders of a platform, with the project's settings
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = {
		MakeSurfaceShape(0, { Behaviour_Stand }),
		MakeSurfaceShape(1, { Behaviour_WallSlide }, FVector(300.0, 0.0, 0.0)),
		MakeSurfaceShape(2, { Behaviour_Launch }, FVector(600.0, 0.0, 0.0)),
		MakeSurfaceShape(3, {}, FVector(900.0, 0.0, 0.0)) };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec);
	const ECollisionChannel PlatformChannel = UGSTN_PlatformSettings::Get().GetPlatformObjectChannel();
	for (int32 Id = 0; Id < 4; ++Id)
	{
		const UPrimitiveComponent* Collider = Platform->GetShapeComponent(Id);
		if (!TestNotNull(*FString::Printf(TEXT("Collider %d"), Id), Collider))
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("Collider %d object type"), Id), int32(Collider->GetCollisionObjectType()), int32(PlatformChannel));
		TestTrue(*FString::Printf(TEXT("Collider %d ignores the camera"), Id), Collider->GetCollisionResponseToChannel(ECC_Camera) == ECR_Ignore);
		const ECollisionResponse ToPawn = Collider->GetCollisionResponseToChannel(ECC_Pawn);
		TestTrue(*FString::Printf(TEXT("Collider %d response to the capsule"), Id), Id < 3 ? ToPawn == ECR_Block : ToPawn == ECR_Ignore);
	}

	// GSTNSurface resolves the collider through its provider: the shape's own behaviour, a moving surface
	UPrimitiveComponent* Flank = Platform->GetShapeComponent(1);
	const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::Resolve(MakeHit(Flank, Flank->GetComponentLocation(), FVector(-1.0, 0.0, 0.0)));
	TestTrue(TEXT("Resolved by the provider"), View.Source == EGSTN_SurfaceSource::Provider);
	TestTrue(TEXT("The flank offers WallSlide"), View.HasBehaviour(Behaviour_WallSlide));
	TestFalse(TEXT("The flank does not offer Stand"), View.HasBehaviour(Behaviour_Stand));
	TestTrue(TEXT("A Movable collider is a moving surface"), View.bMoving);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartNameWarningTest, "GSTNPlatform.Shapes.PartNameWarningWithoutParts", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformPartNameWarningTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformSpec Spec;
	FGSTN_PlatformShape Arm = MakeSurfaceShape(0, { GSTNSurfaceTags::Behaviour_WallSlide });
	Arm.PartName = TEXT("LeftArm");
	Spec.Shapes = { Arm, MakeSurfaceShape(1, { GSTNSurfaceTags::Behaviour_Stand }, FVector(0.0, 0.0, 200.0)) };

	TArray<FText> Errors, Warnings;
	TestTrue(TEXT("A part name is not an error in the platform plugin alone"), Spec.CollectFindings(Errors, Warnings));
	TestTrue(TEXT("It is a warning naming the part and the missing bridge"), ContainsText(Warnings, TEXT("LeftArm")) && ContainsText(Warnings, TEXT("GSTNPlatformParts")));
	TestEqual(TEXT("Only the shape with a part name warns"), Warnings.FilterByPredicate([](const FText& Text) { return Text.ToString().Contains(TEXT("part")); }).Num(), 1);

	FGSTN_PlatformValidationContext WithBridge;
	WithBridge.bPartNamesResolved = true;
	Errors.Reset();
	Warnings.Reset();
	Spec.CollectFindings(Errors, Warnings, WithBridge);
	TestFalse(TEXT("With a bridge the platform plugin leaves part names to it"), ContainsText(Warnings, TEXT("LeftArm")));

	// Without the bridge the shape behaves as body: created and solid
	FRig Rig;
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec);
	Rig.Subsystem()->ApplyPosePoint(MakeContext(1));
	TestTrue(TEXT("The part shape is active as body"), IsSolid(Platform->GetShapeComponent(0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformBreakingGraceTest, "GSTNPlatform.Shapes.BreakingGraceThenOff", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformBreakingGraceTest::RunTest(const FString& Parameters)
{
	// The timeline itself
	{
		FGSTN_PlatformShapeStates States;
		States.Reset(0b1);
		int32 Grace[GSTNPlatform::MaxShapes] = { 20, 20, 20, 20, 20, 20, 20, 20 };
		TestEqual(TEXT("Changed shapes"), int32(States.Request(0, EGSTN_PlatformShapeState::Breaking, Grace, 100)), 0b1);
		TestTrue(TEXT("Frame 99 Active"), States.GetState(0, 99) == EGSTN_PlatformShapeState::Active);
		TestTrue(TEXT("Frame 100 Breaking"), States.GetState(0, 100) == EGSTN_PlatformShapeState::Breaking);
		TestTrue(TEXT("Frame 119 Breaking"), States.GetState(0, 119) == EGSTN_PlatformShapeState::Breaking);
		TestTrue(TEXT("Frame 120 Inactive"), States.GetState(0, 120) == EGSTN_PlatformShapeState::Inactive);
		TestEqual(TEXT("Solid at 119"), int32(States.GetSolidMask(119)), 0b1);
		TestEqual(TEXT("Not solid at 120"), int32(States.GetSolidMask(120)), 0);

		// Asked to go at once while breaking: the grace is cut from that frame
		States.Request(0, EGSTN_PlatformShapeState::Inactive, Grace, 105);
		TestTrue(TEXT("Frame 104 still Breaking"), States.GetState(0, 104) == EGSTN_PlatformShapeState::Breaking);
		TestTrue(TEXT("Frame 105 Inactive"), States.GetState(0, 105) == EGSTN_PlatformShapeState::Inactive);

		// Back on at 130: Active at once, the frames before keep their state
		States.Request(0b1, EGSTN_PlatformShapeState::Breaking, Grace, 130);
		TestTrue(TEXT("Frame 129 Inactive"), States.GetState(0, 129) == EGSTN_PlatformShapeState::Inactive);
		TestTrue(TEXT("Frame 130 Active"), States.GetState(0, 130) == EGSTN_PlatformShapeState::Active);
	}

	// A platform: spec grace 20, shape 1 overrides it to 0
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.DropGraceFrames = 20;
	FGSTN_PlatformShape NoGrace = MakeSurfaceShape(1, { GSTNSurfaceTags::Behaviour_Stand }, FVector(300.0, 0.0, 0.0));
	NoGrace.DropGraceFrames = 0;
	Spec.Shapes = { MakeSurfaceShape(0, { GSTNSurfaceTags::Behaviour_WallSlide }), NoGrace };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec);
	UGSTN_PlatformSubsystem* Subsystem = Rig.Subsystem();
	UPrimitiveComponent* Shape0 = Platform->GetShapeComponent(0);
	UPrimitiveComponent* Shape1 = Platform->GetShapeComponent(1);

	Subsystem->ApplyPosePoint(MakeContext(50));
	Platform->SetShapeMask(0, EGSTN_PlatformShapeState::Breaking);
	for (int32 Frame = 51; Frame <= 70; ++Frame)
	{
		Subsystem->ApplyPosePoint(MakeContext(Frame));
		if (!IsSolid(Shape0) || Platform->GetShapeState(0) != EGSTN_PlatformShapeState::Breaking)
		{
			AddError(FString::Printf(TEXT("Shape 0 should be solid and Breaking at frame %d"), Frame));
		}
	}
	TestTrue(TEXT("Breaking tag during the grace"), Platform->GetShapeStateTag(0) == GSTNPlatformTags::State_Breaking.GetTag());
	TestFalse(TEXT("Shape 1 (no grace) went at once"), IsSolid(Shape1));
	Subsystem->ApplyPosePoint(MakeContext(71));
	TestFalse(TEXT("Shape 0 off after 20 frames of grace"), IsSolid(Shape0));
	TestTrue(TEXT("Inactive"), Platform->GetShapeState(0) == EGSTN_PlatformShapeState::Inactive);

	// Re-simulating the grace window gives the same states back
	Subsystem->ApplyPosePoint(MakeContext(60));
	TestTrue(TEXT("Replayed frame 60: solid, Breaking"), IsSolid(Shape0) && Platform->GetShapeState(0) == EGSTN_PlatformShapeState::Breaking);
	Subsystem->ApplyPosePoint(MakeContext(50));
	TestTrue(TEXT("Replayed frame 50: Active"), IsSolid(Shape0) && Platform->GetShapeState(0) == EGSTN_PlatformShapeState::Active);

	// The enemy's action state uses the same telegraph (launched: off after the grace)
	FRig Rig2;
	UGSTN_EnemyPlatformComponent* Launched = Rig2.SpawnPlatform(Spec);
	Rig2.Subsystem()->ApplyPosePoint(MakeContext(5));
	Launched->SetActionState(EGSTN_PlatformActionState::Launched);
	Rig2.Subsystem()->ApplyPosePoint(MakeContext(6));
	TestTrue(TEXT("Launched: Breaking first"), Launched->GetShapeState(0) == EGSTN_PlatformShapeState::Breaking);
	Rig2.Subsystem()->ApplyPosePoint(MakeContext(26));
	TestTrue(TEXT("Launched: Inactive after the grace"), Launched->GetShapeState(0) == EGSTN_PlatformShapeState::Inactive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPoseOncePerStepTest, "GSTNPlatform.Shapes.PoseAppliedOncePerStep", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformPoseOncePerStepTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = { MakeSurfaceShape(0, { GSTNSurfaceTags::Behaviour_WallSlide }, FVector(-100.0, 0.0, 200.0)) };
	UGSTN_EnemyPlatformComponent* A = Rig.SpawnPlatform(Spec);
	UGSTN_ScriptedPlatformComponent* B = Rig.SpawnPlatform<UGSTN_ScriptedPlatformComponent>(Spec, FVector(0.0, 2000.0, 0.0));
	B->Path.Speed = 300.f;
	B->Path.HalfLength = 400.f;
	B->SetPathOrigin(FTransform(FVector(0.0, 2000.0, 0.0)));
	UGSTN_PlatformSubsystem* Subsystem = Rig.Subsystem();
	const int32 StartA = A->GetPoseApplicationCount();
	const int32 StartB = B->GetPoseApplicationCount();

	// Several pawns tick in frame 11: the first poses the end of frame 10, the others find it done
	TestTrue(TEXT("First pawn of the frame poses"), Subsystem->ApplyPosePoint(MakeContext(10)));
	TestFalse(TEXT("Second pawn of the same frame does not"), Subsystem->ApplyPosePoint(MakeContext(10)));
	TestFalse(TEXT("Nor the third"), Subsystem->ApplyPosePoint(MakeContext(10)));
	TestEqual(TEXT("Platform A posed once"), A->GetPoseApplicationCount() - StartA, 1);
	TestEqual(TEXT("Platform B posed once"), B->GetPoseApplicationCount() - StartB, 1);
	const FVector At10 = B->GetShapeComponent(0)->GetComponentLocation();

	TestTrue(TEXT("Next frame poses again"), Subsystem->ApplyPosePoint(MakeContext(11)));
	TestEqual(TEXT("Platform A posed twice"), A->GetPoseApplicationCount() - StartA, 2);
	const FVector At11 = B->GetShapeComponent(0)->GetComponentLocation();
	TestTrue(TEXT("The scripted platform moved 300 cm/s x 16 ms along +Y"), (At11 - At10).Equals(FVector(0.0, 300.0 * 0.016, 0.0), 1.e-3));

	// A rollback replays frame 10: posed again, at exactly the pose of the first time (a function of the frame time)
	TestTrue(TEXT("A replayed frame poses again"), Subsystem->ApplyPosePoint(MakeContext(10)));
	TestEqual(TEXT("Current frame is the replayed one"), B->GetCurrentPoseFrame(), 10);
	TestEqual(TEXT("Latest frame stays 11"), B->GetLatestPoseFrame(), 11);
	TestTrue(TEXT("Same pose bit for bit"), B->GetShapeComponent(0)->GetComponentLocation() == At10);

	// The surface velocity comes from the pose history of the frame the colliders hold
	Subsystem->ApplyPosePoint(MakeContext(11));
	const FVector Velocity = B->GetSurfaceVelocityAt(0, At11);
	TestTrue(TEXT("Surface velocity 300 cm/s along +Y"), Velocity.Equals(FVector(0.0, 300.0, 0.0), 1.e-2));
	const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::Resolve(MakeHit(B->GetShapeComponent(0), At11, FVector(-1.0, 0.0, 0.0)));
	TestTrue(TEXT("GSTNSurface reads the same velocity through the provider"), View.SurfaceVelocity.Equals(FVector(0.0, 300.0, 0.0), 1.e-2));
	TestTrue(TEXT("The default platform (owner standing still) has no velocity"), A->GetSurfaceVelocityAt(0, FVector::ZeroVector).IsNearlyZero());

	B->SetShapeMask(0, EGSTN_PlatformShapeState::Inactive);
	Subsystem->ApplyPosePoint(MakeContext(12));
	TestTrue(TEXT("An inactive shape has no velocity"), B->GetSurfaceVelocityAt(0, At11).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformValidatorBoneCapsuleTest, "GSTNPlatform.Shapes.ValidatorRejectsBoneAndCapsule", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformValidatorBoneCapsuleTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSurfaceTags;
	TArray<FText> Errors, Warnings;
	FGSTN_PlatformSpec Spec;

	FGSTN_PlatformShape OnBone = MakeSurfaceShape(0, { Behaviour_WallSlide });
	OnBone.Bone = TEXT("spine_03");
	Spec.Shapes = { OnBone };
	TestFalse(TEXT("A shape on a bone is an error"), Spec.CollectFindings(Errors, Warnings));
	TestTrue(TEXT("The error names the bone"), ContainsText(Errors, TEXT("spine_03")));

	Errors.Reset();
	Spec.Shapes = { MakeSurfaceShape(0, { Behaviour_Stand }, FVector::ZeroVector, EGSTN_PlatformShapeType::Capsule) };
	TestFalse(TEXT("Standing on a capsule is an error"), Spec.CollectFindings(Errors, Warnings));
	Errors.Reset();
	Spec.Shapes = { MakeSurfaceShape(0, { Behaviour_Stand }, FVector::ZeroVector, EGSTN_PlatformShapeType::Sphere) };
	Spec.Shapes[0].Extent = FVector(40.0);
	TestFalse(TEXT("Standing on a sphere is an error"), Spec.CollectFindings(Errors, Warnings));

	Errors.Reset();
	Spec.Shapes = { MakeSurfaceShape(0, { Behaviour_WallSlide }, FVector::ZeroVector, EGSTN_PlatformShapeType::Capsule) };
	TestTrue(TEXT("Sliding on a capsule flank is fine"), Spec.CollectFindings(Errors, Warnings));
	Errors.Reset();
	Spec.Shapes = { MakeSurfaceShape(0, { Behaviour_Stand }) };
	TestTrue(TEXT("Standing on a box is fine"), Spec.CollectFindings(Errors, Warnings));

	// A box tilted so that no face is walkable, nor within a narrow wall range: warnings ("players will never grab it")
	Errors.Reset();
	Warnings.Reset();
	FGSTN_PlatformShape Tilted = MakeSurfaceShape(0, { Behaviour_Stand, Behaviour_WallSlide });
	Tilted.LocalRotation = FRotator(35.26, 0.0, 45.0);
	Spec.Shapes = { Tilted };
	FGSTN_PlatformValidationContext Narrow;
	Narrow.MinWallSurfaceAngle = 85.0;
	Narrow.MaxWallSurfaceAngle = 95.0;
	TestTrue(TEXT("Angles are warnings, not errors"), Spec.CollectFindings(Errors, Warnings, Narrow));
	TestTrue(TEXT("No walkable top"), ContainsText(Warnings, TEXT("walkable")));
	TestTrue(TEXT("No wall face in range"), ContainsText(Warnings, TEXT("never grab")));

	// A surface component on the enemy would shadow every shape (resolution step 1)
	FRig Rig;
	Spec.Shapes = { MakeSurfaceShape(0, { Behaviour_Stand }) };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec);
	Errors.Reset();
	Warnings.Reset();
	Platform->CollectFindings(Errors, Warnings);
	TestFalse(TEXT("No shadowing warning without a surface component"), ContainsText(Warnings, TEXT("Surface Behaviour")));
	UGSTN_SurfaceBehaviourComponent* Shadow = NewObject<UGSTN_SurfaceBehaviourComponent>(Platform->GetOwner(), TEXT("Shadow"));
	Shadow->RegisterComponent();
	Warnings.Reset();
	Platform->CollectFindings(Errors, Warnings);
	TestTrue(TEXT("Shadowing warning with one"), ContainsText(Warnings, TEXT("Surface Behaviour")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformScriptedPathTest, "GSTNPlatform.Shapes.ScriptedPathIsAFunctionOfTime", GSTNPlatformTest::PlatformTestFlags)
bool FGSTN_PlatformScriptedPathTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformScriptedPath Path;
	Path.Axis = FVector(0.0, 1.0, 0.0);
	Path.Speed = 300.f;
	Path.HalfLength = 400.f;
	const FTransform Origin(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("Starts at the origin"), Path.Evaluate(Origin, 0.0).GetLocation().Equals(Origin.GetLocation(), 1.e-6));
	// Local +Y of a 90 degree yaw is world -X
	TestTrue(TEXT("After 1 s: 300 cm along the axis in the origin's frame"), Path.Evaluate(Origin, 1.0).GetLocation().Equals(FVector(-200.0, 0.0, 0.0), 1.e-6));
	TestTrue(TEXT("Turns back at the end of the stroke"), FMath::IsNearlyEqual(FGSTN_PlatformScriptedPath::TriangleWave(2.0, 300.0, 400.0), 200.0, 1.e-6));
	TestTrue(TEXT("Same time, same pose"), Path.Evaluate(Origin, 12.345).Equals(Path.Evaluate(Origin, 12.345), 0.0));
	return true;
}

#endif
