// Combat ability-system layer: the kit's locomotion profile.

#include "Kits/GSTN_LocomotionProfile.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "UObject/ObjectKey.h"

namespace
{
	TMap<FObjectKey, FGSTN_LocomotionSimTuning>& Attached()
	{
		static TMap<FObjectKey, FGSTN_LocomotionSimTuning> Instance;
		return Instance;
	}

	/** Forgets the entries whose actor is gone (only when something is attached: never in a tick) */
	void Prune()
	{
		for (auto It = Attached().CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}

	/** The property names of the GASP walking mode (BP_MovementMode_Walking, a USmoothWalkingMode): gait variables, then Mover's own */
	void SetGaspDefaults(TArray<FName>& Speed, TArray<FName>& Acceleration, TArray<FName>& Turning, TArray<FName>& Brake)
	{
		Speed = { TEXT("RunSpeed"), TEXT("SprintSpeed") };
		Acceleration = { TEXT("RunAcceleration"), TEXT("SprintAcceleration") };
		Turning = { TEXT("TurningStrength"), TEXT("SprintTurnStrength") };
		Brake = { TEXT("Deceleration") };
	}
}

UGSTN_LocomotionProfile::UGSTN_LocomotionProfile()
{
	RunStages = FGSTN_RunStagesTuning::MakeDefault();
	SetGaspDefaults(SpeedProperties, AccelerationProperties, TurningProperties, BrakeProperties);
}

FGSTN_LocomotionSimTuning FGSTN_LocomotionSimTuning::FromProfile(const UGSTN_LocomotionProfile& Profile)
{
	FGSTN_LocomotionSimTuning Tuning;
	Tuning.RunStages = Profile.RunStages;
	Tuning.SpeedProperties = Profile.SpeedProperties;
	Tuning.AccelerationProperties = Profile.AccelerationProperties;
	Tuning.TurningProperties = Profile.TurningProperties;
	Tuning.BrakeProperties = Profile.BrakeProperties;
	Tuning.AnalogWalk = Profile.AnalogWalk;
	Tuning.AirSpeedProperties = Profile.AirSpeedProperties;
	Tuning.AirControlProperties = Profile.AirControlProperties;
	// Derived on every machine from the same data, so host and clients jump alike
	Tuning.JumpDesigner = Profile.JumpDesigner;
	if (Tuning.JumpDesigner.bEnabled)
	{
		FGSTN_JumpDesigner::Refresh(Tuning.JumpDesigner);
	}
	return Tuning;
}

void UGSTN_LocomotionProfile::PostLoad()
{
	Super::PostLoad();
	FGSTN_JumpDesigner::Refresh(JumpDesigner);
}

#if WITH_EDITOR
void UGSTN_LocomotionProfile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// The read-only derived fields follow every edit
	FGSTN_JumpDesigner::Refresh(JumpDesigner);
}
#endif

const FGSTN_LocomotionSimTuning* FGSTN_LocomotionAttach::Find(const AActor* Owner)
{
	return Owner ? Attached().Find(FObjectKey(Owner)) : nullptr;
}

void FGSTN_LocomotionAttach::Apply(const AActor* Owner, const FGSTN_LocomotionSimTuning& Tuning)
{
	if (!Owner)
	{
		return;
	}
	Prune();
	Attached().Add(FObjectKey(Owner), Tuning);
}

void FGSTN_LocomotionAttach::Clear(const AActor* Owner)
{
	if (Owner)
	{
		Attached().Remove(FObjectKey(Owner));
	}
}

void FGSTN_LocomotionAttach::Reset()
{
	Attached().Reset();
}

const FGSTN_LocomotionKitExtension* FGSTN_LocomotionLoadout::FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit ? Kit->Extensions.Find<FGSTN_LocomotionKitExtension>() : nullptr;
}

void FGSTN_LocomotionLoadout::AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UActorComponent* Component = Cast<UActorComponent>(Context.WorldContext);
	const AActor* Owner = Component ? Component->GetOwner() : nullptr;
	const FGSTN_LocomotionKitExtension* Extension = FindKitExtension(Context.Resolved ? Context.Resolved->Kit : nullptr);
	if (!Extension)
	{
		if (Owner)
		{
			FGSTN_LocomotionAttach::Clear(Owner);
		}
		return;
	}
	// Loaded here, where the loadout is compiled, never in a tick
	const UGSTN_LocomotionProfile* Profile = Extension->Profile.IsNull() ? nullptr : Extension->Profile.LoadSynchronous();
	const UGSTN_LocomotionProfile& Used = Profile ? *Profile : *GetDefault<UGSTN_LocomotionProfile>();
	// The boost command has no ability, so the table would drop it: it enters as a query command (slot from the kit's command set, the
	// button stays in the input data). Before the owner check, so a tool's compile builds the same table as the pawn's
	if (Used.RunStages.BoostCommand.IsValid())
	{
		Input.Kit.QueryCommands.AddUnique(Used.RunStages.BoostCommand);
	}
	if (!Owner)
	{
		// A tool or a test compiles without a pawn
		return;
	}
	FGSTN_LocomotionAttach::Apply(Owner, FGSTN_LocomotionSimTuning::FromProfile(Used));
}

FName FGSTN_LocomotionLoadout::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNLocomotion"));
	return Id;
}
