// Re-entry rules of the wall entry on moving surfaces and after corrections (traversal-surfaces-plan.md T2): GSTN.Wall.V2Surface.*
//
//  - LegacyRulesUnchanged: with the T2 switches off, UGSTN_WallReentryUtils::Judge gives the answer of the rules as FindEntryWall wrote them before
//    T2 (a copy of the old code is the reference), over a grid of memories, records, heights and policies
//  - RegrabSameMovingShapeNoReconcile: the frame of the T3 finding, as the server and the client see it after a correction erased the client's record
//    of the take-off: with the inference both let the wall be grabbed again; the control (switch off) shows the client refusing what the server accepts
//  - InferenceOnlyFromAResetModeAfterTheWall: a stale record proves nothing when its mode is not a reset mode or it is older than the wall
//  - MovableHitUsesBaseRelativeMemory: on a wall that rose or sank since it was left, the height cap is judged in the wall's frame (controls: world heights
//    refuse a legal re-grab on a risen wall and allow climbing a sinking one)
//  - MovableHitWithoutBaseMemoryIsCooldownOnly / StaticHitKeepsHeightCap: the two other combinations of wall and memory
//  - MemoryRecordsMovingWall: the v2 mode records the moving wall's height in the memory (from the probe, or from the ridden base on a tick that leaves
//    before probing, a wall jump); a static wall and a legacy-style mode record nothing
//  - SyncStateCarriesBaseMemory: the new memory fields survive the wire and reconcile like the height

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "WallMovement/MoveLibrary/GSTN_WallReentryUtils.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "GSTN_WallV2TestAccess.h"

namespace GSTNWallReentryTests
{
	constexpr EAutomationTestFlags ReentryTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static const FName ReentryWalking(TEXT("Walking"));
	static const FName ReentryFalling(TEXT("Falling"));
	static const FName ReentrySlide(TEXT("WallSlidingV2"));
	static const FName ReentryDash(TEXT("Dashing"));
	static const TArray<FName> ReentryResetModes = { ReentryWalking };

	/** Memory of a wall facing -X, left at LastHeight at LastTime */
	static FGSTN_WallMovementSyncState MakeReentryMemory(float LastHeight, double LastTimeMs)
	{
		FGSTN_WallMovementSyncState Memory;
		Memory.bHasWallMemory = true;
		Memory.LastWallNormal = FVector(-1.0, 0.0, 0.0);
		Memory.LastWallHeight = LastHeight;
		Memory.LastWallTimeMs = LastTimeMs;
		return Memory;
	}

	static FGSTN_WallReentryRules MakeReentryRules(bool bT2)
	{
		FGSTN_WallReentryRules Rules;
		Rules.MemoryResetModes = ReentryResetModes;
		Rules.bInferModeChangeLostToRollback = bT2;
		Rules.bBaseRelativeHeightCap = bT2;
		return Rules;
	}

	/** A query against the same wall (-X), at Height, now = NowMs, with Mover's record Prev -> Mode at ChangeMs and the start state in CurrentMode */
	static FGSTN_WallReentryQuery MakeReentryQuery(const FGSTN_WallMovementSyncState& Memory, float Height, double NowMs, FName Prev, FName Mode, double ChangeMs,
		FName CurrentMode)
	{
		FGSTN_WallReentryQuery Query;
		Query.Memory = &Memory;
		Query.bHasModeChange = !Mode.IsNone();
		Query.ChangePrevMode = Prev;
		Query.ChangeMode = Mode;
		Query.ChangeTimeMs = ChangeMs;
		Query.CurrentMode = CurrentMode;
		Query.NowMs = NowMs;
		Query.WallNormal = FVector(-1.0, 0.0, 0.0);
		Query.Height = Height;
		return Query;
	}

	/** The rules exactly as UGSTN_WallEntryTransition::FindEntryWall applied them before T2 (true = the wall may be grabbed) */
	static bool LegacyReentryAllows(const FGSTN_WallReentryRules& Rules, const FGSTN_WallReentryQuery& Q)
	{
		const FGSTN_WallMovementSyncState* WallState = Q.Memory;
		const bool bMemoryWasReset = WallState
			&& ((Q.bHasModeChange && Rules.MemoryResetModes.Contains(Q.ChangePrevMode) && Q.ChangeTimeMs >= WallState->LastWallTimeMs)
				|| (WallState->MemoryResetTimeMs >= 0.0 && WallState->MemoryResetTimeMs + 0.5 >= WallState->LastWallTimeMs)
				|| Q.bResetTagPresent);
		if (WallState && WallState->bHasWallMemory && !bMemoryWasReset)
		{
			const FVector LastFlatNormal = FVector::VectorPlaneProject(WallState->LastWallNormal, Q.GravityUp).GetSafeNormal();
			const FVector FlatWallNormal = FVector::VectorPlaneProject(Q.WallNormal, Q.GravityUp).GetSafeNormal();
			const bool bSameWall = !LastFlatNormal.IsNearlyZero() && !FlatWallNormal.IsNearlyZero()
				&& FVector::DotProduct(LastFlatNormal, FlatWallNormal) >= FMath::Cos(FMath::DegreesToRadians(Rules.SameWallAngle));
			if (bSameWall)
			{
				if (Rules.ReentryCooldown > 0.f && (Q.NowMs - WallState->LastWallTimeMs) < Rules.ReentryCooldown * 1000.0)
				{
					return false;
				}
				if (Rules.SameWallPolicy == EGSTN_SameWallPolicy::Deny)
				{
					return false;
				}
				if (Rules.SameWallPolicy == EGSTN_SameWallPolicy::HeightCap && Q.Height > WallState->LastWallHeight + Rules.HeightCapTolerance)
				{
					return false;
				}
			}
		}
		return true;
	}

	/** A throwaway game world with a capsule (the character) and walls to record */
	struct FReentryScene
	{
		UWorld* World = nullptr;
		UCapsuleComponent* Capsule = nullptr;

		FReentryScene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallReentryTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			AActor* Actor = World->SpawnActor<AActor>();
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(34.f, 88.f);
			Capsule->SetMobility(EComponentMobility::Movable);
			Actor->SetRootComponent(Capsule);
			Capsule->SetWorldLocation(FVector(40.0, 0.0, 300.0));
			Capsule->RegisterComponent();
		}

		~FReentryScene()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		UBoxComponent* MakeWall(bool bMovable, const FVector& Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Wall"));
			Box->SetBoxExtent(FVector(50.0, 500.0, 500.0));
			Box->SetMobility(bMovable ? EComponentMobility::Movable : EComponentMobility::Static);
			Actor->SetRootComponent(Box);
			Box->SetWorldLocation(Location);
			Box->RegisterComponent();
			return Box;
		}

		/** The memory a tick on Wall writes (probe = the wall unless bNoProbe; ridden = the base ridden at the start of the tick) */
		FGSTN_WallMovementSyncState Capture(const UGSTN_WallTraverseModeBase& Mode, UPrimitiveComponent* Probe, UPrimitiveComponent* Ridden, bool bLeave)
		{
			FSimulationTickParams Params;
			Params.MovingComps = FMovingComponentSet(Capsule);
			Params.TimeStep.BaseSimTimeMs = 1000.0;
			Params.TimeStep.StepMs = 1000.f / 60.f;
			FGSTN_WallTickState State;
			State.bHasWall = !bLeave;
			State.WallNormal = FVector(1.0, 0.0, 0.0);
			State.GravityUp = FVector::UpVector;
			State.Orientation = Capsule->GetComponentQuat();
			State.LastProbe.Component = Probe;
			State.LastProbe.bValid = Probe != nullptr;
			State.RiddenBase = Ridden;
			if (bLeave)
			{
				State.LeaveTo(ReentryFalling, 0.f);
			}
			FMoverTickEndData Output;
			FGSTN_WallTraverseTestAccess::Capture(Mode, Params, State, bLeave, Output);
			const FGSTN_WallMovementSyncState* Written = Output.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();
			return Written ? *Written : FGSTN_WallMovementSyncState();
		}
	};
}

using namespace GSTNWallReentryTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryLegacyTest, "GSTN.Wall.V2Surface.LegacyRulesUnchanged", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryLegacyTest::RunTest(const FString& Parameters)
{
	int32 Cases = 0;
	int32 Mismatches = 0;
	const EGSTN_SameWallPolicy Policies[] = { EGSTN_SameWallPolicy::Allow, EGSTN_SameWallPolicy::HeightCap, EGSTN_SameWallPolicy::Deny };
	const FName Modes[] = { NAME_None, ReentryWalking, ReentryFalling, ReentrySlide };
	for (EGSTN_SameWallPolicy Policy : Policies)
	{
		for (int32 MemoryKind = 0; MemoryKind < 4; ++MemoryKind)
		{
			FGSTN_WallMovementSyncState Memory = MakeReentryMemory(300.f, 1000.0);
			Memory.bHasWallMemory = MemoryKind != 0;
			Memory.MemoryResetTimeMs = MemoryKind == 2 ? 1000.0 : (MemoryKind == 3 ? 500.0 : -1.0);
			// A moving-base memory too: the legacy rules ignore it
			Memory.bLastWallOnMovingBase = MemoryKind == 3;
			Memory.LastWallBaseHeight = 200.f;
			for (FName Prev : Modes)
			{
				for (FName Mode : Modes)
				{
					for (double ChangeMs : { 900.0, 1000.0, 1200.0 })
					{
						for (float Height : { 250.f, 309.f, 311.f, 500.f })
						{
							for (double NowMs : { 1100.0, 1300.0 })
							{
								for (int32 Variant = 0; Variant < 4; ++Variant)
								{
									FGSTN_WallReentryRules Rules = MakeReentryRules(false);
									Rules.SameWallPolicy = Policy;
									FGSTN_WallReentryQuery Query = MakeReentryQuery(Memory, Height, NowMs, Prev, Mode, ChangeMs, ReentryFalling);
									Query.bResetTagPresent = Variant == 1;
									Query.WallNormal = Variant == 2 ? FVector(0.0, 1.0, 0.0) : Query.WallNormal;
									// A moving hit: with the switches off it changes nothing
									Query.bWallMoving = Variant == 3;
									Query.WallBaseHeight = 400.f;
									const bool bLegacy = LegacyReentryAllows(Rules, Query);
									const bool bNow = UGSTN_WallReentryUtils::Judge(Rules, Query).IsAllowed();
									++Cases;
									Mismatches += bLegacy != bNow ? 1 : 0;
								}
							}
						}
					}
				}
			}
		}
	}
	TestEqual(FString::Printf(TEXT("Switches off: the same answer as the pre-T2 rules in all %d cases"), Cases), Mismatches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryRegrabTest, "GSTN.Wall.V2Surface.RegrabSameMovingShapeNoReconcile", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryRegrabTest::RunTest(const FString& Parameters)
{
	// The harness frame (Saved/Metrics, scenario_surface_wallslide_enemy rep 1): the flank was left at 116.7 (slid to the floor) at t=8000, the character
	// landed (Falling -> Walking at 9000), took off again (Walking -> Falling at 12000) and now meets the flank at 486. A correction rolled the client back
	// to a frame after the server's take-off but before its own: the server's record is the take-off, the client's the landing; both start states say Falling
	const FGSTN_WallMovementSyncState Memory = MakeReentryMemory(116.7f - 10.f, 8000.0);
	const FGSTN_WallReentryQuery Server = MakeReentryQuery(Memory, 486.f, 12500.0, ReentryWalking, ReentryFalling, 12000.0, ReentryFalling);
	const FGSTN_WallReentryQuery Client = MakeReentryQuery(Memory, 486.f, 12500.0, ReentryFalling, ReentryWalking, 9000.0, ReentryFalling);

	const FGSTN_WallReentryRules T2 = MakeReentryRules(true);
	const FGSTN_WallReentryResult ServerT2 = UGSTN_WallReentryUtils::Judge(T2, Server);
	const FGSTN_WallReentryResult ClientT2 = UGSTN_WallReentryUtils::Judge(T2, Client);
	TestTrue(TEXT("Server: the landing forgot the flank, the re-grab is allowed"), ServerT2.IsAllowed());
	TestTrue(TEXT("Client after the correction: allowed too (no reconcile)"), ClientT2.IsAllowed());
	TestEqual(TEXT("Client: from the stale record"), static_cast<int32>(ClientT2.Reset), static_cast<int32>(EGSTN_WallMemoryReset::InferredModeChange));
	TestEqual(TEXT("Server: from its own record"), static_cast<int32>(ServerT2.Reset), static_cast<int32>(EGSTN_WallMemoryReset::ModeChange));

	// Control: the pre-T2 rules disagree on the same frame (the reconcile of the T3 finding)
	const FGSTN_WallReentryRules Legacy = MakeReentryRules(false);
	TestTrue(TEXT("Control: the server allows it"), UGSTN_WallReentryUtils::Judge(Legacy, Server).IsAllowed());
	TestEqual(TEXT("Control: the client refuses it with the height cap"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(Legacy, Client).Verdict),
		static_cast<int32>(EGSTN_WallReentryVerdict::HeightCap));

	// The anti-climb intent holds: without a landing (still Falling since the wall: record Slide -> Falling after it) both refuse
	const FGSTN_WallReentryQuery NoLanding = MakeReentryQuery(Memory, 486.f, 12500.0, ReentrySlide, ReentryFalling, 8000.0, ReentryFalling);
	TestEqual(TEXT("No landing since the wall: still capped"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(T2, NoLanding).Verdict),
		static_cast<int32>(EGSTN_WallReentryVerdict::HeightCap));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryInferenceTest, "GSTN.Wall.V2Surface.InferenceOnlyFromAResetModeAfterTheWall", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryInferenceTest::RunTest(const FString& Parameters)
{
	const FGSTN_WallMovementSyncState Memory = MakeReentryMemory(100.f, 8000.0);
	const FGSTN_WallReentryRules T2 = MakeReentryRules(true);
	auto Verdict = [&](FName Prev, FName Mode, double ChangeMs, FName Current)
	{
		return UGSTN_WallReentryUtils::Judge(T2, MakeReentryQuery(Memory, 400.f, 12500.0, Prev, Mode, ChangeMs, Current));
	};

	TestTrue(TEXT("Stale Walking record after the wall: inferred"), Verdict(ReentryFalling, ReentryWalking, 9000.0, ReentryFalling).IsAllowed());
	TestFalse(TEXT("Stale Walking record older than the wall: no inference"), Verdict(ReentryFalling, ReentryWalking, 7000.0, ReentryFalling).IsAllowed());
	TestFalse(TEXT("Stale record of a mode that is not a reset mode: no inference"), Verdict(ReentryFalling, ReentryDash, 9000.0, ReentryFalling).IsAllowed());
	TestFalse(TEXT("A consistent record (its mode is the current one) is read as before"), Verdict(ReentryDash, ReentryFalling, 9000.0, ReentryFalling).IsAllowed());
	TestFalse(TEXT("Unknown current mode: no inference"), Verdict(ReentryFalling, ReentryWalking, 9000.0, NAME_None).IsAllowed());
	{
		FGSTN_WallReentryQuery NoRecord = MakeReentryQuery(Memory, 400.f, 12500.0, NAME_None, NAME_None, 0.0, ReentryFalling);
		TestFalse(TEXT("No record at all: the memory applies"), UGSTN_WallReentryUtils::Judge(T2, NoRecord).IsAllowed());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryMovableTest, "GSTN.Wall.V2Surface.MovableHitUsesBaseRelativeMemory", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryMovableTest::RunTest(const FString& Parameters)
{
	// Left the moving flank at 300 with its origin at 0 (150 above the origin... in the wall's frame: 300), no landing since (cooldown over)
	FGSTN_WallMovementSyncState Memory = MakeReentryMemory(300.f, 8000.0);
	Memory.bLastWallOnMovingBase = true;
	Memory.LastWallBaseHeight = 0.f;
	const FGSTN_WallReentryRules T2 = MakeReentryRules(true);
	const FGSTN_WallReentryRules Legacy = MakeReentryRules(false);
	auto At = [&](float Height, float WallBaseHeight)
	{
		FGSTN_WallReentryQuery Query = MakeReentryQuery(Memory, Height, 9000.0, ReentrySlide, ReentryFalling, 8000.0, ReentryFalling);
		Query.bWallMoving = true;
		Query.WallBaseHeight = WallBaseHeight;
		return Query;
	};

	// The wall rose 200: lower on it than where it was left (250 above its origin) is allowed, higher (350) is not
	{
		const FGSTN_WallReentryResult Lower = UGSTN_WallReentryUtils::Judge(T2, At(450.f, 200.f));
		TestTrue(TEXT("Risen wall, lower on it: allowed"), Lower.IsAllowed());
		TestTrue(TEXT("Risen wall: judged in the wall's frame"), Lower.bBaseRelative);
		TestEqual(TEXT("Risen wall, higher on it: refused"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(T2, At(550.f, 200.f)).Verdict),
			static_cast<int32>(EGSTN_WallReentryVerdict::HeightCap));
		TestFalse(TEXT("Control: world heights refuse the legal re-grab on the risen wall"), UGSTN_WallReentryUtils::Judge(Legacy, At(450.f, 200.f)).IsAllowed());
	}
	// The wall sank 200: re-grabbing at a world height below the exit but 50 above it on the wall is the climb the cap forbids
	{
		TestEqual(TEXT("Sunk wall, higher on it: refused"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(T2, At(150.f, -200.f)).Verdict),
			static_cast<int32>(EGSTN_WallReentryVerdict::HeightCap));
		TestTrue(TEXT("Control: world heights let the character climb the sinking wall"), UGSTN_WallReentryUtils::Judge(Legacy, At(150.f, -200.f)).IsAllowed());
		TestTrue(TEXT("Sunk wall, lower on it: allowed"), UGSTN_WallReentryUtils::Judge(T2, At(50.f, -200.f)).IsAllowed());
	}
	// A wall that only travels sideways (the T3 fighter): the same answer as world heights
	TestEqual(TEXT("Sideways wall: same verdict as world heights (above)"), UGSTN_WallReentryUtils::Judge(T2, At(400.f, 0.f)).IsAllowed(),
		UGSTN_WallReentryUtils::Judge(Legacy, At(400.f, 0.f)).IsAllowed());
	TestEqual(TEXT("Sideways wall: same verdict as world heights (below)"), UGSTN_WallReentryUtils::Judge(T2, At(200.f, 0.f)).IsAllowed(),
		UGSTN_WallReentryUtils::Judge(Legacy, At(200.f, 0.f)).IsAllowed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryCooldownOnlyTest, "GSTN.Wall.V2Surface.MovableHitWithoutBaseMemoryIsCooldownOnly", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryCooldownOnlyTest::RunTest(const FString& Parameters)
{
	// The memory is of a static wall (no wall frame); the wall found now moves: nothing to compare heights in
	const FGSTN_WallMovementSyncState Memory = MakeReentryMemory(100.f, 8000.0);
	const FGSTN_WallReentryRules T2 = MakeReentryRules(true);
	auto At = [&](double NowMs)
	{
		FGSTN_WallReentryQuery Query = MakeReentryQuery(Memory, 900.f, NowMs, ReentrySlide, ReentryFalling, 8000.0, ReentryFalling);
		Query.bWallMoving = true;
		Query.WallBaseHeight = 0.f;
		return Query;
	};
	TestEqual(TEXT("Inside the cooldown: refused"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(T2, At(8100.0)).Verdict),
		static_cast<int32>(EGSTN_WallReentryVerdict::Cooldown));
	const FGSTN_WallReentryResult After = UGSTN_WallReentryUtils::Judge(T2, At(9000.0));
	TestTrue(TEXT("After it: allowed, the height cap skipped"), After.IsAllowed() && After.bHeightCapSkipped);
	{
		FGSTN_WallReentryRules Deny = T2;
		Deny.SameWallPolicy = EGSTN_SameWallPolicy::Deny;
		TestEqual(TEXT("Deny is still deny on a moving wall"), static_cast<int32>(UGSTN_WallReentryUtils::Judge(Deny, At(9000.0)).Verdict),
			static_cast<int32>(EGSTN_WallReentryVerdict::Denied));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryStaticTest, "GSTN.Wall.V2Surface.StaticHitKeepsHeightCap", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryStaticTest::RunTest(const FString& Parameters)
{
	const FGSTN_WallReentryRules T2 = MakeReentryRules(true);
	for (bool bMemoryOnBase : { false, true })
	{
		FGSTN_WallMovementSyncState Memory = MakeReentryMemory(300.f, 8000.0);
		Memory.bLastWallOnMovingBase = bMemoryOnBase;
		Memory.LastWallBaseHeight = 1000.f;
		const FGSTN_WallReentryQuery Above = MakeReentryQuery(Memory, 320.f, 9000.0, ReentrySlide, ReentryFalling, 8000.0, ReentryFalling);
		const FGSTN_WallReentryQuery Below = MakeReentryQuery(Memory, 305.f, 9000.0, ReentrySlide, ReentryFalling, 8000.0, ReentryFalling);
		const FGSTN_WallReentryResult AboveResult = UGSTN_WallReentryUtils::Judge(T2, Above);
		TestEqual(FString::Printf(TEXT("Static wall (memory on a base: %d): 20 cm above the exit is refused (world heights)"), bMemoryOnBase ? 1 : 0),
			static_cast<int32>(AboveResult.Verdict), static_cast<int32>(EGSTN_WallReentryVerdict::HeightCap));
		TestFalse(TEXT("... and not judged in a wall frame"), AboveResult.bBaseRelative);
		TestTrue(TEXT("Static wall: inside the tolerance is allowed"), UGSTN_WallReentryUtils::Judge(T2, Below).IsAllowed());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentryRecordTest, "GSTN.Wall.V2Surface.MemoryRecordsMovingWall", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentryRecordTest::RunTest(const FString& Parameters)
{
	FReentryScene Scene;
	UBoxComponent* Moving = Scene.MakeWall(true, FVector(-50.0, 0.0, 123.0));
	UBoxComponent* Fixed = Scene.MakeWall(false, FVector(-50.0, 2000.0, 0.0));
	TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
	TestTrue(TEXT("The v2 modes record the wall's frame by default"), Mode->bRecordWallBaseInMemory);

	{
		const FGSTN_WallMovementSyncState Written = Scene.Capture(*Mode, Moving, nullptr, false);
		TestTrue(TEXT("On a moving wall: memory with the wall's frame"), Written.bHasWallMemory && Written.bLastWallOnMovingBase);
		TestEqual(TEXT("... its origin height"), Written.LastWallBaseHeight, 123.f, 0.01f);
		TestEqual(TEXT("... and the capsule's"), Written.LastWallHeight, 300.f, 0.01f);
	}
	{
		// A wall jump leaves before probing: the ridden base names the wall
		const FGSTN_WallMovementSyncState Written = Scene.Capture(*Mode, nullptr, Moving, true);
		TestTrue(TEXT("Leaving on purpose: the ridden base is recorded"), Written.bHasWallMemory && Written.bLastWallOnMovingBase);
		TestEqual(TEXT("... its origin height"), Written.LastWallBaseHeight, 123.f, 0.01f);
	}
	TestFalse(TEXT("A static wall records no frame"), Scene.Capture(*Mode, Fixed, nullptr, false).bLastWallOnMovingBase);
	Mode->bRecordWallBaseInMemory = false;
	TestFalse(TEXT("Control: a mode with the switch off (the legacy default) records no frame"), Scene.Capture(*Mode, Moving, nullptr, false).bLastWallOnMovingBase);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallReentrySyncStateTest, "GSTN.Wall.V2Surface.SyncStateCarriesBaseMemory", GSTNWallReentryTests::ReentryTestFlags)
bool FGSTN_WallReentrySyncStateTest::RunTest(const FString& Parameters)
{
	auto RoundTrip = [](const FGSTN_WallMovementSyncState& In)
	{
		FGSTN_WallMovementSyncState Source = In;
		FBitWriter Writer(4096);
		bool bSuccess = false;
		Source.NetSerialize(Writer, nullptr, bSuccess);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_WallMovementSyncState Out;
		// Garbage in the receiver, which a state without the flag must clear
		Out.bLastWallOnMovingBase = true;
		Out.LastWallBaseHeight = 77.f;
		Out.NetSerialize(Reader, nullptr, bSuccess);
		return Out;
	};

	FGSTN_WallMovementSyncState OnBase = MakeReentryMemory(300.f, 8000.0);
	OnBase.bLastWallOnMovingBase = true;
	OnBase.LastWallBaseHeight = 123.5f;
	const FGSTN_WallMovementSyncState Received = RoundTrip(OnBase);
	TestTrue(TEXT("The flag survives the wire"), Received.bLastWallOnMovingBase);
	TestEqual(TEXT("The wall's height survives the wire"), Received.LastWallBaseHeight, 123.5f);
	TestFalse(TEXT("Received copy does not reconcile"), Received.ShouldReconcile(OnBase));

	const FGSTN_WallMovementSyncState Static = MakeReentryMemory(300.f, 8000.0);
	const FGSTN_WallMovementSyncState ReceivedStatic = RoundTrip(Static);
	TestFalse(TEXT("No flag: cleared on receipt"), ReceivedStatic.bLastWallOnMovingBase || ReceivedStatic.LastWallBaseHeight != 0.f);
	{
		// Without memory the flag is not sent (only the memory gives it a meaning)
		FGSTN_WallMovementSyncState NoMemory;
		NoMemory.bLastWallOnMovingBase = true;
		TestFalse(TEXT("No memory: the flag does not travel"), RoundTrip(NoMemory).bLastWallOnMovingBase);
	}

	FGSTN_WallMovementSyncState Higher = OnBase;
	Higher.LastWallBaseHeight += 20.f;
	TestTrue(TEXT("A different wall height reconciles"), Higher.ShouldReconcile(OnBase));
	FGSTN_WallMovementSyncState Off = OnBase;
	Off.bLastWallOnMovingBase = false;
	TestTrue(TEXT("A different flag reconciles"), Off.ShouldReconcile(OnBase));
	FGSTN_WallMovementSyncState Noise = OnBase;
	Noise.LastWallBaseHeight += 0.5f;
	TestFalse(TEXT("Noise inside the height tolerance does not"), Noise.ShouldReconcile(OnBase));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
