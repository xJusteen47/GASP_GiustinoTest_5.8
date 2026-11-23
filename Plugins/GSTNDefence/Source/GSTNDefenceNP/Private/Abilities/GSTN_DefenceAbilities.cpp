// Defence, Network Prediction half.

#include "Abilities/GSTN_DefenceAbilities.h"
#include "Abilities/GSTN_GA_Shield.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Timeline/GSTN_TimelineTypes.h"

#include "AbilitySystemComponent.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceLibrary.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputTags.h"
#include "Math/GSTN_DefenceMath.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceTickExtension.h"
#include "Sim/GSTN_TumbleLanding.h"
#include "GSTN_CombatNPTags.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Simulation/GSTN_CombatSimComponent.h"

UGSTN_GA_Shield::UGSTN_GA_Shield(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The out-of-shield option every kit has: the jump (m6 spec 5.3). Kits add the rest as data (up special, grab, dodge, platform drop)
	FGSTN_OutOfShieldOption Jump;
	Jump.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Movement_Jump.GetTag()));
	ShieldSpec.OutOfShieldOptions.Add(MoveTemp(Jump));
}

void UGSTN_GA_Shield::RebuildTimeline()
{
	FGSTN_ShieldRules Rules;
	Rules.StartupFrames = FMath::Max(StartupFrames, 0);
	Rules.ReleaseFrames = FMath::Max(ReleaseFrames, 0);
	Timeline = ShieldSpec.BuildTimeline(Rules);
}

void UGSTN_GA_Shield::PostInitProperties()
{
	Super::PostInitProperties();
	RebuildTimeline();
}

#if WITH_EDITOR
void UGSTN_GA_Shield::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildTimeline();
}
#endif

bool UGSTN_GA_Shield::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UGSTN_CombatSimComponent* Combat = Avatar ? Avatar->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_DefenceEntityData* Data = FGSTN_DefenceAttach::Find(Avatar);
	if (!AbilitySystem || !Combat || !Data || !Data->Rules)
	{
		// No defence attached: the binding should not exist; refuse as the safeguard the spec asks for
		return false;
	}
	// The shield of the frame: the sync state the combat tick is on (its latest frame), the attribute rates of the end of F-1
	const FGSTN_CombatSyncState* Sync = Combat->GetSyncState();
	const FGSTN_DefenceFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Sync->Fragments) : nullptr;
	const FGSTN_DefenceFragment Defaults;
	const FGSTN_DefenceFragment& Frag = Fragment ? *Fragment : Defaults;
	const FGSTN_ShieldRates Rates = FGSTN_DefenceTickExtension::ReadRates(AbilitySystem, *Data->Rules);
	const int32 Frame = Sync ? Sync->Core.Input.History.GetLatestFrame() : 0;
	const double Value = UGSTN_DefenceMath::ShieldValueAt(Frag.MakeShieldView(Rates), FMath::Max(Frame, 0), Rates);
	// Grounded is the binding's requirement (Input.Context.Grounded of the input context provider, not an ability system tag): a command that
	// reached the ability met it on this frame
	const bool bGrounded = true;
	EGSTN_ShieldRefusal Reason = EGSTN_ShieldRefusal::None;
	return UGSTN_DefenceLibrary::CanExecuteShield(Data->Rules, Data->bKitHasShield, Value, Frag.IsBreaking(), bGrounded, Reason);
}

namespace
{
	/** The plain timeline of a thin defence ability: TotalFrames long, actionable at its end, no track (content adds its own) */
	void BuildFixedTimeline(FGSTN_CombatTimelineData& Timeline, int32 TotalFrames)
	{
		if (Timeline.Tracks.IsEmpty())
		{
			Timeline.TotalFrames = FMath::Max(TotalFrames, 1);
			Timeline.FirstActionableFrame = Timeline.TotalFrames;
		}
	}

	/**
	 * A get-up's timeline: BuildFixedTimeline, then control back on ActionableFrame (owner 2026-11-21) with the rest of the clip as the
	 * cancellable tail. ActionableFrame lives outside the timeline struct on purpose: an FAF written into the struct of the Blueprint came
	 * back as TotalFrames after a save and reload (getup-momentum-1), while plain properties of the class survive; applying it on every
	 * init, load and edit makes the result independent of the order those run in. 0 or >= TotalFrames = actionable at the end, no tail
	 */
	void BuildGetupTimeline(FGSTN_CombatTimelineData& Timeline, int32 TotalFrames, int32 ActionableFrame)
	{
		BuildFixedTimeline(Timeline, TotalFrames);
		if (!Timeline.Tracks.IsEmpty())
		{
			return;
		}
		const bool bTail = ActionableFrame > 0 && ActionableFrame < Timeline.TotalFrames;
		Timeline.FirstActionableFrame = bTail ? ActionableFrame : Timeline.TotalFrames;
		Timeline.Tail.bHasTail = bTail;
		Timeline.Tail.StartFrame = INDEX_NONE;
	}

	// Debug knobs for the get-up exit momentum (GSTN Debug window, one control each). -1 = the ability's data. Read only by the simulation of
	// the get-up end, the same value on every machine of a PIE session
	TAutoConsoleVariable<float> CVarRollExitSpeed(TEXT("gstn.Getup.RollExitSpeed"), -1.f,
		TEXT("Speed (cm/s) a get-up roll toward the feet hands to the next move when cut in its tail. -1 = ability data, 0 = none"));
	TAutoConsoleVariable<float> CVarRollBackExitSpeed(TEXT("gstn.Getup.RollBackExitSpeed"), -1.f,
		TEXT("Speed (cm/s) a get-up roll toward the head hands to the next move when cut in its tail. -1 = ability data, 0 = none"));
	TAutoConsoleVariable<float> CVarStandExitSpeed(TEXT("gstn.Getup.StandExitSpeed"), -1.f,
		TEXT("Speed (cm/s) the neutral get-up hands to the next move along the facing when cut in its tail. -1 = ability data, 0 = none"));
	// Owner 2026-11-23: every get-up hands on the same speed. One knob for all three, under the per-move ones above (which stay for tests)
	TAutoConsoleVariable<float> CVarExitSpeed(TEXT("gstn.Getup.ExitSpeed"), 160.f,
		TEXT("Speed (cm/s) EVERY get-up (roll, roll back, stand-up) hands to the next move when cut in its tail; the per-move knobs win when >= 0. -1 = ability data, 0 = none"));

	// The knobs below shape the get-up timelines, which the combat tick reads from the class defaults: a change rebuilds every get-up default
	void RefreshGetupDefaults(IConsoleVariable* /*Changed*/);

	TAutoConsoleVariable<int32> CVarStickCutsTail(TEXT("gstn.Getup.StickCutsTail"), -1,
		TEXT("The move stick alone cuts a get-up's tail (running away keeps the momentum). -1 = ability data (on), 0 = off, 1 = on"),
		FConsoleVariableDelegate::CreateStatic(&RefreshGetupDefaults));
	TAutoConsoleVariable<float> CVarWarpMaxDeg(TEXT("gstn.Getup.WarpMaxDeg"), -1.f,
		TEXT("Directional get-up roll: how far (deg) the lying body turns toward the stick before it rolls. -1 = ability data (45), 0 = off, 90 = any direction"),
		FConsoleVariableDelegate::CreateStatic(&RefreshGetupDefaults));
	TAutoConsoleVariable<int32> CVarWarpFrames(TEXT("gstn.Getup.WarpFrames"), -1,
		TEXT("Directional get-up roll: frames the turn takes from the roll's start. -1 = ability data (until the roll starts travelling)"),
		FConsoleVariableDelegate::CreateStatic(&RefreshGetupDefaults));

	void RefreshGetupDefaults(IConsoleVariable* /*Changed*/)
	{
		// Native classes and Blueprint children alike: the class default is what the combat tick reads (UGSTN_CombatTableSet::GetTimelineData).
		// A knob set from an ini before the object system is up needs nothing: every default reads the knobs when it is built
		if (!UObjectInitialized())
		{
			return;
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				continue;
			}
			if (Class->IsChildOf(UGSTN_GA_GetupRoll::StaticClass()))
			{
				Class->GetDefaultObject<UGSTN_GA_GetupRoll>()->RebuildGetupTimeline();
			}
			else if (Class->IsChildOf(UGSTN_GA_GetupNeutral::StaticClass()))
			{
				Class->GetDefaultObject<UGSTN_GA_GetupNeutral>()->RebuildGetupTimeline();
			}
		}
	}

	/** The tail of a get-up cut by the stick: only with a tail, from the data and the knob */
	void ApplyStickCut(FGSTN_CombatTimelineData& Timeline, bool bData)
	{
		Timeline.Tail.bStickCuts = Timeline.Tail.bHasTail && FGSTN_GetupShape::ResolveFlag(bData, CVarStickCutsTail.GetValueOnAnyThread());
	}
}

bool FGSTN_GetupShape::ResolveFlag(bool bData, int32 CVarValue)
{
	return CVarValue < 0 ? bData : CVarValue != 0;
}

float FGSTN_GetupShape::ResolveValue(float DataValue, float CVarValue)
{
	return CVarValue >= 0.f ? CVarValue : DataValue;
}

int32 FGSTN_GetupShape::FirstTravelFrame(const TArray<FGSTN_GetupTravelKey>& Keys)
{
	for (int32 Index = 0; Index + 1 < Keys.Num(); ++Index)
	{
		if (Keys[Index + 1].Frame > Keys[Index].Frame && !FMath::IsNearlyEqual(Keys[Index + 1].Fraction, Keys[Index].Fraction))
		{
			return Keys[Index].Frame;
		}
	}
	return INDEX_NONE;
}

void FGSTN_GetupShape::ApplyWarp(FGSTN_CombatTimelineData& Timeline, float MaxDegrees, int32 Frames, bool bAwayFromStick, float StepSeconds)
{
	const float Max = FMath::Clamp(MaxDegrees, 0.f, 90.f);
	if (Max <= 0.f || Frames <= 0 || StepSeconds <= 0.f)
	{
		// No turn: the stick neither steers nor aims a get-up (the default 20 degree reticle steer of a timeline does not belong to a lying body)
		Timeline.MaxSteerDegrees = 0.f;
		Timeline.SteerRateDegPerSecond = 0.f;
		return;
	}
	Timeline.MaxSteerDegrees = Max;
	// The whole cone in the window: a full push reaches the cap on the window's last frame, a smaller angle sooner. Never a snap
	Timeline.SteerRateDegPerSecond = Max / (static_cast<float>(Frames) * StepSeconds);
	Timeline.Steer.Source = EGSTN_SteerSource::MoveStick;
	Timeline.Steer.StartFrame = 0;
	Timeline.Steer.EndFrame = Frames;
	Timeline.Steer.bHoldFacing = true;
	Timeline.Steer.bAwayFromStick = bAwayFromStick;
	Timeline.Steer.RateScale = FRuntimeFloatCurve();
}

float FGSTN_GetupExit::ResolveSpeed(float DataSpeedCmPerSecond, float CVarSpeedCmPerSecond)
{
	return FMath::Max(CVarSpeedCmPerSecond >= 0.f ? CVarSpeedCmPerSecond : DataSpeedCmPerSecond, 0.f);
}

float FGSTN_GetupExit::ResolveSpeed(float DataSpeedCmPerSecond, float CVarSpeedCmPerSecond, float SharedCVarSpeedCmPerSecond)
{
	return ResolveSpeed(SharedCVarSpeedCmPerSecond >= 0.f ? SharedCVarSpeedCmPerSecond : DataSpeedCmPerSecond, CVarSpeedCmPerSecond);
}

FVector FGSTN_GetupExit::ExitVelocity(EGSTN_MoveEndReason Reason, const FVector& Along, float SpeedCmPerSecond)
{
	if (Reason != EGSTN_MoveEndReason::TailCancelled || SpeedCmPerSecond <= 0.f)
	{
		return FVector::ZeroVector;
	}
	return Along.GetSafeNormal2D() * SpeedCmPerSecond;
}

void FGSTN_GetupExit::Apply(UAbilitySystemComponent& AbilitySystem, const FVector& Velocity)
{
	if (Velocity.IsNearlyZero() || !FGSTN_TumbleLanding::CanQueueMoves(&AbilitySystem))
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(UGSTN_GE_PushBack::StaticClass(), 1.f, AbilitySystem.MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	// One frame of the roll's own push, no decay: the push leaves its velocity to the movement mode when it ends (the reason the roll
	// zeroes its last key), so walking, a jump or a move's momentum slide starts from it
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, 0.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 0.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, 1.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, FGSTN_TumbleLanding::FrameSeconds);
	AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

UGSTN_GA_Tech::UGSTN_GA_Tech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The stick must not turn a lying pawn: the roll-back reads the facing the knockdown left (react-hook-2 fail)
	Timeline.bIgnoreMoveInput = true;
}

UGSTN_GA_GroundBounce::UGSTN_GA_GroundBounce(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The stick must not turn a lying pawn: the roll-back reads the facing the knockdown left (react-hook-2 fail)
	Timeline.bIgnoreMoveInput = true;
}

UGSTN_GA_GetupRoll::UGSTN_GA_GetupRoll(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The stick neither walks nor turns a get-up: its only motion is its own travel (react-hook-1 review)
	Timeline.bIgnoreMoveInput = true;
	LastTravelFrame.Value = INDEX_NONE;
}

UGSTN_GA_GetupNeutral::UGSTN_GA_GetupNeutral(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The stick neither walks nor turns a get-up: its only motion is its own travel (react-hook-1 review)
	Timeline.bIgnoreMoveInput = true;
}

UGSTN_GA_CounterSample::UGSTN_GA_CounterSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AttackCommands.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Command.Attack"), false));
}

void UGSTN_GA_CounterSample::RebuildTimeline()
{
	const int32 Total = FMath::Max(TotalFrames, 1);
	const int32 Start = FMath::Clamp(ParryStartFrame, 0, Total - 1);
	const int32 End = FMath::Clamp(ParryEndFrame, Start + 1, Total);
	Timeline = FGSTN_CombatTimelineData();
	Timeline.TotalFrames = Total;
	Timeline.FirstActionableFrame = Total;

	FGSTN_ParryWindowTrack Parry;
	Parry.StartFrame = Start;
	Parry.EndFrame = End;
	Parry.ProfileName = ProfileName;
	Parry.bCloseOnParry = true;
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_ParryWindowTrack>(Parry));

	FGSTN_TimelineTrack_CancelWindow Counter;
	Counter.StartFrame = Start;
	Counter.EndFrame = Total;
	Counter.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(AttackCommands);
	Counter.Condition = EGSTN_CancelCondition::OnParry;
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Counter));
}

void UGSTN_GA_CounterSample::PostInitProperties()
{
	Super::PostInitProperties();
	RebuildTimeline();
}

#if WITH_EDITOR
void UGSTN_GA_CounterSample::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildTimeline();
}
#endif

void UGSTN_GA_Tech::PostInitProperties()
{
	Super::PostInitProperties();
	BuildFixedTimeline(Timeline, TotalFrames);
}

#if WITH_EDITOR
void UGSTN_GA_Tech::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildFixedTimeline(Timeline, TotalFrames);
}
#endif

void UGSTN_GA_GroundBounce::PostInitProperties()
{
	Super::PostInitProperties();
	BuildFixedTimeline(Timeline, TotalFrames);
}

#if WITH_EDITOR
void UGSTN_GA_GroundBounce::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildFixedTimeline(Timeline, TotalFrames);
}
#endif

void UGSTN_GA_GetupRoll::PostLoad()
{
	Super::PostLoad();
	RebuildGetupTimeline();
}

void UGSTN_GA_GetupRoll::PostInitProperties()
{
	Super::PostInitProperties();
	RebuildGetupTimeline();
}

#if WITH_EDITOR
void UGSTN_GA_GetupRoll::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildGetupTimeline();
}
#endif

void UGSTN_GA_GetupRoll::RebuildGetupTimeline()
{
	BuildGetupTimeline(Timeline, TotalFrames, ActionableFrame);
	ApplyStickCut(Timeline, bStickCutsTail);
	// The turn is over before the body leaves the floor: by the first frame that travels unless the data or the knob says otherwise
	const int32 DataFrames = WarpFrames >= 0 ? WarpFrames : FGSTN_GetupShape::FirstTravelFrame(TravelKeys);
	const int32 Frames = CVarWarpFrames.GetValueOnAnyThread() >= 0 ? CVarWarpFrames.GetValueOnAnyThread() : DataFrames;
	FGSTN_GetupShape::ApplyWarp(Timeline, FGSTN_GetupShape::ResolveValue(WarpMaxDegrees, CVarWarpMaxDeg.GetValueOnAnyThread()), Frames,
		TravelDirection == EGSTN_GetupTravelDirection::AgainstFacing, FGSTN_TumbleLanding::FrameSeconds);
}

void UGSTN_GA_GetupNeutral::RebuildGetupTimeline()
{
	BuildGetupTimeline(Timeline, TotalFrames, ActionableFrame);
	ApplyStickCut(Timeline, bStickCutsTail);
}

bool UGSTN_GA_GetupRoll::SegmentStartingAt(const TArray<FGSTN_GetupTravelKey>& Keys, float DistanceCm, int32 Frame, float& OutSpeed, int32& OutFrames)
{
	OutSpeed = 0.f;
	OutFrames = 0;
	if (DistanceCm <= 0.f)
	{
		return false;
	}
	for (int32 Index = 0; Index + 1 < Keys.Num(); ++Index)
	{
		const FGSTN_GetupTravelKey& From = Keys[Index];
		const FGSTN_GetupTravelKey& To = Keys[Index + 1];
		const int32 Frames = To.Frame - From.Frame;
		const float Delta = To.Fraction - From.Fraction;
		if (From.Frame == Frame && Frames > 0 && !FMath::IsNearlyZero(Delta))
		{
			OutFrames = Frames;
			OutSpeed = DistanceCm * Delta / (Frames * FGSTN_TumbleLanding::FrameSeconds);
			return true;
		}
	}
	return false;
}

void UGSTN_GA_GetupRoll::OnTimelineBegan(int32 StateStartFrame)
{
	Super::OnTimelineBegan(StateStartFrame);
	LastTravelFrame.Value = INDEX_NONE;
}

void UGSTN_GA_GetupRoll::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (!IsActive() || !HasTimelineStarted() || TravelDistanceCm <= 0.f || TravelKeys.Num() < 2)
	{
		return;
	}
	const int32 Frame = GetTimelineFrame();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (Frame <= LastTravelFrame.Value || !AbilitySystem || !FGSTN_TumbleLanding::CanQueueMoves(AbilitySystem))
	{
		return;
	}
	// Every segment start crossed since the last tick (normally one frame): a resimulation from before the start queues the same pushes again,
	// since LastTravelFrame is rolled back with the ability
	const FVector Facing = FGSTN_TumbleLanding::ReadFacing(GetAvatarActorFromActorInfo());
	const FVector Along = TravelDirection == EGSTN_GetupTravelDirection::Facing ? Facing : -Facing;
	const int32 LastKeyFrame = TravelKeys.Last().Frame;
	for (int32 Check = LastTravelFrame.Value + 1; Check <= Frame; ++Check)
	{
		// The roll starts and ends at rest (react-hook-1 review: it kept sliding after the clip): what the bounce left, or the last push, is
		// zeroed by a one-frame freeze, so the travel is the keys' distance and nothing more
		if (Check == 0 || Check == LastKeyFrame)
		{
			const FGameplayEffectSpecHandle Hold = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_FreezeZeroVelocity::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
			if (Hold.IsValid())
			{
				Hold.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FreezeSeconds, FGSTN_TumbleLanding::FrameSeconds);
				AbilitySystem->ApplyGameplayEffectSpecToSelf(*Hold.Data.Get());
			}
		}
		float Speed = 0.f;
		int32 Frames = 0;
		if (!SegmentStartingAt(TravelKeys, TravelDistanceCm, Check, Speed, Frames))
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_PushBack::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		// The push of the core (as the tech roll): constant velocity on the ground plane, no decay, normal gravity, for the segment's frames
		const FVector Velocity = Along.GetSafeNormal2D() * Speed;
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Velocity.Z));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 0.f);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, 1.f);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, Frames * FGSTN_TumbleLanding::FrameSeconds);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	LastTravelFrame.Value = Frame;
}

void UGSTN_GA_GetupRoll::OnMoveEnded(EGSTN_MoveEndReason Reason)
{
	Super::OnMoveEnded(Reason);
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return;
	}
	const bool bTowardFeet = TravelDirection == EGSTN_GetupTravelDirection::Facing;
	const float Speed = FGSTN_GetupExit::ResolveSpeed(ExitSpeed, (bTowardFeet ? CVarRollExitSpeed : CVarRollBackExitSpeed).GetValueOnAnyThread(),
		CVarExitSpeed.GetValueOnAnyThread());
	const FVector Facing = FGSTN_TumbleLanding::ReadFacing(GetAvatarActorFromActorInfo());
	FGSTN_GetupExit::Apply(*AbilitySystem, FGSTN_GetupExit::ExitVelocity(Reason, bTowardFeet ? Facing : -Facing, Speed));
}

void UGSTN_GA_GetupNeutral::OnMoveEnded(EGSTN_MoveEndReason Reason)
{
	Super::OnMoveEnded(Reason);
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return;
	}
	const float Speed = FGSTN_GetupExit::ResolveSpeed(ExitSpeed, CVarStandExitSpeed.GetValueOnAnyThread(), CVarExitSpeed.GetValueOnAnyThread());
	FGSTN_GetupExit::Apply(*AbilitySystem, FGSTN_GetupExit::ExitVelocity(Reason, FGSTN_TumbleLanding::ReadFacing(GetAvatarActorFromActorInfo()), Speed));
}

void UGSTN_GA_GetupNeutral::PostLoad()
{
	Super::PostLoad();
	RebuildGetupTimeline();
}

void UGSTN_GA_GetupNeutral::PostInitProperties()
{
	Super::PostInitProperties();
	RebuildGetupTimeline();
}

#if WITH_EDITOR
void UGSTN_GA_GetupNeutral::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildGetupTimeline();
}
#endif
