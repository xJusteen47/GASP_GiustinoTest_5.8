// Automation tests of the platform parts bridge (plan T6).

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformPartGates.h"
#include "GSTN_PlatformPartsComponent.h"
#include "GSTN_PlatformSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Sim/GSTN_PartsAttach.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPlatformPartsTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	// Part indices of the set and shape ids of the platform
	constexpr int32 Torso = 0, Arm = 1, Hand = 2, Head = 3;
	constexpr int32 TopShape = 0, ArmShape = 1, HandShape = 2, HeadShape = 3;

	FGSTN_PartDefinition MakePart(FName Name, int32 Parent = INDEX_NONE)
	{
		FGSTN_PartDefinition Part;
		Part.Name = Name;
		Part.ParentIndex = Parent;
		Part.PartCap = 100.f;
		return Part;
	}

	FGSTN_PlatformShape MakeShape(uint8 Id, FName PartName, float X)
	{
		FGSTN_PlatformShape Shape;
		Shape.ShapeId = Id;
		Shape.LocalLocation = FVector(X, 0.0, 0.0);
		Shape.PartName = PartName;
		return Shape;
	}

	FGSTN_PlatformPoseContext MakeContext(int32 Frame)
	{
		FGSTN_PlatformPoseContext Context;
		Context.Frame = Frame;
		Context.StepMs = 16.f;
		Context.TimeMs = double(Frame + 1) * 16.0;
		Context.bFromSimulation = true;
		return Context;
	}

	/**
	 * A throwaway game world (nothing ticks: the test calls the pose point) with one fighter: a body top (shape 0), an arm flank (1), a
	 * hand ledge (2, the hand is the arm's child), a head plate (3). The part set is attached through the parts registry as a kit would.
	 */
	struct FRig
	{
		UWorld* World = nullptr;
		AActor* Actor = nullptr;
		UGSTN_EnemyPlatformComponent* Platform = nullptr;
		UGSTN_PlatformPartsComponent* Bridge = nullptr;
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;

		explicit FRig(int32 GraceFrames)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPlatformPartsTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts = { MakePart("Torso"), MakePart("Arm"), MakePart("Hand", Arm), MakePart("Head") };

			Actor = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Root->SetMobility(EComponentMobility::Movable);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();

			Platform = NewObject<UGSTN_EnemyPlatformComponent>(Actor, TEXT("Platform"));
			Platform->Spec.DropGraceFrames = GraceFrames;
			Platform->Spec.Shapes = { MakeShape(TopShape, NAME_None, 0.f), MakeShape(ArmShape, "Arm", 300.f), MakeShape(HandShape, "Hand", 600.f),
				MakeShape(HeadShape, "Head", 900.f) };
			Platform->RegisterComponent();
			Bridge = NewObject<UGSTN_PlatformPartsComponent>(Actor, TEXT("PlatformParts"));
			Bridge->RegisterComponent();
			FGSTN_PartsAttach::ApplyPartSet(Actor, Set.Get(), nullptr);
		}

		~FRig()
		{
			FGSTN_PartsAttach::ClearPartSet(Actor, nullptr);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		void Pose(int32 Frame) const { World->GetSubsystem<UGSTN_PlatformSubsystem>()->ApplyPosePoint(MakeContext(Frame)); }

		void Break(uint16 Mask) const { Bridge->SetBrokenMaskOverride(Mask); }

		EGSTN_PlatformShapeState State(int32 ShapeId) const { return Platform->GetShapeState(ShapeId); }

		bool Solid(int32 ShapeId) const
		{
			const UPrimitiveComponent* Collider = Platform->GetShapeComponent(ShapeId);
			return Collider && Collider->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		}
	};

	uint16 Bit(int32 Part) { return static_cast<uint16>(1u << Part); }
}

using namespace GSTNPlatformPartsTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartsMaskFollowsTest, "GSTNPlatformParts.MaskFollowsBrokenMask", GSTNPlatformPartsTest::TestFlags)
bool FGSTN_PlatformPartsMaskFollowsTest::RunTest(const FString& Parameters)
{
	FRig Rig(0);
	Rig.Break(0);
	Rig.Pose(10);
	TestTrue(TEXT("Nothing broken: every shape solid"), Rig.Solid(TopShape) && Rig.Solid(ArmShape) && Rig.Solid(HandShape) && Rig.Solid(HeadShape));
	TestEqual(TEXT("Applied mask all on"), Rig.Bridge->GetAppliedShapeMask(), static_cast<uint8>(0xFF));

	Rig.Break(Bit(Head));
	TestTrue(TEXT("Nothing changes before the pose point"), Rig.Solid(HeadShape));
	Rig.Pose(11);
	TestFalse(TEXT("The head plate goes in the frame posed after the break"), Rig.Solid(HeadShape));
	TestTrue(TEXT("The other shapes stay"), Rig.Solid(TopShape) && Rig.Solid(ArmShape) && Rig.Solid(HandShape));
	TestEqual(TEXT("Mask input of the platform"), Rig.Platform->GetShapeMaskInput(), static_cast<uint8>(~(1u << HeadShape) & 0xFF));

	Rig.Break(Bit(Torso));
	Rig.Pose(12);
	TestTrue(TEXT("Breaking a part no shape names changes nothing (the top is body)"), Rig.Solid(TopShape));
	TestTrue(TEXT("Head back: the mask is followed, not the last change, and its bit is clear now"), Rig.Solid(HeadShape));

	// A replayed frame keeps the collision it had, whatever the mask now
	Rig.Break(Bit(Arm));
	Rig.Pose(11);
	TestTrue(TEXT("Replayed frame 11: arm still solid (no input on a replayed frame)"), Rig.Solid(ArmShape));
	TestFalse(TEXT("Replayed frame 11: head off as the first time"), Rig.Solid(HeadShape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartsRestoreTest, "GSTNPlatformParts.RestoreReenables", GSTNPlatformPartsTest::TestFlags)
bool FGSTN_PlatformPartsRestoreTest::RunTest(const FString& Parameters)
{
	FRig Rig(0);
	Rig.Break(Bit(Arm));
	Rig.Pose(10);
	TestFalse(TEXT("Arm broken: flank off"), Rig.Solid(ArmShape));

	// A restore (FGSTN_PartMath::RestoreParts, raising Combat.Event.Part.Restored) clears the bit
	Rig.Break(0);
	Rig.Pose(11);
	TestTrue(TEXT("Restored: the flank is back at once"), Rig.Solid(ArmShape));
	TestTrue(TEXT("Active, no telegraph on the way back"), Rig.State(ArmShape) == EGSTN_PlatformShapeState::Active);
	TestTrue(TEXT("The hand ledge is back with its parent"), Rig.Solid(HandShape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartsChildrenTest, "GSTNPlatformParts.ChildrenDropWithParent", GSTNPlatformPartsTest::TestFlags)
bool FGSTN_PlatformPartsChildrenTest::RunTest(const FString& Parameters)
{
	// Pure: the gate of a child holds its ancestors
	const TArray<int32> Parents = { INDEX_NONE, INDEX_NONE, Arm, INDEX_NONE };
	TestEqual(TEXT("Hand gate = hand + arm"), FGSTN_PlatformPartGates::MakeGate(Hand, Parents), static_cast<uint16>(Bit(Hand) | Bit(Arm)));
	TestEqual(TEXT("Root gate = itself"), FGSTN_PlatformPartGates::MakeGate(Head, Parents), Bit(Head));
	const TArray<int32> Cycle = { 1, 0 };
	TestEqual(TEXT("A cycle stops"), FGSTN_PlatformPartGates::MakeGate(0, Cycle), static_cast<uint16>(0b11));

	// Only the arm bit (a mask from before a cascade, or a hand that is unbreakable): the hand ledge still goes with the arm
	FRig Rig(0);
	Rig.Break(Bit(Arm));
	Rig.Pose(10);
	TestFalse(TEXT("Arm flank off"), Rig.Solid(ArmShape));
	TestFalse(TEXT("Hand ledge off with its parent"), Rig.Solid(HandShape));
	TestTrue(TEXT("Head and top untouched"), Rig.Solid(HeadShape) && Rig.Solid(TopShape));

	// Hand alone: the arm stays
	Rig.Break(Bit(Hand));
	Rig.Pose(11);
	TestTrue(TEXT("Hand broken alone: arm flank back"), Rig.Solid(ArmShape));
	TestFalse(TEXT("Hand ledge off"), Rig.Solid(HandShape));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartsGraceTest, "GSTNPlatformParts.GraceBeforeOff", GSTNPlatformPartsTest::TestFlags)
bool FGSTN_PlatformPartsGraceTest::RunTest(const FString& Parameters)
{
	FRig Rig(3);
	Rig.Break(0);
	Rig.Pose(10);
	Rig.Break(Bit(Arm));
	Rig.Pose(11);
	TestTrue(TEXT("Frame 11: Breaking"), Rig.State(ArmShape) == EGSTN_PlatformShapeState::Breaking);
	TestTrue(TEXT("Breaking is still solid (the telegraph)"), Rig.Solid(ArmShape));
	Rig.Pose(12);
	Rig.Pose(13);
	TestTrue(TEXT("Frame 13: still Breaking"), Rig.State(ArmShape) == EGSTN_PlatformShapeState::Breaking && Rig.Solid(ArmShape));
	Rig.Pose(14);
	TestTrue(TEXT("Frame 14: Inactive after 3 grace frames"), Rig.State(ArmShape) == EGSTN_PlatformShapeState::Inactive);
	TestFalse(TEXT("Not solid"), Rig.Solid(ArmShape));

	// Without the telegraph the shape goes at once
	FRig Instant(3);
	Instant.Bridge->bTelegraphBreak = false;
	Instant.Break(0);
	Instant.Pose(10);
	Instant.Break(Bit(Head));
	Instant.Pose(11);
	TestTrue(TEXT("bTelegraphBreak off: Inactive at once"), Instant.State(HeadShape) == EGSTN_PlatformShapeState::Inactive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPartsUnknownNameTest, "GSTNPlatformParts.UnknownPartNameIsError", GSTNPlatformPartsTest::TestFlags)
bool FGSTN_PlatformPartsUnknownNameTest::RunTest(const FString& Parameters)
{
	FRig Rig(0);
	FGSTN_PlatformSpec Spec = Rig.Platform->Spec;
	Spec.Shapes.Add(MakeShape(4, "Tail", 1200.f));
	TArray<TPair<int32, FName>> Unknown;
	const FGSTN_PlatformPartGates Gates = UGSTN_PlatformPartsComponent::ResolveGates(Spec, Rig.Set.Get(), &Unknown);
	TestEqual(TEXT("One unknown name"), Unknown.Num(), 1);
	TestTrue(TEXT("It is the tail"), Unknown.Num() == 1 && Unknown[0].Value == FName("Tail"));
	TestEqual(TEXT("The unknown shape is body"), Gates.Gates[4], static_cast<uint16>(0));
	TestEqual(TEXT("Body shape has no gate"), Gates.Gates[TopShape], static_cast<uint16>(0));

	Rig.Bridge->ValidationPartSet = Rig.Set.Get();
	Rig.Platform->Spec = Spec;
	TArray<FText> Errors, Warnings;
	TestFalse(TEXT("Validation fails"), Rig.Bridge->CollectFindings(Errors, Warnings));
	TestTrue(TEXT("The error names the part"), Errors.Num() == 1 && Errors[0].ToString().Contains(TEXT("Tail")));
	return true;
}

#endif
