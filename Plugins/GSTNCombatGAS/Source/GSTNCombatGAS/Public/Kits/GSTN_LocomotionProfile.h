// Combat ability-system layer: the kit's locomotion profile (Mario-style run stages, docs/notes/corsa-mario-piano.md).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Jump/GSTN_JumpDesigner.h"
#include "Movement/GSTN_AnalogWalkTypes.h"
#include "Movement/GSTN_RunStageTypes.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_LocomotionProfile.generated.h"

class AActor;
class UGSTN_PrimaryKitDefinition;
struct FGSTN_CompileInput;
struct FGSTN_LoadoutExtensionContext;

/**
 * How a kit moves on the ground: the run stages, the boost to the last stage, and the names of the walking mode's properties the
 * stages drive. One asset per kit (or shared), referenced by the kit through FGSTN_LocomotionKitExtension. Data only: the values are
 * copied to the pawn where the loadout is compiled (FGSTN_LocomotionAttach), so the simulation never reads the asset. Nothing here is
 * a Project Setting and no key is named: the boost is a command tag, its key is in the kit's input data.
 *
 * P2 adds the skid and the air control (RunStages.Skid / RunStages.Air), P3 the attack momentum (RunStages.AttackMomentum).
 */
/*
 * Details panel layout (owner 2026-11-24, "duplicates and poor organisation"): the three tuning structs are pushed out
 * (ShowOnlyInnerProperties), so every inner property shows in the category its own struct gives it. Those categories are the final layout:
 * Run, Skid, Walk, Air, Jump, then Mode Bindings (the Blueprint variable names, advanced). The struct properties below carry the category
 * of their first group only so nothing is left under an extra heading. Never rename a property to tidy the panel: categories and display
 * names are free, a rename needs a CoreRedirect and breaks the setup scripts that set values by name.
 */
UCLASS(BlueprintType, meta = (PrioritizeCategories = "Run Skid Walk Air Jump"))
class GSTNCOMBATGAS_API UGSTN_LocomotionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_LocomotionProfile();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ShowOnlyInnerProperties))
	FGSTN_RunStagesTuning RunStages;

	/**
	 * Properties of the walking mode (Blueprint gait variables or Mover's own) that get the stage's MaxSpeed * stats factor. The
	 * gait variables feed MaxSpeedOverride, so they are the values the mode really uses. A name the mode does not have is skipped
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> SpeedProperties;

	/** Properties that get the stage's Acceleration * stats factor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> AccelerationProperties;

	/** Properties that get the stage's TurningStrength */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> TurningProperties;

	/** Properties that get the stage's ReleaseBrake * stats factor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> BrakeProperties;

	/**
	 * Air (P2, TakeoffCap model): properties of any mode that get the air top speed while airborne, besides Mover's shared MaxSpeed which
	 * is always written. For a Blueprint air mode with its own speed variable. Empty by default
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Air Mode", AdvancedDisplay)
	TArray<FName> AirSpeedProperties;

	/** Properties that get the air control (0..1) besides Mover's shared AirControlPercentage, which is always written. Empty by default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Air Mode", AdvancedDisplay)
	TArray<FName> AirControlProperties;

	/** Analog walk, walk start and air turn (owner playtest 2026-11-16). Works with the run stages on (it scales their stage 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (ShowOnlyInnerProperties))
	FGSTN_AnalogWalkTuning AnalogWalk;

	/**
	 * Jump from designer numbers (owner 2026-11-18): full jump height and time to apex; the take-off speed and the gravities are derived
	 * (read-only fields below the inputs). Off = the kit's jump stats as before. The whole movement setup of a kit lives in this asset
	 * (run stages, skid, air momentum, drift DI, jump), so several kits can share it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ShowOnlyInnerProperties))
	FGSTN_JumpDesignerTuning JumpDesigner;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** Values of a profile as the simulation reads them (no object reference: nothing has to keep the asset alive) */
struct GSTNCOMBATGAS_API FGSTN_LocomotionSimTuning
{
	FGSTN_RunStagesTuning RunStages;
	TArray<FName> SpeedProperties;
	TArray<FName> AccelerationProperties;
	TArray<FName> TurningProperties;
	TArray<FName> BrakeProperties;
	TArray<FName> AirSpeedProperties;
	TArray<FName> AirControlProperties;
	FGSTN_AnalogWalkTuning AnalogWalk;
	/** Derived values refreshed when copied (FromProfile) */
	FGSTN_JumpDesignerTuning JumpDesigner;

	static FGSTN_LocomotionSimTuning FromProfile(const UGSTN_LocomotionProfile& Profile);
};

/**
 * Gives a kit its locomotion profile, by data. An entry of the kit's extension array, like the spin and drill one, so the kit asset
 * never grows a property for it; a kit without the entry moves exactly as before (no run stages).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_LocomotionKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** The kit's profile. Empty = the default profile values (FGSTN_RunStagesTuning::MakeDefault and the GASP gait names) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TSoftObjectPtr<UGSTN_LocomotionProfile> Profile;
};

/**
 * Which locomotion tuning an entity plays with: resolved on every machine from the kit (the loadout extension calls Apply wherever
 * the loadout is compiled, host and clients alike: no OnRep, no IsServer branch). Same idea as FGSTN_SpinDrillAttach. Game thread only.
 */
class GSTNCOMBATGAS_API FGSTN_LocomotionAttach
{
public:
	/** The tuning attached to Owner, null when its kit has no locomotion entry (run stages off) */
	static const FGSTN_LocomotionSimTuning* Find(const AActor* Owner);

	static void Apply(const AActor* Owner, const FGSTN_LocomotionSimTuning& Tuning);
	static void Clear(const AActor* Owner);

	/** Drops every attachment (tests only) */
	static void Reset();
};

/** The loadout side: the kit extension resolved into the pawn's attached tuning */
struct GSTNCOMBATGAS_API FGSTN_LocomotionLoadout
{
	static const FGSTN_LocomotionKitExtension* FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit);

	/** The loadout extension the module registers: attaches the kit's profile values to the loadout's owner, or detaches */
	static void AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	static FName GetLoadoutExtensionId();
};
