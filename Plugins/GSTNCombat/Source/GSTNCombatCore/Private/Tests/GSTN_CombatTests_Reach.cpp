// Automation tests of reach scaling (M11 L4, combat-loadout-mods 8): hitboxes stretch along their ReachAxis and sweet/sour spots
// follow the blade. Pure: in-memory hitboxes and timelines, no world and no content asset.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_HitReachUtils.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNReachTest
{
	/** A 90 cm blade from the grip socket along +X: a box from x = 5 to x = 95 */
	FGSTN_HitboxSpec MakeBlade()
	{
		FGSTN_HitboxSpec Spec;
		Spec.Space = EGSTN_HitboxSpace::BakedSocket;
		Spec.Socket = TEXT("weapon_r");
		Spec.Shape.Type = EGSTN_ShapeType::Box;
		Spec.Shape.Extent = FVector(45.0, 5.0, 5.0);
		Spec.Shape.Local = FTransform(FVector(50.0, 0.0, 0.0));
		Spec.ReachAxis = EGSTN_ReachAxis::X;
		Spec.Zone = EGSTN_HitboxZone::Sour;
		return Spec;
	}

	/** The tip sweetspot of the same group: a sphere of radius 8 at x = 90 that keeps its size */
	FGSTN_HitboxSpec MakeTip()
	{
		FGSTN_HitboxSpec Spec = MakeBlade();
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(8.0);
		Spec.Shape.Local = FTransform(FVector(90.0, 0.0, 0.0));
		Spec.bScaleExtentWithReach = false;
		Spec.Zone = EGSTN_HitboxZone::Sweet;
		Spec.Data.Priority = 10;
		return Spec;
	}

	FGSTN_HitShape MakeVictim()
	{
		FGSTN_HitShape Victim;
		Victim.Type = EGSTN_ShapeType::Sphere;
		Victim.Extent = FVector(5.0);
		return Victim;
	}

	bool Touches(const FGSTN_HitboxSpec& Spec, const FVector& VictimAt)
	{
		return UGSTN_HitShapeUtils::Overlap(Spec.Shape, FTransform::Identity, MakeVictim(), FTransform(VictimAt));
	}

	FGSTN_CombatTimelineData MakeTimeline(const TArray<FGSTN_HitboxSpec>& Hitboxes)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 2;
		Track.EndFrame = 6;
		Track.Part.GroupName = TEXT("Body");
		Track.Hitboxes = Hitboxes;
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 20;
		Data.FirstActionableFrame = 12;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		return Data;
	}

	int32 CountMessages(const FGSTN_CombatTimelineData& Data, const TCHAR* Needle)
	{
		TArray<FGSTN_TimelineMessage> Messages;
		FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
		int32 Count = 0;
		for (const FGSTN_TimelineMessage& Message : Messages)
		{
			Count += Message.Text.Contains(Needle) ? 1 : 0;
		}
		return Count;
	}
}

// ReachScale arrives as a float (1.3f is 1.29999995), so exact positions are checked to a thousandth of a cm
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReachStretchesBladeTest, "GSTNCombat.Core.Reach.StretchesBlade", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReachStretchesBladeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReachTest;
	const FGSTN_HitboxSpec Blade = UGSTN_HitReachUtils::ScaleHitboxReach(MakeBlade(), 1.3f);
	// [5, 95] becomes [6.5, 123.5]: centre 65, half length 58.5; the cross-section is unchanged
	TestTrue(TEXT("blade centre moves along the axis"), Blade.Shape.Local.GetTranslation().Equals(FVector(65.0, 0.0, 0.0), 1e-3));
	TestTrue(TEXT("blade extent stretches along the axis only"), Blade.Shape.Extent.Equals(FVector(58.5, 5.0, 5.0), 1e-3));
	TestFalse(TEXT("base blade misses a victim at 120"), Touches(MakeBlade(), FVector(120.0, 0.0, 0.0)));
	TestTrue(TEXT("long blade reaches a victim at 120"), Touches(Blade, FVector(120.0, 0.0, 0.0)));

	// Scale 1, no axis and projectiles are left as they are
	const FGSTN_HitboxSpec Same = UGSTN_HitReachUtils::ScaleHitboxReach(MakeBlade(), 1.f);
	TestTrue(TEXT("scale 1 changes nothing"), Same.Shape.Local.Equals(MakeBlade().Shape.Local, 0.0) && Same.Shape.Extent == MakeBlade().Shape.Extent);
	FGSTN_HitboxSpec NoAxis = MakeBlade();
	NoAxis.ReachAxis = EGSTN_ReachAxis::None;
	TestTrue(TEXT("no axis ignores the mod"), UGSTN_HitReachUtils::ScaleHitboxReach(NoAxis, 1.3f).Shape.Extent == NoAxis.Shape.Extent);
	FGSTN_HitboxSpec Projectile = MakeBlade();
	Projectile.Space = EGSTN_HitboxSpace::Projectile;
	TestFalse(TEXT("projectiles are not scalable"), UGSTN_HitReachUtils::IsReachScalable(Projectile));
	TestTrue(TEXT("projectile untouched"), UGSTN_HitReachUtils::ScaleHitboxReach(Projectile, 1.3f).Shape.Extent == Projectile.Shape.Extent);

	// Safety clamp
	const FGSTN_HitboxSpec Huge = UGSTN_HitReachUtils::ScaleHitboxReach(MakeBlade(), 100.f);
	TestTrue(TEXT("scale clamped to MaxReachScale"), FMath::IsNearlyEqual(Huge.Shape.Local.GetTranslation().X, 50.0 * UGSTN_HitReachUtils::MaxReachScale, 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReachSweetspotFollowsTipTest, "GSTNCombat.Core.Reach.SweetspotFollowsTip", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReachSweetspotFollowsTipTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReachTest;
	const FGSTN_HitboxSpec Tip = UGSTN_HitReachUtils::ScaleHitboxReach(MakeTip(), 1.3f);
	TestTrue(TEXT("tip moves to the new tip"), Tip.Shape.Local.GetTranslation().Equals(FVector(117.0, 0.0, 0.0), 1e-3));
	TestTrue(TEXT("tip keeps its size"), Tip.Shape.Extent == MakeTip().Shape.Extent);
	TestFalse(TEXT("base tip misses a victim at 120"), Touches(MakeTip(), FVector(120.0, 0.0, 0.0)));
	TestTrue(TEXT("scaled tip hits a victim at 120"), Touches(Tip, FVector(120.0, 0.0, 0.0)));
	// A victim at the old tip is now on the sour blade only: the sweet/sour choice is still M4's priority rule over what overlaps
	const FGSTN_HitboxSpec Blade = UGSTN_HitReachUtils::ScaleHitboxReach(MakeBlade(), 1.3f);
	TestFalse(TEXT("old tip position is no longer sweet"), Touches(Tip, FVector(90.0, 0.0, 0.0)));
	TestTrue(TEXT("old tip position is on the sour blade"), Touches(Blade, FVector(90.0, 0.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReachKeyedHitboxTest, "GSTNCombat.Core.Reach.KeyedHitbox", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReachKeyedHitboxTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReachTest;
	// A swing in root space: at key 1 the blade is turned 90 degrees, so its length lies along the space's Y axis
	FGSTN_HitboxSpec Swing = MakeBlade();
	Swing.Space = EGSTN_HitboxSpace::Root;
	Swing.ReachAxis = EGSTN_ReachAxis::X;
	FGSTN_HitboxKey Key0;
	Key0.Frame = 0;
	FGSTN_HitboxKey Key1;
	Key1.Frame = 4;
	Key1.Offset = FVector(10.0, 0.0, 0.0);
	Key1.Rotation = FRotator(0.0, 90.0, 0.0);
	Swing.Keys = { Key0, Key1 };

	const FGSTN_HitboxSpec Scaled = UGSTN_HitReachUtils::ScaleHitboxReach(Swing, 1.3f);
	TestTrue(TEXT("key 0 centre scaled along X"), UGSTN_HitReachUtils::GetCentreInSpace(Scaled, 0).Equals(FVector(65.0, 0.0, 0.0), 1e-4));
	TestTrue(TEXT("key 0 stretches the shape's X"), Scaled.Keys[0].Scale.Equals(FVector(1.3, 1.0, 1.0), 1e-3));
	// Key 1: centre (10, 50, 0) in space, only its X scales; the shape's own Y now lies along space X, so Y stretches
	TestTrue(TEXT("key 1 centre scaled along X"), UGSTN_HitReachUtils::GetCentreInSpace(Scaled, 1).Equals(FVector(13.0, 50.0, 0.0), 1e-4));
	TestTrue(TEXT("key 1 stretches the shape's Y"), Scaled.Keys[1].Scale.Equals(FVector(1.0, 1.3, 1.0), 1e-3));
	TestTrue(TEXT("shape itself untouched with keys"), Scaled.Shape.Extent == Swing.Shape.Extent && Scaled.Shape.Local.Equals(Swing.Shape.Local, 0.0));

	// Same inputs, same bits (the derived timeline is built on every machine)
	const FGSTN_HitboxSpec Again = UGSTN_HitReachUtils::ScaleHitboxReach(Swing, 1.3f);
	TestTrue(TEXT("deterministic"), Again.Keys[1].Offset == Scaled.Keys[1].Offset && Again.Keys[1].Scale == Scaled.Keys[1].Scale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReachTimelineAndValidationTest, "GSTNCombat.Core.Reach.TimelineAndValidation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ReachTimelineAndValidationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNReachTest;
	FGSTN_HitboxSpec Fist = MakeBlade();
	Fist.ReachAxis = EGSTN_ReachAxis::None;
	Fist.HitGroup = 1;
	const FGSTN_CombatTimelineData Base = MakeTimeline({ MakeBlade(), MakeTip(), Fist });

	int32 Scaled = 0;
	const FGSTN_CombatTimelineData Long = UGSTN_HitReachUtils::ScaleTimelineReach(Base, 1.3f, Scaled);
	TestEqual(TEXT("two hitboxes scaled"), Scaled, 2);
	const FGSTN_TimelineTrack_Hitbox* Track = Long.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>();
	TestTrue(TEXT("track kept its type"), Track != nullptr);
	if (Track)
	{
		TestTrue(TEXT("tip moved in the timeline"), Track->Hitboxes[1].Shape.Local.GetTranslation().Equals(FVector(117.0, 0.0, 0.0), 1e-3));
		TestTrue(TEXT("unscalable hitbox untouched"), Track->Hitboxes[2].Shape.Local.Equals(Fist.Shape.Local, 0.0));
	}
	TestEqual(TEXT("base timeline not modified"), Base.Tracks[0].GetPtr<FGSTN_TimelineTrack_Hitbox>()->Hitboxes[1].Shape.Local.GetTranslation().X, 90.0);

	// Validator: a clean blade gives no reach warning; a sweetspot without the axis in a reaching group, and a tip pinned to the origin, do
	TestEqual(TEXT("clean group"), CountMessages(Base, TEXT("ReachAxis and some do not")), 0);
	FGSTN_HitboxSpec LeftBehind = MakeTip();
	LeftBehind.ReachAxis = EGSTN_ReachAxis::None;
	TestEqual(TEXT("mixed group warned"), CountMessages(MakeTimeline({ MakeBlade(), LeftBehind }), TEXT("ReachAxis and some do not")), 1);
	FGSTN_HitboxSpec Pinned = MakeTip();
	Pinned.Shape.Local = FTransform::Identity;
	TestEqual(TEXT("pinned tip warned"), CountMessages(MakeTimeline({ Pinned }), TEXT("reach axis cannot change")), 1);
	FGSTN_HitboxSpec Thrown = MakeTip();
	Thrown.Space = EGSTN_HitboxSpace::Projectile;
	TestEqual(TEXT("projectile reach warned"), CountMessages(MakeTimeline({ Thrown }), TEXT("projectile hitbox ignores")), 1);
	return true;
}

#endif
