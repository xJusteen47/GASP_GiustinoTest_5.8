// Defence, Network Prediction half.

#include "Abilities/GSTN_DefenceAbilities.h"
#include "Abilities/GSTN_GA_Shield.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Timeline/GSTN_TimelineTypes.h"

#include "AbilitySystemComponent.h"
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
}

UGSTN_GA_Tech::UGSTN_GA_Tech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGSTN_GA_GroundBounce::UGSTN_GA_GroundBounce(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

void UGSTN_GA_GetupRoll::PostInitProperties()
{
	Super::PostInitProperties();
	BuildFixedTimeline(Timeline, TotalFrames);
}

#if WITH_EDITOR
void UGSTN_GA_GetupRoll::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildFixedTimeline(Timeline, TotalFrames);
}
#endif

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

void UGSTN_GA_GetupNeutral::PostInitProperties()
{
	Super::PostInitProperties();
	BuildFixedTimeline(Timeline, TotalFrames);
}

#if WITH_EDITOR
void UGSTN_GA_GetupNeutral::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildFixedTimeline(Timeline, TotalFrames);
}
#endif
