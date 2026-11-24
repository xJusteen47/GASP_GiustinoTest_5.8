// M12 G8: the sweep. One automation test per scenario of the plan (m12-modi-livelli.md G8): the five paths a Levels match walks, composed from the
// pure parts of the frameworks (Experience, ModeVerbs, Respawn, Mission, Levels), with no world, no actors and no Network Prediction.
//
// What this proves and what it does not: the ORDER and the ARITHMETIC of each path, the same on every machine because none of it is simulated. The
// half that needs real sockets (a listen host and a dedicated server with two clients, 2 % loss) is the harness's job; each test names the
// seam the harness drives, so the PIE run only has to confirm the wiring. Filter: `Automation RunTests GSTNMode_Levels.Sweep`.
//
// Test-only edges (Build.cs, editor builds only): GSTNExperience and GSTNModeVerbsCore. The runtime never uses them.

#include "Events/GSTN_SimEvent.h"
#include "GSTN_ExperienceDefinition.h"
#include "GSTN_ExperienceGateComponent.h"
#include "GSTN_ExperienceLoadBackend.h"
#include "GSTN_ExperienceManagerComponent.h"
#include "GSTN_ExperienceTypes.h"
#include "GSTN_LevelsLogic.h"
#include "GSTN_LevelsRules.h"
#include "GSTN_MissionTypes.h"
#include "GSTN_ModeVerbTags.h"
#include "GSTN_ModeVerbTypes.h"
#include "GSTN_RespawnLogic.h"
#include "GSTN_RespawnRules.h"
#include "GSTN_RespawnTags.h"
#include "GSTN_ScoreRules.h"
#include "GameRules/GSTN_GameRules.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// ---------------------------------------------------------------------------------------------------------------------------------------------
	// Scenario 1 fixture: a load backend that completes each step by hand (the Experience's own tests use the same idea)

	class FSweepBackend : public FGSTN_ExperienceLoadBackend
	{
	public:
		TArray<FString> Log;
		TFunction<void()> PendingBundles;
		TMap<FString, TFunction<void(bool)>> PendingPlugins;

		virtual void LoadBundles(const TArray<FPrimaryAssetId>&, const TArray<FName>&, TFunction<void()> OnDone) override
		{
			Log.Add(TEXT("Bundles"));
			PendingBundles = MoveTemp(OnDone);
		}

		virtual void ActivateGameFeaturePlugin(const FString& PluginName, TFunction<void(bool)> OnDone) override
		{
			Log.Add(TEXT("Plugin:") + PluginName);
			PendingPlugins.Add(PluginName, MoveTemp(OnDone));
		}

		virtual void ExecuteActions(const TArray<UGameFeatureAction*>&, UWorld*) override { Log.Add(TEXT("Actions")); }
		virtual void DeactivateActions(const TArray<UGameFeatureAction*>&, UWorld*) override { Log.Add(TEXT("Deactivate")); }

		void PumpBundles()
		{
			if (TFunction<void()> Done = MoveTemp(PendingBundles))
			{
				Done();
			}
		}

		void CompletePlugin(const FString& Name)
		{
			TFunction<void(bool)> Done;
			PendingPlugins.RemoveAndCopyValue(Name, Done);
			if (Done)
			{
				Done(true);
			}
		}
	};

	// ---------------------------------------------------------------------------------------------------------------------------------------------
	// Scenario 2 fixture: the host's side of a verb. The ability only writes a confirmation request; this is what the Verb subsystem does with it

	FGSTN_VerbConfirmRequest MakeVerb(uint32 InstigatorId, int32 ConfirmFrame, int32 HoldFrames)
	{
		FGSTN_VerbConfirmRequest Request;
		Request.Spec.VerbTag = GSTNModeVerbTags::Ability_Mode_Interact;
		Request.Spec.HoldFrames = HoldFrames;
		Request.Payload.VerbTag = Request.Spec.VerbTag;
		Request.Payload.ConfirmFrame = ConfirmFrame;
		Request.InstigatorId = InstigatorId;
		return Request;
	}

	/** A requestable prop on the host: the state a verb may change, and the refusals the host published */
	struct FSweepProp
	{
		int32 UsesLeft = 1;
		int32 Activations = 0;
		TArray<EGSTN_VerbRefusal> Refusals;

		/** The host decides at the confirmation frame (validate against the record, then mutate; a refusal changes nothing) */
		void Resolve(const FGSTN_VerbConfirmRequest& Request, float DistanceCm, int32 ActionableFrom, int32 ToFrame)
		{
			FGSTN_RequestableGate::FInput Input;
			Input.Frame = Request.Payload.ConfirmFrame;
			Input.ActionableFrom = ActionableFrom;
			Input.ToFrame = ToFrame;
			Input.DistanceCm = DistanceCm;
			Input.ReachCm = 200.f;
			Input.SlackCm = 60.f;
			Input.UsesLeft = UsesLeft;
			const EGSTN_VerbRefusal Refusal = FGSTN_RequestableGate::Evaluate(Input);
			if (Refusal != EGSTN_VerbRefusal::None)
			{
				Refusals.Add(Refusal);
				return;
			}
			++Activations;
			UsesLeft = UsesLeft > 0 ? UsesLeft - 1 : UsesLeft;
		}
	};

	// ---------------------------------------------------------------------------------------------------------------------------------------------
	// Scenarios 3 and 4

	FGSTN_SimEvent MakeSweepEvent(const FGameplayTag& Type, int32 Frame = INDEX_NONE, float Magnitude = 1.f)
	{
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Frame = Frame;
		Event.Magnitude = Magnitude;
		return Event;
	}

	FGameplayTagQuery QueryFor(const FGameplayTag& Tag)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(Tag);
		return FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 1. scenario_experience_order: the Experience loads in order and the G4 gate opens only at Loaded

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_M12SweepExperienceOrderTest, "GSTNMode_Levels.Sweep.ExperienceOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_M12SweepExperienceOrderTest::RunTest(const FString& Parameters)
{
	// The Levels experience: the Levels feature plus the two frameworks it ties together
	TStrongObjectPtr<UGSTN_ExperienceManagerComponent> Manager(NewObject<UGSTN_ExperienceManagerComponent>(GetTransientPackage()));
	TStrongObjectPtr<UGSTN_ExperienceDefinition> Experience(NewObject<UGSTN_ExperienceDefinition>(GetTransientPackage()));
	Experience->GameFeaturesToEnable = { TEXT("GSTNMode_Levels"), TEXT("GSTNRespawn"), TEXT("GSTNMission") };
	TSharedPtr<FSweepBackend> Backend = MakeShared<FSweepBackend>();
	Manager->SetLoadBackend(Backend);

	// A player's pawn (its NP tick asks the gate) and what the mode builds the moment the gate opens
	UGSTN_ExperienceGateComponent* Pawn = NewObject<UGSTN_ExperienceGateComponent>(GetTransientPackage());
	TArray<FString> Order;
	Manager->CallOrRegister_OnExperienceLoaded_HighPriority(FGSTN_OnExperienceLoaded::FDelegate::CreateLambda([&Order](const UGSTN_ExperienceDefinition*)
	{
		Order.Add(TEXT("ModeRules"));
	}));
	Pawn->OnGateOpenedNative.AddLambda([&Order](const UGSTN_ExperienceDefinition*) { Order.Add(TEXT("PawnReleased")); });

	// Before the Experience: the mode is off (a map without it plays as before) and nothing is gated
	TestFalse(TEXT("the Levels piece is off until a rules asset enables it"), GetDefault<UGSTN_LevelsRules>()->bEnabled);
	TestTrue(TEXT("no Experience assigned: not gated"), Manager->IsSimulationOpen());

	Manager->SetCurrentExperienceDefinition(Experience.Get());
	Pawn->BindToManager(Manager.Get());

	int32 RanTicks = 0;
	const auto SimulateFrames = [&Pawn, &RanTicks](int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			if (Pawn->ShouldRunSimulationTick())
			{
				++RanTicks;
				Pawn->ReportSimulationTickRan();
			}
		}
	};

	// The pawn spawns early (the game mode is ready before the load ends): it must not simulate in any pre-Loaded state
	SimulateFrames(4);
	Backend->PumpBundles();
	SimulateFrames(4);
	TestEqual(TEXT("bundles come first, then every feature starts"), FString::Join(Backend->Log, TEXT(",")),
		FString(TEXT("Bundles,Plugin:GSTNMode_Levels,Plugin:GSTNRespawn,Plugin:GSTNMission")));

	Backend->CompletePlugin(TEXT("GSTNMission"));
	Backend->CompletePlugin(TEXT("GSTNMode_Levels"));
	SimulateFrames(4);
	TestEqual(TEXT("two of three features active: still loading"), Manager->GetLoadState(), EGSTN_ExperienceLoadState::LoadingGameFeatures);
	TestFalse(TEXT("the G4 gate is closed"), Manager->IsSimulationOpen());
	TestTrue(TEXT("nothing was released"), Order.IsEmpty());
	TestFalse(TEXT("the actions did not run before the last feature"), Backend->Log.Contains(TEXT("Actions")));

	Backend->CompletePlugin(TEXT("GSTNRespawn"));
	TestEqual(TEXT("Loaded"), Manager->GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
	TestTrue(TEXT("the G4 gate is open"), Manager->IsSimulationOpen());
	TestTrue(TEXT("actions ran, after every feature"), Backend->Log.Last() == TEXT("Actions"));
	TestEqual(TEXT("the mode builds its rules first, then the pawns are released"), FString::Join(Order, TEXT(",")), FString(TEXT("ModeRules,PawnReleased")));

	SimulateFrames(4);
	TestEqual(TEXT("not one tick ran before Loaded; the four after it did"), RanTicks, 4);
	TestEqual(TEXT("every early frame was refused (4 + 4 + 4)"), Manager->GetHeldSimTicks(), 12);
	TestEqual(TEXT("zero leaked ticks (spike G4)"), Manager->GetLeakedSimTicks(), 0);
	TestFalse(TEXT("the released pawn is no longer held"), Pawn->IsSimulationHeld());
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 2. scenario_verb_confirm: a held verb gives one authoritative event; a refusal leaves no state

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_M12SweepVerbConfirmTest, "GSTNMode_Levels.Sweep.VerbConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_M12SweepVerbConfirmTest::RunTest(const FString& Parameters)
{
	constexpr int32 HoldFrames = 15;
	constexpr uint32 Player = 7;

	// A console armed at frame 90 and live until 400, one use, 100 cm from the player (reach 200 + slack 60)
	FSweepProp Console;
	FGSTN_VerbConfirmQueue Queue;
	int32 Published = 0;
	TArray<FGSTN_SimEvent> Stream;
	const auto Tick = [&](bool bAuthority, bool bResimulating, int32 Frame, float DistanceCm = 100.f)
	{
		return Queue.Flush(bAuthority, bResimulating, Frame, [&](const FGSTN_VerbConfirmRequest& Request)
		{
			++Published;
			Stream.Add(Request.Payload.ToEvent());
			Console.Resolve(Request, DistanceCm, 90, 400);
		});
	};

	// A released button cancels the hold before the confirmation frame: the ability never writes a request, so nothing exists anywhere
	for (int32 Frame = 100; Frame < 100 + HoldFrames - 1; ++Frame)
	{
		Tick(true, false, Frame);
	}
	TestEqual(TEXT("early release: no event"), Published, 0);
	TestEqual(TEXT("early release: the prop is untouched"), Console.Activations, 0);

	// The hold completes at frame 115. The ability writes the request every pass of every re-simulation of that frame (rule 1: no branches)
	const int32 ConfirmFrame = 100 + HoldFrames;
	Queue.Add(MakeVerb(Player, ConfirmFrame, HoldFrames));
	Tick(true, true, ConfirmFrame);              // a re-simulated flush publishes nothing
	TestEqual(TEXT("a re-simulated flush holds the confirmation"), Published, 0);
	Tick(true, false, ConfirmFrame);             // the real flush
	for (int32 Pass = 0; Pass < 3; ++Pass)       // a rollback to 112 re-runs the confirmation three more times
	{
		Queue.Add(MakeVerb(Player, ConfirmFrame, HoldFrames));
		Tick(true, true, ConfirmFrame - 3);
		Tick(true, false, ConfirmFrame + 1 + Pass);
	}
	TestEqual(TEXT("one authoritative event for the hold"), Published, 1);
	TestEqual(TEXT("the stream carries one Message.Mode.Verb"), Stream.Num(), 1);
	if (Stream.Num() == 1)
	{
		TestTrue(TEXT("type"), Stream[0].EventType == GSTNModeVerbTags::Message_Mode_Verb);
		TestTrue(TEXT("verb"), Stream[0].Tags.HasTagExact(GSTNModeVerbTags::Ability_Mode_Interact));
		TestEqual(TEXT("frame"), Stream[0].Frame, ConfirmFrame);
	}
	TestEqual(TEXT("the console was activated once"), Console.Activations, 1);
	TestEqual(TEXT("and its use was spent"), Console.UsesLeft, 0);
	TestTrue(TEXT("no refusal so far"), Console.Refusals.IsEmpty());

	// A client predicts the same hold and publishes none of it
	{
		FGSTN_VerbConfirmQueue ClientQueue;
		ClientQueue.Add(MakeVerb(Player, ConfirmFrame, HoldFrames));
		TestEqual(TEXT("client: nothing published"), ClientQueue.Flush(false, false, ConfirmFrame + 1, [](const FGSTN_VerbConfirmRequest&) {}), 0);
		TestEqual(TEXT("client: nothing kept"), ClientQueue.NumPending(), 0);
	}

	// Refusals: each is one confirmation that reaches the host and changes nothing
	{
		FSweepProp Fresh;
		Fresh.UsesLeft = 2;
		const FGSTN_VerbConfirmRequest Early = MakeVerb(Player, 80, HoldFrames);   // before ActionableFrom (90)
		Fresh.Resolve(Early, 100.f, 90, 400);
		const FGSTN_VerbConfirmRequest Far = MakeVerb(Player, 200, HoldFrames);
		Fresh.Resolve(Far, 900.f, 90, 400);
		const FGSTN_VerbConfirmRequest Late = MakeVerb(Player, 450, HoldFrames);   // the record ended at 400
		Fresh.Resolve(Late, 100.f, 90, 400);
		TestEqual(TEXT("three refusals recorded"), Fresh.Refusals.Num(), 3);
		if (Fresh.Refusals.Num() == 3)
		{
			TestTrue(TEXT("armed too late for the frame: not actionable"), Fresh.Refusals[0] == EGSTN_VerbRefusal::NotActionable);
			TestTrue(TEXT("out of reach"), Fresh.Refusals[1] == EGSTN_VerbRefusal::OutOfRange);
			TestTrue(TEXT("removed before the confirmation"), Fresh.Refusals[2] == EGSTN_VerbRefusal::Removed);
		}
		TestEqual(TEXT("a refusal leaves no activation"), Fresh.Activations, 0);
		TestEqual(TEXT("a refusal spends no use"), Fresh.UsesLeft, 2);
	}

	// The spent console refuses the next player's hold: exhausted, still one activation in total
	Queue.Add(MakeVerb(8, 300, HoldFrames));
	Tick(true, false, 300);
	TestEqual(TEXT("the second player's confirmation was published (and refused by the prop)"), Published, 2);
	TestEqual(TEXT("exhausted"), Console.Refusals.Num() == 1 ? static_cast<int32>(Console.Refusals[0]) : -1, static_cast<int32>(EGSTN_VerbRefusal::Exhausted));
	TestEqual(TEXT("still one activation"), Console.Activations, 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 3. scenario_respawn_checkpoint: KO zone -> pool -> checkpoint respawn -> team restart

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_M12SweepRespawnCheckpointTest, "GSTNMode_Levels.Sweep.RespawnCheckpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_M12SweepRespawnCheckpointTest::RunTest(const FString& Parameters)
{
	UGSTN_GameRules* GameRules = GSTNLevelsLogic::BuildLevelsGameRules(GetTransientPackage());
	const UGSTN_RespawnRules* Rules = Cast<UGSTN_RespawnRules>(GameRules->FindPiece(UGSTN_RespawnRules::StaticClass()));
	if (!TestNotNull(TEXT("the Levels game rules carry the respawn piece"), Rules))
	{
		return false;
	}
	UGSTN_ScoreRules* Score = UGSTN_ScoreRules::MakeDefault(GetTransientPackage());

	// A KO zone is a KO by reason Zone; the default costs one life
	FGameplayTagContainer ZoneKO;
	const FGameplayTag Zone = FGameplayTag::RequestGameplayTag(TEXT("Combat.KOReason.Zone"), false);
	if (Zone.IsValid())
	{
		ZoneKO.AddTag(Zone);
	}
	const int32 Cost = Rules->GetReason(ZoneKO).LifeCost;
	TestEqual(TEXT("a KO zone costs one life in the Levels defaults"), Cost, 1);

	FGSTN_LivesPool Pool;
	Pool.Reset(*Rules);
	FGSTN_CheckpointProgress Progress;
	FGSTN_LevelTally Tally;
	using namespace GSTNRespawnLogic;
	TestEqual(TEXT("the shared pool starts full"), Pool.Lives, 4);

	// KO 1, before any checkpoint: the pool pays, the player comes back at the level start (nobody safe to join)
	TestTrue(TEXT("KO 1 respawns"), Pool.Spend(Cost) == EGSTN_LifeSpendResult::Respawn);
	Tally.AddEvent(Score, MakeSweepEvent(GSTNRespawnTags::Message_Respawn_Down), 1);
	TestEqual(TEXT("3 lives left"), Pool.Lives, 3);
	TestTrue(TEXT("no checkpoint yet: level start"), ChoosePlacement(*Rules, Progress.HasActive(), false, false) == EGSTN_RespawnPlacement::LevelStart);
	TestEqual(TEXT("20 s later the pool refills one life"), Pool.Advance(Rules->LifeRegenSeconds), 1);
	TestEqual(TEXT("back to 4"), Pool.Lives, 4);

	// The team touches checkpoint A, then B; a collectible is taken on the way
	TestTrue(TEXT("A activates on first touch"), Progress.TryActivate(1, TEXT("Checkpoint_A"), *Rules));
	Tally.AddEvent(Score, MakeSweepEvent(GSTNMissionTags::Message_Mission_Collected), 1);
	TestTrue(TEXT("B takes over"), Progress.TryActivate(2, TEXT("Checkpoint_B"), *Rules));
	TestFalse(TEXT("walking back to A does not regress"), Progress.TryActivate(1, TEXT("Checkpoint_A"), *Rules));
	TestEqual(TEXT("B is the respawn point"), Progress.ActiveId, FName(TEXT("Checkpoint_B")));

	// KO 2 and 3 in the pit: the player comes back at checkpoint B, the second player takes the second spawn point
	for (int32 KO = 0; KO < 2; ++KO)
	{
		TestTrue(TEXT("respawn"), Pool.Spend(Cost) == EGSTN_LifeSpendResult::Respawn);
		Tally.AddEvent(Score, MakeSweepEvent(GSTNRespawnTags::Message_Respawn_Down), 1);
		TestTrue(TEXT("placed at the active checkpoint"), ChoosePlacement(*Rules, Progress.HasActive(), true, false) == EGSTN_RespawnPlacement::Checkpoint);
	}
	TestEqual(TEXT("2 lives left"), Pool.Lives, 2);
	int32 Ring = 0;
	TestEqual(TEXT("player 2 of 2 spawn points"), SpawnPointFor(1, 2, Ring), 1);
	TestEqual(TEXT("first ring"), Ring, 0);
	TestEqual(TEXT("player 3 wraps to a second ring"), SpawnPointFor(2, 2, Ring), 0);
	TestEqual(TEXT("second ring"), Ring, 1);

	// KO 4 leaves one life and KO 5 empties the pool: it restarts the team from the checkpoint (everyone comes back, nobody joins a teammate)
	TestTrue(TEXT("KO 4 respawns"), Pool.Spend(Cost) == EGSTN_LifeSpendResult::Respawn);
	Tally.AddEvent(Score, MakeSweepEvent(GSTNRespawnTags::Message_Respawn_Down), 1);
	const EGSTN_LifeSpendResult Last = Pool.Spend(Cost);
	Tally.AddEvent(Score, MakeSweepEvent(GSTNRespawnTags::Message_Respawn_Down), 1);
	TestTrue(TEXT("the KO that empties the pool restarts the team"), Last == EGSTN_LifeSpendResult::TeamRestart);
	TestEqual(TEXT("empty"), Pool.Lives, 0);
	TestTrue(TEXT("a team restart uses the checkpoint, not a teammate"), ChoosePlacement(*Rules, Progress.HasActive(), true, true) == EGSTN_RespawnPlacement::Checkpoint);

	// The restart: full pool, checkpoint kept, collectibles back, deaths counted
	Pool.Reset(*Rules);
	TestEqual(TEXT("the pool is full again"), Pool.Lives, 4);
	TestEqual(TEXT("the checkpoint is kept"), Progress.ActiveId, FName(TEXT("Checkpoint_B")));
	TestEqual(TEXT("one collectible before the restart"), Tally.TeamCollected, 1);
	Tally.ResetForRestart(Score);
	TestEqual(TEXT("collectibles are back after the restart"), Tally.TeamCollected, 0);
	TestEqual(TEXT("deaths stay (5 KOs in all)"), Tally.Counts.FindRef(TEXT("Deaths")), 5);

	// RestartLevel forgets the checkpoints: everyone at the level start
	Progress.Clear();
	TestTrue(TEXT("restart level: level start"), ChoosePlacement(*Rules, Progress.HasActive(), true, true) == EGSTN_RespawnPlacement::LevelStart);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 4. scenario_mission_replicate: mission state, collectibles and the goal grace reach every machine exactly once

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_M12SweepMissionReplicateTest, "GSTNMode_Levels.Sweep.MissionReplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_M12SweepMissionReplicateTest::RunTest(const FString& Parameters)
{
	// Two collectibles complete the mission (the level's secondary objective)
	UGSTN_MissionDefinition* Def = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	UGSTN_ObjectiveRule_CountEvents* Rule = NewObject<UGSTN_ObjectiveRule_CountEvents>(Def);
	Rule->EventQuery = QueryFor(GSTNMissionTags::Message_Mission_Collected);
	Rule->Count = 2;
	FGSTN_ObjectiveSpec Spec;
	Spec.ObjectiveTag = GSTNMissionTags::Message_Mission_ObjectiveActivated;   // identity borrowed: objective leaves are ini-only
	Spec.Rule = Rule;
	Def->Objectives.Add(Spec);

	FGSTN_MissionTracker Tracker;
	TArray<FGSTN_MissionChange> Changes;
	Tracker.Start(Def, 0.f, true, Changes);

	// The replicated mirror a client holds: the server writes it only when the state differs (a replicated array sends what changed)
	TArray<FGSTN_ObjectiveState> ClientMirror = Tracker.GetObjectives();
	int32 Writes = 0;
	const auto Replicate = [&]()
	{
		if (!(Tracker.GetObjectives() == ClientMirror))
		{
			ClientMirror = Tracker.GetObjectives();
			++Writes;
		}
	};

	UGSTN_ScoreRules* Score = UGSTN_ScoreRules::MakeDefault(GetTransientPackage());
	FGSTN_LevelTally Tally;
	const auto HostEvent = [&](FGSTN_SimEvent& Event)
	{
		Tracker.HandleEvent(Event, 1.f, Changes);
		Tally.AddEvent(Score, Event, 1);
		Replicate();
	};

	// Collectible 1 at frame 100: the client predicts it three times (re-simulation) and publishes none; the host's outbox publishes it once
	FGSTN_SimEventOutbox Client;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		Client.Add(MakeSweepEvent(GSTNMissionTags::Message_Mission_Collected, 100));
	}
	TestEqual(TEXT("the client publishes nothing"), Client.Flush(false, [](FGSTN_SimEvent&) {}), 0);

	FGSTN_SimEventOutbox Host;
	Host.Add(MakeSweepEvent(GSTNMissionTags::Message_Mission_Collected, 100));
	TestEqual(TEXT("the host published one event"), Host.Flush(true, HostEvent), 1);
	TestTrue(TEXT("and its outbox is empty"), Host.IsEmpty());
	TestEqual(TEXT("one collectible counted"), Tracker.GetObjectives()[0].Progress, 1);
	TestEqual(TEXT("the team tally agrees"), Tally.TeamCollected, 1);
	TestTrue(TEXT("the client's copy is the host's"), ClientMirror == Tracker.GetObjectives());
	TestTrue(TEXT("still running"), Tracker.GetState() == EGSTN_MissionState::Running);

	// Collectible 2
	FGSTN_SimEventOutbox Host2;
	Host2.Add(MakeSweepEvent(GSTNMissionTags::Message_Mission_Collected, 160));
	Host2.Flush(true, HostEvent);
	TestTrue(TEXT("the mission completed"), Tracker.GetState() == EGSTN_MissionState::Completed);
	TestTrue(TEXT("the objective completed"), Tracker.GetObjectives()[0].State == EGSTN_ObjectiveState::Completed);
	TestTrue(TEXT("the client's copy is the host's after completion"), ClientMirror == Tracker.GetObjectives());
	TestEqual(TEXT("the state was written once per change (progress 1, then completion)"), Writes, 2);
	int32 MissionCompleted = 0;
	for (const FGSTN_MissionChange& Change : Changes)
	{
		MissionCompleted += Change.EventType == GSTNMissionTags::Message_Mission_MissionCompleted ? 1 : 0;
	}
	TestEqual(TEXT("one MissionCompleted message"), MissionCompleted, 1);
	TestEqual(TEXT("the level state carries the team's 2 collectibles"), Tally.TeamCollected, 2);

	// The goal grace: the server replicates the end time, every machine counts down from its own copy of the server time
	FGSTN_GoalGraceTracker Goal;
	FGSTN_GoalGraceParams Params;
	Params.GraceSeconds = 10.f;
	const FGSTN_GoalGraceResult Started = Goal.Update(100.0, 1, 2, Params);
	TestTrue(TEXT("one alive player starts the timer"), Started.bGraceStarted && !Started.bCompleted);
	const float ReplicatedGraceEnd = static_cast<float>(Goal.GetGraceEnd());   // AGSTN_LevelStateInfo::GraceEndTime
	TestEqual(TEXT("the replicated end time"), ReplicatedGraceEnd, 110.f);
	TestEqual(TEXT("a client at server time 104 counts 6 s, as the host does"), ReplicatedGraceEnd - 104.f, Goal.GetSecondsLeft(104.0));
	TestTrue(TEXT("the host completes the level once the timer ends"), Goal.Update(110.0, 1, 2, Params).bCompleted);

	const FGSTN_LevelResults Results = GSTNLevelsLogic::ComputeResults(*Score, Tally, 60.f, true);
	TestEqual(TEXT("the replicated results carry the team's collectibles"), Results.TeamCollected, 2);
	TestEqual(TEXT("and the completion bonus"), Results.CompletionBonus, 500);

	// A restart brings the collectibles back everywhere: the tally and the mission both start from zero
	Tally.ResetForRestart(Score);
	TestEqual(TEXT("tally reset"), Tally.TeamCollected, 0);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 5. scenario_mode_toggles: what the Levels mode switches, and that the switches are data fixed before pawns exist

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_M12SweepModeTogglesTest, "GSTNMode_Levels.Sweep.ModeToggles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_M12SweepModeTogglesTest::RunTest(const FString& Parameters)
{
	// Friendly fire is ON in Levels (GM.13) and is written into a transient copy of the combat rules, never the shared asset
	UGSTN_CombatRules* Shared = NewObject<UGSTN_CombatRules>(GetTransientPackage());
	Shared->bFriendlyFire = false;
	UGSTN_CombatRules* Copy = DuplicateObject<UGSTN_CombatRules>(Shared, GetTransientPackage());
	GSTNLevelsLogic::ApplyFriendlyFire(*GetDefault<UGSTN_LevelsRules>(), *Copy);
	TestTrue(TEXT("friendly fire is on in the copy the Levels mode uses"), Copy->bFriendlyFire);
	TestFalse(TEXT("the shared combat rules stay as they were (Versus keeps its team rule)"), Shared->bFriendlyFire);

	// The Levels GameRules asset holds only the three server pieces. The Sim toggles (shield, the friendly-fire value the abilities read) come from
	// the rules assets chosen for the map before the first pawn: there is no piece in the Levels asset that could flip one mid-match
	UGSTN_GameRules* GameRules = GSTNLevelsLogic::BuildLevelsGameRules(GetTransientPackage());
	TestEqual(TEXT("levels, respawn and missions pieces, nothing else"), GameRules->Pieces.Num(), 3);
	for (const UGSTN_GameRulesPiece* Piece : GameRules->Pieces)
	{
		const bool bKnown = Cast<UGSTN_LevelsRules>(Piece) || Cast<UGSTN_RespawnRules>(Piece) || Cast<UGSTN_MissionRules>(Piece);
		TestTrue(TEXT("a known server piece"), bKnown);
	}

	// The rules in force are one pointer: a change is detectable (the serial rises once per real change, never for the same asset), so anything
	// built from the old rules knows it is stale. Setting rules while a match runs is therefore visible to the guard that refuses it
	const UGSTN_GameRules* Previous = FGSTN_ActiveGameRules::Get();
	FGSTN_ActiveGameRules::Set(nullptr);
	const uint32 Base = FGSTN_ActiveGameRules::GetSerial();
	int32 Changed = 0;
	const FDelegateHandle Handle = FGSTN_ActiveGameRules::OnChanged().AddLambda([&Changed]() { ++Changed; });
	FGSTN_ActiveGameRules::Set(GameRules);
	FGSTN_ActiveGameRules::Set(GameRules);
	TestEqual(TEXT("setting the same rules twice is one change"), FGSTN_ActiveGameRules::GetSerial(), Base + 1);
	TestEqual(TEXT("and one notification"), Changed, 1);
	TestTrue(TEXT("the Levels pieces are the ones read"), UGSTN_LevelsRules::Get().bEnabled && UGSTN_RespawnRules::Get().bEnabled && UGSTN_MissionRules::Get().bEnabled);
	FGSTN_ActiveGameRules::OnChanged().Remove(Handle);
	FGSTN_ActiveGameRules::Set(Previous);

	// Not covered here, on purpose (see the handover): the shield being unbound in Levels is a value of the defence rules asset chosen for the map
	// (bShieldEnabled false), and the refusal of a Sim toggle change while a pawn exists has no code yet; both need the PIE harness.
	AddInfo(TEXT("scenario_mode_toggles: shield-off and the mid-match refusal are harness checks, not pure ones (docs/notes/handovers/m12-g7-g8.md)"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
