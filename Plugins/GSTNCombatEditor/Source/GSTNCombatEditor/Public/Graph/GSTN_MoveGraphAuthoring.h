// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_MoveGraphAuthoring.generated.h"

class UBlueprint;

/**
 * Builds the event graph of a move Blueprint from an editor script (CP3 of the Blueprint seams): the example moves are made by a committed Python
 * script, never wired by hand, and the same calls let a later tool (the Combat Move factory of CP5) lay down a starting graph. Static, stateless,
 * development only.
 *
 * A node is named by the string of its node GUID, which every Add call returns ("" and OutError on failure); the script keeps its own names for
 * them. Pins are named by their pin name (a parameter's name, ReturnValue, execute, then, else, Condition, a variable's name, a struct member's
 * name); an empty pin name means "the only data pin of that side" (the struct pin of a Break or Make node, the value of a variable get).
 *
 * Order a script follows: add the member variables, compile (a get or set node needs the variable on the skeleton class), add the nodes, connect
 * and set pin defaults, compile again and read the errors, then CheckSimulationRules. Nothing here saves: the script saves the asset.
 */
UCLASS()
class GSTNCOMBATEDITOR_API UGSTN_MoveGraphAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Adds a member variable, or changes the type and default of the one of that name. Type: bool, int, int64, float (a double), name, string, a
	 * struct as "struct:/Script/Module.Struct", an object reference as "object:/Script/Module.Class" (a Blueprint class by its generated class
	 * path). DefaultValue is the text form of the value (an object's path, a struct's "(Member=...)"). Category and Tooltip show in Class Defaults
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static bool SetMemberVariable(UBlueprint* Blueprint, FName Name, const FString& Type, const FString& DefaultValue, const FString& Category,
		const FString& Tooltip, FString& OutError);

	/** Removes every node of the Blueprint's event graph (a script rebuilding its own graph). Member variables stay */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static int32 ClearEventGraph(UBlueprint* Blueprint);

	/** The override event of a Blueprint event of the parent class (K2_OnMoveKey, K2_OnMoveFrame...), made or found in the event graph */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static FString AddEventNode(UBlueprint* Blueprint, FName EventFunctionName, int32 X, int32 Y, FString& OutError);

	/**
	 * A call of FunctionName of the class at ClassPath ("/Script/Module.Class"; empty = the Blueprint's parent class, for its own functions). A
	 * member function of another class has a "self" pin to connect to its target
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static FString AddCallNode(UBlueprint* Blueprint, const FString& ClassPath, FName FunctionName, int32 X, int32 Y, FString& OutError);

	/** A get (bSet false) or set node of a member variable of the Blueprint */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static FString AddVariableNode(UBlueprint* Blueprint, FName VariableName, bool bSet, int32 X, int32 Y, FString& OutError);

	/** A Make (bMake) or Break node of the struct at StructPath ("/Script/Module.Struct") */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static FString AddStructNode(UBlueprint* Blueprint, const FString& StructPath, bool bMake, int32 X, int32 Y, FString& OutError);

	/** A Branch node (pins execute, Condition, then, else) */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static FString AddBranchNode(UBlueprint* Blueprint, int32 X, int32 Y, FString& OutError);

	/** Links an output pin of FromNode to an input pin of ToNode through the K2 schema (its type checks and conversions) */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static bool ConnectPins(UBlueprint* Blueprint, const FString& FromNode, FName FromPin, const FString& ToNode, FName ToPin, FString& OutError);

	/** Sets the default of an unlinked input pin: the text form of the value, or an asset path for an object pin */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static bool SetPinDefault(UBlueprint* Blueprint, const FString& Node, FName Pin, const FString& Value, FString& OutError);

	/** Compiles the Blueprint; returns the number of errors and every error and warning line in OutMessages */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static int32 CompileAndReport(UBlueprint* Blueprint, TArray<FString>& OutMessages);

	/**
	 * Runs the simulation graph rules of the combat validator (latent nodes, non-sync writes, world reads, unseeded random in what the move events
	 * reach) and returns the number of errors; every finding is a line of OutFindings
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static int32 CheckSimulationRules(UBlueprint* Blueprint, TArray<FString>& OutFindings);

	/** One line per node of the event graph and one per link, for a script's log and the designer guide */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Graph Authoring", meta = (DevelopmentOnly))
	static TArray<FString> DescribeEventGraph(UBlueprint* Blueprint);
};
