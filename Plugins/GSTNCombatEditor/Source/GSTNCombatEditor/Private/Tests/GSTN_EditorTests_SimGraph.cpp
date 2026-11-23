// The tests of the simulation graph rules (CP2 of the Blueprint seams): which functions and variables they flag, and that they follow only what a
// simulation event of a combat ability Blueprint reaches. The Blueprint is made here, in a temporary package, from the real ability class and
// compiled for real; no project content is loaded.

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_SimGraphRules.h"
#include "Validation/GSTN_ValidationReport.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_SIM_GRAPH_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNSimGraphTest
{
	const UFunction* FindFunction(const UClass* Class, const TCHAR* Name)
	{
		return Class ? Class->FindFunctionByName(FName(Name)) : nullptr;
	}

	template <typename TNode>
	TNode* AddNode(UEdGraph& Graph)
	{
		TNode* Node = NewObject<TNode>(&Graph);
		Node->CreateNewGuid();
		Graph.AddNode(Node, false, false);
		return Node;
	}

	UK2Node_VariableSet* AddSet(UEdGraph& Graph, FName Variable)
	{
		UK2Node_VariableSet* Node = AddNode<UK2Node_VariableSet>(Graph);
		Node->VariableReference.SetSelfMember(Variable);
		Node->AllocateDefaultPins();
		return Node;
	}

	UK2Node_CallFunction* AddCall(UEdGraph& Graph, const UFunction* Function)
	{
		UK2Node_CallFunction* Node = AddNode<UK2Node_CallFunction>(Graph);
		Node->SetFromFunction(Function);
		Node->AllocateDefaultPins();
		return Node;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SimGraphClassifyTest, "GSTNCombat.Editor.SimGraph.Classify", GSTN_SIM_GRAPH_TEST_FLAGS)
bool FGSTN_SimGraphClassifyTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSimGraphTest;
	const UClass* System = UKismetSystemLibrary::StaticClass();
	const UClass* Math = UKismetMathLibrary::StaticClass();

	const UFunction* Delay = FindFunction(System, TEXT("Delay"));
	const UFunction* SetTimer = FindFunction(System, TEXT("K2_SetTimer"));
	const UFunction* Print = FindFunction(System, TEXT("PrintString"));
	const UFunction* RandomFloat = FindFunction(Math, TEXT("RandomFloat"));
	const UFunction* StreamFloat = FindFunction(Math, TEXT("RandomFloatFromStream"));
	const UFunction* ActorLocation = FindFunction(AActor::StaticClass(), TEXT("K2_GetActorLocation"));
	const UFunction* MoveFrame = FindFunction(UGSTN_GA_BaseCombat::StaticClass(), TEXT("GetMoveTimelineFrame"));
	if (!TestTrue(TEXT("the engine functions exist"), Delay && SetTimer && Print && RandomFloat && StreamFloat && ActorLocation && MoveFrame))
	{
		return false;
	}
	TestTrue(TEXT("Delay is latent"), FGSTN_SimGraphRules::IsLatentFunction(*Delay));
	TestTrue(TEXT("a timer counts as latent"), FGSTN_SimGraphRules::IsLatentFunction(*SetTimer));
	TestFalse(TEXT("Print String is not latent"), FGSTN_SimGraphRules::IsLatentFunction(*Print));
	TestTrue(TEXT("Random Float is unseeded"), FGSTN_SimGraphRules::IsUnseededRandom(*RandomFloat));
	TestFalse(TEXT("a stream draw is seeded"), FGSTN_SimGraphRules::IsUnseededRandom(*StreamFloat));
	TestTrue(TEXT("the actor location is a world read"), FGSTN_SimGraphRules::IsWorldRead(*ActorLocation));
	TestFalse(TEXT("the move's own timeline frame is not"), FGSTN_SimGraphRules::IsWorldRead(*MoveFrame));

	const UScriptStruct* SyncInteger = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AbilitySystemSimulation.SyncInteger"));
	TestTrue(TEXT("Sync Integer is a sync variable"), FGSTN_SimGraphRules::IsSyncVarStruct(SyncInteger));
	TestFalse(TEXT("a vector is not"), FGSTN_SimGraphRules::IsSyncVarStruct(TBaseStructure<FVector>::Get()));

	TestTrue(TEXT("On Move Key runs in the simulation"), FGSTN_SimGraphRules::IsSimulationEvent(TEXT("K2_OnMoveKey")));
	TestTrue(TEXT("On Combat Frames Elapsed too"), FGSTN_SimGraphRules::IsSimulationEvent(TEXT("K2_OnCombatFramesElapsed")));
	TestFalse(TEXT("a tick event is not one of them"), FGSTN_SimGraphRules::IsSimulationEvent(TEXT("ReceiveTick")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SimGraphWalkTest, "GSTNCombat.Editor.SimGraph.WalksSimulationEvents", GSTN_SIM_GRAPH_TEST_FLAGS)
bool FGSTN_SimGraphWalkTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSimGraphTest;
	const UScriptStruct* SyncInteger = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AbilitySystemSimulation.SyncInteger"));
	if (!TestNotNull(TEXT("the fork's Sync Integer"), SyncInteger))
	{
		return false;
	}

	const FString Name = FString::Printf(TEXT("GSTNSimGraphTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	TStrongObjectPtr<UPackage> Package(CreatePackage(*(FString(TEXT("/Temp/")) + Name)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(UGSTN_GA_BaseCombat::StaticClass(), Package.Get(), FName(*Name),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName(TEXT("GSTNSimGraphTest"))));
	if (!TestTrue(TEXT("the ability Blueprint was made"), Blueprint.IsValid()))
	{
		return false;
	}

	// A plain integer and a sync integer as members, compiled so the set nodes find them
	FEdGraphPinType IntType;
	IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
	FEdGraphPinType SyncType;
	SyncType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	SyncType.PinSubCategoryObject = const_cast<UScriptStruct*>(SyncInteger);
	FBlueprintEditorUtils::AddMemberVariable(Blueprint.Get(), TEXT("Counter"), IntType);
	FBlueprintEditorUtils::AddMemberVariable(Blueprint.Get(), TEXT("SyncCounter"), SyncType);
	FKismetEditorUtilities::CompileBlueprint(Blueprint.Get());

	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint.Get());
	if (!TestNotNull(TEXT("the event graph"), Graph))
	{
		return false;
	}
	int32 PosY = 0;
	UK2Node_Event* OnKey = FKismetEditorUtilities::AddDefaultEventNode(Blueprint.Get(), Graph, FName(TEXT("K2_OnMoveKey")), UGSTN_GA_BaseCombat::StaticClass(), PosY);
	if (!TestNotNull(TEXT("the On Move Key event"), OnKey))
	{
		return false;
	}

	// On Move Key -> Set Counter (fed by Random Integer) -> Set SyncCounter -> Delay. And a Set Counter and a Delay nothing reaches
	UK2Node_VariableSet* SetCounter = AddSet(*Graph, TEXT("Counter"));
	UK2Node_VariableSet* SetSync = AddSet(*Graph, TEXT("SyncCounter"));
	UK2Node_CallFunction* Random = AddCall(*Graph, UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("RandomInteger")));
	UK2Node_CallFunction* Delay = AddCall(*Graph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("Delay")));
	AddSet(*Graph, TEXT("Counter"));
	AddCall(*Graph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("Delay")));

	UEdGraphPin* Then = OnKey->FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* CounterValue = SetCounter->FindPin(FName(TEXT("Counter")), EGPD_Input);
	UEdGraphPin* RandomOut = Random->GetReturnValuePin();
	if (!TestTrue(TEXT("the pins exist"), Then && CounterValue && RandomOut && SetCounter->GetThenPin() && SetSync->GetThenPin() && Delay->GetExecPin()))
	{
		return false;
	}
	Then->MakeLinkTo(SetCounter->GetExecPin());
	RandomOut->MakeLinkTo(CounterValue);
	SetCounter->GetThenPin()->MakeLinkTo(SetSync->GetExecPin());
	SetSync->GetThenPin()->MakeLinkTo(Delay->GetExecPin());

	FGSTN_ValidationReport Report;
	FGSTN_SimGraphRules::CheckBlueprint(*Blueprint, Blueprint.Get(), Report);
	TestEqual(TEXT("one non-sync write: the reached Set Counter only"), Report.CountCode(GSTNValidationCodes::SimNonSyncVariable()), 1);
	TestEqual(TEXT("one latent node: the reached Delay only"), Report.CountCode(GSTNValidationCodes::SimLatent()), 1);
	TestEqual(TEXT("the random that feeds a reached node"), Report.CountCode(GSTNValidationCodes::SimRandom()), 1);
	TestEqual(TEXT("no world read"), Report.CountCode(GSTNValidationCodes::SimWorldRead()), 0);
	const FGSTN_ValidationFinding* Latent = Report.FindCode(GSTNValidationCodes::SimLatent());
	TestTrue(TEXT("a delay in a simulation event is an error"), Latent && Latent->Severity == EGSTN_Severity::Error);

	Package->ClearDirtyFlag();
	Blueprint->ClearFlags(RF_Standalone | RF_Public);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SimGraphTaskHintsTest, "GSTNCombat.Editor.SimGraph.TaskHints", GSTN_SIM_GRAPH_TEST_FLAGS)
bool FGSTN_SimGraphTaskHintsTest::RunTest(const FString& Parameters)
{
	// Every GAS node a designer brings from PlayMontageAndWait habits gets the combat node to use instead (move-timeline-decoupling 3A)
	const auto Check = [this](const TCHAR* TaskClass, bool bFork, bool bFlagged, EGSTN_Severity Severity, const TCHAR* ReplacementPart)
	{
		const FGSTN_SimGraphRules::FTaskHint Hint = FGSTN_SimGraphRules::FindTaskHint(FName(TaskClass), bFork);
		TestEqual(FString::Printf(TEXT("%s flagged"), TaskClass), Hint.bFlagged, bFlagged);
		if (bFlagged)
		{
			TestTrue(FString::Printf(TEXT("%s severity"), TaskClass), Hint.Severity == Severity);
			TestTrue(FString::Printf(TEXT("%s names %s"), TaskClass, ReplacementPart), Hint.Replacement.ToString().Contains(ReplacementPart));
		}
	};
	Check(TEXT("AbilityTask_WaitDelay"), false, true, EGSTN_Severity::Error, TEXT("Wait Combat Frames"));
	Check(TEXT("AbilityTask_WaitInputPress"), false, true, EGSTN_Severity::Error, TEXT("Wait Command"));
	Check(TEXT("AbilityTask_WaitInputRelease"), false, true, EGSTN_Severity::Error, TEXT("End On Trigger Release"));
	Check(TEXT("AbilityTask_PlayMontageAndWait"), false, true, EGSTN_Severity::Error, TEXT("Play Combat Timeline And Wait"));
	Check(TEXT("AbilityTask_ApplyRootMotionConstantForce"), false, true, EGSTN_Severity::Error, TEXT("Movement track"));
	Check(TEXT("AbilityTask_WaitTargetData"), false, true, EGSTN_Severity::Error, TEXT("Fire Projectile"));
	Check(TEXT("AbilityTask_WaitGameplayEvent"), false, true, EGSTN_Severity::Warning, TEXT(""));
	Check(TEXT("SomeAsyncAction"), false, true, EGSTN_Severity::Warning, TEXT(""));

	// The fork's tasks are rollback safe: flagged only where a combat move needs another node
	Check(TEXT("WaitInputEventPredictionTask"), true, true, EGSTN_Severity::Warning, TEXT("Wait Command"));
	Check(TEXT("WaitAnyInputEventPredictionTask"), true, true, EGSTN_Severity::Warning, TEXT("Wait Command"));
	Check(TEXT("PlayMontageAndWaitPredictionTask"), true, true, EGSTN_Severity::Warning, TEXT("Play Combat Timeline And Wait"));
	Check(TEXT("WaitForDurationPredictionTask"), true, true, EGSTN_Severity::Warning, TEXT("Wait Combat Frames"));
	Check(TEXT("WaitGameplayEventPredictionTask"), true, false, EGSTN_Severity::Warning, TEXT(""));
	Check(TEXT("WaitAttributeChangePredictionTask"), true, false, EGSTN_Severity::Warning, TEXT(""));
	Check(TEXT("GSTN_PlayCombatTimelineTask"), true, false, EGSTN_Severity::Warning, TEXT(""));
	Check(TEXT("GSTN_WaitCommandTask"), true, false, EGSTN_Severity::Warning, TEXT(""));
	Check(TEXT("GSTN_WaitCombatFramesTask"), true, false, EGSTN_Severity::Warning, TEXT(""));

	const UFunction* DeferredSpawn = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("BeginDeferredActorSpawnFromClass"));
	const UFunction* Print = UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
	if (TestTrue(TEXT("the engine functions exist"), DeferredSpawn && Print))
	{
		TestTrue(TEXT("a deferred spawn is a spawn"), FGSTN_SimGraphRules::IsActorSpawn(*DeferredSpawn));
		TestFalse(TEXT("Print String is not"), FGSTN_SimGraphRules::IsActorSpawn(*Print));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
