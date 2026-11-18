#include "GSTN_AnimBlueprintToolset.h"

#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimStateConduitNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_K2.h"
#include "Engine/EngineTypes.h"
#include "Factories/AnimBlueprintFactory.h"
#include "IAssetTools.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Templates/UniquePtr.h"
#include "Tests/TestAssetSandbox.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTN::AnimToolsetTests
{
	FGSTN_RuleCondition Condition(EGSTN_RuleOperand Operand, const FString& Name, EGSTN_RuleCompare Compare, const FString& Value = FString())
	{
		FGSTN_RuleCondition Result;
		Result.Operand = Operand;
		Result.Name = Name;
		Result.Compare = Compare;
		Result.Value = Value;
		return Result;
	}

	FGSTN_TransitionRule AnyOf(std::initializer_list<TArray<FGSTN_RuleCondition>> Groups)
	{
		FGSTN_TransitionRule Rule;
		for (const TArray<FGSTN_RuleCondition>& Group : Groups)
		{
			Rule.AnyOf.AddDefaulted_GetRef().AllOf = Group;
		}
		return Rule;
	}
}

using namespace GSTN::AnimToolsetTests;

BEGIN_DEFINE_SPEC(FGSTN_AnimBlueprintToolsetSpec, "GSTN.Toolsets.AnimBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestAssetSandbox> Sandbox;
	UAnimBlueprint* AnimBlueprint = nullptr;
	UEdGraph* AnimGraph = nullptr;
	UAnimGraphNode_StateMachineBase* Machine = nullptr;
	UAnimStateNode* Idle = nullptr;
	UAnimStateNode* Run = nullptr;

	FString CaptureError(TFunction<void()> Body)
	{
		UE::ToolsetRegistry::FToolCallExceptionHandler Handler;
		Handler.CaptureErrorsIn(MoveTemp(Body));
		return Handler.GetException();
	}

	/** A template Animation Blueprint (no skeleton) with a bool, a float and an enum variable. */
	UAnimBlueprint* CreateAnimBlueprint()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
		Factory->ParentClass = UAnimInstance::StaticClass();
		Factory->bTemplate = true;
		Factory->BlueprintType = BPTYPE_Normal;
		UAnimBlueprint* Created = Cast<UAnimBlueprint>(AssetTools.CreateAsset(FTestAssetSandbox::UniqueName(TEXT("ABP_GSTNToolsetTest")),
			Sandbox->Folder(), UAnimBlueprint::StaticClass(), Factory));

		FEdGraphPinType BoolType;
		BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(Created, TEXT("bTestFlag"), BoolType);
		FEdGraphPinType FloatType;
		FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
		FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		FBlueprintEditorUtils::AddMemberVariable(Created, TEXT("Speed"), FloatType);
		FEdGraphPinType EnumType;
		EnumType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		EnumType.PinSubCategoryObject = StaticEnum<EMovementMode>();
		FBlueprintEditorUtils::AddMemberVariable(Created, TEXT("Mode"), EnumType);
		FKismetEditorUtilities::CompileBlueprint(Created);
		return Created;
	}

	bool CompilesWithoutErrors(UAnimBlueprint* Blueprint)
	{
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			if (Message->GetSeverity() == EMessageSeverity::Error)
			{
				AddError(FString::Printf(TEXT("Compile error: %s"), *Message->ToText().ToString()));
			}
		}
		return Results.NumErrors == 0 && Blueprint->Status != BS_Error;
	}

	UAnimSequence* AnySequence()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.PackagePath.ToString().StartsWith(TEXT("/Game/")))
			{
				return Cast<UAnimSequence>(Asset.GetAsset());
			}
		}
		return nullptr;
	}

END_DEFINE_SPEC(FGSTN_AnimBlueprintToolsetSpec)

void FGSTN_AnimBlueprintToolsetSpec::Define()
{
	BeforeEach([this]()
	{
		Sandbox = MakeUnique<FTestAssetSandbox>(TEXT("GSTNAnimToolsets"));
		AnimBlueprint = CreateAnimBlueprint();
		for (const FGSTN_AnimGraphInfo& Info : UGSTN_AnimBlueprintToolset::ListAnimGraphs(AnimBlueprint))
		{
			if (Info.Kind == EGSTN_AnimGraphKind::AnimGraph)
			{
				AnimGraph = Info.Graph;
			}
		}
		Machine = UGSTN_AnimBlueprintToolset::AddStateMachine(AnimGraph, TEXT("Locomotion"), FIntPoint(0, 0));
		Idle = Cast<UAnimStateNode>(UGSTN_AnimBlueprintToolset::AddState(Machine, TEXT("Idle"), EGSTN_StateKind::State, {}));
		Run = Cast<UAnimStateNode>(UGSTN_AnimBlueprintToolset::AddState(Machine, TEXT("Run"), EGSTN_StateKind::State, {}));
	});

	AfterEach([this]()
	{
		AnimBlueprint = nullptr;
		AnimGraph = nullptr;
		Machine = nullptr;
		Idle = Run = nullptr;
		Sandbox.Reset();
	});

	Describe("ListAnimGraphs", [this]()
	{
		It("lists the AnimGraph, the machine and its states with their paths", [this]()
		{
			TestNotNull(TEXT("AnimGraph"), AnimGraph);
			TMap<FString, EGSTN_AnimGraphKind> Kinds;
			for (const FGSTN_AnimGraphInfo& Info : UGSTN_AnimBlueprintToolset::ListAnimGraphs(AnimBlueprint))
			{
				Kinds.Add(Info.Path, Info.Kind);
			}
			TestTrue(TEXT("machine graph"), Kinds.FindRef(TEXT("AnimGraph/Locomotion")) == EGSTN_AnimGraphKind::StateMachine);
			TestTrue(TEXT("state graph"), Kinds.FindRef(TEXT("AnimGraph/Locomotion/Idle")) == EGSTN_AnimGraphKind::State);
		});

		It("raises for a null blueprint", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([]() { UGSTN_AnimBlueprintToolset::ListAnimGraphs(nullptr); }).Contains(TEXT("null")));
		});
	});

	Describe("FindStateMachines", [this]()
	{
		It("finds a machine nested in a state, by name, case-insensitive", [this]()
		{
			UAnimGraphNode_StateMachineBase* Inner = UGSTN_AnimBlueprintToolset::AddStateMachine(Run->GetBoundGraph(), TEXT("Run Inner"), {});
			const TArray<FGSTN_StateMachineInfo> Found = UGSTN_AnimBlueprintToolset::FindStateMachines(AnimBlueprint, TEXT("run inner"));
			if (TestEqual(TEXT("one match"), Found.Num(), 1))
			{
				TestTrue(TEXT("node"), Found[0].Machine == Inner);
				TestEqual(TEXT("path"), Found[0].GraphPath, FString(TEXT("AnimGraph/Locomotion/Run")));
			}
			TestEqual(TEXT("all"), UGSTN_AnimBlueprintToolset::FindStateMachines(AnimBlueprint).Num(), 2);
		});

		It("returns nothing for an unknown name", [this]()
		{
			TestEqual(TEXT("none"), UGSTN_AnimBlueprintToolset::FindStateMachines(AnimBlueprint, TEXT("Nope")).Num(), 0);
		});

		It("works inside the SkidLayer of a copy of ABP_GSTN_Locomotion", [this]()
		{
			UAnimBlueprint* Original = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion.ABP_GSTN_Locomotion"));
			if (!Original)
			{
				AddInfo(TEXT("ABP_GSTN_Locomotion not in this project: skipped."));
				return;
			}
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
			UAnimBlueprint* Copy = Cast<UAnimBlueprint>(AssetTools.DuplicateAsset(FTestAssetSandbox::UniqueName(TEXT("ABP_LocoCopy")), Sandbox->Folder(), Original));
			UEdGraph* SkidLayer = nullptr;
			for (const FGSTN_AnimGraphInfo& Graph : UGSTN_AnimBlueprintToolset::ListAnimGraphs(Copy))
			{
				if (Graph.Path == TEXT("SkidLayer"))
				{
					TestTrue(TEXT("SkidLayer is an anim layer"), Graph.Kind == EGSTN_AnimGraphKind::AnimLayer);
					SkidLayer = Graph.Graph;
				}
			}
			if (!TestNotNull(TEXT("SkidLayer"), SkidLayer))
			{
				return;
			}
			// The committed asset may or may not hold the owner's Skid machine yet: add one to the copy so the layer path is always covered.
			const FString Name = UGSTN_AnimBlueprintToolset::FindStateMachines(Copy, TEXT("Skid States")).Num() > 0 ? TEXT("Skid States Test") : TEXT("Skid States");
			UAnimGraphNode_StateMachineBase* SkidMachine = UGSTN_AnimBlueprintToolset::AddStateMachine(SkidLayer, Name, {});
			UAnimStateNodeBase* Loop = UGSTN_AnimBlueprintToolset::AddState(SkidMachine, TEXT("Skid Loop"), EGSTN_StateKind::State, {});
			UAnimStateNodeBase* Exit = UGSTN_AnimBlueprintToolset::AddState(SkidMachine, TEXT("Skid Exit"), EGSTN_StateKind::State, {});
			FGSTN_TransitionRule Rule;
			Rule.bAutomatic = true;
			UGSTN_AnimBlueprintToolset::SetTransitionRule(UGSTN_AnimBlueprintToolset::AddTransition(Loop, Exit), Rule);

			const TArray<FGSTN_StateMachineInfo> Found = UGSTN_AnimBlueprintToolset::FindStateMachines(Copy, Name);
			if (TestEqual(TEXT("found in the layer"), Found.Num(), 1))
			{
				TestEqual(TEXT("path"), Found[0].GraphPath, FString(TEXT("SkidLayer")));
				TestEqual(TEXT("states"), UGSTN_AnimBlueprintToolset::GetStateMachine(Found[0].Machine).States.Num(), 2);
			}
			TestTrue(TEXT("copy compiles"), CompilesWithoutErrors(Copy));
		});
	});

	Describe("AddStateMachine", [this]()
	{
		It("raises on a duplicate name", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::AddStateMachine(AnimGraph, TEXT("locomotion"), {}); }).Contains(TEXT("already exists")));
		});

		It("raises on a graph that holds no poses", [this]()
		{
			UEdGraph* EventGraph = AnimBlueprint->UbergraphPages.Num() > 0 ? AnimBlueprint->UbergraphPages[0].Get() : nullptr;
			TestTrue(TEXT("error"), CaptureError([EventGraph]() { UGSTN_AnimBlueprintToolset::AddStateMachine(EventGraph, TEXT("X"), {}); }).Contains(TEXT("pose graph")));
		});
	});

	Describe("States", [this]()
	{
		It("adds named states, the first one becomes the entry", [this]()
		{
			const FGSTN_StateMachineDetail Detail = UGSTN_AnimBlueprintToolset::GetStateMachine(Machine);
			TestEqual(TEXT("states"), Detail.States.Num(), 2);
			TestEqual(TEXT("entry"), Detail.EntryState, FString(TEXT("Idle")));
			TestTrue(TEXT("find"), UGSTN_AnimBlueprintToolset::FindState(Machine, TEXT("run")) == Run);
		});

		It("adds a conduit", [this]()
		{
			UAnimStateNodeBase* Conduit = UGSTN_AnimBlueprintToolset::AddState(Machine, TEXT("Gate"), EGSTN_StateKind::Conduit, {});
			TestTrue(TEXT("conduit"), Conduit && Conduit->IsA<UAnimStateConduitNode>());
			TestEqual(TEXT("name"), Conduit ? Conduit->GetStateName() : FString(), FString(TEXT("Gate")));
		});

		It("raises on a duplicate state name", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::AddState(Machine, TEXT("IDLE"), EGSTN_StateKind::State, {}); }).Contains(TEXT("already has")));
		});

		It("raises when a state is not found", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::FindState(Machine, TEXT("Walk")); }).Contains(TEXT("No state 'Walk'")));
		});

		It("renames a state and refuses a used name", [this]()
		{
			UGSTN_AnimBlueprintToolset::RenameState(Run, TEXT("Sprint"));
			TestEqual(TEXT("renamed"), Run->GetStateName(), FString(TEXT("Sprint")));
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::RenameState(Run, TEXT("Idle")); }).Contains(TEXT("already used")));
		});

		It("sets the entry state", [this]()
		{
			UGSTN_AnimBlueprintToolset::SetEntryState(Run);
			TestEqual(TEXT("entry"), UGSTN_AnimBlueprintToolset::GetStateMachine(Machine).EntryState, FString(TEXT("Run")));
		});

		It("removes a state with its transitions", [this]()
		{
			UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			UGSTN_AnimBlueprintToolset::AddTransition(Run, Idle);
			UGSTN_AnimBlueprintToolset::RemoveState(Run);
			const FGSTN_StateMachineDetail Detail = UGSTN_AnimBlueprintToolset::GetStateMachine(Machine);
			TestEqual(TEXT("states"), Detail.States.Num(), 1);
			TestEqual(TEXT("transitions"), Detail.Transitions.Num(), 0);
		});
	});

	Describe("SetStateAnimation", [this]()
	{
		It("puts an asset player on the state result and replaces it", [this]()
		{
			UAnimSequence* Sequence = AnySequence();
			if (!Sequence)
			{
				AddInfo(TEXT("No AnimSequence under /Game: skipped."));
				return;
			}
			UGSTN_AnimBlueprintToolset::SetStateAnimation(Idle, Sequence);
			UAnimGraphNode_AssetPlayerBase* Player = UGSTN_AnimBlueprintToolset::SetStateAnimation(Idle, Sequence);
			TestTrue(TEXT("asset"), Player && Player->GetAnimationAsset() == Sequence);
			const FGSTN_StateMachineDetail Detail = UGSTN_AnimBlueprintToolset::GetStateMachine(Machine);
			const FGSTN_StateInfo* IdleInfo = Detail.States.FindByPredicate([](const FGSTN_StateInfo& Info) { return Info.Name == TEXT("Idle"); });
			TestTrue(TEXT("one animation"), IdleInfo && IdleInfo->Animations.Num() == 1);
			int32 Players = 0;
			for (UEdGraphNode* Node : Idle->GetBoundGraph()->Nodes)
			{
				Players += Node->IsA<UAnimGraphNode_AssetPlayerBase>() ? 1 : 0;
			}
			TestEqual(TEXT("replaced, not stacked"), Players, 1);
			TestTrue(TEXT("compiles"), CompilesWithoutErrors(AnimBlueprint));
		});

		It("raises for a null animation", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::SetStateAnimation(Idle, nullptr); }).Contains(TEXT("null")));
		});
	});

	Describe("Transitions", [this]()
	{
		It("adds a transition with a false rule", [this]()
		{
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			const FGSTN_TransitionInfo Info = UGSTN_AnimBlueprintToolset::GetTransition(Transition);
			TestEqual(TEXT("from"), Info.From, FString(TEXT("Idle")));
			TestEqual(TEXT("to"), Info.To, FString(TEXT("Run")));
			TestEqual(TEXT("rule"), Info.Rule, FString(TEXT("false")));
		});

		It("raises on a duplicate and across machines", [this]()
		{
			UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			TestTrue(TEXT("duplicate"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run); }).Contains(TEXT("already exists")));
			UAnimGraphNode_StateMachineBase* Other = UGSTN_AnimBlueprintToolset::AddStateMachine(AnimGraph, TEXT("Other"), {});
			UAnimStateNodeBase* Far = UGSTN_AnimBlueprintToolset::AddState(Other, TEXT("Far"), EGSTN_StateKind::State, {});
			TestTrue(TEXT("other machine"), CaptureError([this, Far]() { UGSTN_AnimBlueprintToolset::AddTransition(Idle, Far); }).Contains(TEXT("different state machines")));
		});

		It("removes a transition", [this]()
		{
			UGSTN_AnimBlueprintToolset::RemoveTransition(UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run));
			TestEqual(TEXT("none"), UGSTN_AnimBlueprintToolset::GetStateMachine(Machine).Transitions.Num(), 0);
		});
	});

	Describe("SetTransitionRule", [this]()
	{
		It("writes (bTestFlag && Speed > 300) || !IsAnyMontagePlaying() and compiles", [this]()
		{
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			const FGSTN_TransitionRule Rule = AnyOf({
				{Condition(EGSTN_RuleOperand::Variable, TEXT("bTestFlag"), EGSTN_RuleCompare::IsTrue),
				 Condition(EGSTN_RuleOperand::Variable, TEXT("Speed"), EGSTN_RuleCompare::Greater, TEXT("300"))},
				{Condition(EGSTN_RuleOperand::Function, TEXT("Is Any Montage Playing"), EGSTN_RuleCompare::IsFalse)}});
			const FString Expression = UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition, Rule);
			TestTrue(FString::Printf(TEXT("expression '%s'"), *Expression), Expression.StartsWith(TEXT("((bTestFlag && (Speed > 300"))
				&& Expression.EndsWith(TEXT(")) || !IsAnyMontagePlaying())")));
			TestEqual(TEXT("read back"), UGSTN_AnimBlueprintToolset::GetTransition(Transition).Rule, Expression);
			TestTrue(TEXT("compiles"), CompilesWithoutErrors(AnimBlueprint));
		});

		It("compares an enum, calls a static function with arguments and reads a state machine getter", [this]()
		{
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			FGSTN_RuleCondition Abs = Condition(EGSTN_RuleOperand::Function, TEXT("Abs"), EGSTN_RuleCompare::GreaterEqual, TEXT("1"));
			Abs.FunctionClass = UKismetMathLibrary::StaticClass();
			Abs.Arguments.Add(TEXT("A"), TEXT("-3"));
			const FGSTN_TransitionRule Rule = AnyOf({{
				Condition(EGSTN_RuleOperand::Variable, TEXT("Mode"), EGSTN_RuleCompare::Equal, TEXT("MOVE_Walking")),
				Abs,
				Condition(EGSTN_RuleOperand::AnimGetter, TEXT("GetRelevantAnimTimeRemaining"), EGSTN_RuleCompare::Less, TEXT("0.2"))}});
			const FString Expression = UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition, Rule);
			TestTrue(TEXT("enum"), Expression.Contains(TEXT("(Mode == MOVE_Walking)")));
			TestTrue(TEXT("function"), Expression.Contains(TEXT("Abs(A=-3")));
			TestTrue(TEXT("getter"), Expression.Contains(TEXT("< 0.2")));
			TestTrue(TEXT("compiles"), CompilesWithoutErrors(AnimBlueprint));
		});

		It("reads Time Remaining (ratio) of the source state's asset player", [this]()
		{
			UAnimSequence* Sequence = AnySequence();
			if (!Sequence)
			{
				AddInfo(TEXT("No AnimSequence under /Game: skipped."));
				return;
			}
			UGSTN_AnimBlueprintToolset::SetStateAnimation(Idle, Sequence);
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			const FString Expression = UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition,
				AnyOf({{Condition(EGSTN_RuleOperand::AnimGetter, TEXT("Time Remaining (ratio)"), EGSTN_RuleCompare::Less, TEXT("0.1"))}}));
			TestTrue(TEXT("getter on the player"), Expression.Contains(Sequence->GetName()));
			TestTrue(TEXT("compiles"), CompilesWithoutErrors(AnimBlueprint));
		});

		It("switches to the automatic rule", [this]()
		{
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			FGSTN_TransitionRule Rule;
			Rule.bAutomatic = true;
			Rule.AutomaticTriggerTime = 0.15f;
			TestEqual(TEXT("expression"), UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition, Rule), FString(TEXT("automatic")));
			TestTrue(TEXT("flag"), Transition->bAutomaticRuleBasedOnSequencePlayerInState);
			TestEqual(TEXT("trigger"), Transition->AutomaticRuleTriggerTime, 0.15f);
		});

		It("writes a conduit rule", [this]()
		{
			UAnimStateNodeBase* Conduit = UGSTN_AnimBlueprintToolset::AddState(Machine, TEXT("Gate"), EGSTN_StateKind::Conduit, {});
			const FString Expression = UGSTN_AnimBlueprintToolset::SetTransitionRule(Conduit,
				AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("bTestFlag"), EGSTN_RuleCompare::IsTrue)}}));
			TestEqual(TEXT("expression"), Expression, FString(TEXT("bTestFlag")));
		});

		It("raises before editing when a condition is wrong", [this]()
		{
			UAnimStateTransitionNode* Transition = UGSTN_AnimBlueprintToolset::AddTransition(Idle, Run);
			UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition, AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("bTestFlag"), EGSTN_RuleCompare::IsTrue)}}));
			const int32 NodesBefore = Transition->BoundGraph->Nodes.Num();

			auto Expect = [this, Transition](const FGSTN_TransitionRule& Rule, const TCHAR* Needle)
			{
				const FString Error = CaptureError([Transition, &Rule]() { UGSTN_AnimBlueprintToolset::SetTransitionRule(Transition, Rule); });
				TestTrue(FString::Printf(TEXT("'%s' in '%s'"), Needle, *Error), Error.Contains(Needle));
			};
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("Nope"), EGSTN_RuleCompare::IsTrue)}}), TEXT("not a Blueprint-visible variable"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("bTestFlag"), EGSTN_RuleCompare::Less, TEXT("1"))}}), TEXT("IsTrue, IsFalse"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("Speed"), EGSTN_RuleCompare::Greater, TEXT("fast"))}}), TEXT("numeric Value"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("Mode"), EGSTN_RuleCompare::Equal, TEXT("MOVE_Teleport"))}}), TEXT("not an entry"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::Function, TEXT("Nope"), EGSTN_RuleCompare::IsTrue)}}), TEXT("not a Blueprint function"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::AnimGetter, TEXT("Nope"), EGSTN_RuleCompare::Less, TEXT("1"))}}), TEXT("not an anim getter"));
			Expect(AnyOf({{Condition(EGSTN_RuleOperand::AnimGetter, TEXT("Time Remaining (ratio)"), EGSTN_RuleCompare::Less, TEXT("1"))}}), TEXT("no asset player"));
			FGSTN_TransitionRule Mixed = AnyOf({{Condition(EGSTN_RuleOperand::Variable, TEXT("bTestFlag"), EGSTN_RuleCompare::IsTrue)}});
			Mixed.bAutomatic = true;
			Expect(Mixed, TEXT("leave AnyOf empty"));

			TestEqual(TEXT("graph untouched"), Transition->BoundGraph->Nodes.Num(), NodesBefore);
			TestEqual(TEXT("rule kept"), UGSTN_AnimBlueprintToolset::GetTransition(Transition).Rule, FString(TEXT("bTestFlag")));
		});
	});

	Describe("AddAnimNode", [this]()
	{
		It("adds a Slot node to the AnimGraph", [this]()
		{
			UAnimGraphNode_Base* Node = UGSTN_AnimBlueprintToolset::AddAnimNode(AnimGraph, UAnimGraphNode_Slot::StaticClass(), FIntPoint(-400, 0));
			TestTrue(TEXT("slot"), Node && Node->IsA<UAnimGraphNode_Slot>() && Node->GetGraph() == AnimGraph);
		});

		It("raises for a class that is not an anim node", [this]()
		{
			TestTrue(TEXT("error"), CaptureError([this]() { UGSTN_AnimBlueprintToolset::AddAnimNode(AnimGraph, UAnimSequence::StaticClass(), {}); }).Contains(TEXT("AnimGraphNode_Base")));
		});
	});
}

#endif
