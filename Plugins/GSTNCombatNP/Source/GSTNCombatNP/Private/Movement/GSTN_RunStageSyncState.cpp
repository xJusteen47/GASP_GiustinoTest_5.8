// Combat simulation (GAS-NPP fork).

#include "Movement/GSTN_RunStageSyncState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/Actor.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "Movement/GSTN_StatsMovementModifier.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/MovementUtilsTypes.h"
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
	Out.Appendf("RunStage: %d  InStage: %d  SinceStart: %d  SinceRelease: %d  Carried: %.1f (%d/%d frames, source %d)  Air: %d takeoff %.1f stage %d  Stamp: %d  Dash: %d\n", Run.Stage + 1, Run.FramesInStage,
		Run.FramesSinceMoveStart, Run.FramesSinceStickRelease, Run.CarriedSpeed, Run.FramesSinceCarry, Run.CarryWindow, static_cast<int32>(Run.CarrySource),
		Run.bAirborne ? 1 : 0, Run.TakeoffSpeed, Run.TakeoffStage + 1, Run.StampFrames, Run.DashFrames);
}

bool FGSTN_RunStageSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	// Counters and stage drive thresholds: any difference is a misprediction. Speeds travel rounded to 0.1 cm/s, so they get a tolerance
	const FGSTN_RunStageState& Authority = static_cast<const FGSTN_RunStageSyncState&>(AuthorityState).Run;
	return Run.Stage != Authority.Stage || Run.FramesInStage != Authority.FramesInStage || Run.FramesSinceMoveStart != Authority.FramesSinceMoveStart
		|| Run.FramesSinceStickRelease != Authority.FramesSinceStickRelease || Run.FramesSinceCarry != Authority.FramesSinceCarry
		|| Run.CarrySource != Authority.CarrySource || !FMath::IsNearlyEqual(Run.CarriedSpeed, Authority.CarriedSpeed, 0.5f)
		|| Run.CarryWindow != Authority.CarryWindow || Run.bAirborne != Authority.bAirborne || Run.TakeoffStage != Authority.TakeoffStage
		|| !FMath::IsNearlyEqual(Run.TakeoffSpeed, Authority.TakeoffSpeed, 0.5f)
		|| Run.StampFrames != Authority.StampFrames || Run.DashFrames != Authority.DashFrames || !Run.ExternalVelocity.Equals(Authority.ExternalVelocity, 0.5);
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

int32 FGSTN_RunStageMover::ApplyStageValues(UMoverComponent& MoverComp, const FGSTN_LocomotionSimTuning& Tuning, int32 Stage, float Factor, float GroundSpeed,
	float AccelRamp)
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
	const float Acceleration = Values.Acceleration * Factor * AccelRamp * FMath::Lerp(1.f, Analog.StartAccelerationMultiplier, Blend);
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
	ApplyStageValues(MoverComp, *Tuning, State ? State->Stage : 0, Factor, GroundSpeed,
		State ? FGSTN_RunStageLogic::GetAccelerationRamp(Tuning->RunStages, *State) : 1.f);
	ApplyAirValues(MoverComp, *Tuning, State, Factor);
}

int32 FGSTN_RunStageMover::ApplyAirValues(UMoverComponent& MoverComp, const FGSTN_LocomotionSimTuning& Tuning, const FGSTN_RunStageState* State, float Factor)
{
	if (!Tuning.RunStages.IsEnabled())
	{
		return 0;
	}
	UCommonLegacyMovementSettings* Shared = MoverComp.FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	if (!Shared)
	{
		return 0;
	}
	const FGSTN_RunAirTuning& Air = Tuning.RunStages.Air;

	// The air control: the AirControl stat (its meaning is Mover's AirControlPercentage, "fraction of the ground acceleration available in
	// the air"; read by nothing before P2), else the falling mode's authored value; times the takeoff stage's scale. In this Mover the
	// percentage lives on each UFallingMode (not on the shared settings), so every falling mode gets it. Assigned every substep, never adjusted
	float StatControl = -1.f;
	if (Air.bUseAirControlStat)
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MoverComp.GetOwner());
		bool bFound = false;
		const float Stat = ASC ? ASC->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetAirControlAttribute(), bFound) : 0.f;
		if (bFound)
		{
			StatControl = Stat;
		}
	}
	const bool bAirborne = State && State->bAirborne;
	const float Scale = bAirborne ? FGSTN_RunStageLogic::GetAirControlScale(Tuning.RunStages, *State) : 1.f;
	// A move that asks for its own air control (the ground dodge's long jump, owner 2026-11-17) replaces stat, authored value and stage scale
	const UGSTN_CombatSimComponent* Combat = FindCombat(MoverComp);
	const float MoveControl = Combat ? Combat->GetRunningMoveAirControl() : -1.f;
	int32 Written = 0;
	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Entry : MoverComp.MovementModes)
	{
		UBaseMovementMode* Mode = Entry.Value;
		if (!Mode)
		{
			continue;
		}
		const UFallingMode* BaseFalling = Cast<UFallingMode>(Mode->GetArchetype());
		const float Authored = BaseFalling ? BaseFalling->AirControlPercentage : 1.f;
		const float Control = MoveControl >= 0.f ? FMath::Min(MoveControl, 1.f) : FMath::Clamp((StatControl >= 0.f ? StatControl : Authored) * Scale, 0.f, 1.f);
		if (UFallingMode* Falling = Cast<UFallingMode>(Mode))
		{
			Falling->AirControlPercentage = Control;
			++Written;
		}
		Written += WriteFloats(*Mode, Tuning.AirControlProperties, Control);
	}

	// Air turn (qa-movement-1117): the falling mode turns the facing toward the orientation intent at most at the shared TurningRate (measured
	// in game: a constant 300 deg/s), so the profile's AirTurnRateDegPerSec (UGSTN_CombatSimComponent's air turn intent) was capped by it: a
	// 180 degree air turn took 31 frames instead of ~3. While airborne the shared rate is raised to the air turn rate; on the ground it is the
	// authored one again. Assigned every substep from the state, like MaxSpeed below, so a resimulation writes the same values
	// m3-air-turn 2026-11-17: this alone did not fix it. GASP's BP_MovementMode_Falling recomputes the turn after Mover's with a fixed 300 deg/s
	// typed on its node, so the real limiter was the Blueprint; FGSTN_AirTurn sets the turn again after the mode. Kept for Mover's own modes
	const UCommonLegacyMovementSettings* AuthoredShared = Cast<UCommonLegacyMovementSettings>(Shared->GetArchetype());
	const float GroundTurnRate = AuthoredShared ? AuthoredShared->TurningRate : Shared->TurningRate;
	const float AirTurnRate = Tuning.AnalogWalk.bEnabled ? Tuning.AnalogWalk.AirTurnRateDegPerSec : 0.f;
	Shared->TurningRate = bAirborne && AirTurnRate > GroundTurnRate ? AirTurnRate : GroundTurnRate;
	++Written;
	// Decision 1C, default model: in the air the top speed is the takeoff speed (with a floor). The stats wrote MaxSpeed = base * factor
	// just before this, so the ground frames keep theirs. The first air frame still uses the ground value (the takeoff is latched at its end)
	if (bAirborne && Air.Model == EGSTN_AirControlModel::TakeoffCap && Factor > 0.f)
	{
		const float Top = FGSTN_RunStageLogic::GetAirTopSpeed(Tuning.RunStages, *State, Factor);
		Shared->MaxSpeed = Top;
		++Written;
		for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Entry : MoverComp.MovementModes)
		{
			if (UObject* Mode = Entry.Value)
			{
				Written += WriteFloats(*Mode, Tuning.AirSpeedProperties, Top);
			}
		}
	}
	return Written;
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
	// A run skid whose first live frame was this step (P2): the run drops (or freezes, bResumeStageAfterSkid) and remembers the speed of before it
	if (const FGSTN_LayeredMove_RunSkid* Skid = FGSTN_LayeredMove_RunSkid::FindActive(OutputSyncState))
	{
		Input.bSkidActive = true;
		if (Skid->CurrentLiveFrame == 0 && Skid->LastStepTimeMs == TimeStep.BaseSimTimeMs)
		{
			Input.bSkidStarted = true;
			Input.SkidStartSpeed = Skid->StartSpeed;
		}
	}

	FGSTN_RunStageSyncState& Entry = OutputSyncState.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_RunStageSyncState>();
	FGSTN_RunStageLogic::Step(Tuning->RunStages, Input, Entry.Run);

	// Air momentum: the external channel of the end of this substep (every substep, not once per frame: it follows the velocity). On the
	// ground it is empty. In the air: the model's value of this substep when it ran (clamped to what collisions left); else the whole moved
	// velocity when an external push owns the frame (the launch frame of a knockback or a trampoline overrides the velocity); else empty
	// (a player-owned override move, a dodge or an attack lunge: the drift is the player's again)
	const FGSTN_AirMomentumTuning& Momentum = Tuning->RunStages.Air.Momentum;
	FVector External = FVector::ZeroVector;
	if (Momentum.IsEnabled() && !Input.bGrounded)
	{
		FVector ModelExternal;
		FVector ModelProposed;
		if (Combat->ConsumeAirMomentumExternal(TimeStep.BaseSimTimeMs, ModelExternal, ModelProposed))
		{
			External = FGSTN_AirMomentumLogic::ClampExternalToMoved(ModelExternal, ModelProposed, Input.GroundVelocity, Momentum.ExternalMinSpeed);
		}
		else if (HasExternalPushMove(OutputSyncState))
		{
			External = Input.GroundVelocity.Size() >= Momentum.ExternalMinSpeed ? Input.GroundVelocity : FVector::ZeroVector;
		}
	}
	Entry.Run.ExternalVelocity = External;
}

bool FGSTN_RunStageMover::HasExternalPushMove(const FMoverSyncState& SyncState)
{
	for (const TSharedPtr<FLayeredMoveBase>& Move : SyncState.LayeredMoves.GetActiveMoves())
	{
		const UScriptStruct* Type = Move.IsValid() ? Move->GetScriptStruct() : nullptr;
		if (Type && (Type->IsChildOf(FLayeredMove_Launch::StaticStruct()) || Type->IsChildOf(FGSTN_LayeredMove_Knockback::StaticStruct())))
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_RunStageMover::ApplyAirMomentum(const UMoverComponent& MoverComp, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	FProposedMove& InOutMove, FVector& OutExternal, FVector& OutProposedHorizontal)
{
	OutExternal = FVector::ZeroVector;
	OutProposedHorizontal = FVector::ZeroVector;
	const FGSTN_LocomotionSimTuning* Tuning = FGSTN_LocomotionAttach::Find(MoverComp.GetOwner());
	if (!Tuning || !Tuning->RunStages.IsEnabled() || !Tuning->RunStages.Air.Momentum.IsEnabled())
	{
		return false;
	}
	// A layered move that overrides the velocity owns the frame (a launch frame, a dodge, a freeze)
	if (InOutMove.MixMode == EMoveMixMode::OverrideVelocity || InOutMove.MixMode == EMoveMixMode::OverrideAll
		|| InOutMove.MixMode == EMoveMixMode::OverrideAllExceptVerticalVelocity)
	{
		return false;
	}
	const UFallingMode* Falling = Cast<UFallingMode>(MoverComp.FindMovementModeByName(StartState.SyncState.MovementMode));
	const FMoverDefaultSyncState* Default = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UCommonLegacyMovementSettings* Shared = MoverComp.FindSharedSettings<UCommonLegacyMovementSettings>();
	if (!Falling || !Default || !Shared)
	{
		return false;
	}
	const double DeltaSeconds = TimeStep.StepMs * 0.001;
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FGSTN_AirMomentumTuning& Momentum = Tuning->RunStages.Air.Momentum;
	const FVector Up = MoverComp.GetUpDirection().GetSafeNormal().IsNearlyZero() ? FVector::UpVector : MoverComp.GetUpDirection().GetSafeNormal();
	const FGSTN_RunStageState* Run = Find(StartState.SyncState);

	// No steering under a movement lock (stats factor 0: hitstun, a reaction), like the stats' contract on the ground
	float Factor = 1.f;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MoverComp.GetOwner());
	if (ASC)
	{
		FGSTN_StatsMovementModifier::ResolveSpeedFactor(*ASC, Factor);
	}

	FGSTN_AirMomentumInput Input;
	const FVector StartVelocity = Default->GetVelocity_WorldSpace();
	Input.Velocity = FVector::VectorPlaneProject(StartVelocity, Up);
	Input.External = Run ? FVector::VectorPlaneProject(Run->ExternalVelocity, Up) : FVector::ZeroVector;
	// Drift DI: under a movement lock with a knockback flying, the stick still nudges the path (kit option)
	bool bKnockbackActive = false;
	for (const TSharedPtr<FLayeredMoveBase>& Move : StartState.SyncState.LayeredMoves.GetActiveMoves())
	{
		bKnockbackActive |= Move.IsValid() && Move->GetScriptStruct()->IsChildOf(FGSTN_LayeredMove_Knockback::StaticStruct());
	}
	Input.bDriftDI = Factor <= 0.f && bKnockbackActive && Momentum.DriftDIControlScale > 0.f;
	if (const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>(); Inputs && (Factor > 0.f || Input.bDriftDI))
	{
		FVector Stick = FVector::VectorPlaneProject(Inputs->GetMoveInput_WorldSpace(), Up);
		if (Inputs->GetMoveInputType() == EMoveInputType::Velocity)
		{
			Stick = Shared->MaxSpeed > UE_KINDA_SMALL_NUMBER ? Stick / Shared->MaxSpeed : FVector::ZeroVector;
		}
		Input.Stick = Stick.GetClampedToMaxSize(1.0);
	}
	// The values PreMovement assigned for this substep: the air top speed (TakeoffCap or the stats' MaxSpeed) and the air control (stat x
	// takeoff stage scale, or the running move's own)
	Input.TopSpeed = Shared->MaxSpeed;
	Input.Control = Falling->AirControlPercentage;
	if (Input.bDriftDI)
	{
		// Under the lock the stats wrote MaxSpeed = 0 and PreMovement kept the ground top speed: the drift uses the archetype's speed (the
		// reference WalkSpeed) with the run tuning's air floor, scaled
		const UCommonLegacyMovementSettings* AuthoredShared = Cast<UCommonLegacyMovementSettings>(Shared->GetArchetype());
		const float BaseTop = FMath::Max(AuthoredShared ? AuthoredShared->MaxSpeed : Shared->MaxSpeed, Tuning->RunStages.Air.MinAirSpeed);
		Input.TopSpeed = BaseTop * Momentum.DriftDIControlScale;
		Input.Control = FMath::Clamp(Falling->AirControlPercentage * Momentum.DriftDIControlScale, 0.f, 1.f);
	}
	Input.DeltaSeconds = static_cast<float>(DeltaSeconds);
	// A knockback's own horizontal drag (its additive frames) acts on the push it gave
	for (const TSharedPtr<FLayeredMoveBase>& Move : StartState.SyncState.LayeredMoves.GetActiveMoves())
	{
		if (Move.IsValid() && Move->GetScriptStruct()->IsChildOf(FGSTN_LayeredMove_Knockback::StaticStruct()))
		{
			Input.ExternalDragPerFrame = FMath::Max(Input.ExternalDragPerFrame, static_cast<const FGSTN_LayeredMove_Knockback&>(*Move).DecayPerFrame);
		}
	}

	const FGSTN_AirMomentumOutput Out = FGSTN_AirMomentumLogic::Step(Momentum, Input);
	const double Vertical = InOutMove.LinearVelocity.Dot(Up);
	InOutMove.LinearVelocity = Out.Velocity + Up * Vertical;
	OutExternal = Out.External;
	OutProposedHorizontal = Out.Velocity;
	return true;
}
