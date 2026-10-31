#include "Editor.h"
#include "Editor/Transactor.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "Hurtbox/GSTN_HurtboxGizmoTarget.h"
#include "Misc/AutomationTest.h"
#include "Preview/GSTN_GizmoMath.h"
#include "Preview/GSTN_HitboxGizmoTarget.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The gizmos and the picking of the preview viewport (plan m5 E7): the maths of GSTN_GizmoMath.h, and the two gizmo targets over their edit models, with their
// undo. The viewport client itself (widget, mouse, keys) needs a rendering editor and is listed in the commit as manual checks.

#define GSTN_GIZMO_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNGizmoTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	FGSTN_HitboxKey Key(int32 Frame, double X, double Y = 0.0)
	{
		FGSTN_HitboxKey Result;
		Result.Frame = Frame;
		Result.Offset = FVector(X, Y, 0.0);
		return Result;
	}

	/** A transient timeline of 40 frames with one hitbox track [4, 14), a model on it and an undo client */
	struct FFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TSharedRef<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

		static TStrongObjectPtr<UGSTN_CombatTimeline> MakeTimeline(const FGSTN_HitboxSpec& Spec)
		{
			TStrongObjectPtr<UGSTN_CombatTimeline> Made(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			Made->Data.TotalFrames = 40;
			Made->Data.FirstActionableFrame = 20;
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 4;
			Track.EndFrame = 14;
			Track.Hitboxes.Add(Spec);
			Made->Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Track));
			return Made;
		}

		explicit FFixture(const FGSTN_HitboxSpec& Spec)
			: Timeline(MakeTimeline(Spec))
			, Model(MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get())))
		{
			Model->SetTickRate(62.5f);
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		FGSTN_TimelineTrack_Hitbox& Track() { return Timeline->Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Hitbox>(); }
	};

	FGSTN_HitboxSpec TwoKeySpec()
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(30.0, 30.0, 30.0);
		Spec.Keys = { Key(0, 0.0), Key(6, 60.0, 30.0) };
		return Spec;
	}

	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}

	bool SameRotation(const FQuat& A, const FQuat& B)
	{
		return FMath::Abs(A | B) > 1.0 - 1e-7;
	}

	GSTNGizmo::FDragDelta MakeDelta(const FVector& Translation, const FQuat& Rotation = FQuat::Identity, const FVector& Scale = FVector::ZeroVector)
	{
		GSTNGizmo::FDragDelta Delta;
		Delta.Accumulate(Translation, Rotation, Scale);
		return Delta;
	}
}

using namespace GSTNGizmoTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoRoundTripTest, "GSTNCombat.Editor.Gizmo.DeltaToKeySpaceRoundTrip", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoRoundTripTest::RunTest(const FString& Parameters)
{
	// A rotated, moved root, and a socket that is itself turned: every space is non-trivial
	const FTransform Root(FRotator(10.0, 70.0, -15.0), FVector(100.0, -40.0, 20.0));
	const FTransform Socket(FRotator(-30.0, 15.0, 40.0), FVector(12.0, 55.0, 130.0));
	const FTransform MoveOrigin(FRotator(0.0, -120.0, 0.0), FVector(-300.0, 80.0, 0.0));

	struct FCase { const TCHAR* Name; EGSTN_HitboxSpace Space; };
	const FCase Cases[] = {
		{ TEXT("Root"), EGSTN_HitboxSpace::Root },
		{ TEXT("BakedSocket"), EGSTN_HitboxSpace::BakedSocket },
		{ TEXT("WorldAtMoveStart"), EGSTN_HitboxSpace::WorldAtMoveStart },
		{ TEXT("Projectile"), EGSTN_HitboxSpace::Projectile }
	};
	const FTransform StartKey(FRotator(5.0, 25.0, -10.0), FVector(40.0, -15.0, 8.0));
	const FVector WorldTranslation(13.0, -27.0, 6.5);
	const FQuat WorldRotation = FRotator(20.0, -35.0, 12.0).Quaternion();

	for (const FCase& Case : Cases)
	{
		const FTransform Space = GSTNGizmo::SpaceTransform(Case.Space, Root, TOptional<FTransform>(Socket), MoveOrigin);

		// Translation: world -> key -> world gives the input back
		const FVector KeyDelta = GSTNGizmo::WorldTranslationToKey(WorldTranslation, Space);
		TestTrue(*FString::Printf(TEXT("%s: world delta survives the trip through the key's space"), Case.Name), Space.TransformVector(KeyDelta).Equals(WorldTranslation, 1e-4));

		// The whole drag: the key's world position moved by exactly the world delta, its world rotation turned by exactly the world rotation
		const GSTNGizmo::FDragDelta Delta = MakeDelta(WorldTranslation, WorldRotation);
		const FTransform NewKey = GSTNGizmo::ApplyDragToKey(StartKey, Space, Delta);
		const FTransform Before = StartKey * Space;
		const FTransform After = NewKey * Space;
		TestTrue(*FString::Printf(TEXT("%s: the key moved by the world delta"), Case.Name), After.GetLocation().Equals(Before.GetLocation() + WorldTranslation, 1e-3));
		TestTrue(*FString::Printf(TEXT("%s: the key turned by the world rotation"), Case.Name), SameRotation(After.GetRotation(), WorldRotation * Before.GetRotation()));
		TestTrue(*FString::Printf(TEXT("%s: the key's own scale is left alone"), Case.Name), NewKey.GetScale3D().Equals(StartKey.GetScale3D()));
	}

	// A baked-socket hitbox with no socket resolved falls back to the root, and the projectile has no space in M5
	TestTrue(TEXT("a socket nobody placed falls back to the root"), GSTNGizmo::SpaceTransform(EGSTN_HitboxSpace::BakedSocket, Root, TOptional<FTransform>()).Equals(Root));
	TestTrue(TEXT("the projectile space is the identity"), GSTNGizmo::SpaceTransform(EGSTN_HitboxSpace::Projectile, Root, TOptional<FTransform>(Socket)).Equals(FTransform::Identity));

	// Many small deltas are the same as their sum, so a long drag does not drift
	GSTNGizmo::FDragDelta Accumulated;
	for (int32 Step = 0; Step < 50; ++Step)
	{
		Accumulated.Accumulate(FVector(0.5, -0.25, 0.1), FRotator(0.0, 0.4, 0.0).Quaternion(), FVector(0.3, 0.0, 0.0));
	}
	TestTrue(TEXT("translations add up"), Accumulated.Translation.Equals(FVector(25.0, -12.5, 5.0), 1e-6));
	TestTrue(TEXT("scale units add up"), Accumulated.Scale.Equals(FVector(15.0, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("rotations compose"), SameRotation(Accumulated.Rotation, FRotator(0.0, 20.0, 0.0).Quaternion()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoScaleTest, "GSTNCombat.Editor.Gizmo.ScaleMapsToExtentNotActorScale", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoScaleTest::RunTest(const FString& Parameters)
{
	FGSTN_HitShape Sphere;
	Sphere.Type = EGSTN_ShapeType::Sphere;
	Sphere.Extent = FVector(30.0, 30.0, 30.0);
	// 100 widget units on one axis double a sphere on all three: a sphere has one radius
	TestTrue(TEXT("sphere: uniform"), GSTNGizmo::ScaleExtent(Sphere, FVector(100.0, 0.0, 0.0)).Equals(FVector(60.0, 60.0, 60.0), 1e-6));
	TestTrue(TEXT("sphere: the axis dragged furthest wins, sign kept"), GSTNGizmo::ScaleExtent(Sphere, FVector(10.0, -50.0, 20.0)).Equals(FVector(15.0, 15.0, 15.0), 1e-6));
	TestTrue(TEXT("sphere: never below the minimum"), GSTNGizmo::ScaleExtent(Sphere, FVector(-1000.0, 0.0, 0.0)).X >= GSTNGizmo::MinExtent);

	FGSTN_HitShape Capsule;
	Capsule.Type = EGSTN_ShapeType::Capsule;
	Capsule.Extent = FVector(20.0, 20.0, 60.0);
	TestTrue(TEXT("capsule: Z is the half height, X and Y the radius"), GSTNGizmo::ScaleExtent(Capsule, FVector(0.0, 0.0, 50.0)).Equals(FVector(20.0, 20.0, 90.0), 1e-6));
	TestTrue(TEXT("capsule: the radius follows X or Y"), GSTNGizmo::ScaleExtent(Capsule, FVector(0.0, 100.0, 0.0)).Equals(FVector(40.0, 40.0, 60.0), 1e-6));
	TestTrue(TEXT("capsule: the half height never goes under the radius"), GSTNGizmo::ScaleExtent(Capsule, FVector(100.0, 100.0, -90.0)).Z >= 40.0 - 1e-9);

	FGSTN_HitShape Box;
	Box.Type = EGSTN_ShapeType::Box;
	Box.Extent = FVector(10.0, 20.0, 30.0);
	TestTrue(TEXT("box: per axis"), GSTNGizmo::ScaleExtent(Box, FVector(100.0, -50.0, 0.0)).Equals(FVector(20.0, 10.0, 30.0), 1e-6));

	// Through the gizmo target: the extent of the hitbox's shape changes, no transform scale does (the key's, the shape's Local, nothing)
	FFixture F(TwoKeySpec());
	F.Model->SelectKey(0, 0);
	F.Model->SetPlayhead(4);
	const TSharedRef<FGSTN_HitboxGizmoTarget> Target = MakeShared<FGSTN_HitboxGizmoTarget>(F.Model, TWeakPtr<FGSTN_TimelinePreviewScene>());
	if (!TestTrue(TEXT("a drag opens on the key at the playhead"), Target->BeginDrag(false)))
	{
		return false;
	}
	Target->ApplyDelta(MakeDelta(FVector::ZeroVector, FQuat::Identity, FVector(100.0, 0.0, 0.0)));
	Target->EndDrag();

	const FGSTN_HitboxSpec& Spec = F.Track().Hitboxes[0];
	TestTrue(TEXT("the extent doubled"), Spec.Shape.Extent.Equals(FVector(60.0, 60.0, 60.0), 1e-6));
	TestTrue(TEXT("the shape's Local scale is untouched"), Spec.Shape.Local.GetScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("no key's scale changed"), Spec.Keys[0].Scale.Equals(FVector::OneVector) && Spec.Keys[1].Scale.Equals(FVector::OneVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoAutoKeyTest, "GSTNCombat.Editor.Gizmo.AutoKeyOneTransaction", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoAutoKeyTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F(TwoKeySpec());
	F.Model->SetAutoKey(true);
	F.Model->SelectKey(0, 0);
	F.Model->SetPlayhead(7); // three frames into the window: halfway between the two keys, no key there
	const TSharedRef<FGSTN_HitboxGizmoTarget> Target = MakeShared<FGSTN_HitboxGizmoTarget>(F.Model, TWeakPtr<FGSTN_TimelinePreviewScene>());

	const GSTNGizmo::FDragKeyResolution Resolution = GSTNGizmo::ResolveDragKey(F.Track().Hitboxes[0].Keys, 3, true);
	TestTrue(TEXT("no key on the frame, auto-key on: create"), Resolution.Action == GSTNGizmo::EDragKeyAction::CreateAtFrame);

	const int32 Steps = AppliedSteps();
	if (!TestTrue(TEXT("the drag opens"), Target->BeginDrag(false)))
	{
		return false;
	}
	// The mouse moves 20 cm along X in four events; the target is always given the whole drag so far
	GSTNGizmo::FDragDelta Delta;
	for (int32 Move = 0; Move < 4; ++Move)
	{
		Delta.Accumulate(FVector(5.0, 0.0, 0.0), FQuat::Identity, FVector::ZeroVector);
		Target->ApplyDelta(Delta);
	}
	Target->EndDrag();

	TestEqual(TEXT("the new key and the move are one undo step"), AppliedSteps(), Steps + 1);
	if (!TestEqual(TEXT("three keys now"), F.Track().Hitboxes[0].Keys.Num(), 3))
	{
		return false;
	}
	const FGSTN_HitboxKey& Made = F.Track().Hitboxes[0].Keys[1];
	TestEqual(TEXT("on the playhead frame, counted from the track"), Made.Frame, 3);
	// The path had (30, 15, 0) there; the drag added 20 along X
	TestTrue(TEXT("the values of the path plus the drag"), Made.Offset.Equals(FVector(50.0, 15.0, 0.0), 1e-4));

	// One undo takes both back
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo removes the key and the move together"), F.Track().Hitboxes[0].Keys.Num(), 2);

	// Escape: nothing stays, in the data or in the history
	const int32 StepsBeforeCancel = AppliedSteps();
	F.Model->SelectKey(0, 0);
	if (TestTrue(TEXT("a second drag opens"), Target->BeginDrag(false)))
	{
		Target->ApplyDelta(MakeDelta(FVector(80.0, 0.0, 0.0)));
		TestEqual(TEXT("it edits live"), F.Track().Hitboxes[0].Keys.Num(), 3);
		Target->CancelDrag();
	}
	TestEqual(TEXT("Escape: the key is gone again"), F.Track().Hitboxes[0].Keys.Num(), 2);
	TestEqual(TEXT("Escape: no undo step was left"), AppliedSteps(), StepsBeforeCancel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoAutoKeyOffTest, "GSTNCombat.Editor.Gizmo.AutoKeyOffEditsNearestKey", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoAutoKeyOffTest::RunTest(const FString& Parameters)
{
	// The pure choice first
	const TArray<FGSTN_HitboxKey> Keys = { Key(0, 0.0), Key(6, 60.0), Key(9, 90.0) };
	TestTrue(TEXT("a key on the frame is used, auto-key or not"), GSTNGizmo::ResolveDragKey(Keys, 6, false).Action == GSTNGizmo::EDragKeyAction::UseKeyAtFrame);
	TestEqual(TEXT("the nearest key of frame 2"), GSTNGizmo::ResolveDragKey(Keys, 2, false).KeyInLane, 0);
	TestEqual(TEXT("the nearest key of frame 5"), GSTNGizmo::ResolveDragKey(Keys, 5, false).KeyInLane, 1);
	TestEqual(TEXT("two equally near keys: the earlier one"), GSTNGizmo::ResolveDragKey(Keys, 3, false).KeyInLane, 0);
	TestTrue(TEXT("no keys and auto-key off: nothing to edit"), GSTNGizmo::ResolveDragKey(TConstArrayView<FGSTN_HitboxKey>(), 3, false).Action == GSTNGizmo::EDragKeyAction::None);
	TestEqual(TEXT("a click on a hitbox picks the key on the frame"), GSTNGizmo::FindKeyForPick(Keys, 9), 2);
	TestEqual(TEXT("or the nearest one"), GSTNGizmo::FindKeyForPick(Keys, 12), 2);
	TestEqual(TEXT("a hitbox with no key picks none"), GSTNGizmo::FindKeyForPick(TConstArrayView<FGSTN_HitboxKey>(), 3), static_cast<int32>(INDEX_NONE));

	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F(TwoKeySpec());
	F.Model->SetAutoKey(false);
	F.Model->SelectKey(0, 0);
	F.Model->SetPlayhead(8); // relative frame 4: nearer to the key at 6 than the one at 0
	const TSharedRef<FGSTN_HitboxGizmoTarget> Target = MakeShared<FGSTN_HitboxGizmoTarget>(F.Model, TWeakPtr<FGSTN_TimelinePreviewScene>());

	const int32 Steps = AppliedSteps();
	if (!TestTrue(TEXT("the drag opens"), Target->BeginDrag(false)))
	{
		return false;
	}
	Target->ApplyDelta(MakeDelta(FVector(0.0, 0.0, 25.0)));
	Target->EndDrag();

	TestEqual(TEXT("no key was made"), F.Track().Hitboxes[0].Keys.Num(), 2);
	TestEqual(TEXT("one undo step"), AppliedSteps(), Steps + 1);
	TestTrue(TEXT("the nearest key (frame 6) moved"), F.Track().Hitboxes[0].Keys[1].Offset.Equals(FVector(60.0, 30.0, 25.0), 1e-4));
	TestTrue(TEXT("the other did not"), F.Track().Hitboxes[0].Keys[0].Offset.Equals(FVector::ZeroVector, 1e-9));

	// Shift: every key of the hitbox moves by the same offset, the whole path, in one step
	const int32 StepsBeforeOffset = AppliedSteps();
	F.Model->SetAutoKey(true);
	if (TestTrue(TEXT("the offset drag opens"), Target->BeginDrag(true)))
	{
		GSTNGizmo::FDragDelta Delta = MakeDelta(FVector(10.0, 0.0, 0.0));
		Delta.bOffsetAllKeys = true;
		Target->ApplyDelta(Delta);
		Target->EndDrag();
	}
	TestEqual(TEXT("offset: no key was made even with auto-key on"), F.Track().Hitboxes[0].Keys.Num(), 2);
	TestEqual(TEXT("offset: one step"), AppliedSteps(), StepsBeforeOffset + 1);
	TestTrue(TEXT("offset: first key"), F.Track().Hitboxes[0].Keys[0].Offset.Equals(FVector(10.0, 0.0, 0.0), 1e-4));
	TestTrue(TEXT("offset: second key"), F.Track().Hitboxes[0].Keys[1].Offset.Equals(FVector(70.0, 30.0, 25.0), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoPickingTest, "GSTNCombat.Editor.Gizmo.PickingRayHitsNearestShape", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoPickingTest::RunTest(const FString& Parameters)
{
	auto MakeShape = [](EGSTN_ShapeType Type, const FVector& Extent)
	{
		FGSTN_HitShape Shape;
		Shape.Type = Type;
		Shape.Extent = Extent;
		return Shape;
	};
	double T = 0.0;

	// Sphere: radius 50 at x = 200, a ray from the origin along +X enters at 150
	const FGSTN_HitShape Sphere = MakeShape(EGSTN_ShapeType::Sphere, FVector(50.0));
	const FTransform SphereAt(FVector(200.0, 0.0, 0.0));
	TestTrue(TEXT("sphere: hit"), GSTNGizmo::RayVsShape(FVector::ZeroVector, FVector::ForwardVector, Sphere, SphereAt, T));
	TestTrue(TEXT("sphere: at the near surface"), FMath::IsNearlyEqual(T, 150.0, 1e-6));
	TestFalse(TEXT("sphere: a ray that passes beside it misses"), GSTNGizmo::RayVsShape(FVector(0.0, 60.0, 0.0), FVector::ForwardVector, Sphere, SphereAt, T));
	TestFalse(TEXT("sphere: behind the ray misses"), GSTNGizmo::RayVsShape(FVector::ZeroVector, -FVector::ForwardVector, Sphere, SphereAt, T));
	TestTrue(TEXT("sphere: a ray that starts inside hits at 0"), GSTNGizmo::RayVsShape(SphereAt.GetLocation(), FVector(1.0, 1.0, 0.0), Sphere, SphereAt, T) && T == 0.0);
	TestTrue(TEXT("the direction need not be normalised"), GSTNGizmo::RayVsShape(FVector::ZeroVector, FVector(5.0, 0.0, 0.0), Sphere, SphereAt, T) && FMath::IsNearlyEqual(T, 150.0, 1e-6));

	// Box, turned 45 degrees about Z: half extents (20, 20, 20) at x = 100. Its corner points at the ray, so it is entered at 100 - 20 * sqrt(2)
	const FGSTN_HitShape Box = MakeShape(EGSTN_ShapeType::Box, FVector(20.0));
	const FTransform BoxAt(FRotator(0.0, 45.0, 0.0), FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("box: hit"), GSTNGizmo::RayVsShape(FVector::ZeroVector, FVector::ForwardVector, Box, BoxAt, T));
	TestTrue(TEXT("box: at the corner"), FMath::IsNearlyEqual(T, 100.0 - 20.0 * UE_SQRT_2, 1e-6));
	TestFalse(TEXT("box: above it misses"), GSTNGizmo::RayVsShape(FVector(0.0, 0.0, 30.0), FVector::ForwardVector, Box, BoxAt, T));
	TestFalse(TEXT("box: parallel to a face and outside misses"), GSTNGizmo::RayVsShape(FVector(0.0, 0.0, 25.0), FVector::ForwardVector, Box, FTransform(FVector(100.0, 0.0, 0.0)), T));

	// Capsule: radius 20, half height 60 (so the side runs z in [-40, 40]), standing at x = 100
	const FGSTN_HitShape Capsule = MakeShape(EGSTN_ShapeType::Capsule, FVector(20.0, 20.0, 60.0));
	const FTransform CapsuleAt(FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("capsule: the side"), GSTNGizmo::RayVsShape(FVector::ZeroVector, FVector::ForwardVector, Capsule, CapsuleAt, T) && FMath::IsNearlyEqual(T, 80.0, 1e-6));
	TestTrue(TEXT("capsule: the top hemisphere"), GSTNGizmo::RayVsShape(FVector(0.0, 0.0, 55.0), FVector::ForwardVector, Capsule, CapsuleAt, T) && T > 80.0 && T < 100.0);
	TestFalse(TEXT("capsule: above the top misses"), GSTNGizmo::RayVsShape(FVector(0.0, 0.0, 61.0), FVector::ForwardVector, Capsule, CapsuleAt, T));
	TestTrue(TEXT("capsule: from above, straight down"), GSTNGizmo::RayVsShape(FVector(100.0, 0.0, 200.0), -FVector::UpVector, Capsule, CapsuleAt, T) && FMath::IsNearlyEqual(T, 140.0, 1e-6));
	TestTrue(TEXT("the shape's Local placement counts"), [&]()
	{
		FGSTN_HitShape Moved = Sphere;
		Moved.Local = FTransform(FVector(0.0, 0.0, 100.0));
		return GSTNGizmo::RayVsShape(FVector(0.0, 0.0, 0.0), FVector::UpVector, Moved, FTransform::Identity, T) && FMath::IsNearlyEqual(T, 50.0, 1e-6);
	}());

	// The nearest of several
	TArray<GSTNGizmo::FPickCandidate> Candidates;
	GSTNGizmo::FPickCandidate Far;
	Far.Track = 0;
	Far.Lane = 0;
	Far.Shape = Sphere;
	Far.Carrier = FTransform(FVector(400.0, 0.0, 0.0));
	GSTNGizmo::FPickCandidate Near;
	Near.Track = 0;
	Near.Lane = 1;
	Near.Shape = Sphere;
	Near.Carrier = FTransform(FVector(200.0, 0.0, 0.0));
	Candidates = { Far, Near };
	double Distance = 0.0;
	TestEqual(TEXT("the nearer shape is picked, whatever the order"), GSTNGizmo::PickNearest(FVector::ZeroVector, FVector::ForwardVector, Candidates, &Distance), 1);
	TestTrue(TEXT("and its distance reported"), FMath::IsNearlyEqual(Distance, 150.0, 1e-6));
	TestEqual(TEXT("a ray through none picks none"), GSTNGizmo::PickNearest(FVector(0.0, 500.0, 0.0), FVector::ForwardVector, Candidates), static_cast<int32>(INDEX_NONE));

	// Camera inside two shapes at once (both hit at 0): the smaller one stays pickable
	GSTNGizmo::FPickCandidate Big;
	Big.Lane = 0;
	Big.Shape = MakeShape(EGSTN_ShapeType::Sphere, FVector(200.0));
	GSTNGizmo::FPickCandidate Small;
	Small.Lane = 1;
	Small.Shape = MakeShape(EGSTN_ShapeType::Sphere, FVector(40.0));
	Candidates = { Big, Small };
	TestEqual(TEXT("both reached at 0: the smaller wins"), GSTNGizmo::PickNearest(FVector::ZeroVector, FVector::ForwardVector, Candidates), 1);

	// The hitboxes the viewport draws and picks: only the active ones, at the key's place in the hitbox's space
	FFixture F(TwoKeySpec());
	TArray<GSTNGizmo::FActiveHitbox> Active;
	auto NoSocket = [](FName, FTransform&) { return false; };
	GSTNGizmo::CollectActiveHitboxes(*F.Model->GetData(), 2, FTransform::Identity, NoSocket, Active);
	TestEqual(TEXT("before the window: nothing is active"), Active.Num(), 0);
	GSTNGizmo::CollectActiveHitboxes(*F.Model->GetData(), 7, FTransform::Identity, NoSocket, Active);
	if (TestEqual(TEXT("inside the window: the hitbox"), Active.Num(), 1))
	{
		const FTransform Placed = Active[0].Shape.Local * Active[0].Space;
		TestTrue(TEXT("at the path's place, halfway between the keys"), Placed.GetLocation().Equals(FVector(30.0, 15.0, 0.0), 1e-4));
	}
	Active.Reset();
	GSTNGizmo::CollectActiveHitboxes(*F.Model->GetData(), 14, FTransform::Identity, NoSocket, Active);
	TestEqual(TEXT("the end of the window is exclusive"), Active.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoMirrorTest, "GSTNCombat.Editor.Gizmo.MirrorAcrossRootYZ", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoMirrorTest::RunTest(const FString& Parameters)
{
	const FTransform Identity = FTransform::Identity;
	const FTransform Key(FRotator(10.0, 35.0, -20.0), FVector(30.0, 10.0, 5.0), FVector(1.5, 1.0, 0.5));

	// In the root's own space: x flips, the rotation mirrors, scale is kept
	const FTransform Mirrored = GSTNGizmo::MirrorKeyAcrossRootYZ(Key, Identity, Identity);
	TestTrue(TEXT("x becomes -x"), Mirrored.GetLocation().Equals(FVector(-30.0, 10.0, 5.0), 1e-6));
	TestTrue(TEXT("the scale is kept"), Mirrored.GetScale3D().Equals(Key.GetScale3D(), 1e-9));
	// A yaw to the right becomes a yaw to the left: the forward axis (1,0,0) of the key turns to its mirror image
	const FVector Forward = Key.GetRotation().RotateVector(FVector::ForwardVector);
	TestTrue(TEXT("the forward axis is mirrored"), Mirrored.GetRotation().RotateVector(FVector::ForwardVector).Equals(FVector(Forward.X, -Forward.Y, -Forward.Z), 1e-6));
	TestTrue(TEXT("mirroring twice gives the key back"), GSTNGizmo::MirrorKeyAcrossRootYZ(Mirrored, Identity, Identity).Equals(Key, 1e-5));

	// In any space, across the ROOT's plane: a rotated root and a socket space
	const FTransform Root(FRotator(0.0, 90.0, 0.0), FVector(50.0, 20.0, 0.0));
	const FTransform Socket(FRotator(15.0, -20.0, 30.0), FVector(10.0, 60.0, 120.0));
	const FTransform Space = Socket * Root;
	const FTransform InSpace = GSTNGizmo::MirrorKeyAcrossRootYZ(Key, Space, Root);
	const FTransform WorldBefore = Key * Space;
	const FTransform WorldAfter = InSpace * Space;
	const FVector RootBefore = Root.InverseTransformPosition(WorldBefore.GetLocation());
	const FVector RootAfter = Root.InverseTransformPosition(WorldAfter.GetLocation());
	TestTrue(TEXT("seen from the root, only x flipped"), RootAfter.Equals(FVector(-RootBefore.X, RootBefore.Y, RootBefore.Z), 1e-3));
	TestTrue(TEXT("twice is the key again, in any space"), GSTNGizmo::MirrorKeyAcrossRootYZ(InSpace, Space, Root).Equals(Key, 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GizmoHurtboxTargetTest, "GSTNCombat.Editor.Gizmo.HurtboxShapeDragIsOneStep", GSTN_GIZMO_TEST_FLAGS)
bool FGSTN_GizmoHurtboxTargetTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transactional));
	FGSTN_Hurtbox Head;
	Head.Name = TEXT("Head");
	Head.Shape.Type = EGSTN_ShapeType::Sphere;
	Head.Shape.Extent = FVector(20.0);
	Head.Shape.Local = FTransform(FVector(0.0, 0.0, 170.0));
	Set->Hurtboxes.Add(Head);
	FGSTN_Hurtbox Body;
	Body.Name = TEXT("Body");
	Body.Shape.Type = EGSTN_ShapeType::Capsule;
	Body.Shape.Extent = FVector(30.0, 30.0, 90.0);
	Body.Shape.Local = FTransform(FVector(0.0, 0.0, 90.0));
	Set->Hurtboxes.Add(Body);

	const TSharedRef<FGSTN_HurtboxEditModel> Model = MakeShared<FGSTN_HurtboxEditModel>(Set.Get());
	FGSTN_HurtboxUndoClient UndoClient(*Model);
	FGSTN_HurtboxGizmoTarget Target(Model);

	FVector Location;
	FQuat Rotation;
	TestFalse(TEXT("nothing selected: no gizmo"), Target.GetWidgetFrame(Location, Rotation));

	// Clicking a hurtbox selects it; the ray finds the one under the cursor
	TArray<GSTNGizmo::FPickCandidate> Candidates;
	Target.CollectPickables(Candidates);
	const int32 Picked = GSTNGizmo::PickNearest(FVector(500.0, 0.0, 185.0), -FVector::ForwardVector, Candidates);
	if (!TestTrue(TEXT("the ray picks a hurtbox"), Picked != INDEX_NONE) || !TestTrue(TEXT("and selects it"), Target.Pick(Candidates[Picked].Track, Candidates[Picked].Lane)))
	{
		return false;
	}
	TestEqual(TEXT("the head was under the ray"), Model->GetSingleSelection(), 0);
	TestTrue(TEXT("the gizmo sits on the shape"), Target.GetWidgetFrame(Location, Rotation) && Location.Equals(FVector(0.0, 0.0, 170.0), 1e-9));

	const int32 Steps = AppliedSteps();
	if (!TestTrue(TEXT("the drag opens"), Target.BeginDrag(false)))
	{
		return false;
	}
	GSTNGizmo::FDragDelta Delta;
	for (int32 Move = 0; Move < 5; ++Move)
	{
		Delta.Accumulate(FVector(0.0, 4.0, 0.0), FRotator(0.0, 3.0, 0.0).Quaternion(), FVector(10.0, 0.0, 0.0));
		Target.ApplyDelta(Delta);
	}
	Target.EndDrag();

	TestEqual(TEXT("move, rotate and scale are one undo step"), AppliedSteps(), Steps + 1);
	const FGSTN_HitShape& Edited = (*Model->GetHurtboxes())[0].Shape;
	TestTrue(TEXT("moved"), Edited.Local.GetLocation().Equals(FVector(0.0, 20.0, 170.0), 1e-6));
	TestTrue(TEXT("turned"), SameRotation(Edited.Local.GetRotation(), FRotator(0.0, 15.0, 0.0).Quaternion()));
	TestTrue(TEXT("the extent grew by half, the Local scale did not"), Edited.Extent.Equals(FVector(30.0), 1e-6) && Edited.Local.GetScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("the other hurtbox is untouched"), (*Model->GetHurtboxes())[1].Shape.Extent.Equals(FVector(30.0, 30.0, 90.0)));

	GEditor->UndoTransaction();
	TestTrue(TEXT("undo puts the head back"), (*Model->GetHurtboxes())[0].Shape.Local.GetLocation().Equals(FVector(0.0, 0.0, 170.0), 1e-9) && (*Model->GetHurtboxes())[0].Shape.Extent.Equals(FVector(20.0)));

	// Escape
	const int32 StepsBeforeCancel = AppliedSteps();
	if (TestTrue(TEXT("a second drag opens"), Target.BeginDrag(false)))
	{
		Target.ApplyDelta(MakeDelta(FVector(100.0, 0.0, 0.0)));
		TestTrue(TEXT("it edits live"), (*Model->GetHurtboxes())[0].Shape.Local.GetLocation().X > 50.0);
		Target.CancelDrag();
	}
	TestTrue(TEXT("Escape restores the shape"), (*Model->GetHurtboxes())[0].Shape.Local.GetLocation().Equals(FVector(0.0, 0.0, 170.0), 1e-9));
	TestEqual(TEXT("and leaves no undo step"), AppliedSteps(), StepsBeforeCancel);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
