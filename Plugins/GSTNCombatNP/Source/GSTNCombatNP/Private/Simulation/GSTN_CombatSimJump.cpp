// Combat simulation (GAS-NPP fork): the jump part of the combat tick (combat-m3 7.9).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Abilities/GSTN_GA_PlatformJump.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Input/GSTN_CombatSettings.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "MoverTypes.h"

UGSTN_MoverSimStateTracker* UGSTN_CombatSimComponent::GetJumpTracker()
{
	if (UGSTN_MoverSimStateTracker* Cached = JumpTracker.Get())
	{
		return Cached;
	}
	const AActor* Owner = GetOwner();
	UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	UGSTN_MoverSimStateTracker* Tracker = Mover ? UGSTN_MoverSimStateTracker::FindOrAdd(Mover) : nullptr;
	JumpTracker = Tracker;
	return Tracker;
}

bool UGSTN_CombatSimComponent::BuildJumpParams(FGSTN_JumpParams& OutParams) const
{
	if (!AbilitySystem)
	{
		return false;
	}

	// The ability system ticks after the combat model: these are the stats of the end of the previous frame, the same on every machine
	bool bFound = false;
	OutParams.RiseGravityScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetJumpRiseGravityScaleAttribute(), bFound);
	if (!bFound)
	{
		return false;
	}
	OutParams.CutGravityScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetJumpCutGravityScaleAttribute(), bFound);
	OutParams.FallGravityScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetFallGravityScaleAttribute(), bFound);
	OutParams.FastFallGravityScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute(), bFound);
	OutParams.MinHoldFrames = FMath::FloorToInt32(AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetMinJumpHoldFramesAttribute(), bFound));
	OutParams.MaxHoldFrames = FMath::FloorToInt32(AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetMaxJumpHoldFramesAttribute(), bFound));
	OutParams.AirJumpCount = FMath::FloorToInt32(AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetAirJumpCountAttribute(), bFound));
	OutParams.CoyoteFrames = TableJumpData && TableJumpData->bOverrideCoyoteFrames ? TableJumpData->CoyoteFrames : UGSTN_CombatSettings::Get().JumpCoyoteFrames;
	return true;
}

void UGSTN_CombatSimComponent::StepJump(int32 Frame, uint32 HeldButtons, FGSTN_CombatSyncCore& Core)
{
	bFrameJumpValid = false;

	// Without a Mover component or the combat stats the jump state stays what it is (the default): nothing of this runs
	const UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	if (!Tracker || !BuildJumpParams(FrameJumpParams))
	{
		return;
	}

	FrameJumpSample.bGrounded = Tracker->HasGameplayTag(Mover_IsOnGround, true);
	FrameJumpSample.UpSpeed = Tracker->GetUpSpeed();
	FrameJumpSample.bExternalLaunch = Tracker->WasExternallyLaunchedThisFrame();
	bFrameJumpValid = true;

	// The hold is read from the command of this frame: the button that started the jump, still down or not
	const int32 Button = Core.Jump.Button;
	const bool bHeld = Button >= 0 && Button < 32 && (HeldButtons & (1u << Button)) != 0;
	const FGSTN_JumpResetRules& JumpRules = TableJumpData && TableJumpData->bOverrideResetRules ? TableJumpData->ResetRules : UGSTN_CombatSettings::Get().JumpResetRules;
	Core.Jump = FGSTN_JumpLogic::Step(Core.Jump, Frame, FrameJumpSample, bHeld, FrameJumpParams, JumpRules);
}

bool UGSTN_CombatSimComponent::IsJumpBindingAllowed(int32 BindingIndex) const
{
	// Outside the combat tick, for a binding that is not a jump, or without the movement facts: not this gate's business
	if (!GateCore || !bFrameJumpValid || !BindingJumpRole.IsValidIndex(BindingIndex) || BindingJumpRole[BindingIndex] == 0)
	{
		return true;
	}

	const EGSTN_PlatformJumpRole Role = static_cast<EGSTN_PlatformJumpRole>(BindingJumpRole[BindingIndex] - 1);
	return Role == EGSTN_PlatformJumpRole::FastFall
		? FGSTN_JumpLogic::CanFastFall(GateCore->Jump, FrameJumpSample.bGrounded)
		: FGSTN_JumpLogic::ClassifyJump(GateCore->Jump, FrameJumpSample.bGrounded, FrameJumpParams) != EGSTN_JumpKind::None;
}

void UGSTN_CombatSimComponent::ApplyJumpRequest(int32 Frame, int32 BindingIndex, FGSTN_CombatSyncCore& Core)
{
	if (!bFrameJumpValid || !BindingJumpRole.IsValidIndex(BindingIndex) || BindingJumpRole[BindingIndex] == 0)
	{
		return;
	}

	const EGSTN_PlatformJumpRole Role = static_cast<EGSTN_PlatformJumpRole>(BindingJumpRole[BindingIndex] - 1);
	if (Role == EGSTN_PlatformJumpRole::FastFall)
	{
		Core.Jump = FGSTN_JumpLogic::ApplyFastFall(Core.Jump);
		return;
	}

	const EGSTN_JumpKind Kind = FGSTN_JumpLogic::ClassifyJump(Core.Jump, FrameJumpSample.bGrounded, FrameJumpParams);
	if (Kind != EGSTN_JumpKind::None)
	{
		// RequestButton is 1 + the button index, 0 for none (a forced command has no button, nothing to hold)
		const int32 Button = Core.Input.RequestButton >= 1 && Core.Input.RequestButton != FGSTN_InputSimState::CommandWithoutButton ? static_cast<int32>(Core.Input.RequestButton) - 1 : INDEX_NONE;
		Core.Jump = FGSTN_JumpLogic::ApplyTakeoff(Core.Jump, Frame, Button, Kind);
	}
}

bool UGSTN_CombatSimComponent::ReadJumpState(FGSTN_JumpState& OutState) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync)
	{
		return false;
	}
	OutState = Sync->Core.Jump;
	return true;
}

void UGSTN_CombatSimComponent::ApplyJumpGravity(UMoverComponent& MoverComp)
{
	FGSTN_JumpParams Params;
	FGSTN_JumpState State;
	if (!BuildJumpParams(Params) || !ReadJumpState(State))
	{
		return;
	}

	bool bWrite = false;
	const FVector Gravity = FGSTN_GravityPhases::Resolve(GravityBase, MoverComp.GetGravityAcceleration(), FGSTN_JumpLogic::GravityScale(State, Params), bWrite);
	if (bWrite)
	{
		MoverComp.SetGravityOverride(true, Gravity);
	}
}
