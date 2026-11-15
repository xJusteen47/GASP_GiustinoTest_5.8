// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Graph/GSTN_MoveGraphAuthoring.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Validation/GSTN_SimGraphRules.h"
#include "Validation/GSTN_ValidationReport.h"

namespace GSTNGraphAuthoring
{
	UEdGraph* EventGraph(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return nullptr;
		}
		// The standard EventGraph; else the first ubergraph page: the engine's Gameplay Ability Blueprint factory names its page "Gameplay
		// Ability Graph", which FindEventGraph does not find (CP5: moves made by that factory, or by hand, are still scriptable)
		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		return Graph ? Graph : (Blueprint->UbergraphPages.IsEmpty() ? nullptr : Blueprint->UbergraphPages[0].Get());
	}

	FString Id(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::Digits) : FString();
	}

	UEdGraphNode* FindNode(UBlueprint* Blueprint, const FString& NodeId)
	{
		UEdGraph* Graph = EventGraph(Blueprint);
		FGuid Guid;
		if (!Graph || !FGuid::Parse(NodeId, Guid))
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Guid)
			{
				return Node;
			}
		}
		return nullptr;
	}

	bool IsDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction)
	{
		return Pin && Pin->Direction == Direction && !Pin->bHidden && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& Pin->PinName != UEdGraphSchema_K2::PN_Self;
	}

	/** The pin of that name and side; None = the only visible data pin of that side; else a match on the name shown in the graph */
	UEdGraphPin* FindPin(UEdGraphNode& Node, FName Name, EEdGraphPinDirection Direction, FString& OutError)
	{
		if (Name.IsNone())
		{
			UEdGraphPin* Only = nullptr;
			for (UEdGraphPin* Pin : Node.Pins)
			{
				if (IsDataPin(Pin, Direction))
				{
					if (Only)
					{
						OutError = FString::Printf(TEXT("'%s' has more than one %s data pin: name it"), *Node.GetNodeTitle(ENodeTitleType::ListView).ToString(),
							Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
						return nullptr;
					}
					Only = Pin;
				}
			}
			if (!Only)
			{
				OutError = FString::Printf(TEXT("'%s' has no %s data pin"), *Node.GetNodeTitle(ENodeTitleType::ListView).ToString(),
					Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
			}
			return Only;
		}
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->PinName == Name)
			{
				return Pin;
			}
		}
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin && Pin->Direction == Direction && Pin->GetDisplayName().ToString().Equals(Name.ToString(), ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		TArray<FString> Names;
		for (const UEdGraphPin* Pin : Node.Pins)
		{
			if (Pin && Pin->Direction == Direction)
			{
				Names.Add(Pin->PinName.ToString());
			}
		}
		OutError = FString::Printf(TEXT("'%s' has no %s pin '%s' (it has: %s)"), *Node.GetNodeTitle(ENodeTitleType::ListView).ToString(),
			Direction == EGPD_Input ? TEXT("input") : TEXT("output"), *Name.ToString(), *FString::Join(Names, TEXT(", ")));
		return nullptr;
	}

	template <typename TNode>
	TNode* NewNode(UEdGraph& Graph, int32 X, int32 Y)
	{
		TNode* Node = NewObject<TNode>(&Graph);
		Node->SetFlags(RF_Transactional);
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Graph.AddNode(Node, false, false);
		return Node;
	}

	UClass* FindClass(const FString& Path)
	{
		UClass* Class = FindObject<UClass>(nullptr, *Path);
		return Class ? Class : LoadObject<UClass>(nullptr, *Path);
	}

	UScriptStruct* FindStruct(const FString& Path)
	{
		UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Path);
		return Struct ? Struct : LoadObject<UScriptStruct>(nullptr, *Path);
	}

	bool ParseType(const FString& Type, FEdGraphPinType& OutType, FString& OutError)
	{
		OutType = FEdGraphPinType();
		const FString Lower = Type.ToLower();
		if (Lower == TEXT("bool"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (Lower == TEXT("int"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (Lower == TEXT("int64"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		}
		else if (Lower == TEXT("float"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Real;
			OutType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if (Lower == TEXT("name"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if (Lower == TEXT("string"))
		{
			OutType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (Lower.StartsWith(TEXT("struct:")))
		{
			UScriptStruct* Struct = FindStruct(Type.Mid(7));
			if (!Struct)
			{
				OutError = FString::Printf(TEXT("no struct %s"), *Type.Mid(7));
				return false;
			}
			OutType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			OutType.PinSubCategoryObject = Struct;
		}
		else if (Lower.StartsWith(TEXT("object:")))
		{
			UClass* Class = FindClass(Type.Mid(7));
			if (!Class)
			{
				OutError = FString::Printf(TEXT("no class %s"), *Type.Mid(7));
				return false;
			}
			OutType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutType.PinSubCategoryObject = Class;
		}
		else
		{
			OutError = FString::Printf(TEXT("unknown variable type '%s' (bool, int, int64, float, name, string, struct:<path>, object:<class path>)"), *Type);
			return false;
		}
		return true;
	}
}

bool UGSTN_MoveGraphAuthoring::SetMemberVariable(UBlueprint* Blueprint, FName Name, const FString& Type, const FString& DefaultValue,
	const FString& Category, const FString& Tooltip, FString& OutError)
{
	OutError.Reset();
	FEdGraphPinType PinType;
	if (!Blueprint || Name.IsNone() || !GSTNGraphAuthoring::ParseType(Type, PinType, OutError))
	{
		OutError = OutError.IsEmpty() ? TEXT("no Blueprint or no name") : OutError;
		return false;
	}
	int32 Index = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Name);
	if (Index == INDEX_NONE)
	{
		if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, Name, PinType, DefaultValue))
		{
			OutError = FString::Printf(TEXT("could not add variable %s (a name the parent class already uses?)"), *Name.ToString());
			return false;
		}
		Index = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Name);
	}
	else if (Blueprint->NewVariables[Index].VarType != PinType)
	{
		FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, Name, PinType);
	}
	if (Index == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("variable %s not found after adding it"), *Name.ToString());
		return false;
	}
	Blueprint->NewVariables[Index].DefaultValue = DefaultValue;
	if (!Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, Name, nullptr, FText::FromString(Category), true);
	}
	if (!Tooltip.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, Name, nullptr, TEXT("tooltip"), Tooltip);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

int32 UGSTN_MoveGraphAuthoring::ClearEventGraph(UBlueprint* Blueprint)
{
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	if (!Graph)
	{
		return 0;
	}
	const TArray<UEdGraphNode*> Nodes = Graph->Nodes;
	int32 Removed = 0;
	for (UEdGraphNode* Node : Nodes)
	{
		if (Node)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			++Removed;
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Removed;
}

FString UGSTN_MoveGraphAuthoring::AddEventNode(UBlueprint* Blueprint, FName EventFunctionName, int32 X, int32 Y, FString& OutError)
{
	OutError.Reset();
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	const UFunction* Function = Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->FindFunctionByName(EventFunctionName) : nullptr;
	if (!Graph || !Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		OutError = FString::Printf(TEXT("no Blueprint event %s on the parent class"), *EventFunctionName.ToString());
		return FString();
	}
	UClass* Declaring = Function->GetOwnerClass();
	UK2Node_Event* Node = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, Declaring, EventFunctionName);
	if (!Node)
	{
		int32 PosY = Y;
		Node = FKismetEditorUtilities::AddDefaultEventNode(Blueprint, Graph, EventFunctionName, Declaring, PosY);
	}
	if (!Node)
	{
		OutError = FString::Printf(TEXT("could not add the event %s"), *EventFunctionName.ToString());
		return FString();
	}
	// An event placed for the user starts as a disabled "ghost" in some engine versions, and the compiler skips a disabled node
	if (Node->IsAutomaticallyPlacedGhostNode() || Node->GetDesiredEnabledState() != ENodeEnabledState::Enabled)
	{
		Node->SetEnabledState(ENodeEnabledState::Enabled, false);
	}
	Node->NodeComment.Reset();
	Node->bCommentBubbleVisible = false;
	Node->bCommentBubblePinned = false;
	Node->NodePosX = X;
	Node->NodePosY = Y;
	return GSTNGraphAuthoring::Id(Node);
}

FString UGSTN_MoveGraphAuthoring::AddCallNode(UBlueprint* Blueprint, const FString& ClassPath, FName FunctionName, int32 X, int32 Y, FString& OutError)
{
	OutError.Reset();
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	UClass* Class = ClassPath.IsEmpty() ? (Blueprint ? Blueprint->ParentClass.Get() : nullptr) : GSTNGraphAuthoring::FindClass(ClassPath);
	const UFunction* Function = Class ? Class->FindFunctionByName(FunctionName) : nullptr;
	if (!Graph || !Function)
	{
		OutError = FString::Printf(TEXT("no function %s on %s"), *FunctionName.ToString(), ClassPath.IsEmpty() ? TEXT("the parent class") : *ClassPath);
		return FString();
	}
	UK2Node_CallFunction* Node = GSTNGraphAuthoring::NewNode<UK2Node_CallFunction>(*Graph, X, Y);
	Node->SetFromFunction(Function);
	Node->AllocateDefaultPins();
	Node->PostPlacedNewNode();
	return GSTNGraphAuthoring::Id(Node);
}

FString UGSTN_MoveGraphAuthoring::AddVariableNode(UBlueprint* Blueprint, FName VariableName, bool bSet, int32 X, int32 Y, FString& OutError)
{
	OutError.Reset();
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	if (!Graph || !Blueprint->SkeletonGeneratedClass || !FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, VariableName))
	{
		OutError = FString::Printf(TEXT("no variable %s on the compiled Blueprint (compile after adding variables)"), *VariableName.ToString());
		return FString();
	}
	UK2Node_Variable* Node = bSet ? static_cast<UK2Node_Variable*>(GSTNGraphAuthoring::NewNode<UK2Node_VariableSet>(*Graph, X, Y))
		: static_cast<UK2Node_Variable*>(GSTNGraphAuthoring::NewNode<UK2Node_VariableGet>(*Graph, X, Y));
	Node->VariableReference.SetSelfMember(VariableName);
	Node->AllocateDefaultPins();
	return GSTNGraphAuthoring::Id(Node);
}

FString UGSTN_MoveGraphAuthoring::AddStructNode(UBlueprint* Blueprint, const FString& StructPath, bool bMake, int32 X, int32 Y, FString& OutError)
{
	OutError.Reset();
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	UScriptStruct* Struct = GSTNGraphAuthoring::FindStruct(StructPath);
	if (!Graph || !Struct)
	{
		OutError = FString::Printf(TEXT("no struct %s"), *StructPath);
		return FString();
	}
	if (bMake ? !UK2Node_MakeStruct::CanBeMade(Struct) : !UK2Node_BreakStruct::CanBeBroken(Struct))
	{
		OutError = FString::Printf(TEXT("%s cannot be %s in a graph"), *StructPath, bMake ? TEXT("made") : TEXT("broken"));
		return FString();
	}
	if (bMake)
	{
		UK2Node_MakeStruct* Node = GSTNGraphAuthoring::NewNode<UK2Node_MakeStruct>(*Graph, X, Y);
		Node->StructType = Struct;
		Node->bMadeAfterOverridePinRemoval = true;
		Node->AllocateDefaultPins();
		return GSTNGraphAuthoring::Id(Node);
	}
	UK2Node_BreakStruct* Node = GSTNGraphAuthoring::NewNode<UK2Node_BreakStruct>(*Graph, X, Y);
	Node->StructType = Struct;
	Node->bMadeAfterOverridePinRemoval = true;
	Node->AllocateDefaultPins();
	return GSTNGraphAuthoring::Id(Node);
}

FString UGSTN_MoveGraphAuthoring::AddBranchNode(UBlueprint* Blueprint, int32 X, int32 Y, FString& OutError)
{
	OutError.Reset();
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	if (!Graph)
	{
		OutError = TEXT("no event graph");
		return FString();
	}
	UK2Node_IfThenElse* Node = GSTNGraphAuthoring::NewNode<UK2Node_IfThenElse>(*Graph, X, Y);
	Node->AllocateDefaultPins();
	return GSTNGraphAuthoring::Id(Node);
}

bool UGSTN_MoveGraphAuthoring::ConnectPins(UBlueprint* Blueprint, const FString& FromNode, FName FromPin, const FString& ToNode, FName ToPin, FString& OutError)
{
	OutError.Reset();
	UEdGraphNode* From = GSTNGraphAuthoring::FindNode(Blueprint, FromNode);
	UEdGraphNode* To = GSTNGraphAuthoring::FindNode(Blueprint, ToNode);
	if (!From || !To)
	{
		OutError = FString::Printf(TEXT("no node %s"), From ? *ToNode : *FromNode);
		return false;
	}
	UEdGraphPin* Out = GSTNGraphAuthoring::FindPin(*From, FromPin, EGPD_Output, OutError);
	UEdGraphPin* In = Out ? GSTNGraphAuthoring::FindPin(*To, ToPin, EGPD_Input, OutError) : nullptr;
	if (!Out || !In)
	{
		return false;
	}
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(Out, In);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = FString::Printf(TEXT("%s.%s -> %s.%s refused: %s"), *From->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Out->PinName.ToString(),
			*To->GetNodeTitle(ENodeTitleType::ListView).ToString(), *In->PinName.ToString(), *Response.Message.ToString());
		return false;
	}
	if (!Schema->TryCreateConnection(Out, In))
	{
		OutError = FString::Printf(TEXT("%s -> %s not linked"), *Out->PinName.ToString(), *In->PinName.ToString());
		return false;
	}
	return true;
}

bool UGSTN_MoveGraphAuthoring::SetPinDefault(UBlueprint* Blueprint, const FString& Node, FName Pin, const FString& Value, FString& OutError)
{
	OutError.Reset();
	UEdGraphNode* Target = GSTNGraphAuthoring::FindNode(Blueprint, Node);
	UEdGraphPin* In = Target ? GSTNGraphAuthoring::FindPin(*Target, Pin, EGPD_Input, OutError) : nullptr;
	if (!In)
	{
		OutError = OutError.IsEmpty() ? FString::Printf(TEXT("no node %s"), *Node) : OutError;
		return false;
	}
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FName Category = In->PinType.PinCategory;
	if (Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Class || Category == UEdGraphSchema_K2::PC_SoftObject
		|| Category == UEdGraphSchema_K2::PC_SoftClass)
	{
		UObject* Object = Value.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *Value);
		if (!Value.IsEmpty() && !Object)
		{
			OutError = FString::Printf(TEXT("no asset %s"), *Value);
			return false;
		}
		Schema->TrySetDefaultObject(*In, Object);
	}
	else
	{
		Schema->TrySetDefaultValue(*In, Value);
	}
	OutError = Schema->IsCurrentPinDefaultValid(In);
	return OutError.IsEmpty();
}

int32 UGSTN_MoveGraphAuthoring::CompileAndReport(UBlueprint* Blueprint, TArray<FString>& OutMessages)
{
	OutMessages.Reset();
	if (!Blueprint)
	{
		OutMessages.Add(TEXT("error: no Blueprint"));
		return 1;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FCompilerResultsLog Results;
	Results.SetSourcePath(Blueprint->GetPathName());
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		const EMessageSeverity::Type Severity = Message->GetSeverity();
		if (Severity == EMessageSeverity::Error || Severity == EMessageSeverity::Warning || Severity == EMessageSeverity::PerformanceWarning)
		{
			OutMessages.Add(FString::Printf(TEXT("%s: %s"), Severity == EMessageSeverity::Error ? TEXT("error") : TEXT("warning"), *Message->ToText().ToString()));
		}
	}
	return Results.NumErrors + (Blueprint->Status == BS_Error && Results.NumErrors == 0 ? 1 : 0);
}

int32 UGSTN_MoveGraphAuthoring::CheckSimulationRules(UBlueprint* Blueprint, TArray<FString>& OutFindings)
{
	OutFindings.Reset();
	if (!Blueprint)
	{
		return 0;
	}
	FGSTN_ValidationReport Report;
	FGSTN_SimGraphRules::CheckBlueprint(*Blueprint, Blueprint, Report);
	for (const FGSTN_ValidationFinding& Finding : Report.Findings)
	{
		const TCHAR* Severity = Finding.Severity == EGSTN_Severity::Error ? TEXT("error") : Finding.Severity == EGSTN_Severity::Warning ? TEXT("warning") : TEXT("info");
		OutFindings.Add(FString::Printf(TEXT("%s %s: %s"), Severity, *Finding.Code.ToString(), *Finding.Message.ToString()));
	}
	return Report.CountSeverity(EGSTN_Severity::Error);
}

TArray<FString> UGSTN_MoveGraphAuthoring::DescribeEventGraph(UBlueprint* Blueprint)
{
	TArray<FString> Lines;
	UEdGraph* Graph = GSTNGraphAuthoring::EventGraph(Blueprint);
	if (!Graph)
	{
		return Lines;
	}
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		TArray<FString> Defaults;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->LinkedTo.IsEmpty() && !Pin->bHidden && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
				&& (!Pin->DefaultValue.IsEmpty() || Pin->DefaultObject))
			{
				Defaults.Add(FString::Printf(TEXT("%s=%s"), *Pin->PinName.ToString(), Pin->DefaultObject ? *Pin->DefaultObject->GetName() : *Pin->DefaultValue));
			}
		}
		Lines.Add(FString::Printf(TEXT("node %s [%s]%s%s"), *Title, *GSTNGraphAuthoring::Id(Node).Left(8), Defaults.IsEmpty() ? TEXT("") : TEXT(" "),
			*FString::Join(Defaults, TEXT(", "))));
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNodeUnchecked())
				{
					Lines.Add(FString::Printf(TEXT("  %s.%s -> %s.%s"), *Title, *Pin->PinName.ToString(),
						*Linked->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Linked->PinName.ToString()));
				}
			}
		}
	}
	return Lines;
}
