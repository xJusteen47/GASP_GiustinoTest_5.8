// M12 G0/G1: Experience activation order and the pre-Active simulation holdback (spike G4, pure part). No world and no Game Features
// subsystem: a fake backend completes each load step by hand, so every intermediate state can be inspected. The host/client GameState
// half of G4 needs a running PIE session and is covered by the harness scenario of G8.

#include "GSTN_ExperienceDefinition.h"
#include "GSTN_ExperienceActionSet.h"
#include "GSTN_ExperienceGateComponent.h"
#include "GSTN_ExperienceLoadBackend.h"
#include "GSTN_ExperienceManagerComponent.h"
#include "GSTN_PawnData.h"
#include "GameFeatureAction.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Completes nothing by itself: the test pumps each step and reads the log in between.
	class FFakeBackend : public FGSTN_ExperienceLoadBackend
	{
	public:
		TArray<FString> Log;
		TArray<FPrimaryAssetId> BundleAssets;
		TArray<FName> BundleNames;
		TFunction<void()> PendingBundles;
		TMap<FString, TFunction<void(bool)>> PendingPlugins;
		TArray<UGameFeatureAction*> Executed;
		TArray<UGameFeatureAction*> Deactivated;

		virtual void LoadBundles(const TArray<FPrimaryAssetId>& Assets, const TArray<FName>& Bundles, TFunction<void()> OnDone) override
		{
			Log.Add(TEXT("Bundles"));
			BundleAssets = Assets;
			BundleNames = Bundles;
			PendingBundles = MoveTemp(OnDone);
		}

		virtual void ActivateGameFeaturePlugin(const FString& PluginName, TFunction<void(bool)> OnDone) override
		{
			Log.Add(TEXT("Plugin:") + PluginName);
			PendingPlugins.Add(PluginName, MoveTemp(OnDone));
		}

		virtual void ExecuteActions(const TArray<UGameFeatureAction*>& Actions, UWorld*) override
		{
			Log.Add(TEXT("Actions"));
			Executed = Actions;
		}

		virtual void DeactivateActions(const TArray<UGameFeatureAction*>& Actions, UWorld*) override
		{
			Log.Add(TEXT("Deactivate"));
			Deactivated = Actions;
		}

		void PumpBundles()
		{
			if (TFunction<void()> Done = MoveTemp(PendingBundles))
			{
				Done();
			}
		}

		void CompletePlugin(const FString& Name, bool bSuccess = true)
		{
			TFunction<void(bool)> Done;
			PendingPlugins.RemoveAndCopyValue(Name, Done);
			if (Done)
			{
				Done(bSuccess);
			}
		}
	};

	// UGameFeatureAction itself is abstract; any concrete engine action will do as a marker, only its identity matters here.
	UGameFeatureAction* NewMarkerAction()
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UGameFeatureAction::StaticClass())
				&& !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
				&& !It->HasAnyClassFlags(CLASS_Transient))
			{
				return NewObject<UGameFeatureAction>(GetTransientPackage(), *It);
			}
		}
		return nullptr;
	}

	struct FFixture
	{
		TStrongObjectPtr<UGSTN_ExperienceManagerComponent> Manager;
		TStrongObjectPtr<UGSTN_ExperienceDefinition> Experience;
		TStrongObjectPtr<UGSTN_ExperienceActionSet> Set;
		TStrongObjectPtr<UGSTN_PawnData> PawnData;
		TSharedPtr<FFakeBackend> Backend;
		TArray<UGameFeatureAction*> Actions; // [0] experience, [1] set
		TArray<EGSTN_ExperienceLoadState> States;

		FFixture()
		{
			Manager = TStrongObjectPtr<UGSTN_ExperienceManagerComponent>(NewObject<UGSTN_ExperienceManagerComponent>(GetTransientPackage()));
			Experience = TStrongObjectPtr<UGSTN_ExperienceDefinition>(NewObject<UGSTN_ExperienceDefinition>(GetTransientPackage()));
			Set = TStrongObjectPtr<UGSTN_ExperienceActionSet>(NewObject<UGSTN_ExperienceActionSet>(GetTransientPackage()));
			PawnData = TStrongObjectPtr<UGSTN_PawnData>(NewObject<UGSTN_PawnData>(GetTransientPackage()));

			Actions.Add(NewMarkerAction());
			Actions.Add(NewMarkerAction());

			Experience->GameFeaturesToEnable = { TEXT("FeatA"), TEXT("FeatB") };
			Experience->DefaultPawnData = PawnData.Get();
			Experience->ActionSets.Add(Set.Get());
			Set->GameFeaturesToEnable = { TEXT("FeatB"), TEXT("FeatC") };
			if (Actions[0] && Actions[1])
			{
				Experience->Actions.Add(Actions[0]);
				Set->Actions.Add(Actions[1]);
			}

			Backend = MakeShared<FFakeBackend>();
			Manager->SetLoadBackend(Backend);
			Manager->OnLoadStateChanged().AddLambda([this](EGSTN_ExperienceLoadState S) { States.Add(S); });
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceGateRuleTest, "GSTNExperience.Gate.Rule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceGateRuleTest::RunTest(const FString& Parameters)
{
	// No Experience assigned: never gated, whatever the state says (classic GameMode, existing maps).
	TestTrue(TEXT("unassigned + Unloaded"), FGSTN_ExperienceGate::IsOpen(false, EGSTN_ExperienceLoadState::Unloaded));
	TestTrue(TEXT("unassigned + Loading"), FGSTN_ExperienceGate::IsOpen(false, EGSTN_ExperienceLoadState::Loading));

	// Assigned: closed in every state but Loaded.
	for (EGSTN_ExperienceLoadState State : { EGSTN_ExperienceLoadState::Unloaded, EGSTN_ExperienceLoadState::Loading,
		EGSTN_ExperienceLoadState::LoadingGameFeatures, EGSTN_ExperienceLoadState::ExecutingActions, EGSTN_ExperienceLoadState::Deactivating })
	{
		TestFalse(FString::Printf(TEXT("assigned + state %d is closed"), static_cast<int32>(State)), FGSTN_ExperienceGate::IsOpen(true, State));
	}
	TestTrue(TEXT("assigned + Loaded is open"), FGSTN_ExperienceGate::IsOpen(true, EGSTN_ExperienceLoadState::Loaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceCollectTest, "GSTNExperience.Definition.Collect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceCollectTest::RunTest(const FString& Parameters)
{
	FFixture F;
	if (!TestNotNull(TEXT("an engine GameFeatureAction subclass exists"), F.Actions[0]) || !TestNotNull(TEXT("second marker"), F.Actions[1]))
	{
		return false;
	}
	F.Experience->Actions.Add(nullptr); // null entries are skipped
	F.Set->GameFeaturesToEnable.Add(FString());

	TArray<FString> Names;
	F.Experience->CollectGameFeatureNames(Names);
	TestEqual(TEXT("names deduplicated, own list first"), FString::Join(Names, TEXT(",")), FString(TEXT("FeatA,FeatB,FeatC")));

	TArray<UGameFeatureAction*> Actions;
	F.Experience->CollectActions(Actions);
	TestEqual(TEXT("two actions"), Actions.Num(), 2);
	TestTrue(TEXT("own action first, then the set's"), Actions.Num() == 2 && Actions[0] == F.Actions[0] && Actions[1] == F.Actions[1]);

	TArray<FPrimaryAssetId> Ids;
	F.Experience->CollectPrimaryAssetIds(Ids);
	TestEqual(TEXT("experience, pawn data, action set"), Ids.Num(), 3);
	TestTrue(TEXT("experience id first"), Ids.Num() > 0 && Ids[0] == F.Experience->GetPrimaryAssetId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceActivationOrderTest, "GSTNExperience.Load.ActivationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceActivationOrderTest::RunTest(const FString& Parameters)
{
	FFixture F;
	if (!TestNotNull(TEXT("marker action"), F.Actions[0]) || !TestNotNull(TEXT("marker action 2"), F.Actions[1]))
	{
		return false;
	}
	UGSTN_ExperienceManagerComponent& M = *F.Manager;
	FFakeBackend& B = *F.Backend;

	TArray<FString> Order;
	M.CallOrRegister_OnExperienceLoaded(FGSTN_OnExperienceLoaded::FDelegate::CreateLambda([&Order, &M](const UGSTN_ExperienceDefinition*)
	{
		Order.Add(M.IsExperienceLoaded() ? TEXT("Normal") : TEXT("Normal-too-early"));
	}));
	M.CallOrRegister_OnExperienceLoaded_HighPriority(FGSTN_OnExperienceLoaded::FDelegate::CreateLambda([&Order](const UGSTN_ExperienceDefinition*)
	{
		Order.Add(TEXT("High"));
	}));

	// A gated simulation registered before the load: it must be released last.
	UGSTN_ExperienceGateComponent* Gate = NewObject<UGSTN_ExperienceGateComponent>(GetTransientPackage());
	Gate->OnGateOpenedNative.AddLambda([&Order](const UGSTN_ExperienceDefinition*) { Order.Add(TEXT("Gate")); });

	TestEqual(TEXT("starts Unloaded"), M.GetLoadState(), EGSTN_ExperienceLoadState::Unloaded);
	TestTrue(TEXT("no Experience yet: simulation not gated"), M.IsSimulationOpen());

	M.SetCurrentExperienceDefinition(F.Experience.Get());
	Gate->BindToManager(&M);
	TestTrue(TEXT("gate component is holding"), Gate->IsSimulationHeld());

	// Step 1: bundles. Nothing else has started.
	TestEqual(TEXT("Loading"), M.GetLoadState(), EGSTN_ExperienceLoadState::Loading);
	TestFalse(TEXT("gate closed while Loading"), M.IsSimulationOpen());
	TestEqual(TEXT("only bundles requested"), FString::Join(B.Log, TEXT(",")), FString(TEXT("Bundles")));
	TestEqual(TEXT("experience, pawn data and set bundles requested"), B.BundleAssets.Num(), 3);
	B.PumpBundles();

	// Step 2: Game Features. All three start (deduplicated, in list order), none finished: still not executing actions.
	TestEqual(TEXT("LoadingGameFeatures"), M.GetLoadState(), EGSTN_ExperienceLoadState::LoadingGameFeatures);
	TestEqual(TEXT("plugins requested after bundles"), FString::Join(B.Log, TEXT(",")), FString(TEXT("Bundles,Plugin:FeatA,Plugin:FeatB,Plugin:FeatC")));
	B.CompletePlugin(TEXT("FeatC"));
	B.CompletePlugin(TEXT("FeatA"));
	TestEqual(TEXT("two of three done: still LoadingGameFeatures"), M.GetLoadState(), EGSTN_ExperienceLoadState::LoadingGameFeatures);
	TestTrue(TEXT("no action ran yet"), B.Executed.IsEmpty());
	TestFalse(TEXT("gate still closed"), M.IsSimulationOpen());
	TestTrue(TEXT("nothing delivered yet"), Order.IsEmpty());

	// Step 3: the last plugin finishes: actions, then Loaded, then listeners, then the simulations.
	B.CompletePlugin(TEXT("FeatB"));
	TestEqual(TEXT("Loaded"), M.GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
	TestTrue(TEXT("gate open"), M.IsSimulationOpen());
	TestFalse(TEXT("gate component released"), Gate->IsSimulationHeld());
	TestTrue(TEXT("actions: the Experience's own, then the set's"), B.Executed.Num() == 2 && B.Executed[0] == F.Actions[0] && B.Executed[1] == F.Actions[1]);
	TestEqual(TEXT("actions ran after every plugin"), B.Log.Last(), FString(TEXT("Actions")));
	TestEqual(TEXT("listener order: high, normal, simulations"), FString::Join(Order, TEXT(",")), FString(TEXT("High,Normal,Gate")));
	TestEqual(TEXT("load states in order"), F.States.Num(), 4);
	if (F.States.Num() == 4)
	{
		TestEqual(TEXT("state 0"), F.States[0], EGSTN_ExperienceLoadState::Loading);
		TestEqual(TEXT("state 1"), F.States[1], EGSTN_ExperienceLoadState::LoadingGameFeatures);
		TestEqual(TEXT("state 2"), F.States[2], EGSTN_ExperienceLoadState::ExecutingActions);
		TestEqual(TEXT("state 3"), F.States[3], EGSTN_ExperienceLoadState::Loaded);
	}

	// A listener that arrives after Loaded runs at once; a second Experience is refused.
	bool bLate = false;
	M.CallOrRegister_OnExperienceLoaded(FGSTN_OnExperienceLoaded::FDelegate::CreateLambda([&bLate](const UGSTN_ExperienceDefinition*) { bLate = true; }));
	TestTrue(TEXT("late listener ran immediately"), bLate);

	UGSTN_ExperienceDefinition* Other = NewObject<UGSTN_ExperienceDefinition>(GetTransientPackage());
	AddExpectedError(TEXT("already set"), EAutomationExpectedErrorFlags::Contains, 1);
	M.SetCurrentExperienceDefinition(Other);
	TestTrue(TEXT("first Experience kept"), M.GetCurrentExperience() == F.Experience.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceFailedFeatureTest, "GSTNExperience.Load.FailedFeatureStillLoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceFailedFeatureTest::RunTest(const FString& Parameters)
{
	FFixture F;
	F.Experience->GameFeaturesToEnable = { TEXT("Good"), TEXT("Bad") };
	F.Set->GameFeaturesToEnable.Reset();

	F.Manager->SetCurrentExperienceDefinition(F.Experience.Get());
	F.Backend->PumpBundles();
	F.Backend->CompletePlugin(TEXT("Bad"), false);
	F.Backend->CompletePlugin(TEXT("Good"), true);

	TestEqual(TEXT("a failed plugin does not hold the Experience back"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
	TestEqual(TEXT("failure recorded"), FString::Join(F.Manager->GetFailedGameFeaturePlugins(), TEXT(",")), FString(TEXT("Bad")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceNoFeaturesTest, "GSTNExperience.Load.NoFeaturesGoesStraightToActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceNoFeaturesTest::RunTest(const FString& Parameters)
{
	FFixture F;
	F.Experience->GameFeaturesToEnable.Reset();
	F.Set->GameFeaturesToEnable.Reset();

	F.Manager->SetCurrentExperienceDefinition(F.Experience.Get());
	F.Backend->PumpBundles();

	TestEqual(TEXT("Loaded with no plugin step"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
	TestEqual(TEXT("log"), FString::Join(F.Backend->Log, TEXT(",")), FString(TEXT("Bundles,Actions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceShutdownTest, "GSTNExperience.Load.ShutdownIgnoresLateCompletions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceShutdownTest::RunTest(const FString& Parameters)
{
	// Cancelled while the plugins load: late completions must not run actions or reach Loaded.
	{
		FFixture F;
		F.Manager->SetCurrentExperienceDefinition(F.Experience.Get());
		F.Backend->PumpBundles();
		F.Manager->ShutdownExperience();
		F.Backend->CompletePlugin(TEXT("FeatA"));
		F.Backend->CompletePlugin(TEXT("FeatB"));
		F.Backend->CompletePlugin(TEXT("FeatC"));
		TestEqual(TEXT("stays Unloaded"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Unloaded);
		TestTrue(TEXT("no action ran"), F.Backend->Executed.IsEmpty());
		TestFalse(TEXT("an assigned, unloaded Experience keeps the gate closed"), F.Manager->IsSimulationOpen());
	}

	// Cancelled while the bundles load.
	{
		FFixture F;
		F.Manager->SetCurrentExperienceDefinition(F.Experience.Get());
		F.Manager->ShutdownExperience();
		F.Backend->PumpBundles();
		TestEqual(TEXT("bundles completing late does nothing"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Unloaded);
		TestEqual(TEXT("no plugin requested"), FString::Join(F.Backend->Log, TEXT(",")), FString(TEXT("Bundles")));
	}

	// Shut down after Loaded: the actions that ran are deactivated, in the same set.
	{
		FFixture F;
		F.Manager->SetCurrentExperienceDefinition(F.Experience.Get());
		F.Backend->PumpBundles();
		F.Backend->CompletePlugin(TEXT("FeatA"));
		F.Backend->CompletePlugin(TEXT("FeatB"));
		F.Backend->CompletePlugin(TEXT("FeatC"));
		TestEqual(TEXT("loaded"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
		F.Manager->ShutdownExperience();
		TestEqual(TEXT("actions deactivated"), F.Backend->Deactivated.Num(), 2);
		TestEqual(TEXT("back to Unloaded"), F.Manager->GetLoadState(), EGSTN_ExperienceLoadState::Unloaded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExperienceTickHoldbackTest, "GSTNExperience.Gate.PreActiveTickHoldback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ExperienceTickHoldbackTest::RunTest(const FString& Parameters)
{
	FFixture F;
	UGSTN_ExperienceManagerComponent& M = *F.Manager;
	FFakeBackend& B = *F.Backend;

	UGSTN_ExperienceGateComponent* Pawn = NewObject<UGSTN_ExperienceGateComponent>(GetTransientPackage());

	// A pawn that respects the gate: one simulated frame = ask, run if allowed, report.
	int32 RanTicks = 0;
	const auto SimulateFrame = [&Pawn, &RanTicks]()
	{
		if (Pawn->ShouldRunSimulationTick())
		{
			++RanTicks;
			Pawn->ReportSimulationTickRan();
		}
	};

	M.SetCurrentExperienceDefinition(F.Experience.Get());
	Pawn->BindToManager(&M);

	// Frames pass in every pre-Loaded state.
	const int32 FramesPerState = 5;
	for (int32 i = 0; i < FramesPerState; ++i) { SimulateFrame(); }   // Loading
	B.PumpBundles();
	for (int32 i = 0; i < FramesPerState; ++i) { SimulateFrame(); }   // LoadingGameFeatures
	B.CompletePlugin(TEXT("FeatA"));
	B.CompletePlugin(TEXT("FeatB"));
	for (int32 i = 0; i < FramesPerState; ++i) { SimulateFrame(); }   // still LoadingGameFeatures
	TestEqual(TEXT("still loading"), M.GetLoadState(), EGSTN_ExperienceLoadState::LoadingGameFeatures);

	TestEqual(TEXT("no tick ran before Loaded"), RanTicks, 0);
	TestEqual(TEXT("spike G4 metric: zero leaked ticks"), M.GetLeakedSimTicks(), 0);
	TestEqual(TEXT("every refusal was counted"), M.GetHeldSimTicks(), 3 * FramesPerState);

	B.CompletePlugin(TEXT("FeatC"));
	TestEqual(TEXT("Loaded"), M.GetLoadState(), EGSTN_ExperienceLoadState::Loaded);
	for (int32 i = 0; i < FramesPerState; ++i) { SimulateFrame(); }
	TestEqual(TEXT("ticks resume after Loaded"), RanTicks, FramesPerState);
	TestEqual(TEXT("no refusal after Loaded"), M.GetHeldSimTicks(), 3 * FramesPerState);
	TestEqual(TEXT("still zero leaks"), M.GetLeakedSimTicks(), 0);

	// Negative control: a pawn that ignores the gate is caught by the leak counter, so the metric can actually fail.
	{
		FFixture Bad;
		UGSTN_ExperienceGateComponent* Rogue = NewObject<UGSTN_ExperienceGateComponent>(GetTransientPackage());
		Bad.Manager->SetCurrentExperienceDefinition(Bad.Experience.Get());
		Rogue->BindToManager(Bad.Manager.Get());
		AddExpectedError(TEXT("before the Experience was Loaded"), EAutomationExpectedErrorFlags::Contains, 2);
		Rogue->ReportSimulationTickRan();
		Rogue->ReportSimulationTickRan();
		TestEqual(TEXT("leaks counted"), Bad.Manager->GetLeakedSimTicks(), 2);
	}

	// Registered after the gate opened: released at once, nothing held.
	UGSTN_ExperienceGateComponent* Late = NewObject<UGSTN_ExperienceGateComponent>(GetTransientPackage());
	Late->BindToManager(&M);
	TestFalse(TEXT("a late gate is not held"), Late->IsSimulationHeld());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
