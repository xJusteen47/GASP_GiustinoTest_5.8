// Editor-only anim graph authoring for scripts (no runtime dependency, no feature knowledge).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_AnimGraphAuthoringLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequenceBase;
class UAnimationAsset;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/** One override slot of the main graph (InsertOverrideLayerSlots): the Linked Anim Layer that feeds pose Slot of the Blend Poses by int */
USTRUCT(BlueprintType)
struct GSTNANIMATIONEDITOR_API FGSTN_OverrideSlotLayer
{
	GENERATED_BODY()

	/** 1.. (0 is the base pose) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override Slot")
	int32 Slot = 1;

	/** Compiled anim layer interface (ALI_GSTN_Wall...) the main Blueprint implements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override Slot")
	TObjectPtr<UAnimBlueprint> LayerInterface;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override Slot")
	FName LayerName;
};

/**
 * Builds anim graph pieces that the Python editor API cannot (it has no graph node creation or pin wiring), so the anim layer assets of
 * anim-layers.md can be made by an idempotent script (Tools/GAS/setup_wall_anim_layer.py) instead of by hand. Every call is "find or
 * add": running a script twice leaves the graph as it is. Nodes are passed around as UEdGraphNode handles; the functions check the
 * actual class and log (LogGSTNAnimGraphAuthoring) and return null / false on a wrong handle instead of asserting.
 *
 * It mirrors what the editor does on the matching clicks (new anim layer, add interface, place a state machine / state / transition,
 * drag a clip in, Blend Poses by bool, Linked Anim Layer), using only virtual or exported engine calls; the property access node of a
 * rule is created by class name (its class is private to PropertyAccessNode) with its Path set by reflection, as the node's own
 * picker would set it. Nothing is compiled or saved here except by CompileWithReport: the script compiles and saves.
 */
UCLASS()
class GSTNANIMATIONEDITOR_API UGSTN_AnimGraphAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//~ Layer interfaces

	/** Adds an empty anim layer graph named LayerName to an anim layer interface (an anim Blueprint of type Interface). True if present or added */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool AddAnimLayer(UAnimBlueprint* LayerInterface, FName LayerName);

	/** Makes AnimBlueprint implement LayerInterface (Class Settings > Implemented Interfaces): one layer graph per layer. True if it does */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool ImplementLayerInterface(UAnimBlueprint* AnimBlueprint, UAnimBlueprint* LayerInterface);

	/** A top-level graph by name: the AnimGraph, a function, or an implemented interface graph (an anim layer). Null if none */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraph* FindGraph(UBlueprint* Blueprint, FName GraphName);

	//~ State machines

	/** The state machine node named Name in Graph (an anim graph, a layer graph or a state's graph), added if missing */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrAddStateMachine(UEdGraph* Graph, FName Name, FVector2D Position);

	/** The state named StateName in a state machine node, added if missing */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrAddState(UEdGraphNode* StateMachine, FName StateName, FVector2D Position);

	/** Points the state machine's entry at State */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool SetEntryState(UEdGraphNode* StateMachine, UEdGraphNode* State);

	/** The transition FromState -> ToState, added if missing, with a standard crossfade of CrossfadeDuration (s) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrAddTransition(UEdGraphNode* FromState, UEdGraphNode* ToState, float CrossfadeDuration);

	/**
	 * Replaces a transition's rule with "property access PropertyPath == EnumValue" (e.g. ["WallAnim", "Phase"], "Active"): a Property Access
	 * node (thread safe, resolved against the Blueprint's class), an enum == node and the result. False if the path does not resolve to an enum
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool SetTransitionRuleEnumEquals(UEdGraphNode* Transition, const TArray<FString>& PropertyPath, const FString& EnumValue);

	/** The graph inside a state (where its pose is built) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraph* GetStateGraph(UEdGraphNode* State);

	//~ Pose nodes

	/** The first Sequence Player of Graph, added if missing, set to Sequence and looping or not */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrAddSequencePlayer(UEdGraph* Graph, UAnimSequenceBase* Sequence, bool bLoop, FVector2D Position);

	/** The first node of NodeClass (an anim graph node class) in Graph, added at Position if missing. For leaf pose nodes such as GSTN Clip At Time */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrAddPoseNode(UEdGraph* Graph, UClass* NodeClass, FVector2D Position);

	/** Wires PoseNode's pose output into its graph's result (Output Pose of an anim or layer graph, the result of a state), replacing what was there */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool ConnectToGraphOutput(UEdGraphNode* PoseNode);

	/**
	 * The override-layer seam in AnimBlueprint's AnimGraph (anim-layers.md section 2), inserted in front of the first pose input of the node of
	 * class InsertBeforeNodeClass (e.g. "AnimGraphNode_PoseSearchHistoryCollector"; empty = the Output Pose): whatever fed that input goes into the
	 * False pose of a new Blend Poses by bool, a Linked Anim Layer node (LayerInterface::LayerName) into its True pose, ActiveVariable (bool)
	 * drives it and BlendTimeVariable (float) gives both blend times. With bInertialization the blend requests inertialization and an
	 * Inertialization node right after it serves the request (an inertialization or dead blending node upstream would not see it). AnimBlueprint
	 * must implement LayerInterface (ImplementLayerInterface). No change when that input is already fed by such a blend for that layer
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool InsertOverrideLayerBlend(UAnimBlueprint* AnimBlueprint, UAnimBlueprint* LayerInterface, FName LayerName, FName ActiveVariable,
		FName BlendTimeVariable, bool bInertialization, const FString& InsertBeforeNodeClass);

	/**
	 * The override seam with SLOTS (anim-layer-skid-wallslide-piano L1), in front of the first pose input of the node of class
	 * InsertBeforeNodeClass (empty = the Output Pose): a Blend Poses by int driven by SlotVariable (int) whose pose 0 is whatever fed that input
	 * and whose pose N is a Linked Anim Layer node for the layer of Slots with Slot N; BlendTimeVariable (float) gives every blend time. With
	 * bInertialization the blend requests inertialization and an Inertialization node right after it serves the request. AnimBlueprint must
	 * implement every interface (ImplementLayerInterface).
	 * Upgrades in place: a seam of InsertOverrideLayerBlend (Blend Poses by bool) or an int seam with other layers is removed first, its base
	 * pose kept (and its Inertialization node reused). No change when the int seam already has exactly these layers on these slots
	 */
	/**
	 * A Blend Poses by int in Graph (an anim graph of an anim Blueprint) with NumPoses pose pins (>= 2), each with its BlendTime_N pin and
	 * the ActiveChildIndex pin shown. The poses are put in the node's arrays BEFORE its pins are made, so the pins exist at once (adding
	 * pose pins to a spawned node did not create them in 5.8). Null and an error when the pins do not come out
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* SpawnBlendPosesByInt(UEdGraph* Graph, int32 NumPoses, FVector2D Position);

	/** Input pose pins (BlendPose_N) of a blend list node */
	UFUNCTION(BlueprintPure, Category = "GSTN|Anim Graph Authoring")
	static int32 CountBlendPosePins(const UEdGraphNode* BlendNode);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool InsertOverrideLayerSlots(UAnimBlueprint* AnimBlueprint, const TArray<FGSTN_OverrideSlotLayer>& Slots, FName SlotVariable,
		FName BlendTimeVariable, bool bInertialization, const FString& InsertBeforeNodeClass);

	/**
	 * Puts one new pose node of NodeClass (a UAnimGraphNode_Base with one local-space pose input and output) in AnimBlueprint's AnimGraph
	 * right after the only node titled UpstreamNodeTitle (e.g. a Linked Anim Layer "Procedural_Feet"): what that node fed now comes out of
	 * the new node. No change when its output already feeds a node of NodeClass
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool InsertPoseNodeAfter(UAnimBlueprint* AnimBlueprint, const FString& UpstreamNodeTitle, UClass* NodeClass);

	/**
	 * Puts one new pose node of NodeClass right in front of the AnimGraph's Output Pose: what fed the output now goes through the new node
	 * (a post-process on the final pose, e.g. the ground contact fix). No change when the output is already fed by a node of NodeClass
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool InsertPoseNodeBeforeOutput(UAnimBlueprint* AnimBlueprint, UClass* NodeClass);

	/**
	 * Sets one property of the runtime node (the "Node" struct) of the only node of NodeClass in the AnimGraph, from text (an asset path
	 * for an object, "1.5" for a float). For setup scripts: Python cannot reach the runtime struct of an anim graph node. True if set
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool SetPoseNodeProperty(UAnimBlueprint* AnimBlueprint, UClass* NodeClass, FName PropertyName, const FString& ValueText);

	/**
	 * Sets one property of the runtime node (the "Node" struct) of one given anim graph node, from text: the per-node form of
	 * SetPoseNodeProperty, for nodes inside states (e.g. the chooser of a GSTN Chooser Player). True if set
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool SetNodeProperty(UEdGraphNode* Node, FName PropertyName, const FString& ValueText);

	/**
	 * Shows the pin of the runtime node property PinName on an anim graph node (e.g. Alpha or LocomotionAngle of a warping node) and drives
	 * it from a Property Access node reading PropertyPath on the anim instance (e.g. ["OrientationWarpAlpha"]). Rebuilt on every call, so a
	 * second run does not stack nodes. True if linked
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool BindNodePinToProperty(UEdGraphNode* Node, FName PinName, const TArray<FString>& PropertyPath);

	/**
	 * Copies the whole runtime node struct (bones, modes, values) of the first node of Target's class found in SourceBlueprint (any graph)
	 * onto Target, e.g. the GASP stock ABP's configured Orientation / Stride Warping onto a new graph's warping nodes. Pin links and
	 * bindings are not copied. True if copied
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool CopyNodeSettingsFrom(UEdGraphNode* Target, UAnimBlueprint* SourceBlueprint);

	/** Every node of NodeClass in AnimBlueprint's AnimGraph, in graph order (nodes nested in states are not included) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static TArray<UEdGraphNode*> FindAnimGraphNodes(UAnimBlueprint* AnimBlueprint, UClass* NodeClass);

	//~ Child anim Blueprints (skin variants, anim-layers.md part B)

	/**
	 * In a child anim Blueprint, plays Asset instead of the parent's asset player in the state named StateName (the Asset Override editor of
	 * a child anim Blueprint): the graph stays the parent's, only the clip changes. True if the override is set
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static bool SetChildAssetOverride(UAnimBlueprint* ChildBlueprint, FName StateName, UAnimationAsset* Asset);

	//~ Inspection

	/** Text dump of every graph of the Blueprint (nodes, pins, links, sub graphs) for logs and checks */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static FString DescribeGraphs(UBlueprint* Blueprint);

	/** Compiles the Blueprint and returns the compiler messages, one per line, prefixed with the severity; bSuccess is false on errors */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static FString CompileWithReport(UBlueprint* Blueprint, bool& bSuccess);
};
