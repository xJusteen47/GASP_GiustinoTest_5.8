// Automation tests of the hit geometry and data (checkpoint H1). Pure: shapes, hit data and the timeline player over
// in-memory data, no world and no content asset.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Tests/GSTN_HitsTestTypes.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHitsTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	/** Each shape is 10 cm wide along X from its centre, whatever its type: the capsule stands along Z */
	/** TInstancedStruct::Make defaults to the base type: the derived type must be named, or the track is sliced */
	template <typename T>
	FTrackInstance Wrap(const T& Track)
	{
		return FTrackInstance::Make<T>(Track);
	}

	FGSTN_HitShape MakeShape(EGSTN_ShapeType Type)
	{
		FGSTN_HitShape Shape;
		Shape.Type = Type;
		switch (Type)
		{
		case EGSTN_ShapeType::Sphere: Shape.Extent = FVector(10.0, 0.0, 0.0); break;
		case EGSTN_ShapeType::Capsule: Shape.Extent = FVector(10.0, 0.0, 30.0); break;
		case EGSTN_ShapeType::Box: Shape.Extent = FVector(10.0, 10.0, 10.0); break;
		}
		return Shape;
	}

	FTransform At(double X, double Y = 0.0, double Z = 0.0)
	{
		return FTransform(FVector(X, Y, Z));
	}

	bool ExactEqual(const FTransform& A, const FTransform& B)
	{
		return A.GetRotation().X == B.GetRotation().X && A.GetRotation().Y == B.GetRotation().Y && A.GetRotation().Z == B.GetRotation().Z && A.GetRotation().W == B.GetRotation().W
			&& A.GetLocation() == B.GetLocation() && A.GetScale3D() == B.GetScale3D();
	}

	FGSTN_HitboxSpec MakeHitbox(double Radius = 10.0)
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(Radius, 0.0, 0.0);
		return Spec;
	}

	FGSTN_TimelineTrack_Hitbox MakeHitboxTrack(int32 Start, int32 End, int32 Count)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Part.GroupName = TEXT("Hands");
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Track.Hitboxes.Add(MakeHitbox());
		}
		return Track;
	}

	FGSTN_TimelineTrack_HurtboxModifier MakeModifier(int32 Start, int32 End, EGSTN_HurtboxModifierKind Kind, const TArray<FName>& Names)
	{
		FGSTN_TimelineTrack_HurtboxModifier Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Kind = Kind;
		Track.AffectedHurtboxes = Names;
		return Track;
	}

	bool HasMessage(const TArray<FGSTN_TimelineMessage>& Messages, bool bError, const TCHAR* Fragment)
	{
		return Messages.ContainsByPredicate([&](const FGSTN_TimelineMessage& Message) { return Message.bError == bError && Message.Text.Contains(Fragment); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsShapePairsTest, "GSTNCombat.Core.Hits.ShapePairs", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsShapePairsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	const EGSTN_ShapeType Types[3] = { EGSTN_ShapeType::Sphere, EGSTN_ShapeType::Capsule, EGSTN_ShapeType::Box };
	const TCHAR* Names[3] = { TEXT("sphere"), TEXT("capsule"), TEXT("box") };

	// Every ordered pair, centres apart along X: the shapes are 10 wide along X, so 20 touches
	TArray<bool> Results[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		for (int32 I = 0; I < 3; ++I)
		{
			for (int32 J = 0; J < 3; ++J)
			{
				const FGSTN_HitShape A = MakeShape(Types[I]);
				const FGSTN_HitShape B = MakeShape(Types[J]);
				const FString Pair = FString::Printf(TEXT("%s - %s"), Names[I], Names[J]);
				const bool bTouching = UGSTN_HitShapeUtils::Overlap(A, At(0.0), B, At(20.0));
				const bool bSeparated = UGSTN_HitShapeUtils::Overlap(A, At(0.0), B, At(20.01));
				const bool bContained = UGSTN_HitShapeUtils::Overlap(A, At(0.0), B, At(0.0));
				const bool bPartly = UGSTN_HitShapeUtils::Overlap(A, At(0.0), B, At(5.0));
				const bool bAbove = UGSTN_HitShapeUtils::Overlap(A, At(0.0), B, At(0.0, 0.0, 200.0));
				Results[Run].Append({ bTouching, bSeparated, bContained, bPartly, bAbove });
				if (Run == 0)
				{
					TestTrue(*(Pair + TEXT(": touching overlaps")), bTouching);
					TestFalse(*(Pair + TEXT(": separated by a hair does not")), bSeparated);
					TestTrue(*(Pair + TEXT(": contained overlaps")), bContained);
					TestTrue(*(Pair + TEXT(": partly inside overlaps")), bPartly);
					TestFalse(*(Pair + TEXT(": far above does not")), bAbove);
					TestEqual(*(Pair + TEXT(": symmetric")), UGSTN_HitShapeUtils::Overlap(B, At(20.0), A, At(0.0)), bTouching);
				}
			}
		}
	}
	TestTrue(TEXT("two runs give the same answers"), Results[0] == Results[1]);

	// The capsule is long: its tip reaches a sphere above the core segment, and not one past the cap
	const FGSTN_HitShape Capsule = MakeShape(EGSTN_ShapeType::Capsule);
	const FGSTN_HitShape Sphere = MakeShape(EGSTN_ShapeType::Sphere);
	TestTrue(TEXT("capsule tip touches a sphere on the axis"), UGSTN_HitShapeUtils::Overlap(Capsule, At(0.0), Sphere, At(0.0, 0.0, 40.0)));
	TestFalse(TEXT("and not one past the cap"), UGSTN_HitShapeUtils::Overlap(Capsule, At(0.0), Sphere, At(0.0, 0.0, 40.01)));

	// A capsule lying on its side crosses a box its centre is far from
	const FGSTN_HitShape Box = MakeShape(EGSTN_ShapeType::Box);
	const FTransform Lying(FRotator(90.0, 0.0, 0.0), FVector::ZeroVector);
	TestTrue(TEXT("a capsule on its side reaches 30 along X"), UGSTN_HitShapeUtils::Overlap(Capsule, Lying, Box, At(39.0)));
	TestFalse(TEXT("and not past it"), UGSTN_HitShapeUtils::Overlap(Capsule, Lying, Box, At(40.5)));

	// Rotated boxes: a box turned 45 degrees reaches 14.142 along X
	const FTransform Turned(FRotator(0.0, 45.0, 0.0), FVector::ZeroVector);
	TestTrue(TEXT("a turned box overlaps where its corner reaches"), UGSTN_HitShapeUtils::Overlap(Box, Turned, Box, At(24.0)));
	TestFalse(TEXT("and not beyond it"), UGSTN_HitShapeUtils::Overlap(Box, Turned, Box, At(24.3)));

	// The local transform of the shape and its scale count
	FGSTN_HitShape Offset = Sphere;
	Offset.Local = FTransform(FQuat::Identity, FVector(30.0, 0.0, 0.0), FVector(2.0, 2.0, 2.0));
	TestTrue(TEXT("the shape is placed by its local transform, scaled radius 20"), UGSTN_HitShapeUtils::Overlap(Offset, At(0.0), Sphere, At(60.0)));
	TestFalse(TEXT("and no further"), UGSTN_HitShapeUtils::Overlap(Offset, At(0.0), Sphere, At(60.01)));
	TestEqual(TEXT("the variant scale multiplies the extent"), UGSTN_HitShapeUtils::ScaleShape(Sphere, 2.f).Extent.X, 20.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsSweptCatchesFastBoxTest, "GSTNCombat.Core.Hits.SweptCatchesFastBox", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsSweptCatchesFastBoxTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	FGSTN_HitShape Fast = MakeShape(EGSTN_ShapeType::Box);
	Fast.Extent = FVector(5.0, 5.0, 5.0);
	const FGSTN_HitShape Victim = Fast;

	// The hitbox goes from 0 to 60 in one frame; the victim stands at 30 and neither frame's position touches it
	TestFalse(TEXT("the previous position misses"), UGSTN_HitShapeUtils::Overlap(Fast, At(0.0), Victim, At(30.0)));
	TestFalse(TEXT("the current position misses"), UGSTN_HitShapeUtils::Overlap(Fast, At(60.0), Victim, At(30.0)));
	TestTrue(TEXT("the swept test catches it"), UGSTN_HitShapeUtils::SweptOverlap(Fast, At(0.0), At(60.0), Victim, At(30.0)));
	TestTrue(TEXT("the same in the other direction"), UGSTN_HitShapeUtils::SweptOverlap(Fast, At(60.0), At(0.0), Victim, At(30.0)));
	TestFalse(TEXT("a victim off the path is still missed"), UGSTN_HitShapeUtils::SweptOverlap(Fast, At(0.0), At(60.0), Victim, At(30.0, 40.0)));
	TestFalse(TEXT("and one behind the start"), UGSTN_HitShapeUtils::SweptOverlap(Fast, At(0.0), At(60.0), Victim, At(-30.0)));

	TestEqual(TEXT("60 cm over a 5 cm box is capped"), UGSTN_HitShapeUtils::GetSweepSubsteps(Fast, At(0.0), At(60.0)), 4);
	TestEqual(TEXT("a hitbox that does not move takes one step"), UGSTN_HitShapeUtils::GetSweepSubsteps(Fast, At(7.0), At(7.0)), 1);

	// The carrier's rotation is interpolated too: a ball 40 cm out on a swinging arm travels an arc, not the chord
	FGSTN_HitShape OnArm = MakeShape(EGSTN_ShapeType::Sphere);
	OnArm.Extent = FVector(5.0, 0.0, 0.0);
	OnArm.Local = FTransform(FVector(40.0, 0.0, 0.0));
	const FTransform Start(FRotator(0.0, 0.0, 0.0), FVector::ZeroVector);
	const FTransform End(FRotator(0.0, 90.0, 0.0), FVector::ZeroVector);
	FGSTN_HitShape Target = MakeShape(EGSTN_ShapeType::Sphere);
	Target.Extent = FVector(5.0, 0.0, 0.0);
	const double Mid = 40.0 * FMath::Cos(UE_PI / 4.0);
	TestTrue(TEXT("the swing passes the middle of its arc"), UGSTN_HitShapeUtils::SweptOverlap(OnArm, Start, End, Target, At(Mid, Mid)));
	TestFalse(TEXT("which the chord between the two ends does not reach"), UGSTN_HitShapeUtils::SweptOverlap(OnArm, At(0.0), At(-40.0, 40.0), Target, At(Mid, Mid)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsSweptSubstepCapTest, "GSTNCombat.Core.Hits.SweptSubstepCap", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsSweptSubstepCapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	FGSTN_HitShape Ball = MakeShape(EGSTN_ShapeType::Sphere);
	Ball.Extent = FVector(5.0, 0.0, 0.0);

	TestEqual(TEXT("no movement: one step"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(0.0)), 1);
	TestEqual(TEXT("5 cm over a 5 cm ball: one step"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(5.0)), 1);
	TestEqual(TEXT("5.1 cm: two steps"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(5.1)), 2);
	TestEqual(TEXT("15 cm: three steps"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(15.0)), 3);
	TestEqual(TEXT("20 cm: four steps"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(20.0)), 4);
	TestEqual(TEXT("1000 cm: still four"), UGSTN_HitShapeUtils::GetSweepSubsteps(Ball, At(0.0), At(1000.0)), UGSTN_HitShapeUtils::MaxSweepSubsteps);

	FGSTN_HitShape Flat = Ball;
	Flat.Extent = FVector::ZeroVector;
	TestEqual(TEXT("a shape with no volume that moves takes the cap"), UGSTN_HitShapeUtils::GetSweepSubsteps(Flat, At(0.0), At(3.0)), UGSTN_HitShapeUtils::MaxSweepSubsteps);

	// The cap is a limit, not a guarantee: samples at 0, 250, 500, 750, 1000 leave a thin victim between them
	TestFalse(TEXT("a victim between two samples is missed past the cap"), UGSTN_HitShapeUtils::SweptOverlap(Ball, At(0.0), At(1000.0), Ball, At(125.0)));
	TestTrue(TEXT("a victim on a sample is caught"), UGSTN_HitShapeUtils::SweptOverlap(Ball, At(0.0), At(1000.0), Ball, At(250.0)));
	TestTrue(TEXT("the end position counts"), UGSTN_HitShapeUtils::SweptOverlap(Ball, At(0.0), At(1000.0), Ball, At(1000.0)));
	TestTrue(TEXT("and the start position"), UGSTN_HitShapeUtils::SweptOverlap(Ball, At(0.0), At(1000.0), Ball, At(0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsKeyInterpolationTest, "GSTNCombat.Core.Hits.KeyInterpolation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsKeyInterpolationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	TestTrue(TEXT("no keys: identity"), UGSTN_HitShapeUtils::EvaluateKeys({}, 5).Equals(FTransform::Identity, 0.0));

	FGSTN_HitboxKey First;
	First.Frame = 2;
	First.Offset = FVector(0.0, 0.0, 0.0);
	FGSTN_HitboxKey Second;
	Second.Frame = 12;
	Second.Offset = FVector(100.0, 20.0, 0.0);
	Second.Rotation = FRotator(0.0, 90.0, 0.0);
	Second.Scale = FVector(2.0, 2.0, 2.0);
	const TArray<FGSTN_HitboxKey> Keys = { First, Second };

	TestEqual(TEXT("before the first key the first holds"), UGSTN_HitShapeUtils::EvaluateKeys(Keys, 0).GetLocation(), FVector::ZeroVector);
	TestEqual(TEXT("at the first key"), UGSTN_HitShapeUtils::EvaluateKeys(Keys, 2).GetLocation(), FVector::ZeroVector);
	TestEqual(TEXT("after the last key the last holds"), UGSTN_HitShapeUtils::EvaluateKeys(Keys, 40).GetLocation(), Second.Offset);

	const FTransform Middle = UGSTN_HitShapeUtils::EvaluateKeys(Keys, 7);
	TestEqual(TEXT("halfway: offset is linear"), Middle.GetLocation(), FVector(50.0, 10.0, 0.0));
	TestEqual(TEXT("halfway: scale is linear"), Middle.GetScale3D(), FVector(1.5, 1.5, 1.5));
	const FVector Forward = Middle.GetRotation().GetAxisX();
	TestTrue(TEXT("halfway: the rotation is a slerp (45 degrees)"), FMath::IsNearlyEqual(Forward.X, FMath::Cos(UE_PI / 4.0), 1e-6) && FMath::IsNearlyEqual(Forward.Y, FMath::Sin(UE_PI / 4.0), 1e-6));

	const FTransform Quarter = UGSTN_HitShapeUtils::EvaluateKeys(Keys, 4);
	TestEqual(TEXT("a quarter of the way: offset"), Quarter.GetLocation(), FVector(20.0, 4.0, 0.0));

	TestTrue(TEXT("the same frame gives the same bits"), ExactEqual(Middle, UGSTN_HitShapeUtils::EvaluateKeys(Keys, 7)));

	// Two keys on one frame never divide by zero
	const TArray<FGSTN_HitboxKey> Twins = { First, First, Second };
	TestFalse(TEXT("equal frames are finite"), UGSTN_HitShapeUtils::EvaluateKeys(Twins, 2).ContainsNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsVariantOverrideAndMultiplierTest, "GSTNCombat.Core.Hits.VariantOverrideAndMultiplier", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsVariantOverrideAndMultiplierTest::RunTest(const FString& Parameters)
{
	FGSTN_HitData Base;
	Base.Damage = 10.f;
	Base.BaseKnockback = 20.f;
	Base.KnockbackGrowth = 5.f;
	Base.AngleDeg = 30.f;
	Base.Priority = 2;
	Base.ShieldDamage = 8.f;
	Base.HitstunMultiplier = 1.5f;
	Base.HitstopMultiplier = 1.f;
	Base.AttackerHitstopMultiplier = 1.f;
	Base.StaggerForceScale = 1.f;

	// An empty variant is the identity
	const FGSTN_HitData Same = UGSTN_HitDataUtils::ApplyVariant(Base, FGSTN_HitVariant());
	TestEqual(TEXT("identity: damage"), Same.Damage, 10.f);
	TestEqual(TEXT("identity: knockback"), Same.BaseKnockback, 20.f);
	TestEqual(TEXT("identity: fixed knockback stays unused"), Same.FixedKnockback, -1.f);
	TestEqual(TEXT("identity: priority"), Same.Priority, 2);
	TestEqual(TEXT("identity: hitstun"), Same.HitstunMultiplier, 1.5f);

	FGSTN_HitVariant Variant;
	Variant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage | EGSTN_HitDataField::Priority | EGSTN_HitDataField::HitType);
	Variant.Override.Values.Damage = 4.f;
	Variant.Override.Values.Priority = 7;
	Variant.Override.Values.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	Variant.Override.Values.AngleDeg = 99.f;
	Variant.Override.Values.BaseKnockback = 1000.f;
	Variant.Override.DamageMultiplier = 0.5f;
	Variant.Override.KnockbackMultiplier = 2.f;
	Variant.Override.HitstopMultiplier = 0.5f;
	Variant.Override.HitstunMultiplier = 2.f;
	Variant.Override.ShieldDamageMultiplier = 0.f;
	Variant.Override.StaggerForceMultiplier = 0.5f;

	const FGSTN_HitData Result = UGSTN_HitDataUtils::ApplyVariant(Base, Variant);
	TestEqual(TEXT("a named field takes the variant's value, then the multiplier"), Result.Damage, 2.f);
	TestEqual(TEXT("a named integer field"), Result.Priority, 7);
	TestEqual(TEXT("a named tag field"), Result.HitType, GSTNCombatHitTags::HitType_Grab.GetTag());
	TestFalse(TEXT("so the hit is no longer normal"), Result.IsNormalHitType());
	TestEqual(TEXT("a field not named keeps the hit's value although the variant holds one"), Result.AngleDeg, 30.f);
	TestEqual(TEXT("knockback scales: base"), Result.BaseKnockback, 40.f);
	TestEqual(TEXT("knockback scales: growth"), Result.KnockbackGrowth, 10.f);
	TestEqual(TEXT("an unused fixed knockback stays unused"), Result.FixedKnockback, -1.f);
	TestEqual(TEXT("hitstun multiplier scales"), Result.HitstunMultiplier, 3.f);
	TestEqual(TEXT("hitstop multiplier scales"), Result.HitstopMultiplier, 0.5f);
	TestEqual(TEXT("the attacker's hitstop multiplier too"), Result.AttackerHitstopMultiplier, 0.5f);
	TestEqual(TEXT("shield damage scales"), Result.ShieldDamage, 0.f);
	TestEqual(TEXT("stagger force scale halves"), Result.StaggerForceScale, 0.5f);

	// A fixed knockback in use scales as well, and a replaced one is replaced before it scales
	Base.FixedKnockback = 30.f;
	TestEqual(TEXT("fixed knockback scales when used"), UGSTN_HitDataUtils::ApplyVariant(Base, Variant).FixedKnockback, 60.f);
	FGSTN_HitVariant Fixed;
	Fixed.Override.Fields = static_cast<int32>(EGSTN_HitDataField::FixedKnockback);
	Fixed.Override.Values.FixedKnockback = 15.f;
	TestEqual(TEXT("a replaced fixed knockback"), UGSTN_HitDataUtils::ApplyVariant(Base, Fixed).FixedKnockback, 15.f);

	// The input is not touched and the same inputs give the same result
	TestEqual(TEXT("the source hit is unchanged"), Base.Damage, 10.f);
	const FGSTN_HitData Again = UGSTN_HitDataUtils::ApplyVariant(Base, Variant);
	TestEqual(TEXT("two runs agree"), Again.Damage, UGSTN_HitDataUtils::ApplyVariant(Base, Variant).Damage);

	// The hitbox-level default of the owner assumption M4.9
	TestEqual(TEXT("yaw source defaults to the attacker's facing"), FGSTN_HitData().YawSource, EGSTN_YawSource::AttackerFacing);
	TestEqual(TEXT("special angle defaults to none"), FGSTN_HitData().SpecialAngle, EGSTN_SpecialAngle::None);
	TestTrue(TEXT("an unset hit type is normal"), FGSTN_HitData().IsNormalHitType());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsVariantExtensionOverrideTest, "GSTNCombat.Core.Hits.VariantExtensionOverride", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsVariantExtensionOverrideTest::RunTest(const FString& Parameters)
{
	FGSTN_TestHitExtensionA A;
	A.Value = 1;
	FGSTN_TestHitExtensionB B;
	B.Value = 2;
	FGSTN_HitData Base;
	Base.ExtensionData.Add(FInstancedStruct::Make(A));
	Base.ExtensionData.Add(FInstancedStruct::Make(B));

	FGSTN_TestHitExtensionA NewA;
	NewA.Value = 9;
	FGSTN_HitVariant Replace;
	Replace.Override.ExtensionOverrides.Add(FInstancedStruct::Make(NewA));
	const FGSTN_HitData Replaced = UGSTN_HitDataUtils::ApplyVariant(Base, Replace);
	if (TestEqual(TEXT("the entry of the same type is replaced, not added"), Replaced.ExtensionData.Num(), 2))
	{
		TestEqual(TEXT("in place: first entry keeps its slot and type"), Replaced.ExtensionData[0].GetScriptStruct() == FGSTN_TestHitExtensionA::StaticStruct(), true);
		TestEqual(TEXT("with the variant's value"), Replaced.ExtensionData[0].Get<FGSTN_TestHitExtensionA>().Value, 9);
		TestEqual(TEXT("the other type is untouched"), Replaced.ExtensionData[1].Get<FGSTN_TestHitExtensionB>().Value, 2);
	}
	TestEqual(TEXT("the source hit keeps its own"), Base.ExtensionData[0].Get<FGSTN_TestHitExtensionA>().Value, 1);

	FGSTN_TestHitExtensionC NewC;
	NewC.Value = 5;
	FGSTN_HitVariant Add;
	Add.Override.ExtensionOverrides.Add(FInstancedStruct::Make(NewC));
	const FGSTN_HitData Added = UGSTN_HitDataUtils::ApplyVariant(Base, Add);
	if (TestEqual(TEXT("a type the hit does not carry is appended"), Added.ExtensionData.Num(), 3))
	{
		TestEqual(TEXT("at the end"), Added.ExtensionData[2].Get<FGSTN_TestHitExtensionC>().Value, 5);
	}

	FGSTN_HitVariant Empty;
	Empty.Override.ExtensionOverrides.Add(FInstancedStruct());
	TestEqual(TEXT("an empty override entry is skipped"), UGSTN_HitDataUtils::ApplyVariant(Base, Empty).ExtensionData.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsPickHurtboxTest, "GSTNCombat.Core.Hits.PickHurtboxPriorityThenIndex", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsPickHurtboxTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	for (const uint8 Priority : { 0, 2, 2, 1 })
	{
		FGSTN_Hurtbox& Hurtbox = Set->Hurtboxes.AddDefaulted_GetRef();
		Hurtbox.Priority = Priority;
	}

	TestEqual(TEXT("the higher priority wins"), UGSTN_HitShapeUtils::PickHurtbox({ 0, 3 }, Set.Get()), 3);
	TestEqual(TEXT("a tie goes to the lower index"), UGSTN_HitShapeUtils::PickHurtbox({ 1, 2 }, Set.Get()), 1);
	TestEqual(TEXT("the order of the overlap list does not matter"), UGSTN_HitShapeUtils::PickHurtbox({ 2, 1 }, Set.Get()), 1);
	TestEqual(TEXT("a single hurtbox"), UGSTN_HitShapeUtils::PickHurtbox({ 0 }, Set.Get()), 0);
	TestEqual(TEXT("all four"), UGSTN_HitShapeUtils::PickHurtbox({ 3, 2, 1, 0 }, Set.Get()), 1);
	TestEqual(TEXT("nothing overlapped"), UGSTN_HitShapeUtils::PickHurtbox({}, Set.Get()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("an index outside the set is skipped"), UGSTN_HitShapeUtils::PickHurtbox({ 9, 2 }, Set.Get()), 2);
	TestEqual(TEXT("only invalid indices"), UGSTN_HitShapeUtils::PickHurtbox({ 9, -1 }, Set.Get()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no set"), UGSTN_HitShapeUtils::PickHurtbox({ 0 }, nullptr), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsModifierTrackByNameOrTagTest, "GSTNCombat.Core.Hits.ModifierTrackByNameOrTag", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsModifierTrackByNameOrTagTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;

	// The leaves come from the plugin tag ini, the root is native
	const FGameplayTag Feet = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Feet"), false);
	const FGameplayTag Head = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Head"), false);
	const FGameplayTag Torso = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Torso"), false);
	if (!TestTrue(TEXT("the three hurtbox leaves exist (plugin tag ini)"), Feet.IsValid() && Head.IsValid() && Torso.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("and sit under the native root"), Feet.MatchesTag(GSTNCombatHitTags::Root_Hurtbox.GetTag()));

	FGSTN_Hurtbox Foot;
	Foot.Name = TEXT("Foot");
	Foot.Tags.AddTag(Feet);
	FGSTN_Hurtbox Skull;
	Skull.Name = TEXT("Skull");
	Skull.Tags.AddTag(Head);
	FGSTN_Hurtbox Chest;
	Chest.Name = TEXT("Chest");
	Chest.Tags.AddTag(Torso);
	FGSTN_Hurtbox Hand;
	Hand.Name = TEXT("Hand");

	FGSTN_TimelineTrack_HurtboxModifier Empty;
	TestTrue(TEXT("a selector with nothing set is empty"), Empty.IsEmpty());
	TestFalse(TEXT("and addresses nothing"), Empty.Addresses(Foot) || Empty.Addresses(Hand));

	FGSTN_TimelineTrack_HurtboxModifier ByName;
	ByName.AffectedHurtboxes = { TEXT("Skull") };
	TestTrue(TEXT("by name"), ByName.Addresses(Skull));
	TestFalse(TEXT("only that name"), ByName.Addresses(Foot) || ByName.Addresses(Chest) || ByName.Addresses(Hand));

	FGSTN_TimelineTrack_HurtboxModifier ByTag;
	ByTag.AffectedTags.AddTag(Feet);
	TestTrue(TEXT("by tag"), ByTag.Addresses(Foot));
	TestFalse(TEXT("only that tag"), ByTag.Addresses(Skull) || ByTag.Addresses(Chest) || ByTag.Addresses(Hand));

	FGSTN_TimelineTrack_HurtboxModifier Union;
	Union.AffectedHurtboxes = { TEXT("Skull") };
	Union.AffectedTags.AddTag(Feet);
	Union.Part.PartName = TEXT("Hand");
	TestTrue(TEXT("union: the named hurtbox"), Union.Addresses(Skull));
	TestTrue(TEXT("union: the tagged hurtbox"), Union.Addresses(Foot));
	TestTrue(TEXT("union: the part selector's name"), Union.Addresses(Hand));
	TestFalse(TEXT("union: nothing else"), Union.Addresses(Chest));

	FGSTN_TimelineTrack_HurtboxModifier ByRoot;
	ByRoot.Part.PartTag = GSTNCombatHitTags::Root_Hurtbox.GetTag();
	TestTrue(TEXT("a root tag matches its leaves"), ByRoot.Addresses(Foot) && ByRoot.Addresses(Skull) && ByRoot.Addresses(Chest));
	TestFalse(TEXT("but not an untagged hurtbox"), ByRoot.Addresses(Hand));

	// The states under the open windows of a timeline
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 30;
	Data.FirstActionableFrame = 20;
	Data.OverrideFrames = 0;
	FGSTN_TimelineTrack_HurtboxModifier Invincible = MakeModifier(0, 10, EGSTN_HurtboxModifierKind::Invincible, { TEXT("Foot"), TEXT("Chest") });
	FGSTN_TimelineTrack_HurtboxModifier Intangible = MakeModifier(2, 6, EGSTN_HurtboxModifierKind::Intangible, {});
	Intangible.AffectedTags.AddTag(Feet);
	FGSTN_TimelineTrack_HurtboxModifier ArmorLow = MakeModifier(12, 20, EGSTN_HurtboxModifierKind::Armor, { TEXT("Chest") });
	ArmorLow.ArmorThreshold.Value = 30.f;
	FGSTN_TimelineTrack_HurtboxModifier ArmorHigh = MakeModifier(12, 20, EGSTN_HurtboxModifierKind::Armor, { TEXT("Chest") });
	ArmorHigh.ArmorThreshold.Value = 50.f;
	FGSTN_TimelineTrack_HurtboxModifier SuperArmor = MakeModifier(14, 16, EGSTN_HurtboxModifierKind::Armor, { TEXT("Chest") });
	SuperArmor.ArmorThreshold.bInfinite = true;
	FGSTN_TimelineTrack_HurtboxModifier Portion = MakeModifier(0, 30, EGSTN_HurtboxModifierKind::Intangible, { TEXT("Skull") });
	Portion.ShapeOverrides.Add(MakeShape(EGSTN_ShapeType::Sphere));
	for (const FGSTN_TimelineTrack_HurtboxModifier& Track : { Invincible, Intangible, ArmorLow, ArmorHigh, SuperArmor, Portion })
	{
		Data.Tracks.Add(Wrap(Track));
	}

	FGSTN_TimelineFrameResult Result;
	FGSTN_TimelinePlayer::EvaluateHits(Data, 1, Result);
	TestEqual(TEXT("frame 1: invincible by name"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Foot).State, EGSTN_HurtState::Invincible);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 3, Result);
	TestEqual(TEXT("frame 3: intangible beats invincible (feet by tag)"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Foot).State, EGSTN_HurtState::Intangible);
	TestEqual(TEXT("frame 3: the chest is only invincible"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest).State, EGSTN_HurtState::Invincible);
	TestEqual(TEXT("a modifier that covers a portion is left to the step that has the contact"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Skull).State, EGSTN_HurtState::Normal);
	TestEqual(TEXT("a hurtbox nobody addresses is normal"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Hand).State, EGSTN_HurtState::Normal);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 8, Result);
	TestEqual(TEXT("frame 8: only the invincible window is left"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Foot).State, EGSTN_HurtState::Invincible);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 11, Result);
	TestEqual(TEXT("frame 11: back to normal"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Foot).State, EGSTN_HurtState::Normal);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 13, Result);
	const FGSTN_HurtboxFrameState Armored = FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest);
	TestEqual(TEXT("frame 13: armor"), Armored.State, EGSTN_HurtState::Armor);
	TestEqual(TEXT("two armor windows keep the larger threshold"), Armored.ArmorThreshold.Value, 50.f);
	TestFalse(TEXT("which is finite"), Armored.ArmorThreshold.bInfinite);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 15, Result);
	TestTrue(TEXT("frame 15: super armor is infinite"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest).ArmorThreshold.bInfinite);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 20, Result);
	TestEqual(TEXT("frame 20: the windows are half open"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest).State, EGSTN_HurtState::Normal);

	// Armor never lowers a stronger state, whatever the order of the tracks
	FGSTN_CombatTimelineData Mixed;
	Mixed.TotalFrames = 10;
	Mixed.FirstActionableFrame = 5;
	Mixed.OverrideFrames = 0;
	FGSTN_TimelineTrack_HurtboxModifier Armor = MakeModifier(0, 10, EGSTN_HurtboxModifierKind::Armor, { TEXT("Chest") });
	Armor.ArmorThreshold.Value = 10.f;
	Mixed.Tracks.Add(Wrap(MakeModifier(0, 10, EGSTN_HurtboxModifierKind::Invincible, { TEXT("Chest") })));
	Mixed.Tracks.Add(Wrap(Armor));
	FGSTN_TimelinePlayer::EvaluateHits(Mixed, 3, Result);
	TestEqual(TEXT("invincible before armor"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest).State, EGSTN_HurtState::Invincible);
	Mixed.Tracks.Swap(0, 1);
	FGSTN_TimelinePlayer::EvaluateHits(Mixed, 3, Result);
	TestEqual(TEXT("and after it"), FGSTN_TimelinePlayer::ResolveHurtState(Result, Chest).State, EGSTN_HurtState::Invincible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsBakedSocketLookupTest, "GSTNCombat.Core.Hits.BakedSocketLookup", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsBakedSocketLookupTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 30;
	Data.FirstActionableFrame = 20;
	Data.OverrideFrames = 0;
	FGSTN_BakedSocketFrames& Hand = Data.BakedSockets.Add(TEXT("hand_r"));
	Hand.Frames = { At(10.0), At(20.0), At(30.0) };
	Data.BakedSockets.Add(TEXT("empty"));

	FTransform Out;
	TestTrue(TEXT("a baked socket is found"), Data.FindBakedSocket(TEXT("hand_r"), 0, Out));
	TestEqual(TEXT("frame 0"), Out.GetLocation().X, 10.0);
	TestTrue(TEXT("found"), Data.FindBakedSocket(TEXT("hand_r"), 1, Out));
	TestEqual(TEXT("frame 1"), Out.GetLocation().X, 20.0);
	TestTrue(TEXT("found"), Data.FindBakedSocket(TEXT("hand_r"), 2, Out));
	TestEqual(TEXT("the last frame"), Out.GetLocation().X, 30.0);
	TestTrue(TEXT("found"), Data.FindBakedSocket(TEXT("hand_r"), 25, Out));
	TestEqual(TEXT("past the baked range the last holds"), Out.GetLocation().X, 30.0);
	TestTrue(TEXT("found"), Data.FindBakedSocket(TEXT("hand_r"), -4, Out));
	TestEqual(TEXT("a negative frame reads the first"), Out.GetLocation().X, 10.0);
	TestFalse(TEXT("an unknown socket is not found"), Data.FindBakedSocket(TEXT("foot_l"), 0, Out));
	TestFalse(TEXT("a socket with no frames is not found"), Data.FindBakedSocket(TEXT("empty"), 0, Out));

	// The validator tells the designer what the bake is missing
	FGSTN_TimelineTrack_Hitbox Track = MakeHitboxTrack(2, 5, 1);
	Track.Hitboxes[0].Space = EGSTN_HitboxSpace::BakedSocket;
	Track.Hitboxes[0].Socket = TEXT("hand_r");
	Data.Tracks.Add(Wrap(Track));
	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("a bake shorter than the timeline is a warning"), HasMessage(Messages, false, TEXT("baked for 3 frames")));
	TestFalse(TEXT("not an error"), Messages.ContainsByPredicate([](const FGSTN_TimelineMessage& Message) { return Message.bError; }));

	Data.Tracks.Reset();
	Track.Hitboxes[0].Socket = TEXT("foot_l");
	Data.Tracks.Add(Wrap(Track));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("a socket that was never baked is a warning"), HasMessage(Messages, false, TEXT("no baked frames")));

	Data.Tracks.Reset();
	Track.Hitboxes[0].Socket = NAME_None;
	Data.Tracks.Add(Wrap(Track));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("a BakedSocket hitbox with no socket name is an error"), HasMessage(Messages, true, TEXT("names no socket")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsTimelinePlayerHitboxesNoAllocTest, "GSTNCombat.Core.Hits.TimelinePlayerHitboxesNoAlloc", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsTimelinePlayerHitboxesNoAllocTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 30;
	Data.FirstActionableFrame = 20;
	Data.OverrideFrames = 0;
	Data.Tracks.Add(Wrap(MakeHitboxTrack(3, 8, 2)));
	Data.Tracks.Add(Wrap(MakeHitboxTrack(6, 12, 1)));
	Data.Tracks.Add(Wrap(MakeModifier(0, 10, EGSTN_HurtboxModifierKind::Invincible, { TEXT("Foot") })));

	FGSTN_TimelineFrameResult Result;
	Result.Reserve(Data);
	const FGSTN_ActiveHitbox* const HitboxMemory = Result.Hitboxes.GetData();
	const int32 HitboxCapacity = Result.Hitboxes.Max();
	const FGSTN_TimelineTrack_HurtboxModifier* const* const ModifierMemory = Result.HurtModifiers.GetData();
	const int32 ModifierCapacity = Result.HurtModifiers.Max();
	TestTrue(TEXT("the scratch is sized for the three hitboxes and the modifier"), HitboxCapacity >= 3 && ModifierCapacity >= 1);

	TArray<int32> Counts[2];
	TArray<int32> TrackFrames[2];
	bool bStable = true;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Frame = -1; Frame <= 35; ++Frame)
		{
			FGSTN_TimelinePlayer::EvaluateHits(Data, Frame, Result);
			bStable &= Result.Hitboxes.GetData() == HitboxMemory && Result.Hitboxes.Max() == HitboxCapacity;
			bStable &= Result.HurtModifiers.GetData() == ModifierMemory && Result.HurtModifiers.Max() == ModifierCapacity;
			Counts[Pass].Add(Result.Hitboxes.Num());
			for (const FGSTN_ActiveHitbox& Active : Result.Hitboxes)
			{
				TrackFrames[Pass].Add(Active.TrackFrame);
			}
		}
	}
	TestTrue(TEXT("the buffers never move or grow after Reserve: no allocation per frame"), bStable);
	TestTrue(TEXT("two passes report the same hitboxes"), Counts[0] == Counts[1] && TrackFrames[0] == TrackFrames[1]);

	FGSTN_TimelinePlayer::EvaluateHits(Data, 2, Result);
	TestEqual(TEXT("frame 2: no hitbox yet"), Result.Hitboxes.Num(), 0);
	TestEqual(TEXT("frame 2: the modifier is open"), Result.HurtModifiers.Num(), 1);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 3, Result);
	TestEqual(TEXT("frame 3: the first track opens with two hitboxes"), Result.Hitboxes.Num(), 2);
	TestEqual(TEXT("keys count from the track's start"), Result.Hitboxes[0].TrackFrame, 0);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 6, Result);
	TestEqual(TEXT("frame 6: both tracks"), Result.Hitboxes.Num(), 3);
	TestEqual(TEXT("the first track is 3 frames in"), Result.Hitboxes[0].TrackFrame, 3);
	TestEqual(TEXT("the second starts at 0"), Result.Hitboxes[2].TrackFrame, 0);
	TestTrue(TEXT("the pointers lead into the data"), Result.Hitboxes[2].Spec == &static_cast<const FGSTN_TimelineTrack_Hitbox*>(Data.Tracks[1].GetPtr<FGSTN_TimelineTrack>())->Hitboxes[0]);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 8, Result);
	TestEqual(TEXT("frame 8: the first track is closed (half open)"), Result.Hitboxes.Num(), 1);
	FGSTN_TimelinePlayer::EvaluateHits(Data, 12, Result);
	TestEqual(TEXT("frame 12: none"), Result.Hitboxes.Num(), 0);

	// The per-frame output of the rest of the player is unchanged by hitbox tracks
	FGSTN_TimelineOutput Output;
	FGSTN_TimelineEvalParams Params;
	Params.Frame = 6;
	Params.PreviousFrame = 5;
	FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	TestEqual(TEXT("Evaluate still ignores the hit tracks"), Output.CancelWindows.Num() + Output.ActiveMovement.Num() + Output.Events.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitsValidatorHitTypeTest, "GSTNCombat.Core.Hits.ValidatorHitTypeNonNormalWarns", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitsValidatorHitTypeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHitsTest;
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 20;
	Data.FirstActionableFrame = 10;
	Data.OverrideFrames = 0;
	FGSTN_TimelineTrack_Hitbox Track = MakeHitboxTrack(2, 5, 1);
	Data.Tracks.Add(Wrap(Track));

	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestEqual(TEXT("an unset hit type is Normal: nothing to report"), Messages.Num(), 0);

	Data.Tracks.Reset();
	Track.Hitboxes[0].Data.HitType = GSTNCombatHitTags::HitType_Normal.GetTag();
	Data.Tracks.Add(Wrap(Track));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestEqual(TEXT("an explicit Normal too"), Messages.Num(), 0);

	for (const FGameplayTag& Type : { GSTNCombatHitTags::HitType_Grab.GetTag(), GSTNCombatHitTags::HitType_Projectile.GetTag(), GSTNCombatHitTags::HitType_Unblockable.GetTag() })
	{
		Data.Tracks.Reset();
		Track.Hitboxes[0].Data.HitType = Type;
		Data.Tracks.Add(Wrap(Track));
		Messages.Reset();
		FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
		TestTrue(*FString::Printf(TEXT("%s warns"), *Type.ToString()), HasMessage(Messages, false, TEXT("HitType")));
		TestFalse(*FString::Printf(TEXT("%s is not an error"), *Type.ToString()), Messages.ContainsByPredicate([](const FGSTN_TimelineMessage& Message) { return Message.bError; }));
	}

	// A relationship variant that turns the hit into a grab warns as well
	Data.Tracks.Reset();
	Track.Hitboxes[0].Data.HitType = FGameplayTag();
	FGSTN_HitVariant Variant;
	Variant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::HitType);
	Variant.Override.Values.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	Track.Hitboxes[0].RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag(), Variant);
	Data.Tracks.Add(Wrap(Track));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("a variant with a non-Normal type warns"), HasMessage(Messages, false, TEXT("variant")));

	// Shape and key errors
	Data.Tracks.Reset();
	FGSTN_TimelineTrack_Hitbox Broken = MakeHitboxTrack(2, 5, 1);
	Broken.Hitboxes[0].Shape.Extent = FVector::ZeroVector;
	FGSTN_HitboxKey KeyA;
	KeyA.Frame = 3;
	FGSTN_HitboxKey KeyB;
	KeyB.Frame = 3;
	Broken.Hitboxes[0].Keys = { KeyA, KeyB };
	Data.Tracks.Add(Wrap(Broken));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("a shape with no volume is an error"), HasMessage(Messages, true, TEXT("no volume")));
	TestTrue(TEXT("keys that do not strictly increase are an error"), HasMessage(Messages, true, TEXT("not sorted")));

	// An armor window without a threshold does nothing, which the designer should hear
	Data.Tracks.Reset();
	Data.Tracks.Add(Wrap(MakeModifier(2, 6, EGSTN_HurtboxModifierKind::Armor, { TEXT("Foot") })));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidationContext(), Messages);
	TestTrue(TEXT("armor with no threshold warns"), HasMessage(Messages, false, TEXT("ArmorThreshold")));
	TestFalse(TEXT("a modifier addressed by the name list alone is a valid selector"), HasMessage(Messages, true, TEXT("addresses no part")));
	return true;
}

#endif
