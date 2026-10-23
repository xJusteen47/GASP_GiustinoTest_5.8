// Automation tests of the surface data contract (traversal-surfaces.md §9, plan T1).

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTN_SurfaceSettings.h"
#include "GSTN_SurfaceTable.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceTestTypes.h"
#include "GSTN_SurfaceUtils.h"
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNSurfaceTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world to own the test actors */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNSurfaceTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			// AActor::ProcessEvent ignores calls until the world's actors are initialised: actor providers need it
			World->InitializeActorsForPlay(FURL());
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		template <typename TActor = AActor>
		TActor* Spawn()
		{
			return World->SpawnActor<TActor>();
		}

		template <typename TComponent>
		TComponent* Add(AActor* Owner, FName Name)
		{
			TComponent* Component = NewObject<TComponent>(Owner, Name);
			Component->RegisterComponent();
			return Component;
		}
	};

	FGSTN_SurfaceBehaviour MakeBehaviour(std::initializer_list<FGameplayTag> Behaviours, FGameplayTag Type = FGameplayTag())
	{
		FGSTN_SurfaceBehaviour Behaviour;
		for (const FGameplayTag& Tag : Behaviours)
		{
			Behaviour.Behaviours.AddTag(Tag);
		}
		Behaviour.SurfaceType = Type;
		return Behaviour;
	}

	/** What the shipped settings default is: everything the wall and walking modes did before the plugin */
	FGSTN_SurfaceBehaviour MakeDefault(FGameplayTag Type = FGameplayTag())
	{
		return MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallSlide, GSTNSurfaceTags::Behaviour_WallClimb,
			GSTNSurfaceTags::Behaviour_Stand }, Type);
	}

	FGameplayTag IniType(const TCHAR* Name)
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
	}

	FGSTN_SurfaceQuery QueryFor(UPrimitiveComponent* Component)
	{
		FGSTN_SurfaceQuery Query;
		Query.Component = Component;
		Query.ComponentName = Component ? Component->GetFName() : NAME_None;
		return Query;
	}
}

using namespace GSTNSurfaceTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceSmokeTest, "GSTNSurface.Core.Smoke", Flags)
bool FGSTN_SurfaceSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("GSTNSurfaceCore module is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("GSTNSurfaceCore")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceTagsDeclaredOnceTest, "GSTNSurface.Core.TagsDeclaredOnce", Flags)
bool FGSTN_SurfaceTagsDeclaredOnceTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNSurfaceTags::GetDeclaredTags(Tags);
	TSet<FGameplayTag> Seen;
	for (const FGameplayTag& Tag : Tags)
	{
		TestTrue(FString::Printf(TEXT("%s is registered"), *Tag.ToString()), Tag.IsValid());
		TestTrue(FString::Printf(TEXT("%s is under Surface"), *Tag.ToString()), Tag.MatchesTag(GSTNSurfaceTags::Root_Surface));
		bool bAlreadySeen = false;
		Seen.Add(Tag, &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("%s is declared once"), *Tag.ToString()), bAlreadySeen);
	}
	TestEqual(TEXT("Every native surface tag is listed"), Tags.Num(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceTypeLeavesFromIniTest, "GSTNSurface.Core.TypeLeavesFromIni", Flags)
bool FGSTN_SurfaceTypeLeavesFromIniTest::RunTest(const FString& Parameters)
{
	// The engine reads Config/Tags only for the project and Game Feature plugins: the module registers the plugin's folder
	for (const TCHAR* Name : { TEXT("Surface.Type.Wall"), TEXT("Surface.Type.Glass"), TEXT("Surface.Type.Crate"),
		TEXT("Surface.Type.Trampoline") })
	{
		TestTrue(FString::Printf(TEXT("%s comes from the plugin's tag ini"), Name), IniType(Name).IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceTypeIndicesStableTest, "GSTNSurface.Core.TypeIndicesStable", Flags)
bool FGSTN_SurfaceTypeIndicesStableTest::RunTest(const FString& Parameters)
{
	// Native tags stand in for type leaves: the table only orders and indexes whatever it is given
	const FGameplayTag A = GSTNSurfaceTags::Behaviour_Launch;
	const FGameplayTag B = GSTNSurfaceTags::Behaviour_Stand;
	const FGameplayTag C = GSTNSurfaceTags::Behaviour_WallSlide;

	FGSTN_SurfaceTable First;
	First.BakeTypes({ C, A, B, A });
	FGSTN_SurfaceTable Second;
	Second.BakeTypes({ B, C, A });

	TestEqual(TEXT("Duplicates are dropped"), First.NumTypes(), 3);
	for (const FGameplayTag& Tag : { A, B, C })
	{
		TestEqual(FString::Printf(TEXT("%s has the same index whatever the input order"), *Tag.ToString()),
			static_cast<int32>(First.GetTypeIndex(Tag)), static_cast<int32>(Second.GetTypeIndex(Tag)));
		TestTrue(TEXT("Index round-trips"), First.GetTypeByIndex(First.GetTypeIndex(Tag)) == Tag);
	}
	TestEqual(TEXT("Lexical order: Launch first"), static_cast<int32>(First.GetTypeIndex(A)), 1);
	TestEqual(TEXT("Empty tag is index 0"), static_cast<int32>(First.GetTypeIndex(FGameplayTag())), 0);
	TestEqual(TEXT("Unknown tag is index 0"), static_cast<int32>(First.GetTypeIndex(GSTNSurfaceTags::Root_Type)), 0);
	TestFalse(TEXT("Index 0 has no tag"), First.GetTypeByIndex(0).IsValid());

	// The settings' table indexes the shipped ini leaves
	const FGameplayTag Wall = IniType(TEXT("Surface.Type.Wall"));
	if (Wall.IsValid())
	{
		TestTrue(TEXT("Surface.Type.Wall has an index in the settings' table"), UGSTN_SurfaceUtils::GetSurfaceTypeIndex(Wall) != 0);
		TestTrue(TEXT("Settings' index round-trips"),
			UGSTN_SurfaceUtils::GetSurfaceTypeFromIndex(UGSTN_SurfaceUtils::GetSurfaceTypeIndex(Wall)) == Wall);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceResolveOrderTest, "GSTNSurface.Core.ResolveOrderComponentBeforeProviderBeforeMaterial", Flags)
bool FGSTN_SurfaceResolveOrderTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(GetTransientPackage());
	FGSTN_SurfaceTable Table;
	Table.BakeMaterials({ { Material, MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallClimb }) } });
	const FGSTN_SurfaceBehaviour Default = MakeDefault();

	// 1. component on the owner beats a provider on the collider and the material
	{
		AActor* Actor = Rig.Spawn();
		UGSTN_TestSurfaceProviderBox* Box = Rig.Add<UGSTN_TestSurfaceProviderBox>(Actor, TEXT("Box"));
		Box->Answer = MakeBehaviour({ GSTNSurfaceTags::Behaviour_Launch });
		UGSTN_SurfaceBehaviourComponent* Surface = Rig.Add<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
		Surface->Default = MakeBehaviour({ GSTNSurfaceTags::Behaviour_Stand });

		const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, Default);
		TestTrue(TEXT("Component wins"), View.Source == EGSTN_SurfaceSource::Component);
		TestTrue(TEXT("Component's behaviour"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand));
		TestFalse(TEXT("Not the provider's"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	}

	// 2. provider on the collider beats the material
	{
		AActor* Actor = Rig.Spawn();
		UGSTN_TestSurfaceProviderBox* Box = Rig.Add<UGSTN_TestSurfaceProviderBox>(Actor, TEXT("Box"));
		Box->Answer = MakeBehaviour({ GSTNSurfaceTags::Behaviour_Launch });

		const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, Default);
		TestTrue(TEXT("Collider provider wins over the material"), View.Source == EGSTN_SurfaceSource::Provider);
		TestTrue(TEXT("Provider's behaviour"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	}

	// 2b. provider on the actor, when the collider is not one
	{
		AGSTN_TestSurfaceProviderActor* Actor = Rig.Spawn<AGSTN_TestSurfaceProviderActor>();
		Actor->Answer = MakeBehaviour({ GSTNSurfaceTags::Behaviour_OneWay });
		UBoxComponent* Box = Rig.Add<UBoxComponent>(Actor, TEXT("Box"));

		const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, Default);
		TestTrue(TEXT("Actor provider wins over the material"), View.Source == EGSTN_SurfaceSource::Provider);
		TestTrue(TEXT("Actor provider's behaviour"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_OneWay));
	}

	// 3. material beats the default
	{
		AActor* Actor = Rig.Spawn();
		UBoxComponent* Box = Rig.Add<UBoxComponent>(Actor, TEXT("Box"));

		const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, Default);
		TestTrue(TEXT("Material wins over the default"), View.Source == EGSTN_SurfaceSource::PhysicalMaterial);
		TestTrue(TEXT("Material's behaviour"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_WallClimb));
		TestFalse(TEXT("Not the default's"), View.HasBehaviour(GSTNSurfaceTags::Behaviour_WallSlide));
	}

	// 3b. the material of a hit: the one the query returned
	{
		FHitResult Hit;
		Hit.PhysMaterial = Material;
		TestTrue(TEXT("Hit's physical material is used"), UGSTN_SurfaceUtils::GetHitPhysicalMaterial(Hit) == Material);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceDefaultPassesEverythingTest, "GSTNSurface.Core.DefaultPassesEverything", Flags)
bool FGSTN_SurfaceDefaultPassesEverythingTest::RunTest(const FString& Parameters)
{
	// The shipped default (plugin ini) keeps unmarked geometry as it was before the plugin
	const FGSTN_SurfaceBehaviour& Shipped = UGSTN_SurfaceSettings::Get().Default;
	TestTrue(TEXT("Shipped default allows wall slide"), Shipped.HasBehaviour(GSTNSurfaceTags::Behaviour_WallSlide));
	TestTrue(TEXT("Shipped default allows wall climb"), Shipped.HasBehaviour(GSTNSurfaceTags::Behaviour_WallClimb));
	TestTrue(TEXT("Shipped default allows standing"), Shipped.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand));
	TestFalse(TEXT("Shipped default does not launch"), Shipped.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));

	FRig Rig;
	AActor* Actor = Rig.Spawn();
	UBoxComponent* Box = Rig.Add<UBoxComponent>(Actor, TEXT("Box"));
	const FGSTN_SurfaceTable Table;
	const FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), nullptr, Table, MakeDefault());
	TestTrue(TEXT("Unmarked collider resolves to the default"), View.Source == EGSTN_SurfaceSource::Default);

	const FGameplayTagQuery SlideQuery = FGameplayTagQuery::MakeQuery_MatchTag(GSTNSurfaceTags::Behaviour_WallSlide);
	TestTrue(TEXT("Wall slide filter accepts unmarked geometry"), View.MatchesBehaviours(SlideQuery));
	TestTrue(TEXT("Empty filter accepts everything"), View.MatchesBehaviours(FGameplayTagQuery()));

	const FGSTN_SurfaceView NoCollider = UGSTN_SurfaceUtils::ResolveQuery(FGSTN_SurfaceQuery(), nullptr, Table, MakeDefault());
	TestTrue(TEXT("A hit without a collider resolves to the default"), NoCollider.Source == EGSTN_SurfaceSource::Default);
	TestFalse(TEXT("A hit without a collider is not moving"), NoCollider.bMoving);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfacePerComponentOverrideTest, "GSTNSurface.Core.PerComponentOverride", Flags)
bool FGSTN_SurfacePerComponentOverrideTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	AActor* Actor = Rig.Spawn();
	UBoxComponent* Flank = Rig.Add<UBoxComponent>(Actor, TEXT("Flank"));
	UBoxComponent* Top = Rig.Add<UBoxComponent>(Actor, TEXT("Top"));
	UGSTN_SurfaceBehaviourComponent* Surface = Rig.Add<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
	Surface->Default = MakeBehaviour({ GSTNSurfaceTags::Behaviour_Stand });
	Surface->PerComponent.Add(TEXT("Flank"), MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallSlide }));

	const FGSTN_SurfaceTable Table;
	const FGSTN_SurfaceView FlankView = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Flank), nullptr, Table, MakeDefault());
	const FGSTN_SurfaceView TopView = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Top), nullptr, Table, MakeDefault());
	TestTrue(TEXT("Named collider takes its override"), FlankView.HasBehaviour(GSTNSurfaceTags::Behaviour_WallSlide));
	TestFalse(TEXT("Named collider does not take Default"), FlankView.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand));
	TestTrue(TEXT("Other collider takes Default"), TopView.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand));
	TestFalse(TEXT("Other collider does not take the override"), TopView.HasBehaviour(GSTNSurfaceTags::Behaviour_WallSlide));

	// Validation: a name that is not a collider of the owner is an error
	Surface->PerComponent.Add(TEXT("Missing"), FGSTN_SurfaceBehaviour());
	TArray<FText> Errors;
	TArray<FText> Warnings;
	Surface->CollectFindings(Errors, Warnings);
	TestEqual(TEXT("Unknown collider name is reported"), Errors.Num(), 1);
	TestEqual(TEXT("No warning with a single surface component"), Warnings.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceTypeAndVelocityTest, "GSTNSurface.Core.TypeFallbackAndProviderVelocity", Flags)
bool FGSTN_SurfaceTypeAndVelocityTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Wall = IniType(TEXT("Surface.Type.Wall"));
	const FGameplayTag Glass = IniType(TEXT("Surface.Type.Glass"));
	const FGameplayTag Trampoline = IniType(TEXT("Surface.Type.Trampoline"));
	if (!TestTrue(TEXT("Surface type leaves are registered"), Wall.IsValid() && Glass.IsValid() && Trampoline.IsValid()))
	{
		return false;
	}

	FRig Rig;
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(GetTransientPackage());
	FGSTN_SurfaceTable Table;
	Table.BakeMaterials({ { Material, MakeBehaviour({}, Glass) } });
	Table.BakeTypes({ Wall, Glass, Trampoline });

	AActor* Actor = Rig.Spawn();
	UGSTN_TestSurfaceProviderBox* Box = Rig.Add<UGSTN_TestSurfaceProviderBox>(Actor, TEXT("Box"));
	Box->SetMobility(EComponentMobility::Movable);
	Box->Velocity = FVector(300.f, 0.f, 0.f);
	UGSTN_SurfaceBehaviourComponent* Surface = Rig.Add<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
	Surface->Default = MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallSlide });

	// Empty type on the component: the material's type, then the default's
	FGSTN_SurfaceView View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, MakeDefault(Wall));
	TestTrue(TEXT("Empty type takes the material's"), View.SurfaceType == Glass);
	TestEqual(TEXT("Type index from the table"), static_cast<int32>(View.SurfaceTypeIndex), static_cast<int32>(Table.GetTypeIndex(Glass)));
	View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), nullptr, Table, MakeDefault(Wall));
	TestTrue(TEXT("No material: the default's type"), View.SurfaceType == Wall);

	// A type on the component wins
	Surface->Default.SurfaceType = Trampoline;
	View = UGSTN_SurfaceUtils::ResolveQuery(QueryFor(Box), Material, Table, MakeDefault(Wall));
	TestTrue(TEXT("Component's own type wins"), View.SurfaceType == Trampoline);

	// The component supplied the behaviour, but the velocity still comes from the provider; mobility is reported
	TestTrue(TEXT("Component supplied the behaviour"), View.Source == EGSTN_SurfaceSource::Component);
	TestEqual(TEXT("Provider supplies the velocity"), View.SurfaceVelocity, FVector(300.f, 0.f, 0.f));
	TestTrue(TEXT("Movable collider is moving"), View.bMoving);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceLaunchParamsValidatedTest, "GSTNSurface.Core.LaunchParamsValidated", Flags)
bool FGSTN_SurfaceLaunchParamsValidatedTest::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;
	TestTrue(TEXT("Defaults are valid"), FGSTN_SurfaceLaunchParams().Validate(Errors));

	FGSTN_SurfaceLaunchParams ZeroDirection;
	ZeroDirection.DirectionMode = EGSTN_SurfaceLaunchDirection::LocalDirection;
	ZeroDirection.LocalDirection = FVector::ZeroVector;
	TestFalse(TEXT("Zero local direction is invalid"), ZeroDirection.Validate(Errors));

	FGSTN_SurfaceLaunchParams Inverted;
	Inverted.SpeedMode = EGSTN_SurfaceLaunchSpeed::ScaledByIncoming;
	Inverted.MinSpeed = 2000.f;
	Inverted.MaxSpeed = 1000.f;
	TestFalse(TEXT("Inverted speed range is invalid"), Inverted.Validate(Errors));

	FGSTN_SurfaceLaunchParams NotFinite;
	NotFinite.Speed = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN speed is invalid"), NotFinite.Validate(Errors));

	FGSTN_SurfaceLaunchParams FlatTilt;
	FlatTilt.DirectionMode = EGSTN_SurfaceLaunchDirection::Tilt;
	FlatTilt.TiltPitch = 90.f;
	TestFalse(TEXT("A tilt flat on the surface is invalid"), FlatTilt.Validate(Errors));

	// The behaviour checks its launch only when it launches, and its tags against their roots
	FGSTN_SurfaceBehaviour NoLaunch = MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallSlide });
	NoLaunch.Launch = Inverted;
	Errors.Reset();
	TestTrue(TEXT("Launch params ignored without the Launch behaviour"), NoLaunch.Validate(Errors));
	NoLaunch.Behaviours.AddTag(GSTNSurfaceTags::Behaviour_Launch);
	TestFalse(TEXT("Launch params checked with the Launch behaviour"), NoLaunch.Validate(Errors));

	FGSTN_SurfaceBehaviour WrongRoot = MakeBehaviour({ GSTNSurfaceTags::Event_Launched });
	TestFalse(TEXT("A non-behaviour tag in Behaviours is invalid"), WrongRoot.Validate(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceLaunchMathTest, "GSTNSurface.Core.LaunchDirectionAndSpeedModes", Flags)
bool FGSTN_SurfaceLaunchMathTest::RunTest(const FString& Parameters)
{
	constexpr float Tolerance = 1.e-4f;
	// A surface rolled 90 degrees: its local up points along world -Y
	const FTransform Rolled(FRotator(0.f, 0.f, 90.f), FVector(100.f, 0.f, 0.f), FVector(2.f, 3.f, 4.f));
	const FVector RolledUp = Rolled.GetRotation().GetUpVector();

	FGSTN_SurfaceLaunchParams Params;
	Params.DirectionMode = EGSTN_SurfaceLaunchDirection::SurfaceNormal;
	TestTrue(TEXT("SurfaceNormal follows the contact normal"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, Rolled, FVector(0.f, 0.f, 2.f)).Equals(FVector::UpVector, Tolerance));
	TestTrue(TEXT("SurfaceNormal without a normal falls back to local up"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, Rolled, FVector::ZeroVector).Equals(RolledUp, Tolerance));

	Params.DirectionMode = EGSTN_SurfaceLaunchDirection::LocalDirection;
	Params.LocalDirection = FVector(0.f, 0.f, 5.f);
	TestTrue(TEXT("LocalDirection rotates with the surface and ignores scale"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, Rolled, FVector::UpVector).Equals(RolledUp, Tolerance));

	Params.DirectionMode = EGSTN_SurfaceLaunchDirection::Tilt;
	Params.TiltPitch = 0.f;
	TestTrue(TEXT("Zero tilt is local up"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, FTransform::Identity, FVector::ZeroVector).Equals(FVector::UpVector, Tolerance));
	Params.TiltPitch = 45.f;
	Params.TiltYaw = 90.f;
	const FVector Expected = FVector(0.f, 1.f, 1.f).GetSafeNormal();
	TestTrue(TEXT("Tilt 45 at yaw 90 leans towards local right"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Params, FTransform::Identity, FVector::ZeroVector).Equals(Expected, Tolerance));

	FGSTN_SurfaceLaunchParams Speed;
	Speed.SpeedMode = EGSTN_SurfaceLaunchSpeed::Fixed;
	Speed.Speed = 1200.f;
	TestEqual(TEXT("Fixed ignores the arrival speed"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(Speed, 5000.f), 1200.f);
	Speed.SpeedMode = EGSTN_SurfaceLaunchSpeed::ScaledByIncoming;
	Speed.Restitution = 0.5f;
	Speed.MinSpeed = 800.f;
	Speed.MaxSpeed = 3000.f;
	TestEqual(TEXT("Scaled by the restitution"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(Speed, 2000.f), 1000.f);
	TestEqual(TEXT("Clamped to the minimum"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(Speed, 100.f), 800.f);
	TestEqual(TEXT("Clamped to the maximum"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(Speed, 10000.f), 3000.f);
	TestEqual(TEXT("Leaving the surface counts as no arrival speed"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(Speed, -500.f), 800.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceMaterialTableBakedTest, "GSTNSurface.Core.MaterialTableBaked", Flags)
bool FGSTN_SurfaceMaterialTableBakedTest::RunTest(const FString& Parameters)
{
	UPhysicalMaterial* Known = NewObject<UPhysicalMaterial>(GetTransientPackage());
	UPhysicalMaterial* Unknown = NewObject<UPhysicalMaterial>(GetTransientPackage());

	FGSTN_SurfaceTable Table;
	TestFalse(TEXT("A new table is not baked"), Table.AreMaterialsBaked());
	Table.BakeMaterials({ { Known, MakeBehaviour({ GSTNSurfaceTags::Behaviour_Launch }) }, { nullptr, FGSTN_SurfaceBehaviour() } });
	TestTrue(TEXT("Baked"), Table.AreMaterialsBaked());
	const FGSTN_SurfaceBehaviour* Entry = Table.FindMaterial(Known);
	TestTrue(TEXT("Known material found"), Entry && Entry->HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestNull(TEXT("Unknown material not found"), Table.FindMaterial(Unknown));
	TestNull(TEXT("Null material not found"), Table.FindMaterial(nullptr));

	TestTrue(TEXT("The settings' table is baked once used"), UGSTN_SurfaceSettings::Get().GetTable().AreMaterialsBaked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceNoRuntimeWriteTest, "GSTNSurface.Core.NoRuntimeWriteInBlueprint", Flags)
bool FGSTN_SurfaceNoRuntimeWriteTest::RunTest(const FString& Parameters)
{
	// Surface data is static configuration: Blueprint may read every field and write none (traversal-surfaces.md §2.1)
	const UStruct* Types[] = {
		FGSTN_SurfaceBehaviour::StaticStruct(), FGSTN_SurfaceLaunchParams::StaticStruct(), FGSTN_SurfaceView::StaticStruct(),
		FGSTN_SurfaceQuery::StaticStruct(), UGSTN_SurfaceBehaviourComponent::StaticClass() };
	for (const UStruct* Type : Types)
	{
		for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_BlueprintVisible))
			{
				TestTrue(FString::Printf(TEXT("%s::%s is read-only in Blueprint"), *Type->GetName(), *It->GetName()),
					It->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
			}
		}
	}
	return true;
}

// Review 2026-10-23 B.2: the type indices are what a sync state carries, so a machine whose tag set differs must be detectable and a
// running game must not shift them
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceTypeChecksumTest, "GSTNSurface.Core.TypeTableChecksum", Flags)
bool FGSTN_SurfaceTypeChecksumTest::RunTest(const FString& Parameters)
{
	const FGameplayTag A = GSTNSurfaceTags::Behaviour_Launch;
	const FGameplayTag B = GSTNSurfaceTags::Behaviour_Stand;
	const FGameplayTag C = GSTNSurfaceTags::Behaviour_WallSlide;

	FGSTN_SurfaceTable First;
	First.BakeTypes({ C, A, B, A });
	FGSTN_SurfaceTable Second;
	Second.BakeTypes({ B, C, A });
	TestEqual(TEXT("The same set gives the same checksum whatever the order and duplicates"), First.GetTypesChecksum(), Second.GetTypesChecksum());
	TestEqual(TEXT("The static form gives the value the bake stores"), FGSTN_SurfaceTable::ComputeTypesChecksum({ A, C, B }), First.GetTypesChecksum());

	FGSTN_SurfaceTable Fewer;
	Fewer.BakeTypes({ A, B });
	TestNotEqual(TEXT("A missing type changes the checksum"), Fewer.GetTypesChecksum(), First.GetTypesChecksum());
	FGSTN_SurfaceTable Other;
	Other.BakeTypes({ A, B, GSTNSurfaceTags::Behaviour_WallClimb });
	TestNotEqual(TEXT("A different type changes the checksum"), Other.GetTypesChecksum(), First.GetTypesChecksum());
	FGSTN_SurfaceTable Empty;
	Empty.BakeTypes({});
	TestNotEqual(TEXT("An empty set differs from a set"), Empty.GetTypesChecksum(), First.GetTypesChecksum());

	// The settings' table exposes the same value through the utils
	TestEqual(TEXT("The utils report the settings' table checksum"), UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum(),
		UGSTN_SurfaceSettings::Get().GetTable().GetTypesChecksum());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceFrozenTableTest, "GSTNSurface.Core.FrozenTypeTableKeepsIndices", Flags)
bool FGSTN_SurfaceFrozenTableTest::RunTest(const FString& Parameters)
{
	const FGameplayTag A = GSTNSurfaceTags::Behaviour_Launch;
	const FGameplayTag B = GSTNSurfaceTags::Behaviour_Stand;
	const FGameplayTag C = GSTNSurfaceTags::Behaviour_WallSlide;

	FGSTN_SurfaceTable Table;
	TestTrue(TEXT("The first bake is accepted"), Table.BakeTypes({ A, C }));
	Table.Freeze();
	const uint32 Checksum = Table.GetTypesChecksum();
	const uint8 IndexOfC = Table.GetTypeIndex(C);
	TestTrue(TEXT("A frozen table reports it"), Table.IsFrozen());

	// A tag that appears later would move C: the table refuses and says so
	AddExpectedError(TEXT("changed after the surface type table was frozen"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A different tag set is refused"), Table.BakeTypes({ A, B, C }));
	TestEqual(TEXT("The checksum did not move"), Table.GetTypesChecksum(), Checksum);
	TestEqual(TEXT("C keeps its index"), static_cast<int32>(Table.GetTypeIndex(C)), static_cast<int32>(IndexOfC));
	TestEqual(TEXT("The new tag has none"), static_cast<int32>(Table.GetTypeIndex(B)), 0);
	TestEqual(TEXT("The table did not grow"), Table.NumTypes(), 2);

	// The same set again, in any order, is a no-op and not an error
	TestTrue(TEXT("The same set is accepted"), Table.BakeTypes({ C, A }));
	TestEqual(TEXT("... and changes nothing"), static_cast<int32>(Table.GetTypeIndex(C)), static_cast<int32>(IndexOfC));

	// Forgetting the table is the only way to a new set
	Table.Reset();
	TestFalse(TEXT("Reset unfreezes"), Table.IsFrozen());
	TestTrue(TEXT("A new set bakes after a reset"), Table.BakeTypes({ A, B, C }));
	TestEqual(TEXT("... with the new indices"), Table.NumTypes(), 3);

	// Materials can be forgotten without touching the frozen types
	Table.Freeze();
	Table.ResetMaterials();
	TestTrue(TEXT("Forgetting the materials keeps the freeze"), Table.IsFrozen() && Table.AreTypesBaked());
	TestFalse(TEXT("... and unbakes the materials"), Table.AreMaterialsBaked());
	return true;
}

// Review 2026-10-23 B.7: GetTable() ran LoadSynchronous from inside the simulation when nothing had baked the table yet
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceGetTableNeverLoadsTest, "GSTNSurface.Core.GetTableNeverLoadsAssets", Flags)
bool FGSTN_SurfaceGetTableNeverLoadsTest::RunTest(const FString& Parameters)
{
	UGSTN_SurfaceSettings* Settings = NewObject<UGSTN_SurfaceSettings>(GetTransientPackage());
	Settings->PhysicalMaterials.Reset();

	// One material that is in memory, one path that is not: a missing asset, and an engine asset that exists on disk but is not loaded
	UPhysicalMaterial* Loaded = NewObject<UPhysicalMaterial>(GetTransientPackage());
	Settings->PhysicalMaterials.Add(TSoftObjectPtr<UPhysicalMaterial>(Loaded), MakeBehaviour({ GSTNSurfaceTags::Behaviour_Launch }));
	Settings->PhysicalMaterials.Add(TSoftObjectPtr<UPhysicalMaterial>(FSoftObjectPath(TEXT("/Game/GSTNSurfaceTest/Missing.Missing"))), MakeBehaviour({ GSTNSurfaceTags::Behaviour_Stand }));

	// Any engine asset on disk that nothing has loaded: loading it is what the lookup must not do
	FString UnloadedPath;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(FPaths::EngineContentDir() / TEXT("EngineMaterials/*.uasset")), true, false);
	for (const FString& File : Files)
	{
		const FString Name = FPaths::GetBaseFilename(File);
		const FString Object = FString::Printf(TEXT("/Engine/EngineMaterials/%s.%s"), *Name, *Name);
		if (StaticFindObject(UObject::StaticClass(), nullptr, *Object) == nullptr)
		{
			UnloadedPath = Object;
			break;
		}
	}
	if (!UnloadedPath.IsEmpty())
	{
		Settings->PhysicalMaterials.Add(TSoftObjectPtr<UPhysicalMaterial>(FSoftObjectPath(UnloadedPath)), MakeBehaviour({ GSTNSurfaceTags::Behaviour_WallClimb }));
	}

	const FGSTN_SurfaceTable& Table = Settings->GetTable();
	TestTrue(TEXT("The table is baked on first use"), Table.AreMaterialsBaked() && Table.AreTypesBaked());
	const FGSTN_SurfaceBehaviour* Entry = Table.FindMaterial(Loaded);
	TestTrue(TEXT("The material that is in memory is in the table"), Entry && Entry->HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	if (!UnloadedPath.IsEmpty())
	{
		TestNull(TEXT("The engine asset that was not loaded was not loaded by the lookup"), StaticFindObject(UObject::StaticClass(), nullptr, *UnloadedPath));
	}
	else
	{
		AddInfo(TEXT("No unloaded engine asset was found to prove the lookup does not load; only the table contents were checked"));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
