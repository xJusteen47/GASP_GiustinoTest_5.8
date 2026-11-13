#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/SkeletalMesh.h"
#include "GameplayTagsManager.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "Hurtbox/GSTN_HurtboxGenerator.h"
#include "Hurtbox/GSTN_HurtboxMirror.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "ReferenceSkeleton.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Tests/GSTN_PreviewTestHelpers.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"

#if WITH_DEV_AUTOMATION_TESTS

// The hurtbox set editor (plan m5 E9, combat-m5-editor 8): the generator over a Physics Asset made in memory, the regeneration merge, the mirror and
// its name matching, the edit model with its undo, and the findings. Pure code, no renderer. The skeleton is built by hand from a reference skeleton
// (a person with sides, then a creature without), so nothing here depends on an imported asset or assumes a bone name outside the test's own.

#define GSTN_HURTBOX_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNHurtboxTest
{
	/** A reference skeleton built bone by bone. Each Add gets its own modifier: the final arrays are rebuilt when the modifier goes away */
	struct FSkeleton
	{
		FReferenceSkeleton Ref;

		void AddBone(FName Name, FName Parent, const FVector& Location)
		{
			FReferenceSkeletonModifier Modifier(Ref, nullptr);
			const int32 ParentIndex = Parent.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(Parent);
			Modifier.Add(FMeshBoneInfo(Name, Name.ToString(), ParentIndex), FTransform(FQuat::Identity, Location));
		}
	};

	/**
	 * A person: component-space positions are root (0,0,0), pelvis (0,0,100), spine_01 (0,0,120), head (0,0,170), upperarm_l (0,-20,150), upperarm_r (0,20,150),
	 * hand_l (0,-50,150), hand_r (0,50,150), finger_l (0,-55,150)
	 */
	FSkeleton MakePerson()
	{
		FSkeleton Skeleton;
		Skeleton.AddBone(TEXT("root"), NAME_None, FVector::ZeroVector);
		Skeleton.AddBone(TEXT("pelvis"), TEXT("root"), FVector(0, 0, 100));
		Skeleton.AddBone(TEXT("spine_01"), TEXT("pelvis"), FVector(0, 0, 20));
		Skeleton.AddBone(TEXT("head"), TEXT("spine_01"), FVector(0, 0, 50));
		Skeleton.AddBone(TEXT("upperarm_l"), TEXT("spine_01"), FVector(0, -20, 30));
		Skeleton.AddBone(TEXT("upperarm_r"), TEXT("spine_01"), FVector(0, 20, 30));
		Skeleton.AddBone(TEXT("hand_l"), TEXT("upperarm_l"), FVector(0, -30, 0));
		Skeleton.AddBone(TEXT("hand_r"), TEXT("upperarm_r"), FVector(0, 30, 0));
		Skeleton.AddBone(TEXT("finger_l"), TEXT("hand_l"), FVector(0, -5, 0));
		return Skeleton;
	}

	USkeletalBodySetup* AddBody(UPhysicsAsset& Asset, FName Bone)
	{
		USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(&Asset);
		Body->BoneName = Bone;
		Asset.SkeletalBodySetups.Add(Body);
		return Body;
	}

	/** One body per bone of the person, with the shape each test counts on */
	TStrongObjectPtr<UPhysicsAsset> MakePersonPhysicsAsset()
	{
		TStrongObjectPtr<UPhysicsAsset> Asset(NewObject<UPhysicsAsset>(GetTransientPackage()));
		AddBody(*Asset, TEXT("pelvis"))->AggGeom.SphylElems.Add(FKSphylElem(12.f, 10.f));
		{
			USkeletalBodySetup* Spine = AddBody(*Asset, TEXT("spine_01"));
			FKSphereElem Sphere;
			Sphere.Radius = 9.f;
			Sphere.Center = FVector(0, 0, 3);
			Spine->AggGeom.SphereElems.Add(Sphere);
			FKBoxElem Box;
			Box.X = 20.f;
			Box.Y = 30.f;
			Box.Z = 10.f;
			Spine->AggGeom.BoxElems.Add(Box);
		}
		{
			USkeletalBodySetup* Head = AddBody(*Asset, TEXT("head"));
			FKSphereElem Sphere;
			Sphere.Radius = 10.f;
			Sphere.Center = FVector(0, 0, 5);
			Head->AggGeom.SphereElems.Add(Sphere);
		}
		AddBody(*Asset, TEXT("upperarm_l"))->AggGeom.SphylElems.Add(FKSphylElem(5.f, 14.f));
		AddBody(*Asset, TEXT("upperarm_r"))->AggGeom.SphylElems.Add(FKSphylElem(5.f, 14.f));
		AddBody(*Asset, TEXT("hand_l"))->AggGeom.SphereElems.Add(FKSphereElem(6.f));
		AddBody(*Asset, TEXT("hand_r"))->AggGeom.SphereElems.Add(FKSphereElem(6.f));
		return Asset;
	}

	TArray<FGSTN_MirrorNamePair> DefaultPairs()
	{
		TArray<FGSTN_MirrorNamePair> Pairs;
		FGSTN_MirrorNamePair Underscore;
		Underscore.Left = TEXT("_l");
		Underscore.Right = TEXT("_r");
		Pairs.Add(Underscore);
		FGSTN_MirrorNamePair Word;
		Word.Left = TEXT("Left");
		Word.Right = TEXT("Right");
		Word.bCaseSensitive = true;
		Pairs.Add(Word);
		FGSTN_MirrorNamePair Dot;
		Dot.Left = TEXT(".L");
		Dot.Right = TEXT(".R");
		Dot.bCaseSensitive = true;
		Pairs.Add(Dot);
		return Pairs;
	}

	FGSTN_Hurtbox MakeHurtbox(FName Name, FName Bone, const FVector& Location, double Radius = 10.0)
	{
		FGSTN_Hurtbox Hurtbox;
		Hurtbox.Name = Name;
		Hurtbox.Bone = Bone;
		Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
		Hurtbox.Shape.Extent = FVector(Radius, Radius, Radius);
		Hurtbox.Shape.Local = FTransform(FQuat::Identity, Location);
		return Hurtbox;
	}

	const FGSTN_Hurtbox* Find(const FGSTN_HurtboxGenResult& Result, FName Name)
	{
		return Result.Hurtboxes.FindByPredicate([Name](const FGSTN_Hurtbox& Hurtbox) { return Hurtbox.Name == Name; });
	}
}

using namespace GSTNHurtboxTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxGenCountsTest, "GSTNCombat.Editor.Hurtbox.FromPhysicsAssetCountsPerPrimitive", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxGenCountsTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();
	const FGSTN_HurtboxGenResult Result = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());

	TestEqual(TEXT("seven bodies"), Result.BodyCount, 7);
	TestEqual(TEXT("eight primitives: spine_01 has two"), Result.PrimitiveCount, 8);
	TestEqual(TEXT("one hurtbox per primitive"), Result.Hurtboxes.Num(), 8);
	TestTrue(TEXT("no warning for sphyl, sphere and box"), Result.Warnings.IsEmpty());

	// A body with one primitive is named after its bone, a body with several bone_N, N counting from 0 in the order spheres, boxes, capsules
	TestNotNull(TEXT("the pelvis keeps the bone name"), Find(Result, TEXT("pelvis")));
	const FGSTN_Hurtbox* Sphere = Find(Result, TEXT("spine_01_0"));
	const FGSTN_Hurtbox* Box = Find(Result, TEXT("spine_01_1"));
	if (TestNotNull(TEXT("the first primitive of the spine"), Sphere) && TestNotNull(TEXT("the second primitive of the spine"), Box))
	{
		TestTrue(TEXT("sphere stays a Sphere"), Sphere->Shape.Type == EGSTN_ShapeType::Sphere);
		TestEqual(TEXT("its radius"), Sphere->Shape.Extent.X, 9.0, 1e-6);
		TestTrue(TEXT("box stays a Box"), Box->Shape.Type == EGSTN_ShapeType::Box);
		TestTrue(TEXT("a box holds half of the lengths the Physics Asset stores"), Box->Shape.Extent.Equals(FVector(10, 15, 5), 1e-6));
		TestEqual(TEXT("both remember the bone"), Box->Bone, FName(TEXT("spine_01")));
	}
	if (const FGSTN_Hurtbox* Pelvis = Find(Result, TEXT("pelvis")))
	{
		TestTrue(TEXT("a sphyl is a Capsule"), Pelvis->Shape.Type == EGSTN_ShapeType::Capsule);
		TestEqual(TEXT("radius"), Pelvis->Shape.Extent.X, 12.0, 1e-6);
		TestEqual(TEXT("half height includes the hemispheres: 12 + 10 / 2"), Pelvis->Shape.Extent.Z, 17.0, 1e-6);
	}
	// The names are unique: a set addresses a hurtbox by name
	TSet<FName> Names;
	for (const FGSTN_Hurtbox& Hurtbox : Result.Hurtboxes)
	{
		Names.Add(Hurtbox.Name);
	}
	TestEqual(TEXT("every name is used once"), Names.Num(), Result.Hurtboxes.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxGenConvexTest, "GSTNCombat.Editor.Hurtbox.ConvexFallsBackToCapsuleWithWarning", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxGenConvexTest::RunTest(const FString& Parameters)
{
	FSkeleton Skeleton = MakePerson();
	TStrongObjectPtr<UPhysicsAsset> Asset(NewObject<UPhysicsAsset>(GetTransientPackage()));

	// A convex body 10 wide, 6 deep and 40 long along X, off centre: the capsule must lie along X
	{
		USkeletalBodySetup* Body = AddBody(*Asset, TEXT("hand_l"));
		FKConvexElem Convex;
		for (const double X : { 0.0, 40.0 })
		{
			for (const double Y : { -3.0, 3.0 })
			{
				for (const double Z : { -5.0, 5.0 })
				{
					Convex.VertexData.Add(FVector(X + 100.0, Y, Z));
				}
			}
		}
		Convex.UpdateElemBox();
		Body->AggGeom.ConvexElems.Add(Convex);
	}
	{
		USkeletalBodySetup* Body = AddBody(*Asset, TEXT("hand_r"));
		Body->AggGeom.TaperedCapsuleElems.Add(FKTaperedCapsuleElem(4.f, 7.f, 20.f));
	}

	const FGSTN_HurtboxGenResult Result = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());
	TestEqual(TEXT("two hurtboxes"), Result.Hurtboxes.Num(), 2);
	TestEqual(TEXT("both are fallbacks"), Result.BoundingFallbacks, 2);
	TestEqual(TEXT("each says so"), Result.Warnings.Num(), 2);

	const FGSTN_Hurtbox* Convex = Find(Result, TEXT("hand_l"));
	if (TestNotNull(TEXT("the convex body made a hurtbox"), Convex))
	{
		TestTrue(TEXT("a capsule"), Convex->Shape.Type == EGSTN_ShapeType::Capsule);
		TestEqual(TEXT("half length 20 along the longest side"), Convex->Shape.Extent.Z, 20.0, 1e-6);
		TestEqual(TEXT("radius is the larger of the other half sides (5)"), Convex->Shape.Extent.X, 5.0, 1e-6);
		// The capsule axis (local Z) must point along the box's X, in root space (identity placement, bone at (0,-50,150))
		const FVector Axis = Convex->Shape.Local.GetUnitAxis(EAxis::Z);
		TestTrue(TEXT("the axis lies along X"), FMath::Abs(Axis.X) > 0.999);
		TestTrue(TEXT("the centre of the box (120, 0, 0) from the bone"), Convex->Shape.Local.GetLocation().Equals(FVector(120, -50, 150), 1e-3));
	}
	const FGSTN_Hurtbox* Tapered = Find(Result, TEXT("hand_r"));
	if (TestNotNull(TEXT("the tapered capsule made a hurtbox"), Tapered))
	{
		TestEqual(TEXT("the larger radius all along"), Tapered->Shape.Extent.X, 7.0, 1e-6);
		TestEqual(TEXT("half height: the segment half plus the larger radius"), Tapered->Shape.Extent.Z, 17.0, 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxGenRootRelativeTest, "GSTNCombat.Editor.Hurtbox.RootRelativePositions", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxGenRootRelativeTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();

	// No placement: component space is root space. The head body is a sphere 5 above the head bone at (0,0,170)
	FGSTN_HurtboxGenOptions Options;
	FGSTN_HurtboxGenResult Result = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Options);
	if (const FGSTN_Hurtbox* Head = Find(Result, TEXT("head")))
	{
		TestTrue(TEXT("bone reference pose x primitive centre"), Head->Shape.Local.GetLocation().Equals(FVector(0, 0, 175), 1e-3));
	}
	if (const FGSTN_Hurtbox* Hand = Find(Result, TEXT("hand_l")))
	{
		TestTrue(TEXT("the component-space position of a bone under three parents"), Hand->Shape.Local.GetLocation().Equals(FVector(0, -50, 150), 1e-3));
	}

	// With the preview scene's placement (a turn of 90 degrees about Z and a drop of 100 cm): the same chain, then the placement
	Options.MeshToRoot = FTransform(FRotator(0, 90, 0), FVector(0, 0, -100));
	Result = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Options);
	if (const FGSTN_Hurtbox* Hand = Find(Result, TEXT("hand_l")))
	{
		const FVector Expected = Options.MeshToRoot.TransformPosition(FVector(0, -50, 150));
		TestTrue(TEXT("root-relative = component space x mesh placement"), Hand->Shape.Local.GetLocation().Equals(Expected, 1e-3));
		TestTrue(TEXT("(that is (50, 0, 50): the -Y of the mesh became +X)"), Expected.Equals(FVector(50, 0, 50), 1e-3));
		TestTrue(TEXT("the scale stays 1"), Hand->Shape.Local.GetScale3D().Equals(FVector::OneVector, 1e-6));
	}
	if (const FGSTN_Hurtbox* Arm = Find(Result, TEXT("upperarm_l")))
	{
		// The capsule axis was the bone's Z; the placement turned about Z, so it is still vertical
		TestTrue(TEXT("a vertical capsule stays vertical"), FMath::Abs(Arm->Shape.Local.GetUnitAxis(EAxis::Z).Z) > 0.999);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxGenOptionsTest, "GSTNCombat.Editor.Hurtbox.GeneratorOptions", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxGenOptionsTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();

	// Kinematic bodies are skipped on request
	Asset->SkeletalBodySetups[0]->PhysicsType = PhysType_Kinematic;
	FGSTN_HurtboxGenOptions Options;
	TestEqual(TEXT("kept by default"), GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Options).Hurtboxes.Num(), 8);
	Options.bSkipKinematic = true;
	const FGSTN_HurtboxGenResult Skipped = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Options);
	TestEqual(TEXT("the kinematic pelvis is left out"), Skipped.Hurtboxes.Num(), 7);
	TestEqual(TEXT("and counted"), Skipped.SkippedKinematic, 1);
	TestNull(TEXT("by name"), Find(Skipped, TEXT("pelvis")));

	// A body whose bone the mesh lacks is skipped with a warning
	AddBody(*Asset, TEXT("tail_that_is_not_there"))->AggGeom.SphereElems.Add(FKSphereElem(3.f));
	const FGSTN_HurtboxGenResult NoBone = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());
	TestEqual(TEXT("the body without a bone is counted"), NoBone.SkippedNoBone, 1);
	TestEqual(TEXT("with a warning that names the bone"), NoBone.Warnings.Num(), 1);
	TestTrue(TEXT("(the text)"), NoBone.Warnings[0].ToString().Contains(TEXT("tail_that_is_not_there")));

	// The margin multiplies the extents
	FGSTN_HurtboxGenOptions Margin;
	Margin.ScaleMargin = 1.5f;
	const FGSTN_HurtboxGenResult Bigger = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Margin);
	if (const FGSTN_Hurtbox* Head = Find(Bigger, TEXT("head")))
	{
		TestEqual(TEXT("a margin of 1.5 makes the radius 15"), Head->Shape.Extent.X, 15.0, 1e-5);
	}

	// Small bodies merge into the nearest ancestor that has a big one: a finger body into the hand
	AddBody(*Asset, TEXT("finger_l"))->AggGeom.SphereElems.Add(FKSphereElem(2.f));
	FGSTN_HurtboxGenOptions Merge;
	Merge.MergeBelowCm = 3.f;
	const FGSTN_HurtboxGenResult Without = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());
	const FGSTN_HurtboxGenResult Merged = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Merge);
	TestEqual(TEXT("the finger is gone"), Merged.Hurtboxes.Num(), Without.Hurtboxes.Num() - 1);
	TestEqual(TEXT("and counted"), Merged.MergedSmall, 1);
	TestNull(TEXT("by name"), Find(Merged, TEXT("finger_l")));
	const FGSTN_Hurtbox* Hand = Find(Merged, TEXT("hand_l"));
	const FGSTN_Hurtbox* HandBefore = Find(Without, TEXT("hand_l"));
	const FGSTN_Hurtbox* Finger = Find(Without, TEXT("finger_l"));
	if (Hand && HandBefore && Finger)
	{
		TestTrue(TEXT("the hand grew"), Hand->Shape.Extent.X > HandBefore->Shape.Extent.X);
		// The finger sphere (centre 5 from the hand centre, radius 2) lies inside the grown hand sphere
		const double Reach = FVector::Dist(Hand->Shape.Local.GetLocation(), Finger->Shape.Local.GetLocation()) + Finger->Shape.Extent.X;
		TestTrue(TEXT("the grown hand contains the finger"), Hand->Shape.Extent.X >= Reach - 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxExpandTest, "GSTNCombat.Editor.Hurtbox.ExpandShapeOnlyGrowsAndContains", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxExpandTest::RunTest(const FString& Parameters)
{
	FGSTN_HitShape Sphere;
	Sphere.Type = EGSTN_ShapeType::Sphere;
	Sphere.Extent = FVector(10, 10, 10);
	GSTNHurtboxGenerator::ExpandShapeToInclude(Sphere, FVector(3, 0, 0), 2.0);
	TestEqual(TEXT("a sphere inside is not changed"), Sphere.Extent.X, 10.0);
	GSTNHurtboxGenerator::ExpandShapeToInclude(Sphere, FVector(0, 20, 0), 5.0);
	TestEqual(TEXT("a sphere far away grows the radius to reach it: 20 + 5"), Sphere.Extent.X, 25.0, 1e-9);

	FGSTN_HitShape Capsule;
	Capsule.Type = EGSTN_ShapeType::Capsule;
	Capsule.Extent = FVector(5, 5, 15);
	GSTNHurtboxGenerator::ExpandShapeToInclude(Capsule, FVector(0, 0, 18), 4.0);
	TestEqual(TEXT("on the axis only the half height grows: 18 + 4"), Capsule.Extent.Z, 22.0, 1e-9);
	TestEqual(TEXT("and not the radius"), Capsule.Extent.X, 5.0, 1e-9);
	GSTNHurtboxGenerator::ExpandShapeToInclude(Capsule, FVector(8, 6, 0), 2.0);
	TestEqual(TEXT("sideways the radius grows: 10 + 2"), Capsule.Extent.X, 12.0, 1e-9);
	TestTrue(TEXT("a capsule is never shorter than it is wide"), Capsule.Extent.Z >= Capsule.Extent.X);

	FGSTN_HitShape Box;
	Box.Type = EGSTN_ShapeType::Box;
	Box.Extent = FVector(10, 5, 5);
	GSTNHurtboxGenerator::ExpandShapeToInclude(Box, FVector(-12, 0, 2), 1.0);
	TestTrue(TEXT("a box grows per axis"), Box.Extent.Equals(FVector(13, 5, 5), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxZoneTest, "GSTNCombat.Editor.Hurtbox.SuggestedTagFromMapping", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxZoneTest::RunTest(const FString& Parameters)
{
	// Two native tags that are always registered: Combat.Hurtbox (the root, standing in for a zone) and Combat.Hurtbox.Feet
	const FGameplayTag Head = GSTNCombatHitTags::Root_Hurtbox.GetTag();
	const FGameplayTag Feet = GSTNCombatHitTags::Hurtbox_Feet.GetTag();
	if (!TestTrue(TEXT("the tags exist"), Head.IsValid() && Feet.IsValid()))
	{
		return false;
	}

	TArray<FGSTN_HurtboxZoneRule> Rules;
	FGSTN_HurtboxZoneRule HeadRule;
	HeadRule.BoneNameContains = TEXT("head");
	HeadRule.Zone = Head;
	Rules.Add(HeadRule);
	FGSTN_HurtboxZoneRule FootRule;
	FootRule.BoneNameContains = TEXT("FOOT");
	FootRule.Zone = Feet;
	Rules.Add(FootRule);
	FGSTN_HurtboxZoneRule Empty;
	Empty.Zone = Feet;
	Rules.Insert(Empty, 0);

	TestTrue(TEXT("a bone whose name contains the fragment"), GSTNHurtboxGenerator::SuggestZone(Rules, TEXT("head_end")) == Head);
	TestTrue(TEXT("case-insensitive"), GSTNHurtboxGenerator::SuggestZone(Rules, TEXT("foot_l")) == Feet);
	TestFalse(TEXT("a bone no rule matches gets nothing (a tail, a turret)"), GSTNHurtboxGenerator::SuggestZone(Rules, TEXT("tail_03")).IsValid());
	TestTrue(TEXT("a line with no fragment suggests nothing, not everything"), GSTNHurtboxGenerator::SuggestZone(Rules, TEXT("spine_01")) != Feet);

	// Through a generation: tags land on the hurtboxes of the matching bones only
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();
	FGSTN_HurtboxGenOptions Options;
	Options.ZoneRules = Rules;
	const FGSTN_HurtboxGenResult Result = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, Options);
	if (const FGSTN_Hurtbox* HeadBox = Find(Result, TEXT("head")))
	{
		TestTrue(TEXT("the head is tagged"), HeadBox->Tags.HasTagExact(Head));
	}
	if (const FGSTN_Hurtbox* Pelvis = Find(Result, TEXT("pelvis")))
	{
		TestTrue(TEXT("the pelvis, unmapped, has no tag"), Pelvis->Tags.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxRegenerateTest, "GSTNCombat.Editor.Hurtbox.RegenerationKeepsHandEdits", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxRegenerateTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();
	const FGSTN_HurtboxGenResult Generation = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());

	// The set so far: the head hand-edited (bigger, a priority, a zone tag), a hand-made hurtbox that the Physics Asset does not have, and the pelvis as generated
	TArray<FGSTN_Hurtbox> Existing;
	FGSTN_Hurtbox EditedHead = *Find(Generation, TEXT("head"));
	EditedHead.Shape.Extent = FVector(30, 30, 30);
	EditedHead.Priority = 7;
	EditedHead.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
	Existing.Add(EditedHead);
	Existing.Add(MakeHurtbox(TEXT("weakpoint"), NAME_None, FVector(0, 30, 120), 4.0));
	Existing.Add(*Find(Generation, TEXT("pelvis")));

	FGSTN_RegenerateSummary Summary;
	const TArray<FGSTN_Hurtbox> Kept = GSTNHurtboxGenerator::Merge(Existing, Generation.Hurtboxes, EGSTN_RegenerateMode::KeepEdits, &Summary);
	TestEqual(TEXT("the three existing stay and the six other generated are added"), Kept.Num(), 3 + 6);
	TestEqual(TEXT("order is kept: the existing stay where they are (a hit carries the index)"), Kept[0].Name, FName(TEXT("head")));
	TestEqual(TEXT("the hand-made one is second"), Kept[1].Name, FName(TEXT("weakpoint")));
	TestEqual(TEXT("the hand edit of the head survives"), Kept[0].Shape.Extent.X, 30.0);
	TestEqual(TEXT("with its priority"), static_cast<int32>(Kept[0].Priority), 7);
	TestEqual(TEXT("summary: kept"), Summary.Kept, 2);
	TestEqual(TEXT("summary: added"), Summary.Added, 6);
	TestEqual(TEXT("the Physics Asset does not make the weak point: it is named, and stays"), Summary.NotInGeneration.Num(), 1);
	TestEqual(TEXT("(its name)"), Summary.NotInGeneration[0], FName(TEXT("weakpoint")));

	// Running it again changes nothing
	const TArray<FGSTN_Hurtbox> Again = GSTNHurtboxGenerator::Merge(Kept, Generation.Hurtboxes, EGSTN_RegenerateMode::KeepEdits, &Summary);
	TestEqual(TEXT("a second regeneration adds nothing"), Again.Num(), Kept.Num());
	TestEqual(TEXT("(summary)"), Summary.Added, 0);

	// Update shapes: the shape and bone come back, the hand-set tag, priority and scale stay
	const TArray<FGSTN_Hurtbox> Updated = GSTNHurtboxGenerator::Merge(Existing, Generation.Hurtboxes, EGSTN_RegenerateMode::UpdateShapes, &Summary);
	TestEqual(TEXT("the head shape is the Physics Asset's again"), Updated[0].Shape.Extent.X, 10.0, 1e-9);
	TestEqual(TEXT("but its priority is kept"), static_cast<int32>(Updated[0].Priority), 7);
	TestTrue(TEXT("and its tag"), Updated[0].Tags.HasTagExact(GSTNCombatHitTags::Root_Hurtbox.GetTag()));
	TestEqual(TEXT("summary: updated"), Summary.Updated, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxReplaceAllTest, "GSTNCombat.Editor.Hurtbox.ReplaceAllDiscardsHandEdits", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxReplaceAllTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();
	const FGSTN_HurtboxGenResult Generation = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());

	TArray<FGSTN_Hurtbox> Existing;
	FGSTN_Hurtbox EditedHead = *Find(Generation, TEXT("head"));
	EditedHead.Shape.Extent = FVector(30, 30, 30);
	EditedHead.Priority = 7;
	Existing.Add(EditedHead);
	Existing.Add(MakeHurtbox(TEXT("weakpoint"), NAME_None, FVector(0, 30, 120)));

	FGSTN_RegenerateSummary Summary;
	const TArray<FGSTN_Hurtbox> Replaced = GSTNHurtboxGenerator::Merge(Existing, Generation.Hurtboxes, EGSTN_RegenerateMode::ReplaceAll, &Summary);
	TestEqual(TEXT("the set is the generation"), Replaced.Num(), Generation.Hurtboxes.Num());
	for (const FGSTN_Hurtbox& Hurtbox : Replaced)
	{
		TestFalse(TEXT("no hand-made hurtbox is left"), Hurtbox.Name == TEXT("weakpoint"));
	}
	const FGSTN_Hurtbox* Head = Replaced.FindByPredicate([](const FGSTN_Hurtbox& H) { return H.Name == TEXT("head"); });
	if (TestNotNull(TEXT("the head is there"), Head))
	{
		TestEqual(TEXT("with the shape of the Physics Asset"), Head->Shape.Extent.X, 10.0, 1e-9);
		TestEqual(TEXT("and no priority"), static_cast<int32>(Head->Priority), 0);
	}
	TestEqual(TEXT("summary: one removed"), Summary.Removed, 1);
	TestEqual(TEXT("summary: one replaced"), Summary.Updated, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxNamePairTest, "GSTNCombat.Editor.Hurtbox.MirrorNameMatching", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxNamePairTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_MirrorNamePair> Pairs = DefaultPairs();
	auto Counterpart = [&Pairs](const TCHAR* Name) -> FString
	{
		FString Out;
		return GSTNHurtboxMirror::FindCounterpart(Name, Pairs, Out) ? Out : FString(TEXT("<none>"));
	};

	TestEqual(TEXT("_l to _r"), Counterpart(TEXT("upperarm_l")), FString(TEXT("upperarm_r")));
	TestEqual(TEXT("_r to _l"), Counterpart(TEXT("upperarm_r")), FString(TEXT("upperarm_l")));
	TestEqual(TEXT("the case of the fragment is kept: _L to _R"), Counterpart(TEXT("Thigh_L")), FString(TEXT("Thigh_R")));
	TestEqual(TEXT("in the middle of a name"), Counterpart(TEXT("upperarm_l_twist")), FString(TEXT("upperarm_r_twist")));
	TestEqual(TEXT("a numbered one"), Counterpart(TEXT("hand_l_1")), FString(TEXT("hand_r_1")));
	TestEqual(TEXT("Left and Right words"), Counterpart(TEXT("LeftHand")), FString(TEXT("RightHand")));
	TestEqual(TEXT("(and back)"), Counterpart(TEXT("RightHand")), FString(TEXT("LeftHand")));
	TestEqual(TEXT("camel case boundary"), Counterpart(TEXT("armLeft")), FString(TEXT("armRight")));
	TestEqual(TEXT("dot style"), Counterpart(TEXT("Hand.L")), FString(TEXT("Hand.R")));

	// Not a side: a fragment inside a longer word
	TestEqual(TEXT("_l inside arm_lower is not a side"), Counterpart(TEXT("arm_lower")), FString(TEXT("<none>")));
	TestEqual(TEXT("Left inside Leftover is not a side"), Counterpart(TEXT("Leftover")), FString(TEXT("<none>")));
	TestEqual(TEXT("a name with no marker"), Counterpart(TEXT("spine_01")), FString(TEXT("<none>")));
	TestEqual(TEXT("the case-sensitive pair does not match in the wrong case"), Counterpart(TEXT("lefthand")), FString(TEXT("<none>")));

	// A line half edited must pair nothing
	TArray<FGSTN_MirrorNamePair> Broken;
	Broken.AddDefaulted();
	FString Out;
	TestFalse(TEXT("an empty pair pairs nothing"), GSTNHurtboxMirror::FindCounterpart(TEXT("hand_l"), Broken, Out));

	// The analysis of a list of names
	const TArray<FName> Names = { TEXT("pelvis"), TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("Spine") };
	const GSTNHurtboxMirror::FNameAnalysis Analysis = GSTNHurtboxMirror::AnalyzeNames(Names, Pairs);
	TestEqual(TEXT("the hands pair, both directions"), Analysis.Pairs.Num(), 2);
	TestEqual(TEXT("the left hand's counterpart"), Analysis.Pairs.FindChecked(TEXT("hand_l")), FName(TEXT("hand_r")));
	TestEqual(TEXT("the foot has no counterpart among the names"), Analysis.Unpaired.Num(), 1);
	TestEqual(TEXT("(the foot)"), Analysis.Unpaired[0], FName(TEXT("foot_l")));
	TestEqual(TEXT("two names have no side"), Analysis.Centre.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxMirrorConfigTest, "GSTNCombat.Editor.Hurtbox.MirrorOnlyWhereTheSkeletonHasPairs", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxMirrorConfigTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_MirrorNamePair> Pairs = DefaultPairs();
	const TArray<FName> Person = { TEXT("root"), TEXT("spine_01"), TEXT("hand_l"), TEXT("hand_r") };
	const TArray<FName> Spider = { TEXT("body"), TEXT("leg_1"), TEXT("leg_2"), TEXT("leg_3"), TEXT("fang") };
	const TArray<FName> NoNames;
	const TArray<FGSTN_MirrorNamePair> NoPairs;

	GSTNHurtboxMirror::FResolvedConfig OnPerson = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, NoPairs, Pairs, Person, NoNames);
	TestTrue(TEXT("a skeleton with a left/right pair can mirror"), OnPerson.bEnabled);
	TestTrue(TEXT("and knows its bones"), OnPerson.bBonesKnown);

	const GSTNHurtboxMirror::FResolvedConfig OnSpider = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, NoPairs, Pairs, Spider, NoNames);
	TestFalse(TEXT("a skeleton without recognisable pairs has no mirror"), OnSpider.bEnabled);
	TestFalse(TEXT("and says why"), OnSpider.Reason.IsEmpty());

	const GSTNHurtboxMirror::FResolvedConfig Off = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Off, NoPairs, Pairs, Person, NoNames);
	TestFalse(TEXT("switched off for the skeleton, even with a pair"), Off.bEnabled);

	// The skeleton's own pairs: legs named A and B
	TArray<FGSTN_MirrorNamePair> Own;
	FGSTN_MirrorNamePair AB;
	AB.Left = TEXT("_A");
	AB.Right = TEXT("_B");
	Own.Add(AB);
	const TArray<FName> Creature = { TEXT("body"), TEXT("claw_A"), TEXT("claw_B") };
	TestFalse(TEXT("the global pairs find nothing in this creature"), GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, NoPairs, Pairs, Creature, NoNames).bEnabled);
	TestTrue(TEXT("its own pairs do"), GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, Own, Pairs, Creature, NoNames).bEnabled);

	// No mesh: the hurtbox names are the evidence
	const GSTNHurtboxMirror::FResolvedConfig FromNames = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, NoPairs, Pairs, NoNames, Person);
	TestTrue(TEXT("without bones, the hurtbox names can enable it"), FromNames.bEnabled);
	TestFalse(TEXT("(bones not known)"), FromNames.bBonesKnown);

	// No pairs at all in the settings
	TestFalse(TEXT("no pairs, no mirror"), GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, NoPairs, NoPairs, Person, NoNames).bEnabled);

	// The settings object's rules: a skeleton listed Off
	UGSTN_CombatEditorSettings* Settings = NewObject<UGSTN_CombatEditorSettings>(GetTransientPackage());
	Settings->MirrorNamePairs = Pairs;
	TestTrue(TEXT("no skeleton given: Auto with the global pairs"), GSTNHurtboxMirror::ResolveConfig(*Settings, nullptr, Person, NoNames).bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxMirrorTransformTest, "GSTNCombat.Editor.Hurtbox.MirrorTransformIsAMirror", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxMirrorTransformTest::RunTest(const FString& Parameters)
{
	// For both lateral axes: a point of the mirrored transform is the mirror of the point of the original (M T M p' = M T p), and it stays a rotation
	FRandomStream Random(1234);
	for (const EGSTN_MirrorForwardAxis Forward : { EGSTN_MirrorForwardAxis::PositiveX, EGSTN_MirrorForwardAxis::NegativeX, EGSTN_MirrorForwardAxis::PositiveY, EGSTN_MirrorForwardAxis::NegativeY })
	{
		const bool bLateralY = GSTNHurtboxMirror::GetLateralAxis(Forward) == EAxis::Y;
		auto Reflect = [bLateralY](FVector V) { (bLateralY ? V.Y : V.X) *= -1.0; return V; };
		for (int32 Case = 0; Case < 20; ++Case)
		{
			const FTransform T(FRotator(Random.FRandRange(-90, 90), Random.FRandRange(-180, 180), Random.FRandRange(-180, 180)), FVector(Random.FRandRange(-100, 100), Random.FRandRange(-100, 100), Random.FRandRange(-100, 100)));
			const FTransform Mirrored = GSTNHurtboxMirror::MirrorTransform(T, Forward);
			const FVector P(Random.FRandRange(-50, 50), Random.FRandRange(-50, 50), Random.FRandRange(-50, 50));
			TestTrue(TEXT("mirror of the point of the original"), Mirrored.TransformPosition(Reflect(P)).Equals(Reflect(T.TransformPosition(P)), 1e-6));
			TestTrue(TEXT("a unit quaternion"), FMath::IsNearlyEqual(Mirrored.GetRotation().Size(), 1.0, 1e-9));
			TestTrue(TEXT("mirroring twice is the identity"), GSTNHurtboxMirror::MirrorTransform(Mirrored, Forward).Equals(T, 1e-6));
		}
	}

	// A capsule lying along the arm: pointing to -Y and slightly up, its mirror points to +Y and slightly up
	FGSTN_HitShape Capsule;
	Capsule.Type = EGSTN_ShapeType::Capsule;
	Capsule.Extent = FVector(5, 5, 20);
	Capsule.Local = FTransform(FQuat::FindBetweenNormals(FVector::ZAxisVector, FVector(0, -1, 0.2).GetSafeNormal()), FVector(0, -30, 150));
	const FGSTN_HitShape Mirrored = GSTNHurtboxMirror::MirrorShape(Capsule, EGSTN_MirrorForwardAxis::PositiveX);
	TestTrue(TEXT("the centre crosses the plane"), Mirrored.Local.GetLocation().Equals(FVector(0, 30, 150), 1e-9));
	TestTrue(TEXT("the axis crosses with it"), Mirrored.Local.GetUnitAxis(EAxis::Z).Equals(FVector(0, 1, 0.2).GetSafeNormal(), 1e-9) || Mirrored.Local.GetUnitAxis(EAxis::Z).Equals(FVector(0, -1, -0.2).GetSafeNormal(), 1e-9));
	TestTrue(TEXT("the extent is the same"), Mirrored.Extent.Equals(Capsule.Extent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxMirrorCreatesTest, "GSTNCombat.Editor.Hurtbox.MirrorCreatesCounterpart", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxMirrorCreatesTest::RunTest(const FString& Parameters)
{
	const FSkeleton Skeleton = MakePerson();
	TArray<FName> Bones;
	for (int32 Bone = 0; Bone < Skeleton.Ref.GetNum(); ++Bone)
	{
		Bones.Add(Skeleton.Ref.GetBoneName(Bone));
	}
	const GSTNHurtboxMirror::FResolvedConfig Config = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, TArray<FGSTN_MirrorNamePair>(), DefaultPairs(), Bones, TArray<FName>());
	if (!TestTrue(TEXT("the person mirrors"), Config.bEnabled))
	{
		return false;
	}

	TArray<FGSTN_Hurtbox> Set;
	FGSTN_Hurtbox Arm = MakeHurtbox(TEXT("upperarm_l"), TEXT("upperarm_l"), FVector(0, -20, 150), 6.0);
	Arm.Priority = 3;
	Arm.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
	Set.Add(Arm);
	Set.Add(MakeHurtbox(TEXT("head"), TEXT("head"), FVector(0, 0, 175), 10.0));

	const int32 Selected[] = { 0 };
	const GSTNHurtboxMirror::FPlan Plan = GSTNHurtboxMirror::MakePlan(Set, Selected, Config, EGSTN_MirrorForwardAxis::PositiveX);
	if (!TestEqual(TEXT("one hurtbox to write"), Plan.Entries.Num(), 1))
	{
		return false;
	}
	TestFalse(TEXT("a new one needs no confirmation"), Plan.NeedsConfirmation());
	const GSTNHurtboxMirror::FPlanEntry& Entry = Plan.Entries[0];
	TestEqual(TEXT("named for the other side"), Entry.Mirrored.Name, FName(TEXT("upperarm_r")));
	TestEqual(TEXT("on the other bone"), Entry.Mirrored.Bone, FName(TEXT("upperarm_r")));
	TestTrue(TEXT("at the mirrored position"), Entry.Mirrored.Shape.Local.GetLocation().Equals(FVector(0, 20, 150), 1e-9));
	TestEqual(TEXT("the priority is copied"), static_cast<int32>(Entry.Mirrored.Priority), 3);
	TestTrue(TEXT("and the zone tag"), Entry.Mirrored.Tags.HasTagExact(GSTNCombatHitTags::Root_Hurtbox.GetTag()));

	const TArray<FGSTN_Hurtbox> After = GSTNHurtboxMirror::Apply(Set, Plan);
	TestEqual(TEXT("the counterpart is appended"), After.Num(), 3);
	TestEqual(TEXT("(last)"), After[2].Name, FName(TEXT("upperarm_r")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxMirrorUnpairedTest, "GSTNCombat.Editor.Hurtbox.MirrorReportsUnpairedBones", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxMirrorUnpairedTest::RunTest(const FString& Parameters)
{
	// A skeleton with a pair (hands) and a left foot that has no right foot
	const TArray<FName> Bones = { TEXT("root"), TEXT("spine"), TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l") };
	const GSTNHurtboxMirror::FResolvedConfig Config = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, TArray<FGSTN_MirrorNamePair>(), DefaultPairs(), Bones, TArray<FName>());
	if (!TestTrue(TEXT("the hands make it mirrorable"), Config.bEnabled))
	{
		return false;
	}

	TArray<FGSTN_Hurtbox> Set;
	Set.Add(MakeHurtbox(TEXT("hand_l"), TEXT("hand_l"), FVector(0, -50, 150)));
	Set.Add(MakeHurtbox(TEXT("foot_l"), TEXT("foot_l"), FVector(0, -10, 5)));
	Set.Add(MakeHurtbox(TEXT("spine"), TEXT("spine"), FVector(0, 0, 120)));
	// Named for the middle but sitting on a sided bone: the name is what is missing
	Set.Add(MakeHurtbox(TEXT("Fist"), TEXT("hand_l"), FVector(0, -55, 150)));
	const int32 All[] = { 0, 1, 2, 3 };

	const GSTNHurtboxMirror::FPlan Plan = GSTNHurtboxMirror::MakePlan(Set, All, Config, EGSTN_MirrorForwardAxis::PositiveX);
	TestEqual(TEXT("the hand is mirrored"), Plan.Entries.Num(), 1);
	TestEqual(TEXT("the foot and the fist are reported, not guessed"), Plan.Unpaired.Num(), 2);
	TestTrue(TEXT("the foot"), Plan.Unpaired.Contains(TEXT("foot_l")));
	TestTrue(TEXT("the fist"), Plan.Unpaired.Contains(TEXT("Fist")));
	TestEqual(TEXT("the spine is the middle"), Plan.Centre.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxMirrorOverwriteTest, "GSTNCombat.Editor.Hurtbox.MirrorOverwriteNeedsConfirmation", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxMirrorOverwriteTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Bones = { TEXT("hand_l"), TEXT("hand_r") };
	const GSTNHurtboxMirror::FResolvedConfig Config = GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode::Auto, TArray<FGSTN_MirrorNamePair>(), DefaultPairs(), Bones, TArray<FName>());

	TArray<FGSTN_Hurtbox> Set;
	Set.Add(MakeHurtbox(TEXT("hand_l"), TEXT("hand_l"), FVector(0, -50, 150), 9.0));
	Set.Add(MakeHurtbox(TEXT("hand_r"), TEXT("hand_r"), FVector(0, 80, 150), 4.0)); // a hand-moved right hand

	const int32 Left[] = { 0 };
	const GSTNHurtboxMirror::FPlan Plan = GSTNHurtboxMirror::MakePlan(Set, Left, Config, EGSTN_MirrorForwardAxis::PositiveX);
	TestEqual(TEXT("one entry"), Plan.Entries.Num(), 1);
	TestTrue(TEXT("it replaces an existing hurtbox, so it needs a confirmation"), Plan.NeedsConfirmation());
	const TArray<FName> Overwritten = Plan.GetOverwrittenNames(Set);
	TestTrue(TEXT("and names it"), Overwritten.Num() == 1 && Overwritten[0] == TEXT("hand_r"));
	TestEqual(TEXT("the plan alone changes nothing"), Set[1].Shape.Extent.X, 4.0);

	const TArray<FGSTN_Hurtbox> After = GSTNHurtboxMirror::Apply(Set, Plan);
	TestEqual(TEXT("replaced in place, nothing appended"), After.Num(), 2);
	TestEqual(TEXT("the right hand is the mirror of the left"), After[1].Shape.Extent.X, 9.0);
	TestTrue(TEXT("(position)"), After[1].Shape.Local.GetLocation().Equals(FVector(0, 50, 150), 1e-9));

	// Both selected: the first is the source, the second is only a target
	const int32 Both[] = { 0, 1 };
	const GSTNHurtboxMirror::FPlan BothPlan = GSTNHurtboxMirror::MakePlan(Set, Both, Config, EGSTN_MirrorForwardAxis::PositiveX);
	TestEqual(TEXT("one source"), BothPlan.Entries.Num(), 1);
	TestEqual(TEXT("(the left one)"), BothPlan.Entries[0].SourceIndex, 0);
	TestEqual(TEXT("the other is skipped as a target"), BothPlan.SkippedAsTarget.Num(), 1);

	// Disabled mirroring plans nothing
	GSTNHurtboxMirror::FResolvedConfig Disabled = Config;
	Disabled.bEnabled = false;
	TestTrue(TEXT("a disabled config plans nothing"), GSTNHurtboxMirror::MakePlan(Set, Left, Disabled, EGSTN_MirrorForwardAxis::PositiveX).Entries.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxModelUndoTest, "GSTNCombat.Editor.Hurtbox.ToolkitUndoRestoresShape", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxModelUndoTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	auto Steps = []() { return GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount(); };

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transactional));
	Set->Hurtboxes.Add(MakeHurtbox(TEXT("head"), TEXT("head"), FVector(0, 0, 175), 10.0));
	Set->Hurtboxes.Add(MakeHurtbox(TEXT("hand_l"), TEXT("hand_l"), FVector(0, -50, 150), 6.0));
	const TSharedRef<FGSTN_HurtboxEditModel> Model = MakeShared<FGSTN_HurtboxEditModel>(Set.Get());
	FGSTN_HurtboxUndoClient UndoClient(*Model);
	const int32 StartSteps = Steps();

	int32 Changed = 0;
	Model->OnChanged.AddLambda([&Changed]() { ++Changed; });

	// A shape edit is one step, and one undo gives the old shape back
	FGSTN_HitShape Moved = Set->Hurtboxes[0].Shape;
	Moved.Local.SetLocation(FVector(0, 0, 190));
	Moved.Extent = FVector(14, 14, 14);
	TestTrue(TEXT("the shape was set"), Model->SetShape(0, Moved));
	TestEqual(TEXT("one step"), Steps(), StartSteps + 1);
	TestEqual(TEXT("the set holds the new shape"), Set->Hurtboxes[0].Shape.Extent.X, 14.0);
	TestTrue(TEXT("listeners were told"), Changed >= 1);
	TestTrue(TEXT("one undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("the old radius is back"), Set->Hurtboxes[0].Shape.Extent.X, 10.0);
	TestTrue(TEXT("and the old position"), Set->Hurtboxes[0].Shape.Local.GetLocation().Equals(FVector(0, 0, 175), 1e-9));
	TestTrue(TEXT("one redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("the new radius again"), Set->Hurtboxes[0].Shape.Extent.X, 14.0);
	GEditor->UndoTransaction();

	// Setting what is already there opens no step
	const int32 Before = Steps();
	TestFalse(TEXT("an unchanged shape is not an edit"), Model->SetShape(0, Set->Hurtboxes[0].Shape));
	TestEqual(TEXT("and leaves no empty step"), Steps(), Before);

	// A session (a slider being dragged) is one step however many values it passes through
	TestTrue(TEXT("a session begins"), Model->BeginEdit(INVTEXT("Drag")));
	FGSTN_Hurtbox Dragged = Set->Hurtboxes[1];
	for (int32 Radius = 7; Radius <= 12; ++Radius)
	{
		Dragged.Shape.Extent = FVector(Radius, Radius, Radius);
		Model->SetHurtbox(1, Dragged);
	}
	Model->EndEdit();
	TestEqual(TEXT("six values, one step"), Steps(), Before + 1);
	TestEqual(TEXT("the last value is in the set"), Set->Hurtboxes[1].Shape.Extent.X, 12.0);
	GEditor->UndoTransaction();
	TestEqual(TEXT("one undo undoes the whole drag"), Set->Hurtboxes[1].Shape.Extent.X, 6.0);

	// Add, remove, and the selection that follows them
	const int32 BeforeAdd = Steps();
	const int32 Added = Model->AddHurtbox(MakeHurtbox(TEXT("head"), NAME_None, FVector::ZeroVector));
	TestEqual(TEXT("added last"), Added, 2);
	TestEqual(TEXT("a taken name is made unique"), Set->Hurtboxes[2].Name, FName(TEXT("head_2")));
	TestEqual(TEXT("the new one is selected"), Model->GetSingleSelection(), 2);
	TestEqual(TEXT("one step"), Steps(), BeforeAdd + 1);
	const int32 ToRemove[] = { 2, 0 };
	TestEqual(TEXT("two removed"), Model->RemoveHurtboxes(ToRemove), 2);
	TestEqual(TEXT("one left"), Set->Hurtboxes.Num(), 1);
	TestEqual(TEXT("(the hand)"), Set->Hurtboxes[0].Name, FName(TEXT("hand_l")));
	TestTrue(TEXT("the selection that no longer exists is dropped"), Model->GetSelection().IsEmpty());
	TestTrue(TEXT("undo of the removal"), GEditor->UndoTransaction());
	TestEqual(TEXT("three again"), Set->Hurtboxes.Num(), 3);

	// Generation and mirror are one step each
	const FSkeleton Skeleton = MakePerson();
	const TStrongObjectPtr<UPhysicsAsset> Asset = MakePersonPhysicsAsset();
	const FGSTN_HurtboxGenResult Generation = GSTNHurtboxGenerator::FromPhysicsAsset(*Asset, Skeleton.Ref, FGSTN_HurtboxGenOptions());
	const int32 BeforeGenerate = Steps();
	const FGSTN_RegenerateSummary Summary = Model->ApplyGeneration(Generation, EGSTN_RegenerateMode::ReplaceAll);
	TestEqual(TEXT("generated in one step"), Steps(), BeforeGenerate + 1);
	TestEqual(TEXT("the set is the generation"), Set->Hurtboxes.Num(), Generation.Hurtboxes.Num());
	TestTrue(TEXT("summary"), Summary.Added + Summary.Updated == Generation.Hurtboxes.Num());
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo brings the earlier set back"), Set->Hurtboxes.Num(), 3);

	// Leave the editor's history where it began
	while (Steps() > StartSteps && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxFindingsTest, "GSTNCombat.Editor.Hurtbox.FindingsFollowTheSet", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxFindingsTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = GSTNPreviewTest::LoadEngineCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	const FName CubeBone = Cube->GetRefSkeleton().GetBoneName(0);

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	FGSTN_Hurtbox Good = MakeHurtbox(TEXT("body"), CubeBone, FVector(0, 0, 10));
	Good.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
	Set->Hurtboxes.Add(Good);

	FGSTN_ValidationReport Clean;
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Clean, Cube);
	TestFalse(TEXT("a hurtbox on a real bone near the root"), Clean.HasCode(GSTNValidationCodes::HurtboxBone()) || Clean.HasCode(GSTNValidationCodes::HurtboxOutside()));

	FGSTN_Hurtbox Lost = MakeHurtbox(TEXT("tail"), TEXT("bone_the_mesh_lacks"), FVector(0, 0, 10));
	Lost.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
	Set->Hurtboxes.Add(Lost);
	FGSTN_Hurtbox Far = MakeHurtbox(TEXT("far"), CubeBone, FVector(0, 0, 5000));
	Far.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
	Set->Hurtboxes.Add(Far);

	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report, Cube);
	const FGSTN_ValidationFinding* BoneFinding = Report.FindCode(GSTNValidationCodes::HurtboxBone());
	if (TestNotNull(TEXT("a hurtbox on a bone the mesh does not have"), BoneFinding))
	{
		TestTrue(TEXT("a warning (mesh optional)"), BoneFinding->Severity == EGSTN_Severity::Warning);
		TestEqual(TEXT("it points at the hurtbox"), BoneFinding->Where.TrackIndex, 1);
	}
	const FGSTN_ValidationFinding* OutsideFinding = Report.FindCode(GSTNValidationCodes::HurtboxOutside());
	if (TestNotNull(TEXT("a hurtbox far from the root"), OutsideFinding))
	{
		TestEqual(TEXT("it points at the hurtbox"), OutsideFinding->Where.TrackIndex, 2);
	}

	// No mesh at all: the bone check is skipped
	FGSTN_ValidationReport NoMesh;
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), NoMesh);
	TestFalse(TEXT("without a mesh the bones are not checked"), NoMesh.HasCode(GSTNValidationCodes::HurtboxBone()));
	TestTrue(TEXT("but the distance still is"), NoMesh.HasCode(GSTNValidationCodes::HurtboxOutside()));

	// The set's own PreviewMesh is used when none is passed
#if WITH_EDITORONLY_DATA
	Set->PreviewMesh = TSoftObjectPtr<USkeletalMesh>(Cube);
	FGSTN_ValidationReport FromSet;
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), FromSet);
	TestTrue(TEXT("the set's PreviewMesh is the mesh of the bone check"), FromSet.HasCode(GSTNValidationCodes::HurtboxBone()));
	const FProperty* Property = UGSTN_HurtboxSetDefinition::StaticClass()->FindPropertyByName(TEXT("PreviewMesh"));
	if (TestNotNull(TEXT("PreviewMesh is a property"), Property))
	{
		TestTrue(TEXT("editor-only: stripped from cooked data"), Property->IsEditorOnlyProperty());
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNHurtboxDrawingExtentTest, "GSTNCombat.Editor.Hurtbox.DrawnExtentFollowsTheScale", GSTN_HURTBOX_TEST_FLAGS)
bool FGSTNHurtboxDrawingExtentTest::RunTest(const FString& Parameters)
{
	FGSTN_HitShape Shape;
	Shape.Type = EGSTN_ShapeType::Sphere;
	Shape.Extent = FVector(10, 10, 10);
	Shape.Local.SetScale3D(FVector(1, 2, 3));
	TestTrue(TEXT("a sphere by the largest scale"), GSTNPreviewDrawing::GetEffectiveExtent(Shape).Equals(FVector(30, 30, 30)));
	Shape.Type = EGSTN_ShapeType::Capsule;
	Shape.Extent = FVector(5, 5, 20);
	TestTrue(TEXT("a capsule: radius by the larger of X and Y, half height by Z"), GSTNPreviewDrawing::GetEffectiveExtent(Shape).Equals(FVector(10, 10, 60)));
	Shape.Type = EGSTN_ShapeType::Box;
	Shape.Extent = FVector(1, 2, 3);
	TestTrue(TEXT("a box per axis"), GSTNPreviewDrawing::GetEffectiveExtent(Shape).Equals(FVector(1, 4, 9)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
