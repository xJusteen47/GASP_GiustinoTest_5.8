// Combat simulation (GAS-NPP fork).

#include "Movement/GSTN_RunStageSyncState.h"

#include "GameFramework/Actor.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Stats/GSTN_StatsScaling.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_RunStageSyncState)

bool FGSTN_RunStageSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);
	Run.SerializeCompact(Ar);
	bOutSuccess = true;
	return true;
}

void FGSTN_RunStageSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("RunStage: %d  InStage: %d  SinceStart: %d  SinceRelease: %d  Carried: %.1f (%d frames, source %d)\n", Run.Stage + 1, Run.FramesInStage,
		Run.FramesSinceMoveStart, Run.FramesSinceStickRelease, Run.CarriedSpeed, Run.FramesSinceCarry, static_cast<int32>(Run.CarrySource));
}

bool FGSTN_RunStageSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	// Counters and stage drive thresholds: any difference is a misprediction. Speeds travel rounded to 0.1 cm/s, so they get a tolerance
	const FGSTN_RunStageState& Authority = static_cast<const FGSTN_RunStageSyncState&>(AuthorityState).Run;
	return Run.Stage != Authority.Stage || Run.FramesInStage != Authority.FramesInStage || Run.FramesSinceMoveStart != Authority.FramesSinceMoveStart
		|| Run.FramesSinceStickRelease != Authority.FramesSinceStickRelease || Run.FramesSinceCarry != Authority.FramesSinceCarry
		|| Run.CarrySource != Authority.CarrySource || !FMath::IsNearlyEqual(Run.CarriedSpeed, Authority.CarriedSpeed, 0.5f);
}

void FGSTN_RunStageSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// Discrete state: a simulated proxy shows the newer frame's
	Run = static_cast<const FGSTN_RunStageSyncState&>(To).Run;
}

void FGSTN_RunStageMover::RegisterPersistentState(UMoverComponent& MoverComp)
{
	UScriptStruct* Type = FGSTN_RunStageSyncState::StaticStruct();
	if (!MoverComp.PersistentSyncStateDataTypes.ContainsByPredicate([Type](const FMoverDataPersistence& Entry) { return Entry.RequiredType == Type; }))
	{
		MoverComp.PersistentSyncStateDataTypes.Add(FMoverDataPersistence(Type, true));
	}
}

const FGSTN_RunStageState* FGSTN_RunStageMover::Find(const FMoverSyncState& SyncState)
{
	const FGSTN_RunStageSyncState* Entry = SyncState.SyncStateCollection.FindDataByType<FGSTN_RunStageSyncState>();
	return Entry ? &Entry->Run : nullptr;
}

namespace
{
	int32 WriteFloats(UObject& Object, TConstArrayView<FName> Names, double Value)
	{
		int32 Written = 0;
		for (const FName Name : Names)
		{
			const FNumericProperty* Property = CastField<FNumericProperty>(Object.GetClass()->FindPropertyByName(Name));
			if (Property && Property->IsFloatingPoint())
			{
				Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(&Object), Value);
				++Written;
			}
		}
		return Written;
	}

	UGSTN_CombatSimComponent* FindCombat(const UMoverComponent& MoverComp)
	{
		AActor* Owner = MoverComp.GetOwner();
		return Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	}
}

int32 FGSTN_RunStageMover::ApplyStageValues(UMoverComponent& MoverComp, const FGSTN_LocomotionSimTuning& Tuning, int32 Stage, float Factor, float GroundSpeed)
{
	if (!Tuning.RunStages.IsEnabled())
	{
		return 0;
	}
	const FGSTN_RunStage& Values = Tuning.RunStages.GetStage(Stage);
	const FGSTN_AnalogWalkTuning& Analog = Tuning.AnalogWalk;
	const bool bAnalog = Analog.bEnabled && GroundSpeed >= 0.f;
	// Walk start: the boost fades out as the speed reaches LowSpeedFraction of the stage top speed (assigned every frame, never adjusted)
	const float Blend = bAnalog ? FGSTN_AnalogWalkLogic::StartBlend(Analog, GroundSpeed, Values.MaxSpeed * Factor) : 0.f;
	const float Acceleration = Values.Acceleration * Factor * FMath::Lerp(1.f, Analog.StartAccelerationMultiplier, Blend);
	const float Turning = FMath::Lerp(Values.TurningStrength, FMath::Max(Values.TurningStrength, Analog.StartTurningStrength), Blend);
	int32 Written = 0;
	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Entry : MoverComp.MovementModes)
	{
		if (UObject* Mode = Entry.Value)
		{
			Written += WriteFloats(*Mode, Tuning.SpeedProperties, Values.MaxSpeed * Factor);
			Written += WriteFloats(*Mode, Tuning.AccelerationProperties, Acceleration);
			Written += WriteFloats(*Mode, Tuning.TurningProperties, Turning);
			if (bAnalog)
			{
				// GASP picks its walk gait for a light tilt: the gait must not cap the speed, the tilt (the intent length) does
				Written += WriteFloats(*Mode, Analog.WalkSpeedProperties, Values.MaxSpeed * Factor);
				Written += WriteFloats(*Mode, Analog.WalkAccelerationProperties, Acceleration);
				if (Analog.StartFacingTime >= 0.f && Analog.RunFacingTime >= 0.f)
				{
					Written += WriteFloats(*Mode, Analog.FacingTimeProperties, FMath::Lerp(Analog.RunFacingTime, Analog.StartFacingTime, Blend));
				}
				if (Analog.AccelerationSmoothingTime >= 0.f)
				{
					Written += WriteFloats(*Mode, Analog.AccelerationSmoothingProperties, Analog.AccelerationSmoothingTime);
				}
			}
			// Under a movement lock the factor is 0 and the character must still brake (the stats' contract): the brake keeps scale 1 then
			Written += WriteFloats(*Mode, Tuning.BrakeProperties, Values.ReleaseBrake * (Factor > 0.f ? Factor : 1.f));
		}
	}
	return Written;
}

void FGSTN_RunStageMover::PreMovement(UMoverComponent& MoverComp, float Factor)
{
	const FGSTN_LocomotionSimTuning* Tuning = FGSTN_LocomotionAttach::Find(MoverComp.GetOwner());
	UGSTN_CombatSimComponent* Combat = Tuning ? FindCombat(MoverComp) : nullptr;
	UGSTN_MoverSimStateTracker* Tracker = Combat ? Combat->GetMoverTracker() : nullptr;
	if (!Tracker)
	{
		return;
	}
	// The stage of the start of this frame: the tracker holds the end of the previous one, and follows rollbacks
	const FMoverSyncState& Start = Tracker->GetSyncState();
	const FGSTN_RunStageState* State = Find(Start);
	float GroundSpeed = 0.f;
	if (const FMoverDefaultSyncState* Default = Start.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		const FVector Up = Tracker->GetUpDirection();
		const FVector Velocity = Default->GetVelocity_WorldSpace();
		GroundSpeed = static_cast<float>((Velocity - Up * Velocity.Dot(Up)).Size());
	}
	ApplyStageValues(MoverComp, *Tuning, State ? State->Stage : 0, Factor, GroundSpeed);
}

void FGSTN_RunStageMover::PostMovement(UMoverComponent& MoverComp, const FMoverTimeStep& TimeStep, FMoverSyncState& OutputSyncState, float Factor)
{
	const FGSTN_LocomotionSimTuning* Tuning = FGSTN_LocomotionAttach::Find(MoverComp.GetOwner());
	if (!Tuning || !Tuning->RunStages.IsEnabled())
	{
		return;
	}
	UGSTN_CombatSimComponent* Combat = FindCombat(MoverComp);
	UGSTN_MoverSimStateTracker* Tracker = Combat ? Combat->GetMoverTracker() : nullptr;
	const FMoverDefaultSyncState* Default = OutputSyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!Combat || !Tracker || !Default)
	{
		return;
	}

	const FVector Up = Tracker->GetUpDirection();
	const FVector Velocity = Default->GetVelocity_WorldSpace();

	FVector2D Stick;
	float CameraYaw = 0.f;
	Combat->GetFrameStick(Stick, CameraYaw);
	const double StickSize = FMath::Min(Stick.Size(), 1.0);

	FGSTN_RunFrameInput Input;
	Input.Frame = TimeStep.ServerFrame;
	Input.bGrounded = MoverComp.HasGameplayTagInState(OutputSyncState, Mover_IsOnGround, false);
	Input.GroundVelocity = Velocity - Up * Velocity.Dot(Up);
	Input.StickWorld = StickSize > 0.0 ? FGSTN_CombatVictim::StickToPlane(Stick, CameraYaw, Up).GetSafeNormal() * StickSize : FVector::ZeroVector;
	Input.bBoostCommand = Tuning->RunStages.BoostCommand.IsValid() && Combat->WasCommandEnteredWithin(Tuning->RunStages.BoostCommand, 0);
	Input.SpeedFactor = Factor;

	FGSTN_RunStageSyncState& Entry = OutputSyncState.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_RunStageSyncState>();
	FGSTN_RunStageLogic::Step(Tuning->RunStages, Input, Entry.Run);
}
