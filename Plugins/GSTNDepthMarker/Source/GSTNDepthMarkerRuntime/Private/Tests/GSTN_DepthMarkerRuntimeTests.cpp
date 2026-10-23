// Runtime automation tests of the depth marker (depth-marker.md §7, GSTNDepthMarker.Runtime.*). Everything runs headless:
// scheduling and ranking are pure, the adapters and the component use a throwaway game world.

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerComponent.h"
#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerScheduler.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthMarkerSubsystem.h"
#include "GSTN_DepthSubject.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerRuntimeTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

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
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerRuntimeTest_##Name, "GSTNDepthMarker.Runtime." #Name, GSTNDepthMarkerRuntimeTest::Flags) \
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

#endif // WITH_DEV_AUTOMATION_TESTS
