// Editor-only anim graph authoring for scripts (no runtime dependency, no feature knowledge).

#include "GSTN_AnimGraphAuthoringLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimLayerInterface.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_Inertialization.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateMachineGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimGraphAuthoringLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogGSTNAnimGraphAuthoring, Log, All);

namespace GSTNAnimGraphAuthoring
{
	/** First pose pin (local or component space) of Node in Direction */
	UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType))
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	/** The node a graph's pose ends in: Output Pose of an anim graph or layer graph (a Root node), or the result of a state */
	UEdGraphNode* FindResultNode(UEdGraph* Graph)
	{
		if (Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Cast<UAnimGraphNode_Root>(Node) || Cast<UAnimGraphNode_StateResult>(Node))
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	UBlueprint* GetBlueprint(const UEdGraph* Graph)
	{
		return Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
	}

	/** NewObject + the steps the editor's node spawner takes, in its order */
	template <typename TNode>
	TNode* SpawnNode(UEdGraph* Graph, const FVector2D& Position, UClass* Class = TNode::StaticClass())
	{
		Graph->Modify();
		TNode* Node = NewObject<TNode>(Graph, Class, NAME_None, RF_Transactional);
		Graph->AddNode(Node, /*bFromUI*/ false, /*bSelectNewNode*/ false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = FMath::RoundToInt(Position.X);
		Node->NodePosY = FMath::RoundToInt(Position.Y);
		return Node;
	}

	/** Sets a member of an anim node's runtime struct (the "Node" property of the editor node) from its text form, as the details panel does */
	bool SetNodeStructMember(UEdGraphNode* Node, const TCHAR* MemberName, const TCHAR* ValueText)
	{
		const FStructProperty* NodeProperty = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("Node"));
		const FProperty* Member = NodeProperty ? FindFProperty<FProperty>(NodeProperty->Struct, MemberName) : nullptr;
		if (!Member)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("%s has no Node.%s"), *Node->GetClass()->GetName(), MemberName);
			return false;
		}
		Node->Modify();
		void* NodeStruct = NodeProperty->ContainerPtrToValuePtr<void>(Node);
		return Member->ImportText_InContainer(ValueText, NodeStruct, Node, PPF_None) != nullptr;
	}

	bool Connect(UEdGraphPin* A, UEdGraphPin* B)
	{
		if (!A || !B)
		{
			return false;
		}
		if (A->LinkedTo.Contains(B))
		{
			return true;
		}
		const bool bConnected = A->GetSchema()->TryCreateConnection(A, B);
		if (!bConnected)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("Could not connect %s.%s to %s.%s"), *A->GetOwningNode()->GetName(), *A->PinName.ToString(),
				*B->GetOwningNode()->GetName(), *B->PinName.ToString());
		}
		return bConnected;
	}

	UEdGraphPin* LinkedPin(UEdGraphPin* Pin)
	{
		return Pin && Pin->LinkedTo.Num() > 0 ? Pin->LinkedTo[0] : nullptr;
	}

	FString DescribePinType(const UEdGraphPin* Pin)
	{
		FString Type = Pin->PinType.PinCategory.ToString();
		if (const UObject* Sub = Pin->PinType.PinSubCategoryObject.Get())
		{
			Type += TEXT(":") + Sub->GetName();
		}
		return Type;
	}
}

using namespace GSTNAnimGraphAuthoring;

bool UGSTN_AnimGraphAuthoringLibrary::AddAnimLayer(UAnimBlueprint* LayerInterface, FName LayerName)
{
	if (!LayerInterface || LayerInterface->BlueprintType != BPTYPE_Interface || LayerName.IsNone())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("AddAnimLayer: needs an anim layer interface and a name"));
		return false;
	}
	if (FindGraph(LayerInterface, LayerName))
	{
		return true;
	}
	LayerInterface->Modify();
	// What Add > Animation Layer does in the editor (FBlueprintEditor::NewDocument_OnClick, CGT_NewAnimationLayer)
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(LayerInterface, LayerName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
	FBlueprintEditorUtils::AddDomainSpecificGraph(LayerInterface, NewGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LayerInterface);
	return NewGraph != nullptr;
}

bool UGSTN_AnimGraphAuthoringLibrary::ImplementLayerInterface(UAnimBlueprint* AnimBlueprint, UAnimBlueprint* LayerInterface)
{
	if (!AnimBlueprint || !LayerInterface || !LayerInterface->GeneratedClass)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("ImplementLayerInterface: needs an anim Blueprint and a compiled layer interface"));
		return false;
	}
	UClass* InterfaceClass = LayerInterface->GeneratedClass;
	for (const FBPInterfaceDescription& Description : AnimBlueprint->ImplementedInterfaces)
	{
		if (Description.Interface == InterfaceClass)
		{
			return true;
		}
	}
	AnimBlueprint->Modify();
	return FBlueprintEditorUtils::ImplementNewInterface(AnimBlueprint, InterfaceClass->GetClassPathName());
}

UEdGraph* UGSTN_AnimGraphAuthoringLibrary::FindGraph(UBlueprint* Blueprint, FName GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == GraphName)
		{
			return Graph;
		}
	}
	for (const FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
	{
		for (UEdGraph* Graph : Description.Graphs)
		{
			if (Graph && Graph->GetFName() == GraphName)
			{
				return Graph;
			}
		}
	}
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == GraphName)
		{
			return Graph;
		}
	}
	return nullptr;
}

UEdGraphNode* UGSTN_AnimGraphAuthoringLibrary::FindOrAddStateMachine(UEdGraph* Graph, FName Name, FVector2D Position)
{
	if (!Graph || Name.IsNone())
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		const UAnimGraphNode_StateMachineBase* StateMachine = Cast<UAnimGraphNode_StateMachineBase>(Node);
		if (StateMachine && StateMachine->EditorStateMachineGraph && StateMachine->EditorStateMachineGraph->GetFName() == Name)
		{
			return Node;
		}
	}
	UAnimGraphNode_StateMachine* StateMachine = SpawnNode<UAnimGraphNode_StateMachine>(Graph, Position);
	// The machine is named after its graph; renaming the node renames the graph through the name validator, as the editor does
	StateMachine->OnRenameNode(Name.ToString());
	if (!StateMachine->EditorStateMachineGraph || StateMachine->EditorStateMachineGraph->GetFName() != Name)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Warning, TEXT("State machine %s got the name %s"), *Name.ToString(),
			StateMachine->EditorStateMachineGraph ? *StateMachine->EditorStateMachineGraph->GetName() : TEXT("(no graph)"));
	}
	return StateMachine;
}

UEdGraphNode* UGSTN_AnimGraphAuthoringLibrary::FindOrAddState(UEdGraphNode* StateMachineNode, FName StateName, FVector2D Position)
{
	const UAnimGraphNode_StateMachineBase* StateMachine = Cast<UAnimGraphNode_StateMachineBase>(StateMachineNode);
	UEdGraph* MachineGraph = StateMachine ? StateMachine->EditorStateMachineGraph.Get() : nullptr;
	if (!MachineGraph || StateName.IsNone())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("FindOrAddState: not a state machine node"));
		return nullptr;
	}
	for (UEdGraphNode* Node : MachineGraph->Nodes)
	{
		const UAnimStateNode* State = Cast<UAnimStateNode>(Node);
		if (State && State->GetStateName() == StateName.ToString())
		{
			return Node;
		}
	}
	UAnimStateNode* State = SpawnNode<UAnimStateNode>(MachineGraph, Position);
	State->OnRenameNode(StateName.ToString());
	if (State->GetStateName() != StateName.ToString())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Warning, TEXT("State %s got the name %s (name taken elsewhere in the Blueprint?)"), *StateName.ToString(), *State->GetStateName());
	}
	return State;
}

bool UGSTN_AnimGraphAuthoringLibrary::SetEntryState(UEdGraphNode* StateMachineNode, UEdGraphNode* StateNode)
{
	const UAnimGraphNode_StateMachineBase* StateMachine = Cast<UAnimGraphNode_StateMachineBase>(StateMachineNode);
	const UAnimationStateMachineGraph* MachineGraph = StateMachine ? StateMachine->EditorStateMachineGraph.Get() : nullptr;
	UAnimStateNode* State = Cast<UAnimStateNode>(StateNode);
	UAnimStateEntryNode* Entry = MachineGraph ? MachineGraph->EntryNode.Get() : nullptr;
	if (!Entry || !State || State->GetGraph() != MachineGraph)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetEntryState: needs a state machine and one of its states"));
		return false;
	}
	UEdGraphPin* EntryOut = nullptr;
	for (UEdGraphPin* Pin : Entry->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output)
		{
			EntryOut = Pin;
			break;
		}
	}
	UEdGraphPin* StateIn = State->GetInputPin();
	if (!EntryOut || !StateIn)
	{
		return false;
	}
	if (EntryOut->LinkedTo.Num() == 1 && EntryOut->LinkedTo[0] == StateIn)
	{
		return true;
	}
	Entry->Modify();
	EntryOut->BreakAllPinLinks();
	EntryOut->MakeLinkTo(StateIn);
	return true;
}

UEdGraphNode* UGSTN_AnimGraphAuthoringLibrary::FindOrAddTransition(UEdGraphNode* FromNode, UEdGraphNode* ToNode, float CrossfadeDuration)
{
	UAnimStateNodeBase* From = Cast<UAnimStateNodeBase>(FromNode);
	UAnimStateNodeBase* To = Cast<UAnimStateNodeBase>(ToNode);
	if (!From || !To || From == To || From->GetGraph() != To->GetGraph())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("FindOrAddTransition: needs two different states of the same machine"));
		return nullptr;
	}
	UEdGraphPin* FromOut = From->GetOutputPin();
	UEdGraphPin* ToIn = To->GetInputPin();
	UAnimStateTransitionNode* Transition = nullptr;
	for (UEdGraphPin* Linked : FromOut->LinkedTo)
	{
		UAnimStateTransitionNode* Candidate = Linked ? Cast<UAnimStateTransitionNode>(Linked->GetOwningNode()) : nullptr;
		if (Candidate && Candidate->GetOutputPin() && Candidate->GetOutputPin()->LinkedTo.Contains(ToIn))
		{
			Transition = Candidate;
			break;
		}
	}
	if (!Transition)
	{
		const FVector2D Position((From->NodePosX + To->NodePosX) * 0.5, (From->NodePosY + To->NodePosY) * 0.5);
		Transition = SpawnNode<UAnimStateTransitionNode>(From->GetGraph(), Position);
		// Pins of states are linked directly: the state machine schema would turn a state-to-state connection into another transition
		FromOut->MakeLinkTo(Transition->GetInputPin());
		Transition->GetOutputPin()->MakeLinkTo(ToIn);
	}
	Transition->Modify();
	Transition->CrossfadeDuration = CrossfadeDuration;
	return Transition;
}

bool UGSTN_AnimGraphAuthoringLibrary::SetTransitionRuleEnumEquals(UEdGraphNode* TransitionNode, const TArray<FString>& PropertyPath, const FString& EnumValue)
{
	UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(TransitionNode);
	UEdGraph* Rule = Transition ? Transition->BoundGraph.Get() : nullptr;
	UBlueprint* Blueprint = GetBlueprint(Rule);
	if (!Rule || !Blueprint || PropertyPath.IsEmpty())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetTransitionRuleEnumEquals: needs a transition and a property path"));
		return false;
	}
	UAnimGraphNode_TransitionResult* Result = nullptr;
	for (UEdGraphNode* Node : Rule->Nodes)
	{
		if (UAnimGraphNode_TransitionResult* Candidate = Cast<UAnimGraphNode_TransitionResult>(Node))
		{
			Result = Candidate;
		}
	}
	UEdGraphPin* CanEnter = Result ? Result->FindPin(TEXT("bCanEnterTransition"), EGPD_Input) : nullptr;
	UClass* PropertyAccessClass = FindObject<UClass>(nullptr, TEXT("/Script/PropertyAccessNode.K2Node_PropertyAccess"));
	FArrayProperty* PathProperty = PropertyAccessClass ? FindFProperty<FArrayProperty>(PropertyAccessClass, TEXT("Path")) : nullptr;
	if (!CanEnter || !PathProperty || !CastField<FStrProperty>(PathProperty->Inner))
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetTransitionRuleEnumEquals: no transition result pin or no property access node class (PropertyAccessNode plugin)"));
		return false;
	}

	// Already this rule: result <- enum == (constant EnumValue) <- property access with this path
	if (const UK2Node_EnumEquality* ExistingEquals = LinkedPin(CanEnter) ? Cast<UK2Node_EnumEquality>(LinkedPin(CanEnter)->GetOwningNode()) : nullptr)
	{
		const UEdGraphPin* ExistingConstant = ExistingEquals->GetInput2Pin();
		const UEdGraphPin* ExistingSource = LinkedPin(ExistingEquals->GetInput1Pin());
		const UEdGraphNode* ExistingAccess = ExistingSource ? ExistingSource->GetOwningNode() : nullptr;
		if (ExistingConstant && ExistingConstant->LinkedTo.Num() == 0 && ExistingConstant->DefaultValue == EnumValue && ExistingAccess
			&& ExistingAccess->GetClass() == PropertyAccessClass && *PathProperty->ContainerPtrToValuePtr<TArray<FString>>(ExistingAccess) == PropertyPath
			&& Rule->Nodes.Num() == 3)
		{
			return true;
		}
	}

	// Otherwise the rule is rebuilt from scratch, so a second run does not stack nodes
	TArray<UEdGraphNode*> Authored;
	for (UEdGraphNode* Node : Rule->Nodes)
	{
		if (Node && Node != Result)
		{
			Authored.Add(Node);
		}
	}
	for (UEdGraphNode* Node : Authored)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/ true);
	}
	CanEnter->BreakAllPinLinks();

	const FVector2D ResultPosition(Result->NodePosX, Result->NodePosY);
	UK2Node* PropertyAccess = NewObject<UK2Node>(Rule, PropertyAccessClass, NAME_None, RF_Transactional);
	Rule->AddNode(PropertyAccess, false, false);
	PropertyAccess->CreateNewGuid();
	*PathProperty->ContainerPtrToValuePtr<TArray<FString>>(PropertyAccess) = PropertyPath;
	PropertyAccess->PostPlacedNewNode();
	PropertyAccess->AllocateDefaultPins();
	PropertyAccess->NodePosX = FMath::RoundToInt(ResultPosition.X) - 520;
	PropertyAccess->NodePosY = FMath::RoundToInt(ResultPosition.Y);

	UEdGraphPin* Value = PropertyAccess->FindPin(TEXT("Value"), EGPD_Output);
	const UEnum* Enum = Value ? Cast<UEnum>(Value->PinType.PinSubCategoryObject.Get()) : nullptr;
	if (!Enum)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetTransitionRuleEnumEquals: %s does not resolve to an enum on %s"),
			*FString::Join(PropertyPath, TEXT(".")), *Blueprint->GetName());
		return false;
	}
	if (Enum->GetIndexByNameString(EnumValue) == INDEX_NONE)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetTransitionRuleEnumEquals: %s has no value %s"), *Enum->GetName(), *EnumValue);
		return false;
	}

	UK2Node_EnumEquality* Equals = SpawnNode<UK2Node_EnumEquality>(Rule, ResultPosition + FVector2D(-240, 0));
	const UEdGraphSchema* Schema = Rule->GetSchema();
	if (!Connect(Value, Equals->GetInput1Pin()))
	{
		return false;
	}
	UEdGraphPin* Constant = Equals->GetInput2Pin();
	Schema->TrySetDefaultValue(*Constant, EnumValue);
	return Connect(Equals->GetReturnValuePin(), CanEnter);
}

UEdGraph* UGSTN_AnimGraphAuthoringLibrary::GetStateGraph(UEdGraphNode* StateNode)
{
	const UAnimStateNode* State = Cast<UAnimStateNode>(StateNode);
	return State ? State->BoundGraph.Get() : nullptr;
}

UEdGraphNode* UGSTN_AnimGraphAuthoringLibrary::FindOrAddSequencePlayer(UEdGraph* Graph, UAnimSequenceBase* Sequence, bool bLoop, FVector2D Position)
{
	if (!Graph || !Sequence)
	{
		return nullptr;
	}
	UAnimGraphNode_SequencePlayer* Player = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UAnimGraphNode_SequencePlayer* Candidate = Cast<UAnimGraphNode_SequencePlayer>(Node))
		{
			Player = Candidate;
			break;
		}
	}
	if (!Player)
	{
		Player = SpawnNode<UAnimGraphNode_SequencePlayer>(Graph, Position);
	}
	Player->Modify();
	Player->SetAnimationAsset(Sequence);
	SetNodeStructMember(Player, TEXT("bLoopAnimation"), bLoop ? TEXT("True") : TEXT("False"));
	return Player;
}

bool UGSTN_AnimGraphAuthoringLibrary::ConnectToGraphOutput(UEdGraphNode* PoseNode)
{
	UEdGraph* Graph = PoseNode ? PoseNode->GetGraph() : nullptr;
	UEdGraphPin* ResultIn = FindPosePin(FindResultNode(Graph), EGPD_Input);
	UEdGraphPin* PoseOut = FindPosePin(PoseNode, EGPD_Output);
	if (!ResultIn || !PoseOut)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("ConnectToGraphOutput: no pose output on the node or no result node in its graph"));
		return false;
	}
	if (ResultIn->LinkedTo.Num() == 1 && ResultIn->LinkedTo[0] == PoseOut)
	{
		return true;
	}
	ResultIn->GetOwningNode()->Modify();
	ResultIn->BreakAllPinLinks();
	return Connect(PoseOut, ResultIn);
}

bool UGSTN_AnimGraphAuthoringLibrary::InsertOverrideLayerBlend(UAnimBlueprint* AnimBlueprint, UAnimBlueprint* LayerInterface, FName LayerName,
	FName ActiveVariable, FName BlendTimeVariable, bool bInertialization, const FString& InsertBeforeNodeClass)
{
	UEdGraph* AnimGraph = FindGraph(AnimBlueprint, UEdGraphSchema_K2::GN_AnimGraph);
	UClass* InterfaceClass = LayerInterface ? LayerInterface->GeneratedClass.Get() : nullptr;
	if (!AnimGraph || !InterfaceClass || !InterfaceClass->IsChildOf(UAnimLayerInterface::StaticClass()))
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerBlend: needs an anim Blueprint with an AnimGraph and a compiled layer interface"));
		return false;
	}
	// The node the seam goes in front of: the Output Pose, or the only node of the given class
	UEdGraphNode* Root = nullptr;
	int32 Matches = 0;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		const bool bMatch = InsertBeforeNodeClass.IsEmpty() ? Cast<UAnimGraphNode_Root>(Node) != nullptr
			: (Node && Node->GetClass()->GetName() == InsertBeforeNodeClass);
		if (bMatch)
		{
			Root = Node;
			++Matches;
		}
	}
	UEdGraphPin* RootIn = Matches == 1 ? FindPosePin(Root, EGPD_Input) : nullptr;
	if (!RootIn)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerBlend: %d node(s) of class %s with a pose input in the AnimGraph, expected one"),
			Matches, InsertBeforeNodeClass.IsEmpty() ? TEXT("AnimGraphNode_Root") : *InsertBeforeNodeClass);
		return false;
	}

	// The interface graph the layer node refers to (its guid lets the node follow a rename)
	const UEdGraph* InterfaceGraph = nullptr;
	for (const FBPInterfaceDescription& Description : AnimBlueprint->ImplementedInterfaces)
	{
		if (Description.Interface == InterfaceClass)
		{
			for (const UEdGraph* Graph : Description.Graphs)
			{
				if (Graph && Graph->GetFName() == LayerName)
				{
					InterfaceGraph = Graph;
				}
			}
		}
	}
	if (!InterfaceGraph)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerBlend: %s does not implement %s with a layer %s (ImplementLayerInterface first)"),
			*AnimBlueprint->GetName(), *InterfaceClass->GetName(), *LayerName.ToString());
		return false;
	}

	// Already there: target <- [Inertialization <-] Blend Poses by bool whose True pose is this layer
	UEdGraphNode* Upstream = LinkedPin(RootIn) ? LinkedPin(RootIn)->GetOwningNode() : nullptr;
	if (Cast<UAnimGraphNode_Inertialization>(Upstream))
	{
		UEdGraphPin* InertialIn = FindPosePin(Upstream, EGPD_Input);
		Upstream = LinkedPin(InertialIn) ? LinkedPin(InertialIn)->GetOwningNode() : nullptr;
	}
	if (UAnimGraphNode_BlendListByBool* ExistingBlend = Cast<UAnimGraphNode_BlendListByBool>(Upstream))
	{
		UEdGraphPin* TruePin = ExistingBlend->FindPin(TEXT("BlendPose_0"), EGPD_Input);
		const UAnimGraphNode_LinkedAnimLayer* ExistingLayer = LinkedPin(TruePin) ? Cast<UAnimGraphNode_LinkedAnimLayer>(LinkedPin(TruePin)->GetOwningNode()) : nullptr;
		if (ExistingLayer && ExistingLayer->Node.Layer == LayerName)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Log, TEXT("InsertOverrideLayerBlend: %s already blends the layer %s"), *AnimBlueprint->GetName(), *LayerName.ToString());
			return true;
		}
	}

	UEdGraphPin* Previous = LinkedPin(RootIn);
	const FVector2D RootPosition(Root->NodePosX, Root->NodePosY);
	AnimGraph->Modify();
	Root->Modify();
	RootIn->BreakAllPinLinks();

	// Linked Anim Layer node: the fields SetupFromLayerId sets when the node is spawned from the layer's menu entry
	UAnimGraphNode_LinkedAnimLayer* Layer = NewObject<UAnimGraphNode_LinkedAnimLayer>(AnimGraph, NAME_None, RF_Transactional);
	AnimGraph->AddNode(Layer, false, false);
	Layer->CreateNewGuid();
	Layer->Node.Layer = LayerName;
	Layer->Node.Interface = InterfaceClass;
	Layer->InterfaceGuid = InterfaceGraph->InterfaceGuid;
	if (FStructProperty* ReferenceProperty = FindFProperty<FStructProperty>(Layer->GetClass(), TEXT("FunctionReference")))
	{
		FGuid FunctionGuid;
		FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(FBlueprintEditorUtils::GetMostUpToDateClass(InterfaceClass), LayerName, FunctionGuid);
		ReferenceProperty->ContainerPtrToValuePtr<FMemberReference>(Layer)->SetExternalMember(LayerName, InterfaceClass, FunctionGuid);
	}
	Layer->PostPlacedNewNode();
	Layer->AllocateDefaultPins();
	Layer->NodePosX = FMath::RoundToInt(RootPosition.X) - 700;
	Layer->NodePosY = FMath::RoundToInt(RootPosition.Y) - 260;

	UAnimGraphNode_BlendListByBool* Blend = SpawnNode<UAnimGraphNode_BlendListByBool>(AnimGraph, RootPosition + FVector2D(bInertialization ? -560 : -300, 0));
	if (bInertialization)
	{
		SetNodeStructMember(Blend, TEXT("TransitionType"), TEXT("Inertialization"));
	}

	UK2Node_VariableGet* Active = NewObject<UK2Node_VariableGet>(AnimGraph, NAME_None, RF_Transactional);
	Active->VariableReference.SetSelfMember(ActiveVariable);
	AnimGraph->AddNode(Active, false, false);
	Active->CreateNewGuid();
	Active->PostPlacedNewNode();
	Active->AllocateDefaultPins();
	Active->NodePosX = Blend->NodePosX - 320;
	Active->NodePosY = Blend->NodePosY + 220;

	UK2Node_VariableGet* BlendTime = NewObject<UK2Node_VariableGet>(AnimGraph, NAME_None, RF_Transactional);
	BlendTime->VariableReference.SetSelfMember(BlendTimeVariable);
	AnimGraph->AddNode(BlendTime, false, false);
	BlendTime->CreateNewGuid();
	BlendTime->PostPlacedNewNode();
	BlendTime->AllocateDefaultPins();
	BlendTime->NodePosX = Blend->NodePosX - 320;
	BlendTime->NodePosY = Blend->NodePosY + 300;

	bool bOk = true;
	bOk &= Connect(FindPosePin(Layer, EGPD_Output), Blend->FindPin(TEXT("BlendPose_0"), EGPD_Input));
	if (Previous)
	{
		bOk &= Connect(Previous, Blend->FindPin(TEXT("BlendPose_1"), EGPD_Input));
	}
	bOk &= Connect(Active->FindPin(ActiveVariable, EGPD_Output), Blend->FindPin(TEXT("bActiveValue"), EGPD_Input));
	UEdGraphPin* TimeOut = BlendTime->FindPin(BlendTimeVariable, EGPD_Output);
	bOk &= Connect(TimeOut, Blend->FindPin(TEXT("BlendTime_0"), EGPD_Input));
	bOk &= Connect(TimeOut, Blend->FindPin(TEXT("BlendTime_1"), EGPD_Input));

	if (bInertialization)
	{
		UAnimGraphNode_Inertialization* Inertialization = SpawnNode<UAnimGraphNode_Inertialization>(AnimGraph, RootPosition + FVector2D(-260, 0));
		bOk &= Connect(FindPosePin(Blend, EGPD_Output), FindPosePin(Inertialization, EGPD_Input));
		bOk &= Connect(FindPosePin(Inertialization, EGPD_Output), RootIn);
	}
	else
	{
		bOk &= Connect(FindPosePin(Blend, EGPD_Output), RootIn);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	if (!bOk)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerBlend: some links failed on %s, see above"), *AnimBlueprint->GetName());
	}
	return bOk;
}

namespace GSTNAnimGraphAuthoring
{
	/** The graph of LayerName in AnimBlueprint's implementation of InterfaceClass; null when it does not implement it */
	const UEdGraph* FindInterfaceGraph(const UAnimBlueprint* AnimBlueprint, const UClass* InterfaceClass, FName LayerName)
	{
		for (const FBPInterfaceDescription& Description : AnimBlueprint->ImplementedInterfaces)
		{
			if (Description.Interface == InterfaceClass)
			{
				for (const UEdGraph* Graph : Description.Graphs)
				{
					if (Graph && Graph->GetFName() == LayerName)
					{
						return Graph;
					}
				}
			}
		}
		return nullptr;
	}

	/** A Linked Anim Layer node: the fields SetupFromLayerId sets when the node is spawned from the layer's menu entry */
	UAnimGraphNode_LinkedAnimLayer* SpawnLinkedLayer(UEdGraph* AnimGraph, UClass* InterfaceClass, const UEdGraph* InterfaceGraph, FName LayerName,
		const FVector2D& Position)
	{
		UAnimGraphNode_LinkedAnimLayer* Layer = NewObject<UAnimGraphNode_LinkedAnimLayer>(AnimGraph, NAME_None, RF_Transactional);
		AnimGraph->AddNode(Layer, false, false);
		Layer->CreateNewGuid();
		Layer->Node.Layer = LayerName;
		Layer->Node.Interface = InterfaceClass;
		Layer->InterfaceGuid = InterfaceGraph->InterfaceGuid;
		if (FStructProperty* ReferenceProperty = FindFProperty<FStructProperty>(Layer->GetClass(), TEXT("FunctionReference")))
		{
			FGuid FunctionGuid;
			FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(FBlueprintEditorUtils::GetMostUpToDateClass(InterfaceClass), LayerName, FunctionGuid);
			ReferenceProperty->ContainerPtrToValuePtr<FMemberReference>(Layer)->SetExternalMember(LayerName, InterfaceClass, FunctionGuid);
		}
		Layer->PostPlacedNewNode();
		Layer->AllocateDefaultPins();
		Layer->NodePosX = FMath::RoundToInt(Position.X);
		Layer->NodePosY = FMath::RoundToInt(Position.Y);
		return Layer;
	}

	UK2Node_VariableGet* SpawnSelfVariableGet(UEdGraph* Graph, FName Variable, const FVector2D& Position)
	{
		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Graph, NAME_None, RF_Transactional);
		Get->VariableReference.SetSelfMember(Variable);
		Graph->AddNode(Get, false, false);
		Get->CreateNewGuid();
		Get->PostPlacedNewNode();
		Get->AllocateDefaultPins();
		Get->NodePosX = FMath::RoundToInt(Position.X);
		Get->NodePosY = FMath::RoundToInt(Position.Y);
		return Get;
	}

	/** Removes Node, then the variable getters that fed it and now feed nothing */
	void RemoveNodeAndGetters(UBlueprint* Blueprint, UEdGraphNode* Node)
	{
		TArray<UEdGraphNode*> Getters;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Cast<UK2Node_VariableGet>(Linked->GetOwningNode()))
					{
						Getters.AddUnique(Linked->GetOwningNode());
					}
				}
			}
		}
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/ true);
		for (UEdGraphNode* Getter : Getters)
		{
			const bool bUnused = !Getter->Pins.ContainsByPredicate([](const UEdGraphPin* Pin) { return Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0; });
			if (bUnused)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Getter, true);
			}
		}
	}

	int32 CountBlendPosePins(const UEdGraphNode* Node)
	{
		int32 Count = 0;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			Count += Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString().StartsWith(TEXT("BlendPose_")) ? 1 : 0;
		}
		return Count;
	}

	UEdGraphPin* BlendPosePin(UEdGraphNode* Node, int32 Index)
	{
		return Node->FindPin(*FString::Printf(TEXT("BlendPose_%d"), Index), EGPD_Input);
	}

	UAnimGraphNode_LinkedAnimLayer* LayerOnPin(UEdGraphPin* Pin)
	{
		UEdGraphPin* Linked = LinkedPin(Pin);
		return Linked ? Cast<UAnimGraphNode_LinkedAnimLayer>(Linked->GetOwningNode()) : nullptr;
	}
}

bool UGSTN_AnimGraphAuthoringLibrary::InsertOverrideLayerSlots(UAnimBlueprint* AnimBlueprint, const TArray<FGSTN_OverrideSlotLayer>& Slots,
	FName SlotVariable, FName BlendTimeVariable, bool bInertialization, const FString& InsertBeforeNodeClass)
{
	UEdGraph* AnimGraph = FindGraph(AnimBlueprint, UEdGraphSchema_K2::GN_AnimGraph);
	if (!AnimGraph || Slots.IsEmpty())
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: needs an anim Blueprint with an AnimGraph and at least one slot"));
		return false;
	}

	// The slots, checked: compiled interfaces the Blueprint implements, slots 1.. without repeats
	struct FResolved
	{
		int32 Slot = 0;
		UClass* Interface = nullptr;
		const UEdGraph* InterfaceGraph = nullptr;
		FName Layer;
	};
	TArray<FResolved> Resolved;
	int32 MaxSlot = 0;
	for (const FGSTN_OverrideSlotLayer& Spec : Slots)
	{
		UClass* Interface = Spec.LayerInterface ? Spec.LayerInterface->GeneratedClass.Get() : nullptr;
		if (!Interface || !Interface->IsChildOf(UAnimLayerInterface::StaticClass()) || Spec.Slot < 1
			|| Resolved.ContainsByPredicate([&Spec](const FResolved& R) { return R.Slot == Spec.Slot; }))
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: slot %d needs a compiled layer interface and a slot number >= 1 used once"), Spec.Slot);
			return false;
		}
		const UEdGraph* InterfaceGraph = FindInterfaceGraph(AnimBlueprint, Interface, Spec.LayerName);
		if (!InterfaceGraph)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: %s does not implement %s with a layer %s (ImplementLayerInterface first)"),
				*AnimBlueprint->GetName(), *Interface->GetName(), *Spec.LayerName.ToString());
			return false;
		}
		Resolved.Add({Spec.Slot, Interface, InterfaceGraph, Spec.LayerName});
		MaxSlot = FMath::Max(MaxSlot, Spec.Slot);
	}
	Resolved.Sort([](const FResolved& A, const FResolved& B) { return A.Slot < B.Slot; });

	// The node the seam goes in front of: the Output Pose, or the only node of the given class
	UEdGraphNode* Root = nullptr;
	int32 Matches = 0;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		const bool bMatch = InsertBeforeNodeClass.IsEmpty() ? Cast<UAnimGraphNode_Root>(Node) != nullptr
			: (Node && Node->GetClass()->GetName() == InsertBeforeNodeClass);
		if (bMatch)
		{
			Root = Node;
			++Matches;
		}
	}
	UEdGraphPin* RootIn = Matches == 1 ? FindPosePin(Root, EGPD_Input) : nullptr;
	if (!RootIn)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: %d node(s) of class %s with a pose input in the AnimGraph, expected one"),
			Matches, InsertBeforeNodeClass.IsEmpty() ? TEXT("AnimGraphNode_Root") : *InsertBeforeNodeClass);
		return false;
	}

	// What feeds the target now: [Inertialization <-] an old seam (bool or int) or anything else (the base pose itself)
	UEdGraphPin* Base = LinkedPin(RootIn);
	UEdGraphNode* Seam = Base ? Base->GetOwningNode() : nullptr;
	UAnimGraphNode_Inertialization* Inertialization = Cast<UAnimGraphNode_Inertialization>(Seam);
	if (Inertialization)
	{
		UEdGraphPin* InertialIn = LinkedPin(FindPosePin(Inertialization, EGPD_Input));
		Seam = InertialIn ? InertialIn->GetOwningNode() : nullptr;
	}
	TArray<UEdGraphNode*> OldNodes;
	if (UAnimGraphNode_BlendListByInt* OldInt = Cast<UAnimGraphNode_BlendListByInt>(Seam))
	{
		int32 LinkedLayers = 0;
		bool bSame = CountBlendPosePins(OldInt) == MaxSlot + 1;
		for (int32 Index = 1; Index < CountBlendPosePins(OldInt); ++Index)
		{
			if (UAnimGraphNode_LinkedAnimLayer* Layer = LayerOnPin(BlendPosePin(OldInt, Index)))
			{
				++LinkedLayers;
				OldNodes.Add(Layer);
			}
		}
		for (const FResolved& R : Resolved)
		{
			const UAnimGraphNode_LinkedAnimLayer* Layer = LayerOnPin(BlendPosePin(OldInt, R.Slot));
			bSame &= Layer && Layer->Node.Layer == R.Layer && Layer->Node.Interface == R.Interface;
		}
		if (bSame && LinkedLayers == Resolved.Num())
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Log, TEXT("InsertOverrideLayerSlots: %s already has these %d slot(s)"), *AnimBlueprint->GetName(), Resolved.Num());
			return true;
		}
		Base = LinkedPin(BlendPosePin(OldInt, 0));
		OldNodes.Add(OldInt);
	}
	else if (UAnimGraphNode_BlendListByBool* OldBool = Cast<UAnimGraphNode_BlendListByBool>(Seam))
	{
		if (UAnimGraphNode_LinkedAnimLayer* Layer = LayerOnPin(OldBool->FindPin(TEXT("BlendPose_0"), EGPD_Input)))
		{
			Base = LinkedPin(OldBool->FindPin(TEXT("BlendPose_1"), EGPD_Input));
			OldNodes.Add(Layer);
			OldNodes.Add(OldBool);
		}
	}
	if (OldNodes.IsEmpty())
	{
		Inertialization = nullptr;  // not ours: it stays upstream, part of the base pose
	}

	const FVector2D RootPosition(Root->NodePosX, Root->NodePosY);
	AnimGraph->Modify();
	Root->Modify();
	for (UEdGraphNode* Old : OldNodes)
	{
		RemoveNodeAndGetters(AnimBlueprint, Old);
	}
	if (!Inertialization)
	{
		RootIn->BreakAllPinLinks();
	}
	else if (!bInertialization)
	{
		FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Inertialization, true);
		Inertialization = nullptr;
	}

	UAnimGraphNode_BlendListByInt* Blend = SpawnNode<UAnimGraphNode_BlendListByInt>(AnimGraph, RootPosition + FVector2D(bInertialization ? -560 : -300, 0));
	while (CountBlendPosePins(Blend) < MaxSlot + 1)
	{
		const int32 Before = CountBlendPosePins(Blend);
		Blend->AddPinToBlendList();
		if (CountBlendPosePins(Blend) == Before)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: could not add a pose pin to the Blend Poses by int"));
			return false;
		}
	}
	if (bInertialization)
	{
		SetNodeStructMember(Blend, TEXT("TransitionType"), TEXT("Inertialization"));
	}

	bool bOk = true;
	if (Base)
	{
		bOk &= Connect(Base, BlendPosePin(Blend, 0));
	}
	for (const FResolved& R : Resolved)
	{
		const FVector2D Position(Blend->NodePosX - 420, Blend->NodePosY - 200 * R.Slot);
		UAnimGraphNode_LinkedAnimLayer* Layer = SpawnLinkedLayer(AnimGraph, R.Interface, R.InterfaceGraph, R.Layer, Position);
		bOk &= Connect(FindPosePin(Layer, EGPD_Output), BlendPosePin(Blend, R.Slot));
	}
	for (int32 Index = 1; Index <= MaxSlot; ++Index)
	{
		if (!LayerOnPin(BlendPosePin(Blend, Index)))
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Warning, TEXT("InsertOverrideLayerSlots: slot %d has no layer (it would show the reference pose)"), Index);
		}
	}

	UK2Node_VariableGet* SlotGet = SpawnSelfVariableGet(AnimGraph, SlotVariable, FVector2D(Blend->NodePosX - 320, Blend->NodePosY + 220));
	UK2Node_VariableGet* TimeGet = SpawnSelfVariableGet(AnimGraph, BlendTimeVariable, FVector2D(Blend->NodePosX - 320, Blend->NodePosY + 300));
	bOk &= Connect(SlotGet->FindPin(SlotVariable, EGPD_Output), Blend->FindPin(TEXT("ActiveChildIndex"), EGPD_Input));
	UEdGraphPin* TimeOut = TimeGet->FindPin(BlendTimeVariable, EGPD_Output);
	for (int32 Index = 0; Index <= MaxSlot; ++Index)
	{
		bOk &= Connect(TimeOut, Blend->FindPin(*FString::Printf(TEXT("BlendTime_%d"), Index), EGPD_Input));
	}

	if (bInertialization)
	{
		if (!Inertialization)
		{
			Inertialization = SpawnNode<UAnimGraphNode_Inertialization>(AnimGraph, RootPosition + FVector2D(-260, 0));
			bOk &= Connect(FindPosePin(Inertialization, EGPD_Output), RootIn);
		}
		bOk &= Connect(FindPosePin(Blend, EGPD_Output), FindPosePin(Inertialization, EGPD_Input));
	}
	else
	{
		bOk &= Connect(FindPosePin(Blend, EGPD_Output), RootIn);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	if (!bOk)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertOverrideLayerSlots: some links failed on %s, see above"), *AnimBlueprint->GetName());
	}
	return bOk;
}

bool UGSTN_AnimGraphAuthoringLibrary::InsertPoseNodeAfter(UAnimBlueprint* AnimBlueprint, const FString& UpstreamNodeTitle, UClass* NodeClass)
{
	UEdGraph* AnimGraph = FindGraph(AnimBlueprint, UEdGraphSchema_K2::GN_AnimGraph);
	if (!AnimGraph || !NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertPoseNodeAfter: needs an anim Blueprint with an AnimGraph and an anim graph node class"));
		return false;
	}
	UEdGraphNode* Upstream = nullptr;
	int32 Matches = 0;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Cast<UAnimGraphNode_Base>(Node) && Node->GetNodeTitle(ENodeTitleType::ListView).ToString() == UpstreamNodeTitle)
		{
			Upstream = Node;
			++Matches;
		}
	}
	UEdGraphPin* UpstreamOut = Matches == 1 ? FindPosePin(Upstream, EGPD_Output) : nullptr;
	if (!UpstreamOut)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertPoseNodeAfter: %d pose node(s) titled %s in the AnimGraph, expected one"), Matches, *UpstreamNodeTitle);
		return false;
	}
	const TArray<UEdGraphPin*> Downstream = UpstreamOut->LinkedTo;
	for (const UEdGraphPin* Pin : Downstream)
	{
		if (Pin && Pin->GetOwningNode()->IsA(NodeClass))
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Log, TEXT("InsertPoseNodeAfter: %s already follows %s"), *NodeClass->GetName(), *UpstreamNodeTitle);
			return true;
		}
	}

	AnimGraph->Modify();
	Upstream->Modify();
	UAnimGraphNode_Base* NewNode = SpawnNode<UAnimGraphNode_Base>(AnimGraph, FVector2D(Upstream->NodePosX + 40, Upstream->NodePosY + 220), NodeClass);
	UEdGraphPin* NewIn = FindPosePin(NewNode, EGPD_Input);
	UEdGraphPin* NewOut = FindPosePin(NewNode, EGPD_Output);
	if (!NewIn || !NewOut)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertPoseNodeAfter: %s has no pose input and output"), *NodeClass->GetName());
		AnimGraph->RemoveNode(NewNode);
		return false;
	}
	UpstreamOut->BreakAllPinLinks();
	bool bOk = Connect(UpstreamOut, NewIn);
	for (UEdGraphPin* Pin : Downstream)
	{
		bOk &= Connect(NewOut, Pin);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	if (!bOk)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("InsertPoseNodeAfter: some links failed on %s, see above"), *AnimBlueprint->GetName());
	}
	return bOk;
}

bool UGSTN_AnimGraphAuthoringLibrary::SetChildAssetOverride(UAnimBlueprint* ChildBlueprint, FName StateName, UAnimationAsset* Asset)
{
	UAnimBlueprint* Parent = ChildBlueprint ? UAnimBlueprint::GetParentAnimBlueprint(ChildBlueprint) : nullptr;
	if (!Parent || !Asset)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetChildAssetOverride: needs a child anim Blueprint and an asset"));
		return false;
	}
	// The asset player of the parent's state with that name (overrides are keyed by the parent node's guid)
	TArray<UEdGraph*> Graphs;
	Parent->GetAllGraphs(Graphs);
	const UAnimGraphNode_Base* Player = nullptr;
	for (const UEdGraph* Graph : Graphs)
	{
		const UAnimStateNode* State = Graph ? Cast<UAnimStateNode>(Graph->GetOuter()) : nullptr;
		if (!State || State->BoundGraph != Graph || State->GetStateName() != StateName.ToString())
		{
			continue;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
			if (AnimNode && AnimNode->GetAnimationAsset())
			{
				Player = AnimNode;
				break;
			}
		}
	}
	if (!Player)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("SetChildAssetOverride: no asset player in a state %s of %s"), *StateName.ToString(), *Parent->GetName());
		return false;
	}
	ChildBlueprint->Modify();
	for (FAnimParentNodeAssetOverride& Override : ChildBlueprint->ParentAssetOverrides)
	{
		if (Override.ParentNodeGuid == Player->NodeGuid)
		{
			Override.NewAsset = Asset;
			ChildBlueprint->NotifyOverrideChange(Override);
			return true;
		}
	}
	FAnimParentNodeAssetOverride& Override = ChildBlueprint->ParentAssetOverrides.Add_GetRef(FAnimParentNodeAssetOverride(Player->NodeGuid, Asset));
	ChildBlueprint->NotifyOverrideChange(Override);
	return true;
}

FString UGSTN_AnimGraphAuthoringLibrary::DescribeGraphs(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return FString();
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	FString Out = FString::Printf(TEXT("Blueprint %s (parent %s)\n"), *Blueprint->GetPathName(), *GetNameSafe(Blueprint->ParentClass));
	for (const FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
	{
		Out += FString::Printf(TEXT("  implements %s\n"), *GetNameSafe(Description.Interface));
	}
	if (const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
	{
		for (const FAnimParentNodeAssetOverride& Override : AnimBlueprint->ParentAssetOverrides)
		{
			Out += FString::Printf(TEXT("  asset override %s -> %s\n"), *Override.ParentNodeGuid.ToString(), *GetNameSafe(Override.NewAsset));
		}
	}
	for (const UEdGraph* Graph : Graphs)
	{
		Out += FString::Printf(TEXT("Graph %s [%s] outer %s\n"), *Graph->GetName(), *Graph->GetClass()->GetName(), *GetNameSafe(Graph->GetOuter()));
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			Out += FString::Printf(TEXT("  %s \"%s\" (%s)\n"), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->GetName());
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->bHidden || (Pin->LinkedTo.Num() == 0 && Pin->DefaultValue.IsEmpty() && !Pin->DefaultObject))
				{
					continue;
				}
				Out += FString::Printf(TEXT("    %s %s [%s]"), Pin->Direction == EGPD_Input ? TEXT("in ") : TEXT("out"), *Pin->PinName.ToString(), *DescribePinType(Pin));
				if (!Pin->DefaultValue.IsEmpty())
				{
					Out += FString::Printf(TEXT(" = %s"), *Pin->DefaultValue);
				}
				if (Pin->DefaultObject)
				{
					Out += FString::Printf(TEXT(" = %s"), *Pin->DefaultObject->GetName());
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked && Linked->GetOwningNode())
					{
						Out += FString::Printf(TEXT(" -> %s.%s"), *Linked->GetOwningNode()->GetName(), *Linked->PinName.ToString());
					}
				}
				Out += TEXT("\n");
			}
			if (const UAnimGraphNode_SequencePlayer* Player = Cast<UAnimGraphNode_SequencePlayer>(Node))
			{
				Out += FString::Printf(TEXT("    sequence %s\n"), *GetNameSafe(const_cast<UAnimGraphNode_SequencePlayer*>(Player)->GetAnimationAsset()));
			}
			if (const UAnimGraphNode_LinkedAnimLayer* Layer = Cast<UAnimGraphNode_LinkedAnimLayer>(Node))
			{
				Out += FString::Printf(TEXT("    layer %s::%s\n"), *GetNameSafe(Layer->Node.Interface.Get()), *Layer->Node.Layer.ToString());
			}
			if (const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
			{
				Out += FString::Printf(TEXT("    crossfade %.2f\n"), Transition->CrossfadeDuration);
			}
		}
	}
	return Out;
}

FString UGSTN_AnimGraphAuthoringLibrary::CompileWithReport(UBlueprint* Blueprint, bool& bSuccess)
{
	bSuccess = false;
	if (!Blueprint)
	{
		return TEXT("Error: no Blueprint");
	}
	FCompilerResultsLog Results;
	Results.SetSourcePath(Blueprint->GetPathName());
	Results.BeginEvent(TEXT("Compile"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
	Results.EndEvent();

	FString Out;
	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		const TCHAR* Severity = Message->GetSeverity() == EMessageSeverity::Error ? TEXT("Error")
			: Message->GetSeverity() == EMessageSeverity::Warning || Message->GetSeverity() == EMessageSeverity::PerformanceWarning ? TEXT("Warning")
			: TEXT("Info");
		Out += FString::Printf(TEXT("%s: %s\n"), Severity, *Message->ToText().ToString());
	}
	bSuccess = Results.NumErrors == 0 && Blueprint->Status != BS_Error;
	Out += FString::Printf(TEXT("%s: %d error(s), %d warning(s), status %d\n"), *Blueprint->GetName(), Results.NumErrors, Results.NumWarnings, (int32)Blueprint->Status);
	return Out;
}
