// Runtime automation tests of the depth marker (depth-marker.md §7, GSTNDepthMarker.Runtime.*). Everything runs headless:
// scheduling and ranking are pure, the adapters and the component use a throwaway game world.

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerComponent.h"
#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerPlane.h"
#include "GSTN_DepthMarkerPresenter.h"
#include "GSTN_DepthMarkerScheduler.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthMarkerSubsystem.h"
#include "GSTN_DepthSubject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerRuntimeTest
{
	constexpr EAutomationTestFlags RuntimeTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world to own the test actors */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDepthMarkerTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}
	};

	static TArray<FGSTN_DepthRankEntry> MakeEntries(int32 Count)
	{
		TArray<FGSTN_DepthRankEntry> Entries;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGSTN_DepthRankEntry Entry;
			Entry.Id = Index + 1;
			Entry.Priority = 2;
			Entry.DistanceSq = static_cast<float>((Index + 1) * 1000);
			Entries.Add(Entry);
		}
		return Entries;
	}
}

#define GSTN_DMR_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerRuntimeTest_##Name, "GSTNDepthMarker.Runtime." #Name, GSTNDepthMarkerRuntimeTest::RuntimeTestFlags) \
	bool FGSTNDepthMarkerRuntimeTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNDepthMarkerRuntimeTest;

GSTN_DMR_TEST(SchedulerRespectsTraceCap)
{
	TArray<FGSTN_DepthRankEntry> Entries = MakeEntries(20);
	FGSTN_DepthSchedulerConfig Config;
	TArray<int32> Dropped;
	FGSTN_DepthMarkerScheduler::Rank(Entries, 20, Dropped);
	TArray<int32> Ids;
	FGSTN_DepthMarkerScheduler::PlanProbes(Entries, Config, 100.0, Ids);
	TestEqual(TEXT("the cap holds although all 20 are due"), Ids.Num(), Config.MaxTraces);

	Config.MaxTraces = 0;
	FGSTN_DepthMarkerScheduler::PlanProbes(Entries, Config, 100.0, Ids);
	TestEqual(TEXT("a zero cap issues nothing"), Ids.Num(), 0);

	// The oldest probe goes first: the cap never starves a marker that waited longest
	Config.MaxTraces = 1;
	for (FGSTN_DepthRankEntry& Entry : Entries)
	{
		Entry.LastProbeTime = 50.0;
	}
	Entries[5].LastProbeTime = 10.0;
	FGSTN_DepthMarkerScheduler::PlanProbes(Entries, Config, 100.0, Ids);
	TestTrue(TEXT("oldest first"), Ids.Num() == 1 && Ids[0] == Entries[5].Id);
	return true;
}

GSTN_DMR_TEST(RankingKeepsLocalPlayer)
{
	TArray<FGSTN_DepthRankEntry> Entries = MakeEntries(30);
	FGSTN_DepthRankEntry Local;
	Local.Id = 999;
	Local.Priority = 0;
	Local.DistanceSq = 1.0e9f;
	Entries.Add(Local);
	TArray<int32> Dropped;
	FGSTN_DepthMarkerScheduler::Rank(Entries, 12, Dropped);
	TestEqual(TEXT("cap applied"), Entries.Num(), 12);
	TestEqual(TEXT("the local player ranks first whatever the distance"), Entries[0].Id, 999);
	TestFalse(TEXT("and is never dropped"), Dropped.Contains(999));
	return true;
}

GSTN_DMR_TEST(CapDropsLowestRank)
{
	TArray<FGSTN_DepthRankEntry> Entries = MakeEntries(6);
	Entries[0].Priority = 1; // an ally, far
	Entries[0].DistanceSq = 1.0e9f;
	TArray<int32> Dropped;
	FGSTN_DepthMarkerScheduler::Rank(Entries, 3, Dropped);
	TestEqual(TEXT("three kept"), Entries.Num(), 3);
	TestEqual(TEXT("the ally outranks nearer enemies"), Entries[0].Id, 1);
	TestTrue(TEXT("the farthest enemies are dropped"), Dropped.Contains(6) && Dropped.Contains(5) && Dropped.Contains(4));
	return true;
}

GSTN_DMR_TEST(GroundedSkipsProbe)
{
	FGSTN_DepthSubjectState Subject;
	Subject.bGrounded = true;
	Subject.bAirborne = false;
	TestFalse(TEXT("grounded and hidden: no probe"), FGSTN_DepthMarkerScheduler::WantsProbe(Subject, false));
	TestTrue(TEXT("grounded but still fading out: probe"), FGSTN_DepthMarkerScheduler::WantsProbe(Subject, true));
	Subject.bAirborne = true;
	Subject.bGrounded = false;
	TestTrue(TEXT("airborne: probe"), FGSTN_DepthMarkerScheduler::WantsProbe(Subject, false));
	Subject.bSuppressed = true;
	TestFalse(TEXT("dead or hidden: never"), FGSTN_DepthMarkerScheduler::WantsProbe(Subject, true));
	return true;
}

GSTN_DMR_TEST(AdapterRegistryPriority)
{
	TestTrue(TEXT("the built-in adapters are registered"), FGSTN_DepthAdapterRegistry::Num() >= 2);
	AActor* Plain = NewObject<AActor>(GetTransientPackage());
	ACharacter* Character = NewObject<ACharacter>(GetTransientPackage());
	const UGSTN_DepthSubjectAdapter* ForPlain = FGSTN_DepthAdapterRegistry::Resolve(Plain);
	const UGSTN_DepthSubjectAdapter* ForCharacter = FGSTN_DepthAdapterRegistry::Resolve(Character);
	TestTrue(TEXT("a plain actor gets the actor adapter"), ForPlain && ForPlain->IsA<UGSTN_DepthSubjectAdapter_Actor>());
	TestTrue(TEXT("a character gets the higher priority character movement adapter"),
		ForCharacter && ForCharacter->IsA<UGSTN_DepthSubjectAdapter_CharacterMovement>());
	return true;
}

GSTN_DMR_TEST(CharacterMovementAdapterReadsGravity)
{
	FRig Rig;
	ACharacter* Character = Rig.World->SpawnActor<ACharacter>(FVector(10.0, 20.0, 30.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("character"), Character))
	{
		return false;
	}
	const FVector Gravity = FVector(1.0, 1.0, -1.0).GetSafeNormal();
	Character->GetCharacterMovement()->SetGravityDirection(Gravity);

	FGSTN_DepthSubjectState State;
	const UGSTN_DepthSubjectAdapter* Adapter = FGSTN_DepthAdapterRegistry::Resolve(Character);
	TestTrue(TEXT("adapter reads"), Adapter && Adapter->ReadState(Character, State));
	TestTrue(TEXT("up is the opposite of the character's own gravity, not world Z"), State.Up.Equals(-Gravity, 1.e-4));
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue(TEXT("the feet are a half height below the capsule centre along its own down"),
		State.Feet.Equals(Character->GetActorLocation() - State.Up * HalfHeight, 0.01));
	return true;
}

GSTN_DMR_TEST(DedicatedServerCreatesNothing)
{
	TestFalse(TEXT("dedicated server"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_DedicatedServer, true, true));
	TestTrue(TEXT("standalone"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_Standalone, true, true));
	TestTrue(TEXT("listen host renders like a client"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_ListenServer, true, true));
	TestTrue(TEXT("client"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_Client, true, true));
	TestFalse(TEXT("disabled"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_Standalone, true, false));
	TestFalse(TEXT("not a game world"), UGSTN_DepthMarkerSubsystem::ShouldCreateFor(NM_Standalone, false, true));
	return true;
}

GSTN_DMR_TEST(ComponentNotReplicated)
{
	UGSTN_DepthMarkerComponent* Component = NewObject<UGSTN_DepthMarkerComponent>(GetTransientPackage());
	TestFalse(TEXT("the marker component never replicates"), Component->GetIsReplicated());
	TestFalse(TEXT("nor does its default object"), GetDefault<UGSTN_DepthMarkerComponent>()->GetIsReplicated());
	return true;
}

GSTN_DMR_TEST(TriggerPawnProjectileNeverTargets)
{
	FGSTN_DepthSubjectState Subject;
	Subject.Feet = FVector(0, 0, 500);
	const TArray<TEnumAsByte<ECollisionChannel>> Excluded = { ECC_Pawn, ECC_GameTraceChannel1 };

	auto MakeHit = [&](ECollisionChannel ObjectType, bool bBlocking) -> FGSTN_DepthCandidate
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(GetTransientPackage());
		Box->SetCollisionObjectType(ObjectType);
		FHitResult Hit;
		Hit.bBlockingHit = bBlocking;
		Hit.ImpactPoint = FVector(0, 0, 0);
		Hit.ImpactNormal = FVector::UpVector;
		Hit.Component = Box;
		FGSTN_DepthCandidate Candidate;
		UGSTN_DepthMarkerSubsystem::MakeCandidate(Hit, Subject, Excluded, Candidate);
		return Candidate;
	};

	const FGSTN_DepthCandidate Floor = MakeHit(ECC_WorldStatic, true);
	const FGSTN_DepthCandidate Pawn = MakeHit(ECC_Pawn, true);
	const FGSTN_DepthCandidate Projectile = MakeHit(ECC_GameTraceChannel1, true);
	const FGSTN_DepthCandidate Trigger = MakeHit(ECC_WorldStatic, false);
	TestTrue(TEXT("level geometry is a target"), Floor.bBlocking && !Floor.bExcluded);
	TestEqual(TEXT("the height is measured along Down"), Floor.Height, 500.f);
	TestTrue(TEXT("a pawn is excluded"), Pawn.bExcluded);
	TestTrue(TEXT("a projectile type is excluded"), Projectile.bExcluded);
	TestFalse(TEXT("an overlap-only volume does not block"), Trigger.bBlocking);

	FGSTN_DepthSelectorParams Params;
	TArray<FGSTN_DepthCandidate> Out;
	FGSTN_DepthMarkerMath::FilterCandidates({ Pawn, Projectile, Trigger, Floor }, FVector::UpVector, Params, FGameplayTagQuery(), Out);
	TestTrue(TEXT("only the floor survives the filter"), Out.Num() == 1 && !Out[0].bExcluded && Out[0].bBlocking);

	FHitResult NoComponent;
	FGSTN_DepthCandidate Unused;
	TestFalse(TEXT("a hit with no component is not a candidate"), UGSTN_DepthMarkerSubsystem::MakeCandidate(NoComponent, Subject, Excluded, Unused));
	return true;
}

GSTN_DMR_TEST(StyleValidates)
{
	UGSTN_DepthMarkerStyle* Style = NewObject<UGSTN_DepthMarkerStyle>(GetTransientPackage());
	Style->PopulateDefaultPresenters();
	TArray<FText> Errors;
	TestTrue(TEXT("the built-in look validates"), Style->Validate(Errors));
	TestTrue(TEXT("and has presenters"), Style->Presenters.Num() > 0);

	Style->Presenters.Add(nullptr);
	TestFalse(TEXT("an empty presenter slot is an error"), Style->Validate(Errors));

	Errors.Reset();
	Style->Presenters.Pop();
	Style->Fade.HideHeight = Style->Fade.ShowHeight + 10.f;
	TestFalse(TEXT("no hysteresis is an error"), Style->Validate(Errors));
	return true;
}

GSTN_DMR_TEST(MarkerFollowsPresentedPoseNotSimulatedSteps)
{
	// A Mover pawn: the root is simulated at the fixed tick (steps every 16 ms), the visual mesh is smoothed every frame. The subject must
	// report the mesh pose, and a probe answer taken on an older column must be put on the current column every frame
	FRig Rig;
	AActor* Actor = Rig.World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
	Actor->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(Actor, TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->RegisterComponent();

	const UGSTN_DepthSubjectAdapter* Adapter = GetDefault<UGSTN_DepthSubjectAdapter_Actor>();
	const float Speed = 600.f;
	const double FrameSeconds = 0.004;
	const double FixedSeconds = 0.016;

	FGSTN_DepthTarget Floor;
	Floor.Kind = EGSTN_DepthTargetKind::Surface;
	Floor.LocalPoint = FVector(0, 0, -100);
	Floor.LocalNormal = FVector::UpVector;

	double MaxFeetStep = 0.0;
	double MaxPointStep = 0.0;
	double MaxRootStep = 0.0;
	double MaxError = 0.0;
	FVector PreviousFeet = FVector::ZeroVector;
	FVector PreviousPoint = FVector::ZeroVector;
	FVector PreviousRoot = FVector::ZeroVector;
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		const double Time = Frame * FrameSeconds;
		const FVector Presented(Speed * Time, 0.0, 0.0);
		const FVector Simulated(Speed * FMath::FloorToDouble(Time / FixedSeconds) * FixedSeconds, 0.0, 0.0);
		Root->SetWorldLocation(Simulated);
		Visual->SetWorldLocation(Presented);

		FGSTN_DepthSubjectState State;
		if (!TestTrue(TEXT("the adapter reads the actor"), Adapter->ReadState(Actor, State)))
		{
			return false;
		}
		const FGSTN_DepthTarget Followed = FGSTN_DepthMarkerMath::ReprojectTarget(Floor, FTransform::Identity, State.Feet, State.Up);
		MaxError = FMath::Max(MaxError, FMath::Abs(State.Feet.X - Presented.X));
		if (Frame > 0)
		{
			MaxFeetStep = FMath::Max(MaxFeetStep, FMath::Abs(State.Feet.X - PreviousFeet.X));
			MaxPointStep = FMath::Max(MaxPointStep, FMath::Abs(Followed.Point.X - PreviousPoint.X));
			MaxRootStep = FMath::Max(MaxRootStep, FMath::Abs(Simulated.X - PreviousRoot.X));
		}
		PreviousFeet = State.Feet;
		PreviousPoint = Followed.Point;
		PreviousRoot = Simulated;
	}
	const double PerFrame = Speed * FrameSeconds;
	TestTrue(TEXT("sanity: the simulated root really moves in steps"), MaxRootStep > PerFrame * 3.5);
	TestTrue(TEXT("the feet are the presented pose"), MaxError < 0.01);
	TestTrue(TEXT("the feet move by at most one frame of motion"), MaxFeetStep < PerFrame * 1.05);
	TestTrue(TEXT("the marker point is on the current column and moves smoothly"), MaxPointStep < PerFrame * 1.05);

	// Opting out reads the simulated root (no mesh smoothing for that actor)
	UGSTN_DepthSubjectAdapter_Actor* Raw = NewObject<UGSTN_DepthSubjectAdapter_Actor>(GetTransientPackage());
	Raw->bFollowPresentedMesh = false;
	Root->SetWorldLocation(FVector(96, 0, 0));
	Visual->SetWorldLocation(FVector(100, 0, 0));
	FGSTN_DepthSubjectState RawState;
	Raw->ReadState(Actor, RawState);
	TestTrue(TEXT("opted out: the root"), FMath::IsNearlyEqual(static_cast<float>(RawState.Feet.X), 96.f, 0.01f));
	return true;
}

GSTN_DMR_TEST(SubsystemTicksWithoutContent)
{
	FRig Rig;
	UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("a standalone game world has the subsystem"), Subsystem))
	{
		return false;
	}
	AActor* Actor = Rig.World->SpawnActor<AActor>(FVector(0, 0, 400), FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
	Actor->SetRootComponent(Root);
	Root->RegisterComponent();
	UGSTN_DepthMarkerComponent* Marker = NewObject<UGSTN_DepthMarkerComponent>(Actor);
	Marker->Audience = EGSTN_DepthAudience::All;
	Marker->RegisterComponent();
	Subsystem->RegisterMarker(Marker);
	TestEqual(TEXT("registered"), Subsystem->GetNumMarkers(), 1);

	// A few frames: no crash, the cap holds, the marker is active
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Subsystem->TickMarkers(0.016f);
	}
	TestTrue(TEXT("within the trace cap"), Subsystem->GetLastTraceCount() <= UGSTN_DepthMarkerSettings::Get().MaxTracesPerFrame);
	Subsystem->UnregisterMarker(Marker);
	TestEqual(TEXT("unregistered"), Subsystem->GetNumMarkers(), 0);
	return true;
}

namespace GSTNDepthMarkerRuntimeTest
{
	/** An actor that counts as airborne for the generic adapter (it moves along Up) and carries a marker for everyone to see */
	static UGSTN_DepthMarkerComponent* SpawnAirborneMarker(UWorld* World, UGSTN_DepthMarkerSubsystem* Subsystem, const FVector& Location)
	{
		AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Root->ComponentVelocity = FVector(0, 0, 500);
		UGSTN_DepthMarkerComponent* Marker = NewObject<UGSTN_DepthMarkerComponent>(Actor);
		Marker->Audience = EGSTN_DepthAudience::All;
		Marker->RegisterComponent();
		Subsystem->RegisterMarker(Marker);
		return Marker;
	}

	/** Sets a console variable for the length of a scope */
	struct FScopedCVar
	{
		IConsoleVariable* Variable = nullptr;
		int32 Previous = 0;

		FScopedCVar(const TCHAR* Name, int32 Value)
		{
			Variable = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Variable)
			{
				Previous = Variable->GetInt();
				Variable->Set(Value, ECVF_SetByCode);
			}
		}

		~FScopedCVar()
		{
			if (Variable)
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}
	};
}

GSTN_DMR_TEST(ApproachProbesRespectTraceCap)
{
	FRig Rig;
	UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		SpawnAirborneMarker(Rig.World, Subsystem, FVector(Index * 150.0, 0, 400));
	}
	const int32 Cap = UGSTN_DepthMarkerSettings::Get().MaxTracesPerFrame;
	int32 Worst = 0;
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		Subsystem->TickMarkers(0.016f);
		Worst = FMath::Max(Worst, Subsystem->GetLastTraceCount());
	}
	TestTrue(*FString::Printf(TEXT("primary, footprint and approach probes together stay within the cap (%d of %d)"), Worst, Cap), Worst <= Cap);
	TestEqual(TEXT("markers beyond MaxMarkers are not drawn"), Subsystem->GetLastActiveCount(), FMath::Min(20, UGSTN_DepthMarkerSettings::Get().MaxMarkers));
	return true;
}

GSTN_DMR_TEST(ApproachProbesAreLowestPriority)
{
	FRig Rig;
	UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	SpawnAirborneMarker(Rig.World, Subsystem, FVector(0, 0, 400));

	{
		// One trace per frame: the primary probe takes it, the approach probes get nothing
		FScopedCVar Cap(TEXT("gstn.DepthMarker.TracesPerFrame"), 1);
		Subsystem->TickMarkers(0.016f);
		TestEqual(TEXT("the primary probe goes first"), Subsystem->GetLastTraceCount(), 1);
	}
	{
		// The primary probe is still in flight: all the budget left goes to approach probes, a few slots per tick
		FScopedCVar Cap(TEXT("gstn.DepthMarker.TracesPerFrame"), 8);
		Subsystem->TickMarkers(0.016f);
		const int32 PerTick = UGSTN_DepthMarkerSettings::Get().DefaultParams.ApproachProbesPerTick;
		TestEqual(TEXT("approach probes are staggered: only a few slots per tick"), Subsystem->GetLastTraceCount(), PerTick);
	}
	return true;
}

GSTN_DMR_TEST(PlaneComponentContainsAndProjects)
{
	FRig Rig;
	UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	// A volume tilted 30 degrees about X: its plane follows its own up, not world Z
	AGSTN_DepthMarkerPlaneVolume* Volume = Rig.World->SpawnActor<AGSTN_DepthMarkerPlaneVolume>(FVector(100, 200, 300), FRotator(0, 0, 30));
	if (!TestNotNull(TEXT("volume"), Volume))
	{
		return false;
	}
	UGSTN_DepthMarkerPlaneComponent* Plane = Volume->Plane;
	Plane->SetBoxExtent(FVector(100, 100, 50));
	Plane->PlaneOffsetCm = 10.f;

	FVector Point;
	FVector Normal;
	Plane->GetProjectionPlane(Point, Normal);
	TestTrue(TEXT("the normal is the component's up"), Normal.Equals(Plane->GetUpVector(), 1.e-4));
	TestTrue(TEXT("the plane sits on the top face plus the offset"), Point.Equals(Plane->GetComponentLocation() + Normal * 60.0, 0.01));
	TestTrue(TEXT("inside"), Plane->ContainsPoint(Plane->GetComponentLocation() + Plane->GetUpVector() * 20.0));
	TestFalse(TEXT("above the box"), Plane->ContainsPoint(Plane->GetComponentLocation() + Plane->GetUpVector() * 80.0));
	TestFalse(TEXT("beside the box"), Plane->ContainsPoint(Plane->GetComponentLocation() + Plane->GetRightVector() * 150.0));

	const FGSTN_DepthPlaneCandidate Candidate = Plane->MakeCandidate();
	TestTrue(TEXT("an explicit candidate whose key cannot clash with a hit component"), Candidate.bExplicit && Candidate.SurfaceKey < 0);

	Subsystem->RegisterPlane(Plane);
	TestEqual(TEXT("registered"), Subsystem->GetNumPlanes(), 1);
	Subsystem->UnregisterPlane(Plane);
	TestEqual(TEXT("unregistered"), Subsystem->GetNumPlanes(), 0);
	return true;
}

GSTN_DMR_TEST(ApproachStyleValidates)
{
	UGSTN_DepthMarkerStyle* Style = NewObject<UGSTN_DepthMarkerStyle>(GetTransientPackage());
	Style->PopulateApproachPresenters();
	TArray<FText> Errors;
	TestTrue(TEXT("the built-in approach look validates"), Style->Validate(Errors));
	bool bAllApproach = Style->Presenters.Num() > 0;
	for (const UGSTN_DepthMarkerPresenter* Presenter : Style->Presenters)
	{
		bAllApproach &= Presenter && Presenter->Layer == EGSTN_DepthLayer::Approach;
	}
	TestTrue(TEXT("every presenter draws the approach layer"), bAllApproach);

	UGSTN_DepthMarkerStyle* Primary = NewObject<UGSTN_DepthMarkerStyle>(GetTransientPackage());
	Primary->PopulateDefaultPresenters();
	TestTrue(TEXT("a style has an approach slot, empty by default"), Primary->ApproachStyle.IsNull());
	TestFalse(TEXT("the approach colour differs from the default"), Style->DefaultTint.Equals(Primary->DefaultTint, 0.01f));
	return true;
}

// The approach layer draws from its own target: a missing or "no ground" primary never gates it
GSTN_DMR_TEST(ApproachLayerIndependentOfGround)
{
	UGSTN_DepthPresenter_Ring* Ring = NewObject<UGSTN_DepthPresenter_Ring>(GetTransientPackage());
	Ring->Layer = EGSTN_DepthLayer::Approach;
	Ring->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	for (const EGSTN_DepthTargetKind GroundKind : { EGSTN_DepthTargetKind::None, EGSTN_DepthTargetKind::ReferencePlane })
	{
		FGSTN_DepthMarkerView View;
		View.Below.Kind = GroundKind;
		View.Below.State = GroundKind == EGSTN_DepthTargetKind::None ? EGSTN_DepthMarkerState::Hidden : EGSTN_DepthMarkerState::NoGround;
		View.Approach.Kind = EGSTN_DepthTargetKind::ApproachPlane;
		View.Approach.State = EGSTN_DepthMarkerState::Approach;
		View.bApproachVisible = true;
		TestTrue(TEXT("approach draws whatever the ground below is"), Ring->ShouldDraw(View));
	}
	return true;
}

GSTN_DMR_TEST(PlaneLayerOnlyWithMaterialAndPushesParameters)
{
	FRig Rig;
	AActor* Holder = Rig.World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	FGSTN_DepthMarkerView View;
	View.Approach.Kind = EGSTN_DepthTargetKind::ApproachPlane;
	View.Approach.State = EGSTN_DepthMarkerState::Approach;
	View.Approach.Point = FVector(10, 20, 30);
	View.Approach.PresentNormal = FVector::UpVector;
	View.Approach.EdgeDistanceCm = 77.f;
	View.bApproachVisible = true;
	View.ApproachAlpha = 0.4f;

	UGSTN_DepthPresenter_Plane* Plane = NewObject<UGSTN_DepthPresenter_Plane>(GetTransientPackage());
	Plane->Layer = EGSTN_DepthLayer::Approach;
	Plane->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Plane->OnMarkerUpdated(View, Holder);
	TestNull(TEXT("no material, no layer"), Plane->GetPlaneComponent());

	Plane->PlaneMaterial = TSoftObjectPtr<UMaterialInterface>(UMaterial::GetDefaultMaterial(MD_Surface));
	Plane->OnMarkerUpdated(View, Holder);
	if (!TestNotNull(TEXT("material set: the layer is created"), Plane->GetPlaneComponent()) || !TestNotNull(TEXT("dynamic material"), Plane->GetPlaneMaterialInstance()))
	{
		return false;
	}
	UStaticMeshComponent* Created = Plane->GetPlaneComponent();
	UMaterialInstanceDynamic* Material = Plane->GetPlaneMaterialInstance();
	FLinearColor Position;
	FLinearColor Normal;
	float Opacity = 0.f;
	float Edge = 0.f;
	TestTrue(TEXT("MarkerWorldPosition"), Material->GetVectorParameterValue(TEXT("MarkerWorldPosition"), Position) && FVector(Position.R, Position.G, Position.B).Equals(FVector(10, 20, 30), 0.01));
	TestTrue(TEXT("MarkerNormal"), Material->GetVectorParameterValue(TEXT("MarkerNormal"), Normal) && FVector(Normal.R, Normal.G, Normal.B).Equals(FVector::UpVector, 0.01));
	TestTrue(TEXT("Opacity"), Material->GetScalarParameterValue(TEXT("Opacity"), Opacity) && FMath::IsNearlyEqual(Opacity, 0.4f, 0.001f));
	TestTrue(TEXT("EdgeDistance"), Material->GetScalarParameterValue(TEXT("EdgeDistance"), Edge) && FMath::IsNearlyEqual(Edge, 77.f, 0.01f));

	// Pooled: a second frame reuses the component and the material
	View.Approach.Point = FVector(0, 0, 50);
	Plane->OnMarkerUpdated(View, Holder);
	TestTrue(TEXT("same component"), Plane->GetPlaneComponent() == Created && Plane->GetPlaneMaterialInstance() == Material);
	Material->GetVectorParameterValue(TEXT("MarkerWorldPosition"), Position);
	TestTrue(TEXT("position follows"), FMath::IsNearlyEqual(Position.B, 50.f, 0.01f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
