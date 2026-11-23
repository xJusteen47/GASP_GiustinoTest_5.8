// Editor-only authoring helpers for the presentation VFX kit (polish plan M2).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_VfxAuthoringLibrary.generated.h"

class UNiagaraRendererProperties;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UMaterialInterface;

/**
 * Builds the kit's Niagara systems from a script (Tools/GAS/setup_presentation_vfx.py) through the same editor APIs the Niagara stack
 * uses, because neither Python nor the editor MCP bridge can: a module input's local value lives in the scripts' rapid iteration
 * parameters (not in a graph pin), static switches are pins of the module node, user parameters live in the system's exposed store.
 * Module lookup: the stack module whose function name equals ModuleName (else contains it), in the stack named by Stack
 * ("EmitterSpawn", "EmitterUpdate", "ParticleSpawn", "ParticleUpdate"; empty = any). Every function returns an empty string when done,
 * else the error. None of them saves or compiles: the caller calls CompileAndWait, then saves.
 */
UCLASS()
class GSTNPRESENTATIONEDITOR_API UGSTN_VfxAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds (or keeps) User.<Name> as a float with this default */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static bool AddUserFloat(UNiagaraSystem* System, FName Name, float Default);

	/** Adds (or keeps) User.<Name> as a linear colour with this default */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static bool AddUserColor(UNiagaraSystem* System, FName Name, FLinearColor Default);

	/** Adds (or keeps) User.<Name> as a 2D vector with this default (e.g. a sprite size scale the cue sets from its intensity) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static bool AddUserVector2(UNiagaraSystem* System, FName Name, FVector2D Default);

	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static bool AddUserInt(UNiagaraSystem* System, FName Name, int32 Default);

	/** Adds (or keeps) User.<Name> as a 3D vector (v2: the ground plane point and normal the cue sets for the bounces) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static bool AddUserVector(UNiagaraSystem* System, FName Name, FVector Default);

	/**
	 * VFX v2 (meshes, not sprites): adds a mesh renderer drawing Mesh with Material (an override, so the mesh asset keeps its own).
	 * bReplace removes the emitter's renderers first (the template's sprite); false appends, e.g. the flattened shadow pass of the same
	 * particles. Facing: ENiagaraMeshFacingMode as an int (0 default, 1 velocity, 2 camera position; the enum is not exposed to Python).
	 * Null if no emitter
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static UNiagaraRendererProperties* AddMeshRenderer(UNiagaraSystem* System, const FString& EmitterName, UStaticMesh* Mesh,
		UMaterialInterface* Material, bool bReplace, int32 Facing = 0);

	/**
	 * +1 when most triangles of LOD 0 have a geometric normal (V1 - V0) x (V2 - V0) along their vertex normals, -1 when against, 0 if no
	 * render data. The kit's meshes are written by a script: compared with an engine mesh, it tells whether they render inside out
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static int32 MeshWindingSign(UStaticMesh* Mesh);

	/** Adds an emitter copied from an emitter asset (a template), renamed; returns its unique name or an error starting with "!" */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString AddEmitter(UNiagaraSystem* System, const FString& EmitterAssetPath, const FString& NewName);

	/** Removes every emitter (the setup script rebuilds a system in place, so the tables keep their reference) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static void ClearEmitters(UNiagaraSystem* System);

	/** One line per stack module: stack, function name, then its inputs (name:type) and static switches (name=value) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString DescribeEmitter(UNiagaraSystem* System, const FString& EmitterName);

	/** Debug: where a module input's value comes from (override pin and what feeds it, rapid iteration constants matching the input) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString DescribeInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName, const FString& InputName);

	/** Appends a stock or project module script (asset path) to a stack */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString AddModule(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleScriptPath);

	/** Inserts a module before another one of the same stack. A force (Drag, GravityForce) must run before SolveForcesAndVelocity: appended
	 *  after it, its force is cleared before the next solve and the particle keeps a constant speed (review r2: shards and run puffs) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString AddModuleBefore(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleScriptPath,
		const FString& BeforeModule);

	/** Debug: the live particles of one emitter of a CPU system, "x,y,z/vx,vy,vz" per particle (component space for a local-space
	 *  emitter), at most Max. Used with AdvanceSimulation to measure speeds frame by frame without a capture */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString DescribeParticles(UNiagaraComponent* Component, const FString& EmitterName, int32 Max = 8);

	/** The stack's modules in execution order, "A > B > C(off)" */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString ListModules(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack);

	/** Enables or disables a stack module (a template module the effect does not want is disabled, not removed) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString SetModuleEnabled(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName, bool bEnabled);

	/**
	 * Sets a module input to a local value, as the stack does (rapid iteration parameter of every script that runs the module). A link or a
	 * dynamic input on it is removed first. Value text: "1.5", "3", "true", "1,2,3", "1,0.9,0.8,1". A static switch input is set on the node
	 * (enum by name, display name or index).
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString SetInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName,
		const FString& InputName, const FString& Value);

	/** Links a module input to a parameter (a full name: "User.Tint", "Particles.NormalizedAge"...); replaces a local value */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString LinkModuleInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName,
		const FString& InputName, FName Parameter);

	/** The emitter's renderer at Index (Python sets its properties: material, alignment, facing...); null if none */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static UNiagaraRendererProperties* GetRenderer(UNiagaraSystem* System, const FString& EmitterName, int32 Index);

	/** Replaces the emitter's renderers with one ribbon renderer (linked by spawn order) and returns it; null if no emitter */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static UNiagaraRendererProperties* SetRibbonRenderer(UNiagaraSystem* System, const FString& EmitterName);

	/** Emitter settings: CPU sim, local or world space, fixed bounds of this half extent around the system (0 = dynamic) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static FString SetEmitterSettings(UNiagaraSystem* System, const FString& EmitterName, bool bLocalSpace, float FixedBoundsHalfExtent);

	/** System fixed bounds (cheap culling on toaster PCs): a box of this half extent around the component */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static void SetSystemFixedBounds(UNiagaraSystem* System, float HalfExtent);

	/** Requests a compile and waits for it (the script then saves a system whose scripts are built) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|VFX Authoring")
	static void CompileAndWait(UNiagaraSystem* System);
};
