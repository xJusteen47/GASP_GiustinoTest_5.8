// Transition creation and rule wiring follow the approach of ChiR24/Unreal_mcp (EngineWire, MIT License, see
// Plugins/GSTNAnimToolsets/ThirdPartyNotices.md), extended to any graph depth and to composite rules.

#include "GSTN_AnimBlueprintToolset.h"

#include "GSTN_AnimRuleGraph.h"

#include "Animation/BlendProfile.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateAliasNode.h"
#include "AnimStateConduitNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "AnimationCustomTransitionGraph.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationTransitionGraph.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimationAsset.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "ScopedTransaction.h"
#include "ToolsetRegistry/RaiseScriptError.h"

#define LOCTEXT_NAMESPACE "GSTNAnimToolsets"

namespace GSTN::AnimToolsets
{
	void Raise(const FString& Message)
	{
		UE::ToolsetRegistry::RaiseScriptError(Message);
	}

	UAnimBlueprint* AnimBlueprintOf(const UEdGraph* Graph)
	{
		return Graph ? Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(Graph)) : nullptr;
	}

	UAnimBlueprint* AnimBlueprintOf(const UEdGraphNode* Node)
	{
		return Node ? AnimBlueprintOf(Node->GetGraph()) : nullptr;
	}

	/** A graph that holds poses: AnimGraph, anim layers, state graphs (not rule graphs, which are K2 graphs). */
	bool IsPoseGraph(const UEdGraph* Graph)
	{
		return Graph && Graph->GetSchema() && Graph->GetSchema()->IsA<UAnimationGraphSchema>();
	}

	/** Right of the rightmost node, so new nodes never land on top of old ones. */
	FIntPoint NextFreePosition(const UEdGraph* Graph)
	{
		bool bAny = false;
		FIntPoint Max(0, 0);
		int32 MinY = 0;
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			Max.X = bAny ? FMath::Max(Max.X, Node->NodePosX) : Node->NodePosX;
			MinY = bAny ? FMath::Min(MinY, Node->NodePosY) : Node->NodePosY;
			bAny = true;
		}
		return bAny ? FIntPoint(Max.X + 350, MinY) : FIntPoint(0, 0);
	}

	UAnimationStateMachineGraph* MachineGraphOf(const UAnimGraphNode_StateMachineBase* Machine)
	{
		return Machine ? Machine->EditorStateMachineGraph.Get() : nullptr;
	}

	UAnimGraphNode_StateMachineBase* MachineOfGraph(const UEdGraph* Graph)
	{
		const UAnimationStateMachineGraph* MachineGraph = Cast<UAnimationStateMachineGraph>(Graph);
		return MachineGraph ? MachineGraph->OwnerAnimGraphNode.Get() : nullptr;
	}

	FString StateNameOf(const UAnimStateNodeBase* Node)
	{
		return Node ? Node->GetStateName() : FString();
	}

	FString TransitionLabel(const UAnimStateTransitionNode* Transition)
	{
		return FString::Printf(TEXT("%s -> %s"), *StateNameOf(Transition->GetPreviousState()), *StateNameOf(Transition->GetNextState()));
	}

	/** The graph that contains Graph's owner, following nodes upwards. Null at the top. */
	UEdGraph* ParentGraph(const UEdGraph* Graph)
	{
		if (const UEdGraphNode* OwnerNode = Cast<UEdGraphNode>(Graph->GetOuter()))
		{
			return OwnerNode->GetGraph();
		}
		if (const UAnimGraphNode_StateMachineBase* Machine = MachineOfGraph(Graph))
		{
			return Machine->GetGraph();
		}
		return nullptr;
	}

	FString GraphSegment(const UEdGraph* Graph)
	{
		if (const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Graph->GetOuter()))
		{
			return Graph->IsA<UAnimationCustomTransitionGraph>() ? TransitionLabel(Transition) + TEXT(" (blend)") : TransitionLabel(Transition);
		}
		return Graph->GetName();
	}

	FString GraphPath(const UEdGraph* Graph)
	{
		TArray<FString> Segments;
		for (const UEdGraph* Current = Graph; Current && Segments.Num() < 64; Current = ParentGraph(Current))
		{
			Segments.Insert(GraphSegment(Current), 0);
		}
		return FString::Join(Segments, TEXT("/"));
	}

	EGSTN_AnimGraphKind GraphKind(const UAnimBlueprint* AnimBlueprint, const UEdGraph* Graph)
	{
		const UObject* Outer = Graph->GetOuter();
		if (Outer && Outer->IsA<UAnimStateTransitionNode>())
		{
			return Graph->IsA<UAnimationCustomTransitionGraph>() ? EGSTN_AnimGraphKind::CustomBlend : EGSTN_AnimGraphKind::TransitionRule;
		}
		if (Graph->IsA<UAnimationStateMachineGraph>())
		{
			return EGSTN_AnimGraphKind::StateMachine;
		}
		if (Outer && Outer->IsA<UAnimStateConduitNode>())
		{
			return EGSTN_AnimGraphKind::Conduit;
		}
		if (Graph->IsA<UAnimationStateGraph>())
		{
			return EGSTN_AnimGraphKind::State;
		}
		const bool bTopLevel = Outer && Outer->IsA<UBlueprint>();
		if (Graph->IsA<UAnimationGraph>())
		{
			if (!bTopLevel)
			{
				return EGSTN_AnimGraphKind::Other;
			}
			return Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph ? EGSTN_AnimGraphKind::AnimGraph : EGSTN_AnimGraphKind::AnimLayer;
		}
		if (AnimBlueprint->UbergraphPages.Contains(Graph))
		{
			return EGSTN_AnimGraphKind::EventGraph;
		}
		return bTopLevel ? EGSTN_AnimGraphKind::Function : EGSTN_AnimGraphKind::Other;
	}

	void GatherAssetPlayers(UEdGraph* Graph, TArray<UAnimGraphNode_AssetPlayerBase*>& OutPlayers)
	{
		if (!Graph)
		{
			return;
		}
		TArray<UEdGraph*> Graphs{Graph};
		Graph->GetAllChildrenGraphs(Graphs);
		for (const UEdGraph* Each : Graphs)
		{
			for (UEdGraphNode* Node : Each->Nodes)
			{
				if (UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(Node))
				{
					OutPlayers.Add(Player);
				}
			}
		}
	}

	FGSTN_StateInfo MakeStateInfo(UAnimStateNodeBase* Node)
	{
		FGSTN_StateInfo Info;
		Info.State = Node;
		Info.Name = Node->GetStateName();
		Info.Position = FIntPoint(Node->NodePosX, Node->NodePosY);
		if (Node->IsA<UAnimStateConduitNode>())
		{
			Info.Kind = EGSTN_StateKind::Conduit;
			Info.Graph = Node->GetBoundGraph();
		}
		else if (Node->IsA<UAnimStateAliasNode>())
		{
			Info.Kind = EGSTN_StateKind::Alias;
		}
		else
		{
			Info.Kind = EGSTN_StateKind::State;
			Info.Graph = Node->GetBoundGraph();
			TArray<UAnimGraphNode_AssetPlayerBase*> Players;
			GatherAssetPlayers(Info.Graph, Players);
			for (UAnimGraphNode_AssetPlayerBase* Player : Players)
			{
				if (UAnimationAsset* Asset = Player->GetAnimationAsset())
				{
					Info.Animations.AddUnique(Asset);
				}
			}
		}
		return Info;
	}

	/** States, conduits and aliases of a machine graph: every state-like node that is not a transition. */
	TArray<UAnimStateNodeBase*> StatesOf(const UEdGraph* MachineGraph)
	{
		TArray<UAnimStateNodeBase*> States;
		for (UEdGraphNode* Node : MachineGraph->Nodes)
		{
			UAnimStateNodeBase* State = Cast<UAnimStateNodeBase>(Node);
			if (State && !State->IsA<UAnimStateTransitionNode>())
			{
				States.Add(State);
			}
		}
		return States;
	}

	TArray<UAnimStateTransitionNode*> TransitionsOf(const UEdGraph* MachineGraph)
	{
		TArray<UAnimStateTransitionNode*> Transitions;
		for (UEdGraphNode* Node : MachineGraph->Nodes)
		{
			if (UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
			{
				Transitions.Add(Transition);
			}
		}
		return Transitions;
	}

	UAnimStateNodeBase* FindStateByName(const UEdGraph* MachineGraph, const FString& Name)
	{
		for (UAnimStateNodeBase* State : StatesOf(MachineGraph))
		{
			if (State->GetStateName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return State;
			}
		}
		return nullptr;
	}

	/** Spawns a pose-graph node the way the editor's context menu does, so PostPlacedNewNode builds its inner graphs. */
	UAnimGraphNode_Base* SpawnAnimNode(UEdGraph* Graph, UClass* NodeClass, const TOptional<FIntPoint>& Position)
	{
		const FIntPoint Where = Position.Get(NextFreePosition(Graph));
		UAnimGraphNode_Base* Template = NewObject<UAnimGraphNode_Base>(GetTransientPackage(), NodeClass);
		return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UAnimGraphNode_Base>(
			Graph, Template, FVector2f(static_cast<float>(Where.X), static_cast<float>(Where.Y)), false);
	}

	/** Whether a name is accepted for Node by the editor's own validator (ExistingName means unchanged). */
	bool IsNameAccepted(UEdGraphNode* Node, const FString& Name)
	{
		const TSharedPtr<INameValidatorInterface> Validator = Node->MakeNameValidator();
		if (!Validator.IsValid())
		{
			return !Name.IsEmpty();
		}
		const EValidatorResult Result = Validator->IsValid(Name);
		return Result == EValidatorResult::Ok || Result == EValidatorResult::ExistingName;
	}
}

using namespace GSTN::AnimToolsets;

TArray<FGSTN_AnimGraphInfo> UGSTN_AnimBlueprintToolset::ListAnimGraphs(UAnimBlueprint* AnimBlueprint)
{
	if (!AnimBlueprint)
	{
		Raise(TEXT("AnimBlueprint is null."));
		return {};
	}
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	TArray<FGSTN_AnimGraphInfo> Infos;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		FGSTN_AnimGraphInfo& Info = Infos.AddDefaulted_GetRef();
		Info.Graph = Graph;
		Info.Kind = GraphKind(AnimBlueprint, Graph);
		Info.Path = GraphPath(Graph);
		Info.OwnerNode = Cast<UEdGraphNode>(Graph->GetOuter());
		if (!Info.OwnerNode && Info.Kind == EGSTN_AnimGraphKind::StateMachine)
		{
			Info.OwnerNode = MachineOfGraph(Graph);
		}
	}
	return Infos;
}

TArray<FGSTN_StateMachineInfo> UGSTN_AnimBlueprintToolset::FindStateMachines(UAnimBlueprint* AnimBlueprint, const FString& Name)
{
	if (!AnimBlueprint)
	{
		Raise(TEXT("AnimBlueprint is null."));
		return {};
	}
	TArray<UAnimGraphNode_StateMachineBase*> Machines;
	FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, Machines);

	TArray<FGSTN_StateMachineInfo> Infos;
	for (UAnimGraphNode_StateMachineBase* Machine : Machines)
	{
		const FString MachineName = Machine->GetStateMachineName();
		if (!Name.IsEmpty() && !MachineName.Equals(Name, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FGSTN_StateMachineInfo& Info = Infos.AddDefaulted_GetRef();
		Info.Machine = Machine;
		Info.Name = MachineName;
		Info.GraphPath = GraphPath(Machine->GetGraph());
	}
	return Infos;
}

FGSTN_StateMachineDetail UGSTN_AnimBlueprintToolset::GetStateMachine(UAnimGraphNode_StateMachineBase* Machine)
{
	UAnimationStateMachineGraph* MachineGraph = MachineGraphOf(Machine);
	if (!MachineGraph)
	{
		Raise(TEXT("Machine is null or has no state machine graph."));
		return {};
	}
	FGSTN_StateMachineDetail Detail;
	Detail.Machine = Machine;
	Detail.Name = Machine->GetStateMachineName();
	Detail.Graph = MachineGraph;
	if (MachineGraph->EntryNode)
	{
		Detail.EntryState = StateNameOf(Cast<UAnimStateNodeBase>(MachineGraph->EntryNode->GetOutputNode()));
	}
	for (UAnimStateNodeBase* State : StatesOf(MachineGraph))
	{
		Detail.States.Add(MakeStateInfo(State));
	}
	for (UAnimStateTransitionNode* Transition : TransitionsOf(MachineGraph))
	{
		Detail.Transitions.Add(GetTransition(Transition));
	}
	return Detail;
}

UAnimGraphNode_StateMachineBase* UGSTN_AnimBlueprintToolset::AddStateMachine(UEdGraph* Graph, const FString& Name,
	TOptional<FIntPoint> Position)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(Graph);
	if (!AnimBlueprint || !IsPoseGraph(Graph))
	{
		Raise(TEXT("Graph must be a pose graph (AnimGraph, anim layer or state) of an Animation Blueprint."));
		return nullptr;
	}
	if (Name.IsEmpty())
	{
		Raise(TEXT("Name must not be empty."));
		return nullptr;
	}
	TArray<UAnimGraphNode_StateMachineBase*> Existing;
	FBlueprintEditorUtils::GetAllNodesOfClass(AnimBlueprint, Existing);
	for (UAnimGraphNode_StateMachineBase* Other : Existing)
	{
		if (Other->GetStateMachineName().Equals(Name, ESearchCase::IgnoreCase))
		{
			Raise(FString::Printf(TEXT("A state machine named '%s' already exists (%s)."), *Name, *GraphPath(Other->GetGraph())));
			return nullptr;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateMachine", "Add State Machine"));
	UAnimGraphNode_StateMachineBase* Machine =
		Cast<UAnimGraphNode_StateMachineBase>(SpawnAnimNode(Graph, UAnimGraphNode_StateMachine::StaticClass(), Position));
	if (!Machine || !Machine->EditorStateMachineGraph)
	{
		Raise(TEXT("The state machine node could not be created."));
		return nullptr;
	}
	Machine->OnRenameNode(Name);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return Machine;
}

UAnimStateNodeBase* UGSTN_AnimBlueprintToolset::AddState(UAnimGraphNode_StateMachineBase* Machine, const FString& Name,
	EGSTN_StateKind Kind, TOptional<FIntPoint> Position)
{
	UAnimationStateMachineGraph* MachineGraph = MachineGraphOf(Machine);
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(MachineGraph);
	if (!MachineGraph || !AnimBlueprint)
	{
		Raise(TEXT("Machine is null or is not in an Animation Blueprint."));
		return nullptr;
	}
	if (Name.IsEmpty())
	{
		Raise(TEXT("Name must not be empty."));
		return nullptr;
	}
	if (Kind == EGSTN_StateKind::Alias)
	{
		Raise(TEXT("Aliases cannot be added with AddState; use State or Conduit."));
		return nullptr;
	}
	if (FindStateByName(MachineGraph, Name))
	{
		Raise(FString::Printf(TEXT("State machine '%s' already has a state named '%s'."), *Machine->GetStateMachineName(), *Name));
		return nullptr;
	}

	const bool bFirstState = !MachineGraph->EntryNode || !MachineGraph->EntryNode->GetOutputNode();
	const FIntPoint Where = Position.Get(NextFreePosition(MachineGraph));
	const FVector2f Location(static_cast<float>(Where.X), static_cast<float>(Where.Y));

	const FScopedTransaction Transaction(LOCTEXT("AddState", "Add State"));
	UAnimStateNodeBase* State = nullptr;
	if (Kind == EGSTN_StateKind::Conduit)
	{
		State = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateConduitNode>(
			MachineGraph, NewObject<UAnimStateConduitNode>(), Location, false);
	}
	else
	{
		State = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(
			MachineGraph, NewObject<UAnimStateNode>(), Location, false);
	}
	if (!State)
	{
		Raise(TEXT("The state node could not be created."));
		return nullptr;
	}
	State->OnRenameNode(Name);

	if (bFirstState && Kind == EGSTN_StateKind::State && MachineGraph->EntryNode)
	{
		MachineGraph->EntryNode->Modify();
		MachineGraph->EntryNode->GetOutputPin()->MakeLinkTo(State->GetInputPin());
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return State;
}

UAnimStateNodeBase* UGSTN_AnimBlueprintToolset::AddStateAlias(UAnimGraphNode_StateMachineBase* Machine, const FString& Name,
	const TArray<FString>& AliasedStateNames, bool bGlobalAlias, TOptional<FIntPoint> Position)
{
	UAnimationStateMachineGraph* MachineGraph = MachineGraphOf(Machine);
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(MachineGraph);
	if (!MachineGraph || !AnimBlueprint)
	{
		Raise(TEXT("Machine is null or is not in an Animation Blueprint."));
		return nullptr;
	}
	if (Name.IsEmpty() || FindStateByName(MachineGraph, Name))
	{
		Raise(FString::Printf(TEXT("Name '%s' is empty or already used in the machine."), *Name));
		return nullptr;
	}
	// Resolve every aliased state first: nothing is created when a name is wrong
	TArray<UAnimStateNodeBase*> Aliased;
	for (const FString& StateName : AliasedStateNames)
	{
		UAnimStateNodeBase* State = FindStateByName(MachineGraph, StateName);
		if (!State || State->IsA<UAnimStateAliasNode>())
		{
			Raise(FString::Printf(TEXT("'%s' is not a state or conduit of the machine."), *StateName));
			return nullptr;
		}
		Aliased.Add(State);
	}
	if (Aliased.IsEmpty() && !bGlobalAlias)
	{
		Raise(TEXT("An alias needs at least one state, or bGlobalAlias."));
		return nullptr;
	}

	const FIntPoint Where = Position.Get(NextFreePosition(MachineGraph));
	const FScopedTransaction Transaction(LOCTEXT("AddStateAlias", "Add State Alias"));
	UAnimStateAliasNode* Alias = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateAliasNode>(
		MachineGraph, NewObject<UAnimStateAliasNode>(), FVector2f(static_cast<float>(Where.X), static_cast<float>(Where.Y)), false);
	if (!Alias)
	{
		Raise(TEXT("The alias node could not be created."));
		return nullptr;
	}
	Alias->OnRenameNode(Name);
	Alias->Modify();
	Alias->bGlobalAlias = bGlobalAlias;
	TSet<TWeakObjectPtr<UAnimStateNodeBase>>& Set = Alias->GetAliasedStates();
	Set.Reset();
	for (UAnimStateNodeBase* State : Aliased)
	{
		Set.Add(State);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return Alias;
}

UAnimStateNodeBase* UGSTN_AnimBlueprintToolset::FindState(UAnimGraphNode_StateMachineBase* Machine, const FString& Name)
{
	UAnimationStateMachineGraph* MachineGraph = MachineGraphOf(Machine);
	if (!MachineGraph)
	{
		Raise(TEXT("Machine is null or has no state machine graph."));
		return nullptr;
	}
	UAnimStateNodeBase* State = FindStateByName(MachineGraph, Name);
	if (!State)
	{
		TArray<FString> Names;
		for (const UAnimStateNodeBase* Each : StatesOf(MachineGraph))
		{
			Names.Add(Each->GetStateName());
		}
		Raise(FString::Printf(TEXT("No state '%s' in '%s'. States: %s."), *Name, *Machine->GetStateMachineName(), *FString::Join(Names, TEXT(", "))));
	}
	return State;
}

void UGSTN_AnimBlueprintToolset::RenameState(UEdGraphNode* Node, const FString& NewName)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(Node);
	const bool bRenamable = Node && (Node->IsA<UAnimGraphNode_StateMachineBase>()
		|| (Node->IsA<UAnimStateNodeBase>() && !Node->IsA<UAnimStateTransitionNode>()));
	if (!AnimBlueprint || !bRenamable)
	{
		Raise(TEXT("Node must be a state, conduit, alias or state machine of an Animation Blueprint."));
		return;
	}
	if (!IsNameAccepted(Node, NewName))
	{
		Raise(FString::Printf(TEXT("'%s' is empty, invalid or already used."), *NewName));
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("RenameState", "Rename State"));
	Node->Modify();
	Node->OnRenameNode(NewName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
}

void UGSTN_AnimBlueprintToolset::RemoveState(UAnimStateNodeBase* State)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(State);
	if (!AnimBlueprint || State->IsA<UAnimStateTransitionNode>() || !State->GetGraph()->IsA<UAnimationStateMachineGraph>())
	{
		Raise(TEXT("State must be a state, conduit or alias in a state machine."));
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("RemoveState", "Remove State"));
	UEdGraph* MachineGraph = State->GetGraph();
	MachineGraph->Modify();
	for (UAnimStateTransitionNode* Transition : TransitionsOf(MachineGraph))
	{
		if (Transition->GetPreviousState() == State || Transition->GetNextState() == State)
		{
			FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Transition, true);
		}
	}
	FBlueprintEditorUtils::RemoveNode(AnimBlueprint, State, true);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
}

void UGSTN_AnimBlueprintToolset::SetEntryState(UAnimStateNode* State)
{
	UAnimationStateMachineGraph* MachineGraph = State ? Cast<UAnimationStateMachineGraph>(State->GetGraph()) : nullptr;
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(MachineGraph);
	if (!MachineGraph || !MachineGraph->EntryNode || !AnimBlueprint)
	{
		Raise(TEXT("State must be a state in a state machine."));
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("SetEntryState", "Set Entry State"));
	UEdGraphPin* EntryPin = MachineGraph->EntryNode->GetOutputPin();
	MachineGraph->EntryNode->Modify();
	State->Modify();
	EntryPin->BreakAllPinLinks();
	EntryPin->MakeLinkTo(State->GetInputPin());
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
}

UAnimGraphNode_AssetPlayerBase* UGSTN_AnimBlueprintToolset::SetStateAnimation(UAnimStateNode* State, UAnimationAsset* Animation)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(State);
	UEdGraph* StateGraph = State ? State->GetBoundGraph() : nullptr;
	UEdGraphPin* Sink = State ? State->GetPoseSinkPinInsideState() : nullptr;
	if (!AnimBlueprint || !StateGraph || !Sink)
	{
		Raise(TEXT("State must be a state with a pose graph."));
		return nullptr;
	}
	if (!Animation)
	{
		Raise(TEXT("Animation is null."));
		return nullptr;
	}
	UClass* PlayerClass = GetNodeClassForAsset(Animation->GetClass());
	if (!PlayerClass || !PlayerClass->IsChildOf(UAnimGraphNode_AssetPlayerBase::StaticClass()))
	{
		Raise(FString::Printf(TEXT("No asset player node plays a %s."), *Animation->GetClass()->GetName()));
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetStateAnimation", "Set State Animation"));
	StateGraph->Modify();
	const UEdGraphNode* ResultNode = Sink->GetOwningNode();
	FIntPoint Where(ResultNode->NodePosX - 350, ResultNode->NodePosY);
	TArray<UEdGraphPin*> Feeding = Sink->LinkedTo;
	for (UEdGraphPin* Linked : Feeding)
	{
		UEdGraphNode* Feeder = Linked->GetOwningNode();
		if (Feeder->IsA<UAnimGraphNode_AssetPlayerBase>())
		{
			Where = FIntPoint(Feeder->NodePosX, Feeder->NodePosY);
			FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Feeder, true);
		}
	}
	Sink->BreakAllPinLinks();

	UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(SpawnAnimNode(StateGraph, PlayerClass, Where));
	if (!Player)
	{
		Raise(TEXT("The asset player node could not be created."));
		return nullptr;
	}
	Player->SetAnimationAsset(Animation);
	Player->CopySettingsFromAnimationAsset(Animation);

	UEdGraphPin* PoseOut = nullptr;
	for (UEdGraphPin* Pin : Player->Pins)
	{
		if (Pin->Direction == EGPD_Output && UAnimationGraphSchema::IsPosePin(Pin->PinType))
		{
			PoseOut = Pin;
			break;
		}
	}
	if (!PoseOut || !StateGraph->GetSchema()->TryCreateConnection(PoseOut, Sink))
	{
		Raise(TEXT("The asset player could not be connected to the state result."));
		return nullptr;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return Player;
}

UAnimStateTransitionNode* UGSTN_AnimBlueprintToolset::AddTransition(UAnimStateNodeBase* From, UAnimStateNodeBase* To)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(From);
	UEdGraph* MachineGraph = From ? From->GetGraph() : nullptr;
	if (!AnimBlueprint || !To || !MachineGraph || !MachineGraph->IsA<UAnimationStateMachineGraph>())
	{
		Raise(TEXT("From and To must be states of a state machine."));
		return nullptr;
	}
	if (From->IsA<UAnimStateTransitionNode>() || To->IsA<UAnimStateTransitionNode>())
	{
		Raise(TEXT("From and To must be states, conduits or aliases, not transitions."));
		return nullptr;
	}
	if (To->GetGraph() != MachineGraph)
	{
		Raise(FString::Printf(TEXT("'%s' and '%s' are in different state machines."), *From->GetStateName(), *To->GetStateName()));
		return nullptr;
	}
	if (To->IsA<UAnimStateAliasNode>())
	{
		Raise(TEXT("A transition cannot target an alias."));
		return nullptr;
	}
	if (From == To)
	{
		Raise(TEXT("A transition needs two different states."));
		return nullptr;
	}
	for (const UAnimStateTransitionNode* Existing : TransitionsOf(MachineGraph))
	{
		if (Existing->GetPreviousState() == From && Existing->GetNextState() == To)
		{
			Raise(FString::Printf(TEXT("A transition %s already exists."), *TransitionLabel(Existing)));
			return nullptr;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTransition", "Add Transition"));
	const FVector2f Location((From->NodePosX + To->NodePosX) * 0.5f, (From->NodePosY + To->NodePosY) * 0.5f);
	UAnimStateTransitionNode* Transition = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateTransitionNode>(
		MachineGraph, NewObject<UAnimStateTransitionNode>(), Location, false);
	if (!Transition)
	{
		Raise(TEXT("The transition node could not be created."));
		return nullptr;
	}
	Transition->CreateConnections(From, To);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return Transition;
}

FGSTN_TransitionInfo UGSTN_AnimBlueprintToolset::GetTransition(UAnimStateTransitionNode* Transition)
{
	if (!Transition)
	{
		Raise(TEXT("Transition is null."));
		return {};
	}
	FGSTN_TransitionInfo Info;
	Info.Transition = Transition;
	Info.From = StateNameOf(Transition->GetPreviousState());
	Info.To = StateNameOf(Transition->GetNextState());
	Info.RuleGraph = Transition->BoundGraph;
	Info.CrossfadeDuration = Transition->CrossfadeDuration;
	Info.PriorityOrder = Transition->PriorityOrder;
	Info.bSharedRules = Transition->bSharedRules;
	Info.Rule = Transition->bAutomaticRuleBasedOnSequencePlayerInState
		? FString(TEXT("automatic"))
		: GSTN::AnimRuleGraph::DescribeRule(Transition->BoundGraph);
	return Info;
}

void UGSTN_AnimBlueprintToolset::RemoveTransition(UAnimStateTransitionNode* Transition)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(Transition);
	if (!AnimBlueprint)
	{
		Raise(TEXT("Transition is null or is not in an Animation Blueprint."));
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("RemoveTransition", "Remove Transition"));
	Transition->GetGraph()->Modify();
	FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Transition, true);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
}

FString UGSTN_AnimBlueprintToolset::SetTransitionRule(UEdGraphNode* TransitionOrConduit, const FGSTN_TransitionRule& Rule)
{
	FString Error;
	FString Expression;
	if (!GSTN::AnimRuleGraph::WriteRule(TransitionOrConduit, Rule, Expression, Error))
	{
		Raise(Error);
		return {};
	}
	return Expression;
}

UAnimGraphNode_Base* UGSTN_AnimBlueprintToolset::AddAnimNode(UEdGraph* Graph, UClass* NodeClass, TOptional<FIntPoint> Position)
{
	UAnimBlueprint* AnimBlueprint = AnimBlueprintOf(Graph);
	if (!AnimBlueprint || !IsPoseGraph(Graph))
	{
		Raise(TEXT("Graph must be a pose graph (AnimGraph, anim layer or state) of an Animation Blueprint."));
		return nullptr;
	}
	if (!NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass())
		|| NodeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		Raise(TEXT("NodeClass must be a concrete subclass of AnimGraphNode_Base."));
		return nullptr;
	}
	if (NodeClass->IsChildOf(UAnimGraphNode_Root::StaticClass()) || NodeClass->IsChildOf(UAnimGraphNode_StateResult::StaticClass())
		|| NodeClass->IsChildOf(UAnimGraphNode_TransitionResult::StaticClass()))
	{
		Raise(TEXT("Result nodes are created with their graph and cannot be added."));
		return nullptr;
	}
	if (!NodeClass->GetDefaultObject<UEdGraphNode>()->CanCreateUnderSpecifiedSchema(Graph->GetSchema()))
	{
		Raise(FString::Printf(TEXT("%s cannot be placed in %s."), *NodeClass->GetName(), *GraphPath(Graph)));
		return nullptr;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddAnimNode", "Add Anim Node"));
	UAnimGraphNode_Base* Node = SpawnAnimNode(Graph, NodeClass, Position);
	if (!Node)
	{
		Raise(TEXT("The node could not be created."));
		return nullptr;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	return Node;
}

#undef LOCTEXT_NAMESPACE

FString UGSTN_AnimBlueprintToolset::SetBlendProfileScales(UBlendProfile* Profile, USkeleton* Skeleton, const TMap<FName, float>& Scales, int32 Mode)
{
	if (!Profile)
	{
		return TEXT("no profile");
	}
	Profile->Modify();
	if (Skeleton)
	{
		Profile->OwningSkeleton = Skeleton;
	}
	Profile->Mode = static_cast<EBlendProfileMode>(FMath::Clamp(Mode, 0, 2));
	int32 Set = 0;
	for (const TPair<FName, float>& Pair : Scales)
	{
		Profile->SetBoneBlendScale(Pair.Key, Pair.Value, false, true);
		++Set;
	}
	Profile->MarkPackageDirty();
	return FString::Printf(TEXT("%d bones set, %d entries, skeleton %s"), Set, Profile->GetNumBlendEntries(),
		*GetNameSafe(Profile->GetSkeleton().Get()));
}
