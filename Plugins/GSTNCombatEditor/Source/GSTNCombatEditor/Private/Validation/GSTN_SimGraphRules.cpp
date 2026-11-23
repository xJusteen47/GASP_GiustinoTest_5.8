// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_SimGraphRules.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_ValidationReport.h"

#define LOCTEXT_NAMESPACE "GSTNSimGraphRules"

namespace GSTNSimGraph
{
	/** The fork's sync variable base, found by path: this module has no edge to AbilitySystemSimulation */
	const UScriptStruct* FindSyncVarBase()
	{
		static TWeakObjectPtr<const UScriptStruct> Cached;
		if (!Cached.IsValid())
		{
			Cached = FindObject<UScriptStruct>(nullptr, TEXT("/Script/AbilitySystemSimulation.BaseSyncVar"));
		}
		return Cached.Get();
	}

	bool IsChildOfPath(const UClass* Class, const TCHAR* Path)
	{
		const UClass* Base = FindObject<UClass>(nullptr, Path);
		return Class != nullptr && Base != nullptr && Class->IsChildOf(Base);
	}

	bool NameIn(FName Name, std::initializer_list<const TCHAR*> Names)
	{
		for (const TCHAR* Candidate : Names)
		{
			if (Name == FName(Candidate))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The class an async or prediction task node runs, read by reflection: UK2Node_BaseAsyncTask keeps it protected (ProxyClass), and the fork's
	 * prediction node (TaskInstance) is in an editor module this one has no edge to
	 */
	const UClass* ReadTaskClass(const UEdGraphNode& Node, const TCHAR* PropertyName)
	{
		if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Node.GetClass(), PropertyName))
		{
			const UObject* Value = Property->GetObjectPropertyValue_InContainer(&Node);
			if (const UClass* AsClass = Cast<UClass>(Value))
			{
				return AsClass;
			}
			return Value ? Value->GetClass() : nullptr;
		}
		return nullptr;
	}

	bool IsForkTaskNode(const UEdGraphNode& Node)
	{
		return Node.GetClass()->GetFName() == FName(TEXT("K2Node_PredictionTask"));
	}

	FText NodeName(const UEdGraphNode& Node)
	{
		const UEdGraph* Graph = Node.GetGraph();
		return FText::Format(LOCTEXT("NodeWhere", "'{0}' in {1}"), Node.GetNodeTitle(ENodeTitleType::ListView),
			Graph ? FText::FromName(Graph->GetFName()) : LOCTEXT("NoGraph", "a graph"));
	}
}

bool FGSTN_SimGraphRules::IsSimulationEvent(FName EventFunctionName)
{
	return GSTNSimGraph::NameIn(EventFunctionName, {
		TEXT("K2_ActivateAbility"), TEXT("K2_ActivateAbilityFromEvent"), TEXT("K2_OnEndAbility"),
		TEXT("K2_OnMoveStarted"), TEXT("K2_OnMoveKey"), TEXT("K2_OnMoveHitConfirmed"), TEXT("K2_OnMoveEnded"), TEXT("K2_OnMoveFrame"),
		TEXT("K2_OnCombatFramesElapsed") });
}

bool FGSTN_SimGraphRules::IsSyncVarStruct(const UScriptStruct* Struct)
{
	const UScriptStruct* Base = GSTNSimGraph::FindSyncVarBase();
	return Struct != nullptr && Base != nullptr && Struct->IsChildOf(Base);
}

bool FGSTN_SimGraphRules::IsSyncVarType(const FEdGraphPinType& Type)
{
	return Type.PinCategory == UEdGraphSchema_K2::PC_Struct && !Type.IsContainer() && IsSyncVarStruct(Cast<UScriptStruct>(Type.PinSubCategoryObject.Get()));
}

bool FGSTN_SimGraphRules::IsLatentFunction(const UFunction& Function)
{
	if (Function.HasMetaData(FBlueprintMetadata::MD_Latent))
	{
		return true;
	}
	return Function.GetOwnerClass() == UKismetSystemLibrary::StaticClass() && Function.GetName().StartsWith(TEXT("K2_SetTimer"));
}

bool FGSTN_SimGraphRules::IsWorldRead(const UFunction& Function)
{
	const UClass* Owner = Function.GetOwnerClass();
	const FName Name = Function.GetFName();
	if (GSTNSimGraph::IsChildOfPath(Owner, TEXT("/Script/Mover.MoverComponent")))
	{
		return true;
	}
	if (Owner != nullptr && Owner->IsChildOf(AActor::StaticClass()) && GSTNSimGraph::NameIn(Name, {
		TEXT("K2_GetActorLocation"), TEXT("K2_GetActorRotation"), TEXT("GetTransform"), TEXT("GetVelocity"), TEXT("GetActorForwardVector"),
		TEXT("GetActorRightVector"), TEXT("GetActorUpVector"), TEXT("GetActorScale3D") }))
	{
		return true;
	}
	if (Owner != nullptr && Owner->IsChildOf(USceneComponent::StaticClass()) && GSTNSimGraph::NameIn(Name, {
		TEXT("K2_GetComponentLocation"), TEXT("K2_GetComponentRotation"), TEXT("K2_GetComponentToWorld"), TEXT("GetComponentVelocity"),
		TEXT("GetForwardVector"), TEXT("GetRightVector"), TEXT("GetUpVector"), TEXT("GetSocketLocation"), TEXT("GetSocketRotation"), TEXT("GetSocketTransform") }))
	{
		return true;
	}
	if (Owner == UGameplayStatics::StaticClass() && GSTNSimGraph::NameIn(Name, {
		TEXT("GetTimeSeconds"), TEXT("GetRealTimeSeconds"), TEXT("GetUnpausedTimeSeconds"), TEXT("GetAudioTimeSeconds"), TEXT("GetWorldDeltaSeconds"),
		TEXT("GetAllActorsOfClass"), TEXT("GetAllActorsWithTag"), TEXT("GetAllActorsOfClassWithTag"), TEXT("GetActorOfClass") }))
	{
		return true;
	}
	if (Owner == UKismetSystemLibrary::StaticClass())
	{
		const FString Text = Name.ToString();
		return Name == FName(TEXT("GetGameTimeInSeconds")) || Text.Contains(TEXT("Trace")) || Text.Contains(TEXT("Overlap"));
	}
	return false;
}

bool FGSTN_SimGraphRules::IsUnseededRandom(const UFunction& Function)
{
	// The stream variants are seeded, and a stream held by the ability would be a non-sync member, which the variable rule reports
	const FString Name = Function.GetName();
	return Function.GetOwnerClass() == UKismetMathLibrary::StaticClass() && Name.StartsWith(TEXT("Random")) && !Name.EndsWith(TEXT("FromStream"));
}

FGSTN_SimGraphRules::FTaskHint FGSTN_SimGraphRules::FindTaskHint(FName TaskClassName, bool bForkTask)
{
	FTaskHint Hint;
	const FString Name = TaskClassName.ToString();
	const auto Flag = [&Hint](EGSTN_Severity Severity, const FText& Replacement, const FText& Reason)
	{
		Hint.bFlagged = true;
		Hint.Severity = Severity;
		Hint.Replacement = Replacement;
		Hint.Reason = Reason;
	};
	const FText WaitCombatFrames = LOCTEXT("UseWaitCombatFrames", "Wait Combat Frames");
	const FText WaitCommand = LOCTEXT("UseWaitCommand", "Wait Command (for a release: End On Trigger Release, or Get Button Held Frames)");
	const FText PlayTimeline = LOCTEXT("UsePlayTimeline", "Play Combat Timeline And Wait");

	if (bForkTask)
	{
		// Rollback safe, but each skips something a combat move relies on
		if (Name == TEXT("WaitInputEventPredictionTask") || Name == TEXT("WaitAnyInputEventPredictionTask"))
		{
			Flag(EGSTN_Severity::Warning, WaitCommand, LOCTEXT("ReasonRawInput", "it reads the raw Input Action: no buffer, no motions or chords, and a binding of the kit can fire on the same press"));
		}
		else if (Name.StartsWith(TEXT("PlayMontageAndWait")))
		{
			Flag(EGSTN_Severity::Warning, PlayTimeline, LOCTEXT("ReasonMontage", "it plays only the montage: no hitboxes, hitstop, cancel windows or first actionable frame, and it fights the timeline for the montage slot"));
		}
		else if (Name.StartsWith(TEXT("WaitFor")) && Name.Contains(TEXT("Duration")))
		{
			Flag(EGSTN_Severity::Warning, WaitCombatFrames, LOCTEXT("ReasonSeconds", "it counts seconds (rounded to 16 ms) and keeps counting in hitstop, so it drifts from the move"));
		}
		return Hint;
	}

	if (Name == TEXT("AbilityTask_WaitDelay"))
	{
		Flag(EGSTN_Severity::Error, WaitCombatFrames, LOCTEXT("ReasonWorldTimer", "it waits on a world timer, outside the simulation, and a rollback never undoes it"));
	}
	else if (Name == TEXT("AbilityTask_WaitInputPress") || Name == TEXT("AbilityTask_WaitInputRelease"))
	{
		Flag(EGSTN_Severity::Error, WaitCommand, LOCTEXT("ReasonInputRpc", "it waits for a replicated input event and a prediction key, not the simulated input"));
	}
	else if (Name.StartsWith(TEXT("AbilityTask_PlayMontageAndWait")) || Name == TEXT("AbilityTask_PlayAnimAndWait"))
	{
		Flag(EGSTN_Severity::Error, PlayTimeline, LOCTEXT("ReasonGasMontage", "it plays a montage through the prediction key: no hitboxes, hitstop or cancel windows, and no rollback"));
	}
	else if (Name.StartsWith(TEXT("AbilityTask_ApplyRootMotion")))
	{
		Flag(EGSTN_Severity::Error, LOCTEXT("UseMovementTrack", "the Movement track of the timeline, or Apply Move Effect (a Mover layered move)"),
			LOCTEXT("ReasonRootMotion", "it is root motion of Character Movement, which Mover does not run"));
	}
	else if (Name.StartsWith(TEXT("AbilityTask_WaitTargetData")) || Name.StartsWith(TEXT("AbilityTask_WaitConfirm")))
	{
		Flag(EGSTN_Severity::Error, LOCTEXT("UseAim", "Get Aim Rotation, the hitboxes of the timeline, or Fire Projectile"),
			LOCTEXT("ReasonClientConfirm", "it waits for the client to confirm through an RPC, outside the simulation"));
	}
	else if (Name.StartsWith(TEXT("AbilityTask_")))
	{
		Flag(EGSTN_Severity::Warning, FText::GetEmpty(), LOCTEXT("ReasonAbilityTask", "a GAS ability task runs outside the simulation and is not rolled back; use a prediction task of the fork, or Wait Combat Frames and the move events"));
	}
	else
	{
		Flag(EGSTN_Severity::Warning, FText::GetEmpty(), LOCTEXT("ReasonAsync", "an async node resumes outside the simulation; only a rollback-safe task of the fork may run there"));
	}
	return Hint;
}

bool FGSTN_SimGraphRules::IsActorSpawn(const UFunction& Function)
{
	return Function.GetOwnerClass() == UGameplayStatics::StaticClass() && GSTNSimGraph::NameIn(Function.GetFName(), {
		TEXT("BeginDeferredActorSpawnFromClass"), TEXT("BeginSpawningActorFromClass"), TEXT("FinishSpawningActor") });
}

void FGSTN_SimGraphRules::CheckBlueprint(const UBlueprint& Blueprint, UObject* Asset, FGSTN_ValidationReport& Report)
{
	// Roots: the simulation events of the event graphs
	TArray<const UEdGraphNode*> Stack;
	for (const UEdGraph* Graph : Blueprint.UbergraphPages)
	{
		if (Graph == nullptr)
		{
			continue;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
			if (Event && !Event->IsA<UK2Node_CustomEvent>() && IsSimulationEvent(Event->EventReference.GetMemberName()))
			{
				Stack.Add(Event);
			}
		}
	}

	// The Blueprint's own callees: function graphs by name, custom events by name
	const auto FindOwnEntry = [&Blueprint](FName FunctionName) -> const UEdGraphNode*
	{
		for (const UEdGraph* Graph : Blueprint.FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == FunctionName)
			{
				for (const UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node && Node->IsA<UK2Node_FunctionEntry>())
					{
						return Node;
					}
				}
			}
		}
		for (const UEdGraph* Graph : Blueprint.UbergraphPages)
		{
			if (Graph == nullptr)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node);
				if (Custom && Custom->CustomFunctionName == FunctionName)
				{
					return Custom;
				}
			}
		}
		return nullptr;
	};

	TSet<FName> MemberVariables;
	for (const FBPVariableDescription& Variable : Blueprint.NewVariables)
	{
		MemberVariables.Add(Variable.VarName);
	}

	TSet<const UEdGraphNode*> Visited;
	while (Stack.Num() > 0)
	{
		const UEdGraphNode* Node = Stack.Pop(EAllowShrinking::No);
		if (Node == nullptr || Visited.Contains(Node))
		{
			continue;
		}
		Visited.Add(Node);

		// What the node runs next and what feeds it
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
			if ((bExec && Pin->Direction == EGPD_Output) || (!bExec && Pin->Direction == EGPD_Input))
			{
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode())
					{
						Stack.Add(Linked->GetOwningNode());
					}
				}
			}
		}

		const bool bForkTask = GSTNSimGraph::IsForkTaskNode(*Node);
		if (Node->IsA<UK2Node_BaseAsyncTask>() || bForkTask)
		{
			const UClass* TaskClass = GSTNSimGraph::ReadTaskClass(*Node, bForkTask ? TEXT("TaskInstance") : TEXT("ProxyClass"));
			const FTaskHint Hint = FindTaskHint(TaskClass ? TaskClass->GetFName() : NAME_None, bForkTask);
			if (Hint.bFlagged)
			{
				const FText Message = Hint.Replacement.IsEmpty()
					? FText::Format(LOCTEXT("SimTaskNoHint", "{0} in a simulation event: {1}."), GSTNSimGraph::NodeName(*Node), Hint.Reason)
					: FText::Format(LOCTEXT("SimTaskHint", "{0} in a simulation event: {1}. Use {2}."), GSTNSimGraph::NodeName(*Node), Hint.Reason, Hint.Replacement);
				Report.Add(Hint.Severity, bForkTask ? GSTNValidationCodes::SimTaskHint() : GSTNValidationCodes::SimLatent(), Message, Asset);
			}
		}
		else if (Node->IsA<UK2Node_SpawnActorFromClass>())
		{
			Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::SimSpawn(),
				FText::Format(LOCTEXT("SimSpawnNode", "{0} spawns an actor in a simulation event: the actor is not rolled back and every re-simulation spawns another. Use Fire Projectile (a Projectile Definition asset)."), GSTNSimGraph::NodeName(*Node)), Asset);
		}
		else if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
		{
			const FName Variable = Set->VariableReference.GetMemberName();
			const UEdGraphPin* ValuePin = Set->FindPin(Variable, EGPD_Input);
			const bool bMember = Set->VariableReference.IsSelfContext() && MemberVariables.Contains(Variable);
			if (bMember && ValuePin && !IsSyncVarType(ValuePin->PinType))
			{
				Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::SimNonSyncVariable(),
					FText::Format(LOCTEXT("SimNonSyncVar", "{0} writes the variable {1} in a simulation event, and it is not a sync variable: a rollback does not restore it. Use a Sync type (Sync Integer, Sync Bool...)."),
						GSTNSimGraph::NodeName(*Node), FText::FromName(Variable)), Asset);
			}
		}
		else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			const UFunction* Function = Call->GetTargetFunction();
			if (Function == nullptr)
			{
				continue;
			}
			if (IsLatentFunction(*Function))
			{
				Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::SimLatent(),
					FText::Format(LOCTEXT("SimLatentCall", "{0} is latent (a delay or a timer) in a simulation event: it resumes outside the simulation and a rollback never undoes it. Use Wait Combat Frames (a task node, stands still in hitstop)."), GSTNSimGraph::NodeName(*Node)), Asset);
			}
			else if (IsWorldRead(*Function))
			{
				Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::SimWorldRead(),
					FText::Format(LOCTEXT("SimWorldRead", "{0} reads the world or the Mover component in a simulation event: that is not the state of the simulated frame. Read the combat state and the input questions instead."), GSTNSimGraph::NodeName(*Node)), Asset);
			}
			else if (IsActorSpawn(*Function))
			{
				Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::SimSpawn(),
					FText::Format(LOCTEXT("SimSpawnCall", "{0} spawns an actor in a simulation event: the actor is not rolled back and every re-simulation spawns another. Use Fire Projectile (a Projectile Definition asset)."), GSTNSimGraph::NodeName(*Node)), Asset);
			}
			else if (IsUnseededRandom(*Function))
			{
				Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::SimRandom(),
					FText::Format(LOCTEXT("SimRandom", "{0} is random in a simulation event: every machine draws another number. Use Get Move Random Float or Get Move Random Integer."), GSTNSimGraph::NodeName(*Node)), Asset);
			}

			// A call into the Blueprint's own function or custom event runs in the simulation too
			const UClass* Owner = Function->GetOwnerClass();
			if (Owner != nullptr && (Owner == Blueprint.GeneratedClass || Owner == Blueprint.SkeletonGeneratedClass))
			{
				Stack.Add(FindOwnEntry(Function->GetFName()));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
