// Rule wiring (a result pin driven by K2 getters and Kismet comparisons) follows the approach of ChiR24/Unreal_mcp (EngineWire,
// MIT License, see Plugins/GSTNAnimToolsets/ThirdPartyNotices.md). This version adds OR-of-ANDs rules, functions, enum/name
// comparisons and state machine getters, and validates the whole rule before touching the graph.

#include "GSTN_AnimRuleGraph.h"

#include "GSTN_AnimBlueprintToolset.h"

#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateConduitNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_AnimGetter.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_EnumInequality.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GSTNAnimToolsets"

namespace GSTN::AnimRuleGraph
{
	namespace
	{
		const FName CanEnterPinName(TEXT("bCanEnterTransition"));

		enum class EValueType : uint8 { Bool, Real, Int, Int64, Byte, Enum, Name, String, Other };

		const TCHAR* ValueTypeName(EValueType Type)
		{
			switch (Type)
			{
			case EValueType::Bool: return TEXT("bool");
			case EValueType::Real: return TEXT("float");
			case EValueType::Int: return TEXT("int");
			case EValueType::Int64: return TEXT("int64");
			case EValueType::Byte: return TEXT("byte");
			case EValueType::Enum: return TEXT("enum");
			case EValueType::Name: return TEXT("name");
			case EValueType::String: return TEXT("string");
			default: return TEXT("unsupported type");
			}
		}

		EValueType Classify(const FProperty* Property, UEnum*& OutEnum)
		{
			OutEnum = nullptr;
			if (CastField<FBoolProperty>(Property)) { return EValueType::Bool; }
			if (CastField<FFloatProperty>(Property) || CastField<FDoubleProperty>(Property)) { return EValueType::Real; }
			if (CastField<FIntProperty>(Property)) { return EValueType::Int; }
			if (CastField<FInt64Property>(Property)) { return EValueType::Int64; }
			if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
			{
				OutEnum = Byte->Enum;
				return OutEnum ? EValueType::Enum : EValueType::Byte;
			}
			if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
			{
				OutEnum = Enum->GetEnum();
				return EValueType::Enum;
			}
			if (CastField<FNameProperty>(Property)) { return EValueType::Name; }
			if (CastField<FStrProperty>(Property)) { return EValueType::String; }
			return EValueType::Other;
		}

		/** Everything the builder needs about one condition, found before the graph is touched. */
		struct FResolvedCondition
		{
			const FGSTN_RuleCondition* Condition = nullptr;
			FProperty* Variable = nullptr;
			UFunction* Function = nullptr;
			FName OutputPin;
			bool bGetter = false;
			UAnimGraphNode_Base* GetterSourceNode = nullptr;
			UAnimStateNodeBase* GetterSourceState = nullptr;
			UClass* GetterClass = nullptr;
			TMap<FName, FString> Arguments;
			EValueType Type = EValueType::Other;
			/** The literal as the comparison pin expects it (enum entry short name, canonical bool). */
			FString Literal;
			bool bWantTrue = true;
		};

		struct FRuleContext
		{
			UAnimBlueprint* AnimBlueprint = nullptr;
			UEdGraph* RuleGraph = nullptr;
			UAnimStateTransitionNode* Transition = nullptr;
			UAnimStateNodeBase* SourceState = nullptr;
			UAnimGraphNode_StateMachineBase* Machine = nullptr;
			UClass* SelfClass = nullptr;
		};

		bool LooseNameMatch(const FString& Candidate, const FString& DisplayName, const FString& Wanted)
		{
			const FString Squashed = Wanted.Replace(TEXT(" "), TEXT(""));
			return Candidate.Equals(Wanted, ESearchCase::IgnoreCase) || Candidate.Equals(Squashed, ESearchCase::IgnoreCase)
				|| (!DisplayName.IsEmpty() && DisplayName.Equals(Wanted, ESearchCase::IgnoreCase));
		}

		FProperty* FindVariable(UClass* Class, const FString& Name)
		{
			if (FProperty* Exact = FindFProperty<FProperty>(Class, *Name))
			{
				return Exact;
			}
			for (TFieldIterator<FProperty> It(Class); It; ++It)
			{
				if (LooseNameMatch(It->GetName(), It->GetDisplayNameText().ToString(), Name))
				{
					return *It;
				}
			}
			return nullptr;
		}

		UFunction* FindFunction(UClass* Class, const FString& Name, TFunctionRef<bool(const UFunction*)> Filter)
		{
			for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				if (Filter(*It) && LooseNameMatch(It->GetName(), It->GetMetaData(TEXT("DisplayName")), Name))
				{
					return *It;
				}
			}
			return nullptr;
		}

		bool FunctionHasParam(const UFunction* Function, const TCHAR* ParamName)
		{
			return FindFProperty<FProperty>(Function, ParamName) != nullptr;
		}

		bool ResolveArguments(const UFunction* Function, const FGSTN_RuleCondition& Condition, FResolvedCondition& Out, FString& OutError)
		{
			for (const TPair<FString, FString>& Argument : Condition.Arguments)
			{
				const FProperty* Param = nullptr;
				for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
				{
					const bool bInput = !It->HasAnyPropertyFlags(CPF_ReturnParm) && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm));
					if (bInput && It->GetName().Equals(Argument.Key, ESearchCase::IgnoreCase))
					{
						Param = *It;
						break;
					}
				}
				if (!Param)
				{
					OutError = FString::Printf(TEXT("%s has no input '%s'."), *Function->GetName(), *Argument.Key);
					return false;
				}
				Out.Arguments.Add(Param->GetFName(), Argument.Value);
			}
			return true;
		}

		/** The value a function hands back: its return value, or its only output parameter. */
		const FProperty* FunctionOutput(const UFunction* Function)
		{
			if (const FProperty* Return = Function->GetReturnProperty())
			{
				return Return;
			}
			const FProperty* Found = nullptr;
			for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
			{
				if (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm))
				{
					if (Found)
					{
						return nullptr;
					}
					Found = *It;
				}
			}
			return Found;
		}

		bool ResolveVariable(const FRuleContext& Context, const FGSTN_RuleCondition& Condition, FResolvedCondition& Out, FString& OutError)
		{
			FProperty* Variable = FindVariable(Context.SelfClass, Condition.Name);
			if (!Variable || !Variable->HasAnyPropertyFlags(CPF_BlueprintVisible))
			{
				OutError = FString::Printf(TEXT("'%s' is not a Blueprint-visible variable of %s."), *Condition.Name, *Context.AnimBlueprint->GetName());
				return false;
			}
			Out.Variable = Variable;
			UEnum* Enum = nullptr;
			Out.Type = Classify(Variable, Enum);
			return true;
		}

		bool ResolveFunction(const FRuleContext& Context, const FGSTN_RuleCondition& Condition, FResolvedCondition& Out, FString& OutError)
		{
			UClass* Owner = Condition.FunctionClass ? Condition.FunctionClass.Get() : Context.SelfClass;
			UFunction* Function = FindFunction(Owner, Condition.Name, [](const UFunction* Candidate)
			{
				return Candidate->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure) && !Candidate->HasMetaData(TEXT("AnimGetter"));
			});
			if (!Function)
			{
				OutError = FString::Printf(TEXT("'%s' is not a Blueprint function of %s."), *Condition.Name, *Owner->GetName());
				return false;
			}
			if (!Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
			{
				OutError = FString::Printf(TEXT("%s is not pure: a transition rule can only call pure functions."), *Function->GetName());
				return false;
			}
			if (Condition.FunctionClass && !Function->HasAnyFunctionFlags(FUNC_Static) && !Context.SelfClass->IsChildOf(Function->GetOwnerClass()))
			{
				OutError = FString::Printf(TEXT("%s is not static and %s does not derive from %s."), *Function->GetName(),
					*Context.AnimBlueprint->GetName(), *Function->GetOwnerClass()->GetName());
				return false;
			}
			const FProperty* Output = FunctionOutput(Function);
			if (!Output)
			{
				OutError = FString::Printf(TEXT("%s must have a return value or exactly one output."), *Function->GetName());
				return false;
			}
			Out.Function = Function;
			Out.OutputPin = Output->GetFName();
			UEnum* Enum = nullptr;
			Out.Type = Classify(Output, Enum);
			return ResolveArguments(Function, Condition, Out, OutError);
		}

		bool ResolveGetter(const FRuleContext& Context, const FGSTN_RuleCondition& Condition, FResolvedCondition& Out, FString& OutError)
		{
			UClass* NativeClass = Context.AnimBlueprint->ParentClass;
			while (NativeClass && !NativeClass->HasAnyClassFlags(CLASS_Native))
			{
				NativeClass = NativeClass->GetSuperClass();
			}
			UFunction* Getter = NativeClass ? FindFunction(NativeClass, Condition.Name, [](const UFunction* Candidate)
			{
				return Candidate->HasMetaData(TEXT("AnimGetter")) && Candidate->HasAnyFunctionFlags(FUNC_Native);
			}) : nullptr;
			if (!Getter)
			{
				OutError = FString::Printf(TEXT("'%s' is not an anim getter (e.g. GetRelevantAnimTimeRemaining, \"Time Remaining (ratio)\")."), *Condition.Name);
				return false;
			}
			const FString Contexts = Getter->GetMetaData(TEXT("GetterContext"));
			if (!Contexts.IsEmpty() && !Contexts.Contains(TEXT("Transition")))
			{
				OutError = FString::Printf(TEXT("%s is only valid in %s graphs."), *Getter->GetName(), *Contexts);
				return false;
			}

			const bool bNeedsPlayer = FunctionHasParam(Getter, TEXT("AssetPlayerIndex"));
			const bool bNeedsMachine = FunctionHasParam(Getter, TEXT("MachineIndex"));
			const bool bNeedsState = FunctionHasParam(Getter, TEXT("StateIndex"));
			const bool bNeedsTransition = FunctionHasParam(Getter, TEXT("TransitionIndex"));
			if ((bNeedsPlayer || bNeedsState) && !Cast<UAnimStateNode>(Context.SourceState))
			{
				OutError = FString::Printf(TEXT("%s reads the source state, so it needs a transition that leaves a state."), *Getter->GetName());
				return false;
			}
			if (bNeedsTransition && !Context.Transition)
			{
				OutError = FString::Printf(TEXT("%s needs a transition, not a conduit."), *Getter->GetName());
				return false;
			}
			if (bNeedsPlayer)
			{
				TArray<UEdGraph*> Graphs{Context.SourceState->GetBoundGraph()};
				Context.SourceState->GetBoundGraph()->GetAllChildrenGraphs(Graphs);
				for (const UEdGraph* Graph : Graphs)
				{
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(Node);
						if (!Player || Out.GetterSourceNode)
						{
							continue;
						}
						const UAnimationAsset* Asset = Player->GetAnimationAsset();
						const bool bMatch = Condition.AssetPlayer.IsEmpty()
							|| Player->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(Condition.AssetPlayer)
							|| (Asset && Asset->GetName().Equals(Condition.AssetPlayer, ESearchCase::IgnoreCase));
						if (bMatch)
						{
							Out.GetterSourceNode = Player;
						}
					}
				}
				if (!Out.GetterSourceNode)
				{
					OutError = FString::Printf(TEXT("State '%s' has no asset player%s."), *Context.SourceState->GetStateName(),
						Condition.AssetPlayer.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" matching '%s'"), *Condition.AssetPlayer));
					return false;
				}
			}
			else if (bNeedsMachine)
			{
				Out.GetterSourceNode = Context.Machine;
				Out.GetterSourceState = bNeedsTransition ? static_cast<UAnimStateNodeBase*>(Context.Transition) : (bNeedsState ? Context.SourceState : nullptr);
			}
			const FProperty* Output = FunctionOutput(Getter);
			if (!Output)
			{
				OutError = FString::Printf(TEXT("%s has no return value."), *Getter->GetName());
				return false;
			}
			Out.bGetter = true;
			Out.Function = Getter;
			Out.GetterClass = NativeClass;
			Out.OutputPin = Output->GetFName();
			UEnum* Enum = nullptr;
			Out.Type = Classify(Output, Enum);
			return ResolveArguments(Getter, Condition, Out, OutError);
		}

		bool ResolveCompare(const FProperty* ValueProperty, FResolvedCondition& Out, FString& OutError)
		{
			const FGSTN_RuleCondition& Condition = *Out.Condition;
			const EGSTN_RuleCompare Compare = Condition.Compare;
			const bool bEquality = Compare == EGSTN_RuleCompare::Equal || Compare == EGSTN_RuleCompare::NotEqual;
			const bool bOrdering = !bEquality && Compare != EGSTN_RuleCompare::IsTrue && Compare != EGSTN_RuleCompare::IsFalse;
			const FString What = FString::Printf(TEXT("'%s' (%s)"), *Condition.Name, ValueTypeName(Out.Type));

			switch (Out.Type)
			{
			case EValueType::Bool:
			{
				if (bOrdering)
				{
					OutError = What + TEXT(" can only be tested with IsTrue, IsFalse, Equal or NotEqual.");
					return false;
				}
				bool bLiteral = true;
				if (bEquality)
				{
					if (!Condition.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) && !Condition.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
					{
						OutError = What + TEXT(" needs Value true or false.");
						return false;
					}
					bLiteral = Condition.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
				}
				Out.bWantTrue = Compare == EGSTN_RuleCompare::IsTrue || (Compare == EGSTN_RuleCompare::Equal && bLiteral)
					|| (Compare == EGSTN_RuleCompare::NotEqual && !bLiteral);
				return true;
			}
			case EValueType::Real:
			case EValueType::Int:
			case EValueType::Int64:
			case EValueType::Byte:
			{
				double Number = 0.0;
				if (!bEquality && !bOrdering)
				{
					OutError = What + TEXT(" is a number: use Equal, NotEqual, Less, LessEqual, Greater or GreaterEqual.");
					return false;
				}
				if (!LexTryParseString(Number, *Condition.Value.TrimStartAndEnd()))
				{
					OutError = What + FString::Printf(TEXT(" needs a numeric Value, got '%s'."), *Condition.Value);
					return false;
				}
				Out.Literal = Condition.Value.TrimStartAndEnd();
				return true;
			}
			case EValueType::Enum:
			{
				UEnum* Enum = nullptr;
				Classify(ValueProperty, Enum);
				if (!bEquality || !Enum)
				{
					OutError = What + TEXT(" can only be tested with Equal or NotEqual.");
					return false;
				}
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
				{
					const FString ShortName = Enum->GetNameStringByIndex(Index);
					if (ShortName.Equals(Condition.Value, ESearchCase::IgnoreCase)
						|| Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(Condition.Value, ESearchCase::IgnoreCase))
					{
						Out.Literal = ShortName;
						return true;
					}
				}
				TArray<FString> Names;
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
				{
					Names.Add(Enum->GetNameStringByIndex(Index));
				}
				OutError = FString::Printf(TEXT("'%s' is not an entry of %s (%s)."), *Condition.Value, *Enum->GetName(), *FString::Join(Names, TEXT(", ")));
				return false;
			}
			case EValueType::Name:
			case EValueType::String:
				if (!bEquality)
				{
					OutError = What + TEXT(" can only be tested with Equal or NotEqual.");
					return false;
				}
				Out.Literal = Condition.Value;
				return true;
			default:
				OutError = What + TEXT(": only bool, number, enum, name and string values can be tested.");
				return false;
			}
		}

		bool Resolve(const FRuleContext& Context, const FGSTN_RuleCondition& Condition, FResolvedCondition& Out, FString& OutError)
		{
			Out.Condition = &Condition;
			if (Condition.Name.IsEmpty())
			{
				OutError = TEXT("A condition has an empty Name.");
				return false;
			}
			bool bResolved = false;
			switch (Condition.Operand)
			{
			case EGSTN_RuleOperand::Variable: bResolved = ResolveVariable(Context, Condition, Out, OutError); break;
			case EGSTN_RuleOperand::Function: bResolved = ResolveFunction(Context, Condition, Out, OutError); break;
			case EGSTN_RuleOperand::AnimGetter: bResolved = ResolveGetter(Context, Condition, Out, OutError); break;
			}
			if (!bResolved)
			{
				return false;
			}
			const FProperty* ValueProperty = Out.Variable ? Out.Variable : FindFProperty<FProperty>(Out.Function, Out.OutputPin);
			return ResolveCompare(ValueProperty, Out, OutError);
		}

		// ---- Building ----

		struct FBuilder
		{
			const FRuleContext& Context;
			const UEdGraphSchema* Schema = nullptr;
			FString Error;

			explicit FBuilder(const FRuleContext& InContext)
				: Context(InContext), Schema(InContext.RuleGraph->GetSchema())
			{
			}

			bool Link(UEdGraphPin* Output, UEdGraphPin* Input)
			{
				if (Output && Input && Schema->TryCreateConnection(Output, Input))
				{
					return true;
				}
				Error = FString::Printf(TEXT("Could not connect %s to %s."), Output ? *Output->PinName.ToString() : TEXT("null"),
					Input ? *Input->PinName.ToString() : TEXT("null"));
				return false;
			}

			UK2Node_CallFunction* Call(UFunction* Function, const FIntPoint& Where)
			{
				FGraphNodeCreator<UK2Node_CallFunction> Creator(*Context.RuleGraph);
				UK2Node_CallFunction* Node = Creator.CreateNode(false);
				Node->SetFromFunction(Function);
				Node->NodePosX = Where.X;
				Node->NodePosY = Where.Y;
				Creator.Finalize();
				return Node;
			}

			UEdGraphPin* CallBinary(UClass* Library, const FString& FunctionName, UEdGraphPin* A, UEdGraphPin* B, const FString& BLiteral, const FIntPoint& Where)
			{
				UFunction* Function = Library->FindFunctionByName(*FunctionName);
				if (!Function)
				{
					Error = FString::Printf(TEXT("%s::%s not found."), *Library->GetName(), *FunctionName);
					return nullptr;
				}
				UK2Node_CallFunction* Node = Call(Function, Where);
				if (!Link(A, Node->FindPin(TEXT("A"))))
				{
					return nullptr;
				}
				UEdGraphPin* PinB = Node->FindPin(TEXT("B"));
				if (B)
				{
					if (!Link(B, PinB))
					{
						return nullptr;
					}
				}
				else if (PinB)
				{
					Schema->TrySetDefaultValue(*PinB, BLiteral);
				}
				return Node->GetReturnValuePin();
			}

			UEdGraphPin* SetArguments(UK2Node* Node, const FResolvedCondition& Resolved, UEdGraphPin* Output)
			{
				for (const TPair<FName, FString>& Argument : Resolved.Arguments)
				{
					if (UEdGraphPin* Pin = Node->FindPin(Argument.Key, EGPD_Input))
					{
						Schema->TrySetDefaultValue(*Pin, Argument.Value);
					}
				}
				if (!Output)
				{
					Error = FString::Printf(TEXT("%s has no output pin %s."), *Resolved.Condition->Name, *Resolved.OutputPin.ToString());
				}
				return Output;
			}

			UEdGraphPin* Operand(const FResolvedCondition& Resolved, const FIntPoint& Where)
			{
				if (Resolved.Variable)
				{
					FGraphNodeCreator<UK2Node_VariableGet> Creator(*Context.RuleGraph);
					UK2Node_VariableGet* Node = Creator.CreateNode(false);
					Node->VariableReference.SetSelfMember(Resolved.Variable->GetFName());
					Node->NodePosX = Where.X;
					Node->NodePosY = Where.Y;
					Creator.Finalize();
					UEdGraphPin* Value = Node->FindPin(Resolved.Variable->GetFName(), EGPD_Output);
					if (!Value)
					{
						Error = FString::Printf(TEXT("Variable %s could not be read."), *Resolved.Variable->GetName());
					}
					return Value;
				}
				if (Resolved.bGetter)
				{
					FGraphNodeCreator<UK2Node_AnimGetter> Creator(*Context.RuleGraph);
					UK2Node_AnimGetter* Node = Creator.CreateNode(false);
					Node->SourceNode = Resolved.GetterSourceNode;
					Node->SourceStateNode = Resolved.GetterSourceState;
					Node->GetterClass = Resolved.GetterClass;
					Node->SourceAnimBlueprint = Context.AnimBlueprint;
					Node->SetFromFunction(Resolved.Function);
					Resolved.Function->GetMetaData(TEXT("GetterContext")).ParseIntoArray(Node->Contexts, TEXT("|"), true);
					FString Subject;
					if (const UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(Resolved.GetterSourceNode))
					{
						Subject = Player->GetAnimationAsset() ? Player->GetAnimationAsset()->GetName() : Player->GetNodeTitle(ENodeTitleType::ListView).ToString();
					}
					else if (Resolved.GetterSourceState)
					{
						Subject = Resolved.GetterSourceState->GetStateName();
					}
					const FText FunctionTitle = Resolved.Function->GetDisplayNameText();
					Node->CachedTitle = Subject.IsEmpty() ? FunctionTitle : FText::Format(LOCTEXT("GetterTitle", "{0} ({1})"), FunctionTitle, FText::FromString(Subject));
					Node->NodePosX = Where.X;
					Node->NodePosY = Where.Y;
					Creator.Finalize();
					return SetArguments(Node, Resolved, Node->FindPin(Resolved.OutputPin, EGPD_Output));
				}
				UK2Node_CallFunction* Node = Call(Resolved.Function, Where);
				return SetArguments(Node, Resolved, Node->FindPin(Resolved.OutputPin, EGPD_Output));
			}

			UEdGraphPin* Condition(const FResolvedCondition& Resolved, const FIntPoint& Where)
			{
				UEdGraphPin* Value = Operand(Resolved, Where);
				if (!Value)
				{
					return nullptr;
				}
				const FIntPoint Next(Where.X + 320, Where.Y);
				const EGSTN_RuleCompare Compare = Resolved.Condition->Compare;
				switch (Resolved.Type)
				{
				case EValueType::Bool:
					if (Resolved.bWantTrue)
					{
						return Value;
					}
					return Not(Value, Next);
				case EValueType::Enum:
				{
					UK2Node_EnumEquality* Node = nullptr;
					if (Compare == EGSTN_RuleCompare::NotEqual)
					{
						FGraphNodeCreator<UK2Node_EnumInequality> Creator(*Context.RuleGraph);
						Node = Creator.CreateNode(false);
						Node->NodePosX = Next.X;
						Node->NodePosY = Next.Y;
						Creator.Finalize();
					}
					else
					{
						FGraphNodeCreator<UK2Node_EnumEquality> Creator(*Context.RuleGraph);
						Node = Creator.CreateNode(false);
						Node->NodePosX = Next.X;
						Node->NodePosY = Next.Y;
						Creator.Finalize();
					}
					if (!Link(Value, Node->FindPin(TEXT("A"))))
					{
						return nullptr;
					}
					UEdGraphPin* PinB = Node->FindPin(TEXT("B"));
					Schema->TrySetDefaultValue(*PinB, Resolved.Literal);
					return Node->FindPin(UEdGraphSchema_K2::PN_ReturnValue);
				}
				case EValueType::Name:
					return CallBinary(UKismetMathLibrary::StaticClass(),
						Compare == EGSTN_RuleCompare::Equal ? TEXT("EqualEqual_NameName") : TEXT("NotEqual_NameName"), Value, nullptr, Resolved.Literal, Next);
				case EValueType::String:
					return CallBinary(UKismetStringLibrary::StaticClass(),
						Compare == EGSTN_RuleCompare::Equal ? TEXT("EqualEqual_StriStri") : TEXT("NotEqual_StriStri"), Value, nullptr, Resolved.Literal, Next);
				default:
				{
					const TCHAR* Prefix = TEXT("EqualEqual_");
					switch (Compare)
					{
					case EGSTN_RuleCompare::NotEqual: Prefix = TEXT("NotEqual_"); break;
					case EGSTN_RuleCompare::Less: Prefix = TEXT("Less_"); break;
					case EGSTN_RuleCompare::LessEqual: Prefix = TEXT("LessEqual_"); break;
					case EGSTN_RuleCompare::Greater: Prefix = TEXT("Greater_"); break;
					case EGSTN_RuleCompare::GreaterEqual: Prefix = TEXT("GreaterEqual_"); break;
					default: break;
					}
					const TCHAR* Suffix = Resolved.Type == EValueType::Real ? TEXT("DoubleDouble")
						: Resolved.Type == EValueType::Int ? TEXT("IntInt")
						: Resolved.Type == EValueType::Int64 ? TEXT("Int64Int64") : TEXT("ByteByte");
					return CallBinary(UKismetMathLibrary::StaticClass(), FString(Prefix) + Suffix, Value, nullptr, Resolved.Literal, Next);
				}
				}
			}

			UEdGraphPin* Not(UEdGraphPin* Value, const FIntPoint& Where)
			{
				UK2Node_CallFunction* Node = Call(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool")), Where);
				return Link(Value, Node->FindPin(TEXT("A"))) ? Node->GetReturnValuePin() : nullptr;
			}

			UEdGraphPin* Chain(const TArray<UEdGraphPin*>& Values, const TCHAR* FunctionName, const FIntPoint& Where)
			{
				UEdGraphPin* Accumulated = Values[0];
				for (int32 Index = 1; Index < Values.Num() && Accumulated; ++Index)
				{
					Accumulated = CallBinary(UKismetMathLibrary::StaticClass(), FunctionName, Accumulated, Values[Index], FString(),
						FIntPoint(Where.X + (Index - 1) * 200, Where.Y + Index * 40));
				}
				return Accumulated;
			}
		};

		// ---- Describing ----

		FString DescribeOutput(const UEdGraphPin* Output, int32 Depth);

		FString DescribeInput(const UEdGraphPin* Input, int32 Depth)
		{
			if (Input->LinkedTo.Num() > 0)
			{
				return DescribeOutput(Input->LinkedTo[0], Depth + 1);
			}
			if (Input->DefaultObject)
			{
				return Input->DefaultObject->GetName();
			}
			const FString Literal = Input->GetDefaultAsString();
			return Literal.IsEmpty() ? FString(TEXT("\"\"")) : Literal;
		}

		TArray<const UEdGraphPin*> VisibleInputs(const UEdGraphNode* Node)
		{
			TArray<const UEdGraphPin*> Inputs;
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Input && !Pin->bHidden && Pin->PinName != UEdGraphSchema_K2::PN_Self
					&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
				{
					Inputs.Add(Pin);
				}
			}
			return Inputs;
		}

		FString OperatorFor(const FString& FunctionName)
		{
			static const TPair<const TCHAR*, const TCHAR*> Operators[] = {
				{TEXT("EqualEqual_"), TEXT("==")}, {TEXT("NotEqual_"), TEXT("!=")}, {TEXT("LessEqual_"), TEXT("<=")},
				{TEXT("GreaterEqual_"), TEXT(">=")}, {TEXT("Less_"), TEXT("<")}, {TEXT("Greater_"), TEXT(">")}};
			for (const TPair<const TCHAR*, const TCHAR*>& Operator : Operators)
			{
				if (FunctionName.StartsWith(Operator.Key))
				{
					return Operator.Value;
				}
			}
			return FString();
		}

		FString DescribeOutput(const UEdGraphPin* Output, int32 Depth)
		{
			const UEdGraphNode* Node = Output->GetOwningNode();
			if (Depth > 32)
			{
				return TEXT("...");
			}
			if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
			{
				return Get->GetVarName().ToString();
			}
			if (Cast<UK2Node_EnumEquality>(Node))
			{
				const TCHAR* Operator = Node->IsA<UK2Node_EnumInequality>() ? TEXT("!=") : TEXT("==");
				return FString::Printf(TEXT("(%s %s %s)"), *DescribeInput(Node->FindPin(TEXT("A")), Depth), Operator, *DescribeInput(Node->FindPin(TEXT("B")), Depth));
			}
			if (Cast<UK2Node_AnimGetter>(Node))
			{
				return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			}
			if (const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				const FString FunctionName = CallNode->FunctionReference.GetMemberName().ToString();
				const TArray<const UEdGraphPin*> Inputs = VisibleInputs(Node);
				TArray<FString> Parts;
				for (const UEdGraphPin* Input : Inputs)
				{
					Parts.Add(DescribeInput(Input, Depth));
				}
				if (FunctionName == TEXT("BooleanAND") || FunctionName == TEXT("BooleanOR"))
				{
					return TEXT("(") + FString::Join(Parts, FunctionName == TEXT("BooleanAND") ? TEXT(" && ") : TEXT(" || ")) + TEXT(")");
				}
				if (FunctionName == TEXT("Not_PreBool") && Parts.Num() == 1)
				{
					return TEXT("!") + Parts[0];
				}
				const FString Operator = OperatorFor(FunctionName);
				if (!Operator.IsEmpty() && Parts.Num() >= 2)
				{
					return FString::Printf(TEXT("(%s %s %s)"), *Parts[0], *Operator, *Parts[1]);
				}
				TArray<FString> Arguments;
				for (int32 Index = 0; Index < Inputs.Num(); ++Index)
				{
					Arguments.Add(FString::Printf(TEXT("%s=%s"), *Inputs[Index]->PinName.ToString(), *Parts[Index]));
				}
				FString Text = FString::Printf(TEXT("%s(%s)"), *FunctionName, *FString::Join(Arguments, TEXT(", ")));
				if (Output->PinName != UEdGraphSchema_K2::PN_ReturnValue)
				{
					Text += TEXT(".") + Output->PinName.ToString();
				}
				return Text;
			}
			return Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Replace(TEXT("\n"), TEXT(" "));
		}

		UAnimGraphNode_TransitionResult* ResultNodeOf(UEdGraph* RuleGraph)
		{
			UAnimationTransitionGraph* TransitionGraph = Cast<UAnimationTransitionGraph>(RuleGraph);
			return TransitionGraph ? TransitionGraph->GetResultNode() : nullptr;
		}
	}

	FString DescribeRule(const UEdGraph* RuleGraph)
	{
		const UAnimGraphNode_TransitionResult* Result = ResultNodeOf(const_cast<UEdGraph*>(RuleGraph));
		const UEdGraphPin* CanEnter = Result ? Result->FindPin(CanEnterPinName) : nullptr;
		return CanEnter ? DescribeInput(CanEnter, 0) : FString(TEXT("false"));
	}

	bool WriteRule(UEdGraphNode* TransitionOrConduit, const FGSTN_TransitionRule& Rule, FString& OutExpression, FString& OutError)
	{
		FRuleContext Context;
		Context.Transition = Cast<UAnimStateTransitionNode>(TransitionOrConduit);
		UAnimStateConduitNode* Conduit = Cast<UAnimStateConduitNode>(TransitionOrConduit);
		if (!Context.Transition && !Conduit)
		{
			OutError = TEXT("TransitionOrConduit must be a transition or a conduit.");
			return false;
		}
		Context.RuleGraph = Context.Transition ? Context.Transition->BoundGraph.Get() : Conduit->BoundGraph.Get();
		Context.AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(TransitionOrConduit->GetGraph()));
		UAnimGraphNode_TransitionResult* Result = ResultNodeOf(Context.RuleGraph);
		UEdGraphPin* CanEnter = Result ? Result->FindPin(CanEnterPinName) : nullptr;
		if (!Context.AnimBlueprint || !CanEnter)
		{
			OutError = TEXT("The node has no rule graph with a result in an Animation Blueprint.");
			return false;
		}
		if (Context.Transition && Context.Transition->bSharedRules)
		{
			OutError = FString::Printf(TEXT("This transition shares its rule ('%s') with other transitions; edit an unshared transition."),
				*Context.Transition->SharedRulesName);
			return false;
		}
		if (Rule.bAutomatic && Rule.AnyOf.Num() > 0)
		{
			OutError = TEXT("bAutomatic replaces the conditions: leave AnyOf empty.");
			return false;
		}
		if (Rule.bAutomatic && !Context.Transition)
		{
			OutError = TEXT("Only transitions have an automatic rule.");
			return false;
		}
		Context.SourceState = Context.Transition ? Context.Transition->GetPreviousState() : nullptr;
		Context.Machine = Cast<UAnimationStateMachineGraph>(TransitionOrConduit->GetGraph())
			? Cast<UAnimationStateMachineGraph>(TransitionOrConduit->GetGraph())->OwnerAnimGraphNode.Get() : nullptr;
		Context.SelfClass = Context.AnimBlueprint->SkeletonGeneratedClass ? Context.AnimBlueprint->SkeletonGeneratedClass.Get()
			: Context.AnimBlueprint->GeneratedClass.Get();
		if (!Context.SelfClass)
		{
			OutError = TEXT("The Animation Blueprint has no generated class: compile it first.");
			return false;
		}

		// Resolve everything before the first edit.
		TArray<TArray<FResolvedCondition>> Groups;
		for (const FGSTN_RuleGroup& Group : Rule.AnyOf)
		{
			if (Group.AllOf.Num() == 0)
			{
				OutError = TEXT("A group in AnyOf has no conditions.");
				return false;
			}
			TArray<FResolvedCondition>& Resolved = Groups.AddDefaulted_GetRef();
			for (const FGSTN_RuleCondition& Condition : Group.AllOf)
			{
				if (!Resolve(Context, Condition, Resolved.AddDefaulted_GetRef(), OutError))
				{
					return false;
				}
			}
		}

		const FScopedTransaction Transaction(LOCTEXT("SetTransitionRule", "Set Transition Rule"));
		Context.RuleGraph->Modify();
		TArray<UEdGraphNode*> OldNodes;
		for (UEdGraphNode* Node : Context.RuleGraph->Nodes)
		{
			if (Node && Node != Result)
			{
				OldNodes.Add(Node);
			}
		}
		for (UEdGraphNode* Node : OldNodes)
		{
			FBlueprintEditorUtils::RemoveNode(Context.AnimBlueprint, Node, true);
		}
		Result->Modify();
		CanEnter->BreakAllPinLinks();
		CanEnter->DefaultValue = TEXT("false");

		if (Context.Transition)
		{
			Context.Transition->Modify();
			Context.Transition->bAutomaticRuleBasedOnSequencePlayerInState = Rule.bAutomatic;
			if (Rule.bAutomatic)
			{
				Context.Transition->AutomaticRuleTriggerTime = Rule.AutomaticTriggerTime;
			}
		}

		FBuilder Builder(Context);
		const int32 Rows = [&Groups]() { int32 Count = 0; for (const TArray<FResolvedCondition>& Group : Groups) { Count += Group.Num(); } return Count; }();
		const FIntPoint Origin(Result->NodePosX - 1400, Result->NodePosY - (Rows * 150) / 2);
		TArray<UEdGraphPin*> GroupPins;
		int32 Row = 0;
		for (const TArray<FResolvedCondition>& Group : Groups)
		{
			TArray<UEdGraphPin*> ConditionPins;
			const int32 FirstRow = Row;
			for (const FResolvedCondition& Resolved : Group)
			{
				UEdGraphPin* Pin = Builder.Condition(Resolved, FIntPoint(Origin.X, Origin.Y + Row * 150));
				++Row;
				if (!Pin)
				{
					OutError = Builder.Error;
					return false;
				}
				ConditionPins.Add(Pin);
			}
			UEdGraphPin* GroupPin = Builder.Chain(ConditionPins, TEXT("BooleanAND"), FIntPoint(Origin.X + 700, Origin.Y + FirstRow * 150));
			if (!GroupPin)
			{
				OutError = Builder.Error;
				return false;
			}
			GroupPins.Add(GroupPin);
		}
		if (GroupPins.Num() > 0)
		{
			UEdGraphPin* RulePin = Builder.Chain(GroupPins, TEXT("BooleanOR"), FIntPoint(Origin.X + 1050, Origin.Y));
			if (!RulePin || !Builder.Link(RulePin, CanEnter))
			{
				OutError = Builder.Error;
				return false;
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.AnimBlueprint);
		OutExpression = Rule.bAutomatic ? FString(TEXT("automatic")) : DescribeRule(Context.RuleGraph);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
