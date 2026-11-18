#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "GSTN_AnimBlueprintToolset.generated.h"

class UAnimationAsset;
class UAnimBlueprint;
class UAnimGraphNode_AssetPlayerBase;
class UAnimGraphNode_Base;
class UAnimGraphNode_StateMachineBase;
class UAnimStateNode;
class UAnimStateNodeBase;
class UAnimStateTransitionNode;
class UEdGraph;
class UEdGraphNode;

/** What a graph of an Animation Blueprint is. */
UENUM(BlueprintType)
enum class EGSTN_AnimGraphKind : uint8
{
	/** The main AnimGraph. */
	AnimGraph,
	/** An anim layer (own or from an implemented layer interface). */
	AnimLayer,
	/** The inside of a state machine: states and transitions. */
	StateMachine,
	/** The pose graph of a state. */
	State,
	/** The rule graph of a transition. */
	TransitionRule,
	/** The rule graph of a conduit. */
	Conduit,
	/** The custom blend graph of a transition. */
	CustomBlend,
	EventGraph,
	Function,
	Other,
};

/** A graph of an Animation Blueprint, at any depth. */
USTRUCT(BlueprintType)
struct FGSTN_AnimGraphInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UEdGraph> Graph = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	EGSTN_AnimGraphKind Kind = EGSTN_AnimGraphKind::Other;

	/** Graph names from the top-level graph down, joined by '/', e.g. "SkidLayer/Skid States/Skid Loop". A transition reads "From -> To". */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Path;

	/** The node that owns the graph (state machine, state, transition, conduit), or null for a top-level graph. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UEdGraphNode> OwnerNode = nullptr;
};

/** A state machine node and where it is. */
USTRUCT(BlueprintType)
struct FGSTN_StateMachineInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UAnimGraphNode_StateMachineBase> Machine = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Name;

	/** Path of the graph that contains the machine node, e.g. "SkidLayer" or "AnimGraph/Locomotion/Jump". */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString GraphPath;
};

/** The kind of a node inside a state machine. */
UENUM(BlueprintType)
enum class EGSTN_StateKind : uint8
{
	State,
	Conduit,
	Alias,
};

/** A state, conduit or alias of a state machine. */
USTRUCT(BlueprintType)
struct FGSTN_StateInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UAnimStateNodeBase> State = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	EGSTN_StateKind Kind = EGSTN_StateKind::State;

	/** The pose graph of a state or the rule graph of a conduit; null for an alias. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UEdGraph> Graph = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FIntPoint Position = FIntPoint::ZeroValue;

	/** Assets played by the asset players inside the state, nested graphs included. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TArray<TObjectPtr<UAnimationAsset>> Animations;
};

/** A transition of a state machine. */
USTRUCT(BlueprintType)
struct FGSTN_TransitionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UAnimStateTransitionNode> Transition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString From;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString To;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UEdGraph> RuleGraph = nullptr;

	/**
	 * The rule as a readable expression, e.g. "(bIsSkidding && (Speed > 300))". "automatic" when the automatic rule is on,
	 * "false" when nothing drives the result.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Rule;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset", meta = (ForceUnits = "s"))
	float CrossfadeDuration = 0.f;

	/** Lower values are checked first. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	int32 PriorityOrder = 1;

	/** True when the rule graph is shared with other transitions. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	bool bSharedRules = false;
};

/** Everything inside a state machine. */
USTRUCT(BlueprintType)
struct FGSTN_StateMachineDetail
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UAnimGraphNode_StateMachineBase> Machine = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Name;

	/** The graph with the states and transitions. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UEdGraph> Graph = nullptr;

	/** Name of the state the entry points to, empty if none. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString EntryState;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TArray<FGSTN_StateInfo> States;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TArray<FGSTN_TransitionInfo> Transitions;
};

/** Where the left-hand value of a rule condition comes from. */
UENUM(BlueprintType)
enum class EGSTN_RuleOperand : uint8
{
	/** A variable of the Animation Blueprint or of its parent class. */
	Variable,
	/** A pure function: of the Animation Blueprint (BP or parent C++), or a static function of FunctionClass. */
	Function,
	/**
	 * A state machine getter of UAnimInstance, bound to this transition: e.g. GetRelevantAnimTimeRemaining,
	 * GetRelevantAnimTimeRemainingFraction, GetInstanceAssetPlayerTimeFromEnd ("Time Remaining"),
	 * GetInstanceAssetPlayerTimeFromEndFraction ("Time Remaining (ratio)"), GetInstanceCurrentStateElapsedTime.
	 */
	AnimGetter,
};

/** How a condition tests its value. */
UENUM(BlueprintType)
enum class EGSTN_RuleCompare : uint8
{
	/** Bool value is true. */
	IsTrue,
	/** Bool value is false. */
	IsFalse,
	Equal,
	NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
};

/** One test in a transition rule: a value, compared against a literal. */
USTRUCT(BlueprintType)
struct FGSTN_RuleCondition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	EGSTN_RuleOperand Operand = EGSTN_RuleOperand::Variable;

	/** Variable name, or function / getter name. A function also matches its display name ("Is Skidding" finds IsSkidding). */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Name;

	/** Function only: the class declaring a static function (e.g. KismetMathLibrary). Null for a function of the Animation Blueprint. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TObjectPtr<UClass> FunctionClass = nullptr;

	/** Literal values of the function's or getter's input pins, by pin name. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TMap<FString, FString> Arguments;

	/**
	 * AnimGetter that reads an asset player: the player in the transition's source state, matched against its node title or asset name.
	 * Empty takes the first one.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString AssetPlayer;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	EGSTN_RuleCompare Compare = EGSTN_RuleCompare::IsTrue;

	/** Right-hand literal for Equal to GreaterEqual: a number, an enum entry name, a name, or true/false for a bool. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	FString Value;
};

/** Conditions that must all hold. */
USTRUCT(BlueprintType)
struct FGSTN_RuleGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TArray<FGSTN_RuleCondition> AllOf;
};

/** A transition rule in OR-of-ANDs form: the transition is taken when every condition of at least one group holds. */
USTRUCT(BlueprintType)
struct FGSTN_TransitionRule
{
	GENERATED_BODY()

	/** Empty means the transition is never taken, unless bAutomatic. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	TArray<FGSTN_RuleGroup> AnyOf;

	/** Use the automatic rule instead: leave when the most relevant asset player of the source state is about to end. AnyOf must be empty. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset")
	bool bAutomatic = false;

	/** Automatic rule only: seconds before the end at which it fires. Negative fires one crossfade duration before the end. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|AnimToolset", meta = (ForceUnits = "s"))
	float AutomaticTriggerTime = -1.f;
};

/**
 * Edits Animation Blueprints: finds graphs and state machines at any depth (anim layers, states, nested machines), adds and removes
 * states, conduits, transitions and AnimGraph nodes, sets what a state plays, and writes transition rules made of variables, pure
 * functions and state machine getters combined with AND/OR. Node pins, connections, compiling and plain properties (e.g. a
 * transition's crossfade) are handled by BlueprintToolset and ObjectToolset on the graphs and nodes returned here.
 */
UCLASS(BlueprintType, MinimalAPI)
class UGSTN_AnimBlueprintToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lists every graph of an Animation Blueprint, nested graphs included.
	 * @param AnimBlueprint The Animation Blueprint to read.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|AnimToolset")
	static TArray<FGSTN_AnimGraphInfo> ListAnimGraphs(UAnimBlueprint* AnimBlueprint);

	/**
	 * Finds state machines anywhere in an Animation Blueprint, anim layers and nested machines included.
	 * @param AnimBlueprint The Animation Blueprint to search.
	 * @param Name Machine name, case-insensitive. Empty returns all.
	 * @return The matches, or an empty array.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|AnimToolset")
	static TArray<FGSTN_StateMachineInfo> FindStateMachines(UAnimBlueprint* AnimBlueprint, const FString& Name = TEXT(""));

	/**
	 * Returns the states, conduits, aliases, transitions (with their rules) and entry of a state machine.
	 * @param Machine The state machine node.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|AnimToolset")
	static FGSTN_StateMachineDetail GetStateMachine(UAnimGraphNode_StateMachineBase* Machine);

	/**
	 * Adds an empty state machine node to a pose graph (AnimGraph, anim layer or state). Connect its pose output with BlueprintToolset.
	 * @param Graph The pose graph to add it to.
	 * @param Name Machine name, unique in the Blueprint.
	 * @param Position Node position; next to the existing nodes if unset.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimGraphNode_StateMachineBase* AddStateMachine(UEdGraph* Graph, const FString& Name, TOptional<FIntPoint> Position);

	/**
	 * Adds a state or a conduit to a state machine. The first state added to an empty machine becomes the entry state.
	 * @param Machine The state machine node.
	 * @param Name Unique name in the machine.
	 * @param Kind State or Conduit.
	 * @param Position Node position; next to the existing states if unset.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimStateNodeBase* AddState(UAnimGraphNode_StateMachineBase* Machine, const FString& Name,
		EGSTN_StateKind Kind, TOptional<FIntPoint> Position);

	/**
	 * Adds a state alias: one node standing for several states, so a single transition out of it replaces one transition per state
	 * (owner 2026-11-18: the Skid entry from the locomotion and idle states, without a tangle of arrows).
	 * @param Machine The state machine node.
	 * @param Name Unique name in the machine.
	 * @param AliasedStateNames States (or conduits) the alias stands for, by name; empty with bGlobalAlias = every state.
	 * @param bGlobalAlias True = the alias stands for every state of the machine.
	 * @param Position Node position; next to the existing states if unset.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimStateNodeBase* AddStateAlias(UAnimGraphNode_StateMachineBase* Machine, const FString& Name,
		const TArray<FString>& AliasedStateNames, bool bGlobalAlias, TOptional<FIntPoint> Position);

	/**
	 * Returns the state, conduit or alias of a machine with the given name, case-insensitive.
	 * @param Machine The state machine node.
	 * @param Name The state name.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|AnimToolset")
	static UAnimStateNodeBase* FindState(UAnimGraphNode_StateMachineBase* Machine, const FString& Name);

	/**
	 * Renames a state, conduit or state machine.
	 * @param Node A state, conduit or state machine node.
	 * @param NewName Unique name.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static void RenameState(UEdGraphNode* Node, const FString& NewName);

	/**
	 * Removes a state, conduit or alias and every transition into or out of it.
	 * @param State The node to remove.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static void RemoveState(UAnimStateNodeBase* State);

	/**
	 * Makes a state the entry state of its machine.
	 * @param State A state (not a conduit or alias).
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static void SetEntryState(UAnimStateNode* State);

	/**
	 * Makes a state play an animation: replaces the asset player that feeds the state result, or adds one. A sequence gets a
	 * Sequence Player, a blend space a Blend Space Player, and so on.
	 * @param State The state.
	 * @param Animation The asset to play.
	 * @return The asset player node.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimGraphNode_AssetPlayerBase* SetStateAnimation(UAnimStateNode* State, UAnimationAsset* Animation);

	/**
	 * Adds a transition between two nodes of the same machine. Its rule is false until set with SetTransitionRule.
	 * @param From Source state, conduit or alias.
	 * @param To Target state or conduit.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimStateTransitionNode* AddTransition(UAnimStateNodeBase* From, UAnimStateNodeBase* To);

	/**
	 * Returns a transition's ends, rule and timing.
	 * @param Transition The transition node.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|AnimToolset")
	static FGSTN_TransitionInfo GetTransition(UAnimStateTransitionNode* Transition);

	/**
	 * Removes a transition.
	 * @param Transition The transition node.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static void RemoveTransition(UAnimStateTransitionNode* Transition);

	/**
	 * Replaces the rule of a transition (or the rule of a conduit). Everything is checked before the graph changes.
	 * @param TransitionOrConduit A transition node or a conduit node.
	 * @param Rule The new rule.
	 * @return The rule as a readable expression.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static FString SetTransitionRule(UEdGraphNode* TransitionOrConduit, const FGSTN_TransitionRule& Rule);

	/**
	 * Adds an AnimGraph node (Sequence Player, Blend Poses by Bool, Slot, Layered Blend per Bone, Inertialization, Save/Use Cached
	 * Pose, Linked Anim Layer...) to a pose graph. Find classes with ObjectToolset.SearchSubclasses of AnimGraphNode_Base.
	 * @param Graph A pose graph (AnimGraph, anim layer or state).
	 * @param NodeClass A concrete subclass of AnimGraphNode_Base.
	 * @param Position Node position; next to the existing nodes if unset.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|AnimToolset")
	static UAnimGraphNode_Base* AddAnimNode(UEdGraph* Graph, UClass* NodeClass, TOptional<FIntPoint> Position);
};
