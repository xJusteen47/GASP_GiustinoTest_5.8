// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_Dash.h"

#include "AbilitySystemComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GSTN_CombatCoreTags.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Movement/GSTN_GravityFrame.h"
#include "Timeline/GSTN_StartupSteer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNMoveTimeline, Log, All);

FEffectMoveContainer UGSTN_MoveImpulseCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FApplyVelocityEffect Effect;
	Effect.bAdditiveVelocity = false;
	if (!TargetMoverComponent)
	{
		return FEffectMoveContainer(&Effect);
	}

	// The movement facts of the end of the previous frame, the same on every machine and in a re-simulation
	const UGSTN_MoverSimStateTracker* Tracker = UGSTN_MoverSimStateTracker::FindOrAdd(TargetMoverComponent);
	const FVector Up = Tracker->GetUpDirection();
	FVector Facing = FVector::ForwardVector;
	if (const FMoverDefaultSyncState* State = Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		Facing = State->GetOrientation_WorldSpace().Vector();
	}
	// Stick priority (owner 2026-11-16: rolls and dodges follow the stick too): the move origin of the timeline that just started already holds
	// the start aim of this frame (the combat tick ran before the ability system), while the Mover state above is still the facing of the frame before
	const UGSTN_CombatSimComponent* Combat = TargetMoverComponent->GetOwner() ? TargetMoverComponent->GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	if (const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr; Sync && Sync->Core.ActionState == EGSTN_ActionState::InTimeline && Sync->Core.Hit.MoveOrigin.bSet)
	{
		Facing = FGSTN_GravityFrame::DirectionOfYaw(FGSTN_StartupSteer::DequantizeYaw(static_cast<int16>(Sync->Core.Hit.MoveOrigin.RotationQ[1])), Up);
	}
	const FVector Fallback = FVector::CrossProduct(FVector::RightVector, Up);
	const FVector Forward = FGSTN_DashMath::HorizontalForward(Up, Facing, Fallback);

	// The long jump keeps the run's speed (CarriedSpeedScale): the velocity at the end of the previous frame, along the jump's direction
	FGSTN_DashMoveData Effective = Move;
	Effective.ForwardSpeed = FGSTN_DashMath::EffectiveForwardSpeed(Move, Tracker->GetVelocity(), Forward);
	const FVector Impulse = FGSTN_DashMath::ImpulseVelocity(Up, Forward, Effective);
	Effect.VelocityToApply = FGSTN_DashMath::ResolveVelocity(Tracker->GetVelocity(), Impulse, Move.MixMode, Move.MomentumMode);

	Effect.ForceMovementMode = Tracker->GetMovementModeName();
	if (Move.ModeChange == EGSTN_DashModeChange::AirMode)
	{
		if (const UCommonLegacyMovementSettings* CommonSettings = TargetMoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>())
		{
			Effect.ForceMovementMode = CommonSettings->AirMovementModeName;
		}
	}
	return FEffectMoveContainer(&Effect);
}

void UGSTN_GE_MoveBase::Configure(const FGSTN_DashMoveData& Move)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_MoveImpulseCreator* Creator = CreateDefaultSubobject<UGSTN_MoveImpulseCreator>(TEXT("MoveImpulseCreator"));
	Creator->Move = Move;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}

UGSTN_GE_MoveDash::UGSTN_GE_MoveDash()
{
	Configure(FGSTN_MoveTimelines::DashMove());
}

UGSTN_GE_MoveGroundDodge::UGSTN_GE_MoveGroundDodge()
{
	Configure(FGSTN_MoveTimelines::GroundDodgeMove());
}

UGSTN_GE_MoveAirDodge::UGSTN_GE_MoveAirDodge()
{
	// Owner 2026-11-17: the air dodge is the ground one for now (AirDodgeMove, the dive roll, stays for the real air dodge)
	Configure(FGSTN_MoveTimelines::GroundDodgeMove());
}

UGSTN_GA_MoveTimeline::UGSTN_GA_MoveTimeline(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_GA_MoveTimeline::SetDefaultMove(const FGSTN_MoveTimelineParams& Params, TSubclassOf<UGameplayEffect> Effect)
{
	Timeline = FGSTN_MoveTimelines::Make(Params);
	MoveEffect = Effect;
}

void UGSTN_GA_MoveTimeline::BlockInHitReaction()
{
	ActivationBlockedTags.AddTag(GSTNCombatTags::State_Hitstun);
	ActivationBlockedTags.AddTag(GSTNCombatTags::State_Hitstop);
}

const FGSTN_TimelineTrack_Movement* UGSTN_GA_MoveTimeline::FindMovementTrack() const
{
	// The running entry's data: a modded move moves with its own (baked) movement track
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : GetRunningTimelineData().Tracks)
	{
		const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>();
		if (Base && Base->GetKind() == EGSTN_TimelineTrackKind::Movement)
		{
			return static_cast<const FGSTN_TimelineTrack_Movement*>(Base);
		}
	}
	return nullptr;
}

void UGSTN_GA_MoveTimeline::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Before the base: it starts the timeline, which applies the move
	bLeftGround.Value = false;
	LandedFrame.Value = INDEX_NONE;
	LandedLagFrames.Value = 0;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGSTN_GA_MoveTimeline::OnTimelineBegan(int32 StateStartFrame)
{
	Super::OnTimelineBegan(StateStartFrame);
	if (MoveEffect)
	{
		ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), MoveEffect.GetDefaultObject(), GetAbilityLevel());
	}
}

void UGSTN_GA_MoveTimeline::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive() || !HasTimelineStarted())
	{
		return;
	}
	const FGSTN_TimelineTrack_Movement* Track = FindMovementTrack();
	UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	const UGSTN_MoverSimStateTracker* Tracker = Combat ? Combat->GetMoverTracker() : nullptr;
	if (!Track || !Track->bCancelOnLand || !Tracker)
	{
		return;
	}

	// The ground as of the end of the previous frame: the take-off frame still reads grounded, so landing only counts after the air was seen
	if (!Tracker->HasGameplayTag(Mover_IsOnGround, true))
	{
		bLeftGround.Value = true;
		LandedFrame.Value = INDEX_NONE;
		return;
	}
	if (!bLeftGround.Value)
	{
		return;
	}

	const int32 Frame = Combat->GetSimulationFrame();
	if (LandedFrame.Value == INDEX_NONE)
	{
		LandedFrame.Value = Frame;
		// A dodge opens its landing in the combat state (the lag, the re-dodge lock, the belly slide) and holds for the lag it resolved
		LandedLagFrames.Value = bDodgeLanding ? Combat->OpenDodgeLanding(Frame, Track->LandingLagFrames) : Track->LandingLagFrames;
	}
	if (Frame - LandedFrame.Value >= LandedLagFrames.Value)
	{
		UE_LOG(LogGSTNMoveTimeline, Verbose, TEXT("%s: landed on frame %d, lag %d: ended"), *GetName(), LandedFrame.Value, LandedLagFrames.Value);
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

UGSTN_GA_Dash::UGSTN_GA_Dash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultMove(FGSTN_MoveTimelines::DashParams(), UGSTN_GE_MoveDash::StaticClass());
}

UGSTN_GA_GroundDodge::UGSTN_GA_GroundDodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultMove(FGSTN_MoveTimelines::GroundDodgeParams(FGSTN_MoveTimelines::GroundDodgeMove(), FGSTN_MoveTimelines::DefaultTickRate), UGSTN_GE_MoveGroundDodge::StaticClass());
	BlockInHitReaction();
	bDodgeLanding = true;
}

UGSTN_GA_AirDodge::UGSTN_GA_AirDodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetDefaultMove(FGSTN_MoveTimelines::AirDodgeFromGroundParams(FGSTN_MoveTimelines::GroundDodgeMove(), FGSTN_MoveTimelines::DefaultTickRate), UGSTN_GE_MoveAirDodge::StaticClass());
	BlockInHitReaction();
	bDodgeLanding = true;
}
