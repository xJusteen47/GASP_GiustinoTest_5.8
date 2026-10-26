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
	 * A pose node of class NodeClassPath (e.g. "/Script/ControlRigDeveloper.AnimGraphNode_ControlRig") right in front of Graph's result
	 * (Output Pose of an anim or layer graph): whatever fed the result now feeds the new node's pose input. NodeMembers sets members of the
	 * node's runtime struct from their text form (e.g. "ControlRigClass" -> the class path, "InputMapping" -> a map in text form),
	 * then the node is reconstructed so pins that depend on them appear. Found by class when already there: only the members are set again.
	 * The class is found by path, so this module does not link the node's module. Returns the node, or null on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Anim Graph Authoring")
	static UEdGraphNode* FindOrInsertPoseNodeBeforeOutput(UEdGraph* Graph, const FString& NodeClassPath, const TMap<FString, FString>& NodeMembers);

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
