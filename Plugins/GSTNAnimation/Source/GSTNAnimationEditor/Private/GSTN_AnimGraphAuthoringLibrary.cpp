// Editor-only anim graph authoring for scripts (no runtime dependency, no feature knowledge).

#include "GSTN_AnimGraphAuthoringLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimLayerInterface.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_BlendListByBool.h"
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

UEdGraphNode* UGSTN_AnimGraphAuthoringLibrary::FindOrInsertPoseNodeBeforeOutput(UEdGraph* Graph, const FString& NodeClassPath, const TMap<FString, FString>& NodeMembers)
{
	UClass* NodeClass = FindObject<UClass>(nullptr, *NodeClassPath);
	UEdGraphNode* Result = FindResultNode(Graph);
	UEdGraphPin* ResultIn = FindPosePin(Result, EGPD_Input);
	if (!NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()) || !ResultIn)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("FindOrInsertPoseNodeBeforeOutput: needs a graph with a result node and an anim graph node class (%s %s)"),
			*NodeClassPath, NodeClass ? TEXT("is not an anim graph node") : TEXT("not found: is its plugin enabled?"));
		return nullptr;
	}

	// Already there: the result is fed by a node of that class
	UEdGraphNode* Node = LinkedPin(ResultIn) ? LinkedPin(ResultIn)->GetOwningNode() : nullptr;
	if (!Node || Node->GetClass() != NodeClass)
	{
		UEdGraphPin* Previous = LinkedPin(ResultIn);
		Node = SpawnNode<UAnimGraphNode_Base>(Graph, FVector2D(Result->NodePosX - 300, Result->NodePosY), NodeClass);
		Result->Modify();
		ResultIn->BreakAllPinLinks();
		bool bOk = Connect(FindPosePin(Node, EGPD_Output), ResultIn);
		if (Previous)
		{
			bOk &= Connect(Previous, FindPosePin(Node, EGPD_Input));
		}
		if (!bOk)
		{
			UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("FindOrInsertPoseNodeBeforeOutput: could not wire %s in front of the result"), *NodeClass->GetName());
			return nullptr;
		}
	}

	bool bMembersOk = true;
	for (const TPair<FString, FString>& Member : NodeMembers)
	{
		bMembersOk &= SetNodeStructMember(Node, *Member.Key, *Member.Value);
	}
	if (!bMembersOk)
	{
		UE_LOG(LogGSTNAnimGraphAuthoring, Error, TEXT("FindOrInsertPoseNodeBeforeOutput: some members of %s were not set, see above"), *NodeClass->GetName());
	}

	// Pins that depend on the members (a Control Rig's exposed variables) appear on reconstruction; the pose links are kept by pin name
	Node->ReconstructNode();
	if (UBlueprint* Blueprint = GetBlueprint(Graph))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	return bMembersOk ? Node : nullptr;
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
