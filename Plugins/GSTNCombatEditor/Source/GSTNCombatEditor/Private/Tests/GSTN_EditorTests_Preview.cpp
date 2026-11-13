#include "Animation/DebugSkelMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Preview/GSTN_PreviewCamera.h"
#include "Preview/GSTN_PreviewController.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Preview/GSTN_PreviewPose.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "Tests/GSTN_PreviewTestHelpers.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The preview scene of the timeline editor (plan m5 E6, combat-m5-editor 5.4): the pose per frame, the generic subject, attachments, the camera
// maths and the per-asset storage. They use the engine's own SkeletalCube and BasicShapes content and a montage built in memory, so they need no
// project content. The pose tests run without a renderer; the snapshot of the viewport is GSTN.Visual.Timeline.Preview (Tests/GSTN_EditorTests_Visual.cpp).

#define GSTN_PREVIEW_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

using namespace GSTNPreviewTest;

// ---------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewPoseAtFrameTest, "GSTNCombat.Editor.Preview.PoseAtFrameMatchesSectionTime", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewPoseAtFrameTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = LoadEngineCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	TStrongObjectPtr<UAnimSequence> Sequence;
	TStrongObjectPtr<UAnimMontage> Holder;
	UAnimMontage* Montage = MakeLinearMontage(*Cube, Sequence, Holder);
	if (!TestNotNull(TEXT("the montage was built"), Montage))
	{
		return false;
	}
	TestTrue(TEXT("the montage has the late section"), Montage->GetSectionIndex(TEXT("Late")) != INDEX_NONE);

	constexpr float TickRate = 62.5f;

	// The formula, without any mesh: SectionStart + Frame / TickRate x PlayRate, clamped to the montage
	TestEqual(TEXT("frame 0 of the whole montage is time 0"), FGSTN_PreviewPose::ComputePosition(Montage, NAME_None, 1.f, TickRate, 0), 0.0);
	TestEqual(TEXT("frame 25 at rate 1 is 0.4 s"), FGSTN_PreviewPose::ComputePosition(Montage, NAME_None, 1.f, TickRate, 25), 0.4, 1e-9);
	TestEqual(TEXT("a section starts at its own time"), FGSTN_PreviewPose::ComputePosition(Montage, TEXT("Late"), 1.f, TickRate, 0), 1.0, 1e-6);
	TestEqual(TEXT("frame 25 of the late section at rate 2 is 1.8 s"), FGSTN_PreviewPose::ComputePosition(Montage, TEXT("Late"), 2.f, TickRate, 25), 1.8, 1e-6);
	TestEqual(TEXT("an unknown section is the start of the montage"), FGSTN_PreviewPose::ComputePosition(Montage, TEXT("Nope"), 1.f, TickRate, 0), 0.0);
	TestEqual(TEXT("past the end it stops at the end"), FGSTN_PreviewPose::ComputePosition(Montage, NAME_None, 1.f, TickRate, 100000), static_cast<double>(Montage->GetPlayLength()), 1e-6);
	TestEqual(TEXT("a negative frame is frame 0"), FGSTN_PreviewPose::ComputePosition(Montage, NAME_None, 1.f, TickRate, -5), 0.0);
	TestEqual(TEXT("no montage is time 0"), FGSTN_PreviewPose::ComputePosition(nullptr, NAME_None, 1.f, TickRate, 10), 0.0);

	// The pose: the bone slides along X at LinearSpeed, so its component-space X at a frame is LinearSpeed x the montage time of that frame
	FGSTN_TimelinePreviewScene Scene;
	Scene.SetSubject(ResolvedFor(Cube));
	if (!TestNotNull(TEXT("the scene has a mesh"), Scene.GetMeshComponent()))
	{
		return false;
	}
	const FName Bone = Cube->GetRefSkeleton().GetBoneName(0);
	Scene.GetPose().SetMontage(Montage, TEXT("Late"), 2.f, TickRate);
	for (const int32 Frame : { 0, 10, 25, 30 })
	{
		TestTrue(TEXT("the pose was evaluated"), Scene.GetPose().Evaluate(Frame));
		// Previewing a montage must not move the mesh out of its root-space placement (the debug mesh component resets its own relative transform)
		TestTrue(TEXT("the mesh stays where the scene placed it"), Scene.GetMeshComponent()->GetComponentTransform().Equals(Scene.GetMeshToRoot(), 1e-3f));
		FTransform InComponent;
		if (TestTrue(TEXT("the bone is found by the name the skeleton gives it"), Scene.GetPose().GetSocketComponentSpace(Bone, InComponent)))
		{
			const double Time = 1.0 + Frame / static_cast<double>(TickRate) * 2.0;
			TestEqual(*FString::Printf(TEXT("frame %d is at montage time %.4f"), Frame, Time), Scene.GetPose().GetLastPosition(), Time, 1e-6);
			TestEqual(*FString::Printf(TEXT("frame %d: the bone is where the montage has it at that time"), Frame), InComponent.GetLocation().X, LinearSpeed * Time, 0.05);
		}
	}

	// The whole montage and a different rate: frame 25 at rate 1 is time 0.4 s
	Scene.GetPose().SetMontage(Montage, NAME_None, 1.f, TickRate);
	Scene.GetPose().Evaluate(25);
	FTransform AtFrame25;
	if (TestTrue(TEXT("the bone is found"), Scene.GetPose().GetSocketComponentSpace(Bone, AtFrame25)))
	{
		TestEqual(TEXT("frame 25 of the whole montage"), AtFrame25.GetLocation().X, LinearSpeed * 0.4, 0.05);
	}

	// Without a montage the reference pose: the bone is where the skeleton puts it
	Scene.GetPose().SetMontage(nullptr, NAME_None, 1.f, TickRate);
	Scene.GetPose().Evaluate(25);
	FTransform Reference;
	if (TestTrue(TEXT("the bone is found without a montage"), Scene.GetPose().GetSocketComponentSpace(Bone, Reference)))
	{
		const FTransform RefBone = Cube->GetRefSkeleton().GetRefBonePose()[0];
		TestTrue(TEXT("no montage shows the reference pose"), Reference.GetLocation().Equals(RefBone.GetLocation(), 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewRepeatableTest, "GSTNCombat.Editor.Preview.EvaluateIsRepeatable", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewRepeatableTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = LoadEngineCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	TStrongObjectPtr<UAnimSequence> Sequence;
	TStrongObjectPtr<UAnimMontage> Holder;
	UAnimMontage* Montage = MakeLinearMontage(*Cube, Sequence, Holder);
	if (!TestNotNull(TEXT("the montage was built"), Montage))
	{
		return false;
	}
	FGSTN_TimelinePreviewScene Scene;
	Scene.SetSubject(ResolvedFor(Cube));
	Scene.GetPose().SetMontage(Montage, NAME_None, 1.f, 62.5f);
	const FName Bone = Cube->GetRefSkeleton().GetBoneName(0);

	FTransform First, Again, Third;
	Scene.GetPose().Evaluate(17);
	Scene.GetPose().GetSocketRootSpace(Bone, First);
	// Other frames in between, forward and backward: the pose of frame 17 does not remember where it was before
	Scene.GetPose().Evaluate(60);
	Scene.GetPose().Evaluate(3);
	Scene.GetPose().Evaluate(17);
	Scene.GetPose().GetSocketRootSpace(Bone, Again);
	Scene.GetPose().Evaluate(17);
	Scene.GetPose().Evaluate(17);
	Scene.GetPose().GetSocketRootSpace(Bone, Third);

	TestTrue(TEXT("the second evaluation is identical to the first"), First.Equals(Again, 0.f));
	TestTrue(TEXT("the third evaluation is identical to the first"), First.Equals(Third, 0.f));
	TestTrue(TEXT("the frame moved the bone (the test would pass on a frozen pose)"), FMath::Abs(First.GetLocation().Size() - 0.0) > 1.0);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewFollowRootTest, "GSTNCombat.Editor.Preview.FollowRootFramesMesh", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewFollowRootTest::RunTest(const FString& Parameters)
{
	constexpr double Fov = 60.0;
	const FBoxSphereBounds Shapes[] = {
		FBoxSphereBounds(FBox(FVector(-30, -30, -90), FVector(30, 30, 90))),      // a person
		FBoxSphereBounds(FBox(FVector(-200, -80, -60), FVector(200, 80, 60))),    // a long low thing
		FBoxSphereBounds(FBox(FVector(150, 40, 10), FVector(250, 300, 400))),     // off-centre, with a long sword out of one side
	};
	for (const EGSTN_PreviewCameraPreset Preset : { EGSTN_PreviewCameraPreset::Side, EGSTN_PreviewCameraPreset::Front, EGSTN_PreviewCameraPreset::ThreeQuarter })
	{
		const FVector Direction = GSTNPreviewCamera::LookDirection(Preset);
		TestTrue(TEXT("a preset direction is a unit vector"), FMath::IsNearlyEqual(Direction.Size(), 1.0, 1e-9));
		for (const FBoxSphereBounds& Bounds : Shapes)
		{
			const FVector Camera = GSTNPreviewCamera::FrameBounds(Preset, Bounds, Fov);
			TestTrue(TEXT("the camera looks at the middle of the bounds"), ((Bounds.Origin - Camera).GetSafeNormal() - Direction).IsNearlyZero(1e-6));
			TestTrue(TEXT("the whole bounding sphere is inside the field of view"), GSTNPreviewCamera::SphereInView(Camera, Bounds.Origin, Bounds.Origin, Bounds.SphereRadius, Fov));
			TestFalse(TEXT("a camera that is too close does not frame it"), GSTNPreviewCamera::SphereInView(Bounds.Origin - Direction * Bounds.SphereRadius * 1.05, Bounds.Origin, Bounds.Origin, Bounds.SphereRadius, Fov));

			// Following: a root that moves takes the camera with it, so the framing is unchanged relative to the root
			const FVector RootBefore(10, 20, 0);
			const FVector RootAfter(310, -45, 12);
			const FVector Followed = GSTNPreviewCamera::FollowRoot(Camera, RootBefore, RootAfter);
			const FBoxSphereBounds Moved(Bounds.Origin + (RootAfter - RootBefore), Bounds.BoxExtent, Bounds.SphereRadius);
			TestTrue(TEXT("after the root moved, the mesh is still framed"), GSTNPreviewCamera::SphereInView(Followed, Moved.Origin, Moved.Origin, Moved.SphereRadius, Fov));
			TestTrue(TEXT("the offset from the root is kept"), ((Followed - RootAfter) - (Camera - RootBefore)).IsNearlyZero(1e-9));
		}
	}

	// A narrower field of view stands further back
	TestTrue(TEXT("a narrower field of view needs a larger distance"), GSTNPreviewCamera::FramingDistance(100.0, 30.0) > GSTNPreviewCamera::FramingDistance(100.0, 90.0));

	// Through the scene: the bounds it reports for a mesh are the ones the camera frames, with attachments included
	USkeletalMesh* Cube = LoadEngineCube();
	if (TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		FGSTN_TimelinePreviewScene Scene;
		Scene.SetSubject(ResolvedFor(Cube));
		const FBoxSphereBounds Bounds = Scene.GetSubjectBounds();
		const FVector Camera = GSTNPreviewCamera::FrameBounds(EGSTN_PreviewCameraPreset::ThreeQuarter, Bounds, Fov);
		TestTrue(TEXT("the scene's mesh is framed"), GSTNPreviewCamera::SphereInView(Camera, Bounds.Origin, Bounds.Origin, Bounds.SphereRadius, Fov));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewNonHumanoidTest, "GSTNCombat.Editor.Preview.NonHumanoidSubjectHasNoBoneAssumptions", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewNonHumanoidTest::RunTest(const FString& Parameters)
{
	// 1. Placement from the bounds and the forward axis alone, for shapes no character convention fits
	struct FShape { const TCHAR* Name; FBoxSphereBounds Bounds; EAxis::Type Forward; };
	const FShape Shapes[] = {
		{ TEXT("a long low animal facing +X"), FBoxSphereBounds(FBox(FVector(-150, -40, -30), FVector(150, 40, 30))), EAxis::X },
		{ TEXT("a turret base facing +Y"), FBoxSphereBounds(FBox(FVector(-60, -60, 0), FVector(60, 60, 50))), EAxis::Y },
		{ TEXT("a tall thing facing +Z"), FBoxSphereBounds(FBox(FVector(-20, -30, -200), FVector(20, 30, 200))), EAxis::Z },
	};
	for (const FShape& Shape : Shapes)
	{
		FTransform MeshToRoot;
		const FGSTN_PreviewCapsule Capsule = FGSTN_TimelinePreviewScene::ComputePlacement(Shape.Bounds, Shape.Forward, MeshToRoot);
		TestTrue(*FString::Printf(TEXT("%s: the capsule has a positive size"), Shape.Name), Capsule.Radius > 0.f && Capsule.HalfHeight > 0.f);
		TestTrue(*FString::Printf(TEXT("%s: the radius is not larger than the half height"), Shape.Name), Capsule.Radius <= Capsule.HalfHeight);

		// The mesh's forward axis ends up along +X
		FVector Axis = FVector::ZeroVector;
		Axis[Shape.Forward == EAxis::X ? 0 : (Shape.Forward == EAxis::Y ? 1 : 2)] = 1.0;
		TestTrue(*FString::Printf(TEXT("%s: the mesh's forward axis points along +X in root space"), Shape.Name), MeshToRoot.GetRotation().RotateVector(Axis).Equals(FVector::ForwardVector, 1e-6));

		// The middle of the bounds is at the height of the origin: the capsule is centred there
		const FVector Middle = MeshToRoot.TransformPosition(Shape.Bounds.Origin);
		TestTrue(*FString::Printf(TEXT("%s: the middle of the bounds is at the origin's height"), Shape.Name), FMath::IsNearlyZero(Middle.Z, 1e-6));
	}

	// 2. A real mesh whose bones carry no character names: every bone is reachable by the name the skeleton gives it, nothing else is asked for
	USkeletalMesh* Cube = LoadEngineCube();
	if (TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		FGSTN_TimelinePreviewScene Scene;
		const TArray<FText> Problems = Scene.SetSubject(ResolvedFor(Cube));
		TestEqual(TEXT("a mesh with no attachments has no scene problems"), Problems.Num(), 0);
		TestNotNull(TEXT("the mesh is shown"), Scene.GetMeshComponent());
		TestTrue(TEXT("the capsule is derived from the mesh"), Scene.GetRootCapsule().HalfHeight > 0.f);
		Scene.GetPose().Evaluate(0);
		const FReferenceSkeleton& Ref = Cube->GetRefSkeleton();
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			FTransform InRoot;
			TestTrue(*FString::Printf(TEXT("bone %s is reachable"), *Ref.GetBoneName(Bone).ToString()), Scene.GetPose().GetSocketRootSpace(Ref.GetBoneName(Bone), InRoot));
		}
		FTransform Nothing;
		TestFalse(TEXT("a name the skeleton does not have is simply absent"), Scene.GetPose().GetSocketRootSpace(TEXT("pelvis"), Nothing));
		TestFalse(TEXT("an empty name is absent"), Scene.GetPose().GetSocketRootSpace(NAME_None, Nothing));
	}

	// 3. The preview code names no bone and no humanoid part. Built from pieces so this file does not match its own search
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombatEditor"));
	if (TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		const FString PreviewDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNCombatEditor/Private/Preview"));
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *PreviewDir, TEXT("*.*"), true, false);
		TestTrue(TEXT("the preview sources were found"), Files.Num() > 0);
		const TCHAR* Names[] = { TEXT("pel") TEXT("vis"), TEXT("spi") TEXT("ne"), TEXT("han") TEXT("d_"), TEXT("fo") TEXT("ot_"), TEXT("clav") TEXT("icle"), TEXT("thi") TEXT("gh"), TEXT("ne") TEXT("ck") };
		for (const FString& File : Files)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				continue;
			}
			// A name right after an opening quote is a literal; the same word in a comment explaining what is not assumed does not count
			for (const TCHAR* Name : Names)
			{
				TestFalse(*FString::Printf(TEXT("%s has no literal starting with the bone name '%s'"), *FPaths::GetCleanFilename(File), Name),
					Text.Contains(FString(TEXT("\"")) + Name, ESearchCase::CaseSensitive));
			}
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewAttachmentsTest, "GSTNCombat.Editor.Preview.AttachmentsHangFromSocketsWithFullTransform", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewAttachmentsTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = LoadEngineCube();
	UStaticMesh* Box = LoadEngineStaticCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube) || !TestNotNull(TEXT("the engine's static cube loads"), Box))
	{
		return false;
	}
	TStrongObjectPtr<UAnimSequence> Sequence;
	TStrongObjectPtr<UAnimMontage> Holder;
	UAnimMontage* Montage = MakeLinearMontage(*Cube, Sequence, Holder);
	const FName Bone = Cube->GetRefSkeleton().GetBoneName(0);

	// A "sword": stretched along one axis only, hung from the bone with an offset
	FGSTN_ResolvedSubject Subject = ResolvedFor(Cube);
	FGSTN_ResolvedAttachment& Sword = Subject.Attachments.AddDefaulted_GetRef();
	Sword.Mesh = Box;
	Sword.ParentSocket = Bone;
	Sword.Relative = FTransform(FRotator(0.0, 30.0, 0.0), FVector(5.0, 0.0, 12.0), FVector(0.2, 0.2, 3.0));
	// A second one on a bone the mesh does not have: shown at the mesh root, with a problem reported
	FGSTN_ResolvedAttachment& Lost = Subject.Attachments.AddDefaulted_GetRef();
	Lost.Mesh = Box;
	Lost.ParentSocket = TEXT("NoSuchBone");

	FGSTN_TimelinePreviewScene Scene;
	const TArray<FText> Problems = Scene.SetSubject(Subject);
	TestEqual(TEXT("two attachments are in the scene"), Scene.GetNumAttachments(), 2);
	TestEqual(TEXT("the missing socket is reported once"), Problems.Num(), 1);

	Scene.GetPose().SetMontage(Montage, NAME_None, 1.f, 62.5f);
	Scene.GetPose().Evaluate(31);

	UMeshComponent* SwordComponent = Scene.GetAttachmentComponent(0);
	if (TestNotNull(TEXT("the sword has a component"), SwordComponent))
	{
		TestTrue(TEXT("it hangs from the main mesh"), SwordComponent->GetAttachParent() == Scene.GetMeshComponent());
		TestEqual(TEXT("from the bone it was given"), SwordComponent->GetAttachSocketName(), Bone);
		TestTrue(TEXT("its relative transform is the one given, scale included"), SwordComponent->GetRelativeTransform().Equals(Sword.Relative, 1e-4f));
		// World = relative x the bone's world transform: the stretch is one axis, and the bone moved with the pose
		const FTransform SocketWorld = Scene.GetMeshComponent()->GetSocketTransform(Bone, RTS_World);
		const FTransform Expected = Sword.Relative * SocketWorld;
		TestTrue(TEXT("its world transform follows the bone"), SwordComponent->GetComponentTransform().Equals(Expected, 1e-3f));
		TestTrue(TEXT("the stretch is the given one along one axis: the other two stay thin"),
			SwordComponent->GetComponentScale().Equals(SocketWorld.GetScale3D() * Sword.Relative.GetScale3D(), 1e-3));
	}
	UMeshComponent* LostComponent = Scene.GetAttachmentComponent(1);
	if (TestNotNull(TEXT("the second attachment has a component"), LostComponent))
	{
		TestTrue(TEXT("it hangs from the main mesh anyway"), LostComponent->GetAttachParent() == Scene.GetMeshComponent());
		TestTrue(TEXT("with no socket"), LostComponent->GetAttachSocketName().IsNone());
	}

	// Bounds include what hangs from the mesh: a long attachment makes the subject larger than the mesh alone
	FGSTN_TimelinePreviewScene Bare;
	Bare.SetSubject(ResolvedFor(Cube));
	TestTrue(TEXT("the bounds grow with the attachments"), Scene.GetSubjectBounds().SphereRadius > Bare.GetSubjectBounds().SphereRadius);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewKitSkinTest, "GSTNCombat.Editor.Preview.KitSkinSubjectResolvesReadOnly", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewKitSkinTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = LoadEngineCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_SkinDefinition> Skin(NewObject<UGSTN_SkinDefinition>(GetTransientPackage()));
	Skin->Mesh = Cube;
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	const FString KitBefore = Kit->GetName();

	FGSTN_PreviewSubject Subject;
	Subject.Kind = EGSTN_PreviewSubjectKind::KitSkin;
	Subject.Kit = Kit.Get();
	Subject.Skin = Skin.Get();
	FGSTN_ResolvedSubject Resolved = GSTNPreviewSubject::Resolve(Subject);
	TestTrue(TEXT("the skin's mesh is the subject's mesh"), Resolved.Mesh == Cube);
	TestEqual(TEXT("an empty CompatibleSkins query reports nothing"), Resolved.Problems.Num(), 0);
	TestTrue(TEXT("the label names the kit and the skin"), Resolved.Label.ToString().Contains(Kit->GetName()) && Resolved.Label.ToString().Contains(Skin->GetName()));

	// A skin without a mesh, and a missing kit, are said in words
	Skin->Mesh.Reset();
	Resolved = GSTNPreviewSubject::Resolve(Subject);
	TestNull(TEXT("no mesh"), Resolved.Mesh);
	TestTrue(TEXT("the missing mesh is a problem"), Resolved.Problems.Num() > 0);
	Subject.Kit.Reset();
	Resolved = GSTNPreviewSubject::Resolve(Subject);
	TestTrue(TEXT("the missing kit is a problem too"), Resolved.Problems.Num() >= 2);

	// A mesh subject needs no kit
	FGSTN_PreviewSubject Enemy;
	Enemy.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
	Enemy.Mesh = Cube;
	Resolved = GSTNPreviewSubject::Resolve(Enemy);
	TestTrue(TEXT("an enemy mesh is the subject"), Resolved.Mesh == Cube);
	TestEqual(TEXT("with no problems"), Resolved.Problems.Num(), 0);

	// The kit was only read
	TestEqual(TEXT("the kit was not touched"), Kit->GetName(), KitBefore);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
	/** Removes the test entries from the user's config even when an assertion fails half way */
	struct FScopedPreviewKeys
	{
		TArray<FSoftObjectPath> Keys;
		~FScopedPreviewKeys()
		{
			for (const FSoftObjectPath& Key : Keys)
			{
				UGSTN_CombatEditorPreviewState::Remove(Key);
			}
		}
	};

	bool SameSubject(const FGSTN_PreviewSubject& A, const FGSTN_PreviewSubject& B)
	{
		return A == B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewPersistTest, "GSTNCombat.Editor.Preview.SubjectPersistsPerAsset", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewPersistTest::RunTest(const FString& Parameters)
{
	FScopedPreviewKeys Cleanup;
	const FSoftObjectPath AssetA(TEXT("/Temp/GSTNPreviewTest/TimelineA.TimelineA"));
	const FSoftObjectPath AssetB(TEXT("/Temp/GSTNPreviewTest/BP_InlineB.BP_InlineB"));
	const FSoftObjectPath AssetC(TEXT("/Temp/GSTNPreviewTest/NeverStored.NeverStored"));
	Cleanup.Keys = { AssetA, AssetB, AssetC };

	// An enemy mesh with a sword that is long and thin, and a player kit and skin with no attachment: two assets, two subjects
	FGSTN_PreviewSubject Enemy;
	Enemy.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
	Enemy.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
	FGSTN_PreviewAttachment& Sword = Enemy.Attachments.AddDefaulted_GetRef();
	Sword.Mesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Sword.ParentSocket = TEXT("any_name_at_all");
	Sword.Relative = FTransform(FRotator(10.0, 20.0, 30.0), FVector(1.5, -2.0, 30.0), FVector(0.05, 0.05, 2.5));

	FGSTN_PreviewSubject Player;
	Player.Kind = EGSTN_PreviewSubjectKind::KitSkin;
	Player.Kit = TSoftObjectPtr<UGSTN_PrimaryKitDefinition>(FSoftObjectPath(TEXT("/Temp/GSTNPreviewTest/Kit.Kit")));
	Player.Skin = TSoftObjectPtr<UGSTN_SkinDefinition>(FSoftObjectPath(TEXT("/Temp/GSTNPreviewTest/Skin.Skin")));

	UGSTN_CombatEditorPreviewState::Store(AssetA, Enemy);
	UGSTN_CombatEditorPreviewState::Store(AssetB, Player);

	// A restart: what is in memory is dropped and read back from the user's config file
	UGSTN_CombatEditorPreviewState::ReloadFromDisk();

	FGSTN_PreviewSubject Read;
	if (TestTrue(TEXT("the enemy subject is found after a restart"), UGSTN_CombatEditorPreviewState::Find(AssetA, Read)))
	{
		TestTrue(TEXT("it is what was stored, attachments and their full transform included"), SameSubject(Read, Enemy));
		if (TestEqual(TEXT("the attachment survived"), Read.Attachments.Num(), 1))
		{
			TestTrue(TEXT("the one-axis scale survived"), Read.Attachments[0].Relative.GetScale3D().Equals(FVector(0.05, 0.05, 2.5), 1e-4));
		}
	}
	FGSTN_PreviewSubject ReadPlayer;
	if (TestTrue(TEXT("the player subject is found after a restart"), UGSTN_CombatEditorPreviewState::Find(AssetB, ReadPlayer)))
	{
		TestTrue(TEXT("it is what was stored"), SameSubject(ReadPlayer, Player));
		TestFalse(TEXT("and it is not the other asset's subject"), SameSubject(ReadPlayer, Read));
	}
	TestFalse(TEXT("an asset nothing was stored for has no entry"), UGSTN_CombatEditorPreviewState::Find(AssetC, Read));

	// Replacing keeps one entry per asset; an empty subject forgets it
	Player.Kind = EGSTN_PreviewSubjectKind::None;
	Player.Attachments.Reset();
	UGSTN_CombatEditorPreviewState::Store(AssetB, Player);
	UGSTN_CombatEditorPreviewState::ReloadFromDisk();
	TestFalse(TEXT("an empty subject removes the entry"), UGSTN_CombatEditorPreviewState::Find(AssetB, Read));
	TestTrue(TEXT("the other asset's entry is still there"), UGSTN_CombatEditorPreviewState::Find(AssetA, Read));

	// Through the controller: the subject chosen in one editor session is the one the next session opens with
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	{
		const TSharedRef<FGSTN_PreviewController> First = FGSTN_PreviewController::Create(Model, AssetC);
		TestEqual(TEXT("a fresh asset starts with no subject"), First->GetSubject().Kind, EGSTN_PreviewSubjectKind::None);
		USkeletalMesh* Cube = LoadEngineCube();
		FGSTN_PreviewSubject Chosen;
		Chosen.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
		Chosen.Mesh = Cube;
		First->SetSubject(Chosen);
		TestNotNull(TEXT("the subject's mesh is in the scene"), First->GetScene()->GetMeshComponent());
	}
	UGSTN_CombatEditorPreviewState::ReloadFromDisk();
	{
		const TSharedRef<FGSTN_PreviewController> Second = FGSTN_PreviewController::Create(Model, AssetC);
		TestEqual(TEXT("the next session opens with the stored subject"), Second->GetSubject().Kind, EGSTN_PreviewSubjectKind::SkeletalMesh);
		TestNotNull(TEXT("and shows its mesh"), Second->GetScene()->GetMeshComponent());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewControllerTest, "GSTNCombat.Editor.Preview.ControllerFollowsPlayheadAndMontage", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewControllerTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Cube = LoadEngineCube();
	if (!TestNotNull(TEXT("the engine's skeletal cube loads"), Cube))
	{
		return false;
	}
	TStrongObjectPtr<UAnimSequence> Sequence;
	TStrongObjectPtr<UAnimMontage> Holder;
	UAnimMontage* Montage = MakeLinearMontage(*Cube, Sequence, Holder);
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data.TotalFrames = 100;
	Timeline->Data.Montage = Montage;
	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model->SetTickRate(62.5f);

	const TSharedRef<FGSTN_PreviewController> Controller = FGSTN_PreviewController::CreateUnstored(Model);
	TestTrue(TEXT("an unstored controller has no key"), Controller->GetStorageKey().IsNull());
	FGSTN_PreviewSubject Subject;
	Subject.Kind = EGSTN_PreviewSubjectKind::SkeletalMesh;
	Subject.Mesh = Cube;
	Controller->SetSubject(Subject);

	int32 Repaints = 0;
	Controller->OnPreviewChanged.AddLambda([&Repaints]() { ++Repaints; });
	const FName Bone = Cube->GetRefSkeleton().GetBoneName(0);

	Model->SetPlayhead(40);
	FTransform InComponent;
	TestTrue(TEXT("the bone is found"), Controller->GetScene()->GetPose().GetSocketComponentSpace(Bone, InComponent));
	TestEqual(TEXT("moving the playhead poses the mesh at that frame"), InComponent.GetLocation().X, LinearSpeed * 40.0 / 62.5, 0.05);
	TestTrue(TEXT("the viewport was told to repaint"), Repaints > 0);

	Model->SetPlayhead(10);
	Controller->GetScene()->GetPose().GetSocketComponentSpace(Bone, InComponent);
	TestEqual(TEXT("and back"), InComponent.GetLocation().X, LinearSpeed * 10.0 / 62.5, 0.05);

	// The montage section and play rate are the timeline's: editing them re-poses without a new subject
	Model->ApplyDataEdit([](FGSTN_CombatTimelineData& Data) { Data.MontageSection = TEXT("Late"); Data.MontagePlayRate = 2.f; }, FText::FromString(TEXT("Set section")));
	Model->SetPlayhead(31);
	Controller->GetScene()->GetPose().GetSocketComponentSpace(Bone, InComponent);
	TestEqual(TEXT("the section and the rate apply"), InComponent.GetLocation().X, LinearSpeed * (1.0 + 31.0 / 62.5 * 2.0), 0.05);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
	class FCountingOverlay : public IGSTN_PreviewOverlay
	{
	public:
		explicit FCountingOverlay(int32& InCount) : Count(InCount) {}
		virtual FName GetOverlayName() const override { return TEXT("GSTNTestCountingOverlay"); }
		virtual void Draw(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI) const override { Count += 1 + Context.Frame; }
		int32& Count;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPreviewOverlayTest, "GSTNCombat.Editor.Preview.OverlayRegistryDrawsRegistered", GSTN_PREVIEW_TEST_FLAGS)
bool FGSTNPreviewOverlayTest::RunTest(const FString& Parameters)
{
	const int32 Before = FGSTN_PreviewOverlayRegistry::Num();
	int32 Count = 0;
	FGSTN_PreviewDrawContext Context;
	Context.Frame = 4;

	FGSTN_PreviewOverlayRegistry::DrawAll(Context, nullptr);
	TestEqual(TEXT("nothing registered by this test, nothing drawn by it"), Count, 0);

	FGSTN_PreviewOverlayRegistry::Register(MakeShared<FCountingOverlay>(Count));
	FGSTN_PreviewOverlayRegistry::Register(MakeShared<FCountingOverlay>(Count));
	TestEqual(TEXT("one overlay per name"), FGSTN_PreviewOverlayRegistry::Num(), Before + 1);
	FGSTN_PreviewOverlayRegistry::DrawAll(Context, nullptr);
	TestEqual(TEXT("the registered overlay drew once, with the frame"), Count, 5);

	FGSTN_PreviewOverlayRegistry::Unregister(TEXT("GSTNTestCountingOverlay"));
	TestEqual(TEXT("unregistered"), FGSTN_PreviewOverlayRegistry::Num(), Before);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
