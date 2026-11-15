// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_GA_BaseCombat.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Curves/CurveVector.h"
#include "GameplayEffect.h"
#include "GSTN_CombatTags.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Timeline/GSTN_CombatTimeline.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNBaseCombat, Log, All);

UGSTN_GA_BaseCombat::UGSTN_GA_BaseCombat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(GSTNCombatTags::Ability_Timeline);

	// The cost is committed by the combat tick, and a cancel window into the same ability starts it again
	bRetriggerInstancedAbility = true;

	// Another timeline ability that interrupts this kind (InterruptCurrent) cancels the running ones of the kind
	InterruptTags.AddTag(GSTNCombatTags::Ability_Timeline);

	bStarted.Value = false;
	StartClock.Value = INDEX_NONE;
	StartFrame.Value = INDEX_NONE;
	RunningEntry.Value = INDEX_NONE;
	bAwaitingAdoption.Value = false;
	AwaitSinceFrame.Value = INDEX_NONE;
	bRecoilPlayed.Value = false;
	RecoilStartClock.Value = INDEX_NONE;
	RecoilLength.Value = 0;
	EvaluatedFrame.Value = INDEX_NONE;
	ReportedHitGroups.Value = 0;
	MoveEventsFrame.Value = INDEX_NONE;
	for (int32 Slot = 0; Slot < MaxTagWindows; ++Slot)
	{
		TagWindowTag(Slot).Value = FGameplayTag();
		TagWindowStart(Slot).Value = INDEX_NONE;
		TagWindowFrames(Slot).Value = 0;
	}
}

const FGSTN_CombatTimelineData& UGSTN_GA_BaseCombat::GetTimelineData() const
{
	if (!TimelineAsset.IsNull())
	{
		if (const UGSTN_CombatTimeline* Asset = TimelineAsset.Get())
		{
			return Asset->Data;
		}
	}
	return Timeline;
}

void UGSTN_GA_BaseCombat::LoadTimelineContent(TArray<TObjectPtr<UObject>>& OutKeep) const
{
	if (!TimelineAsset.IsNull())
	{
		OutKeep.AddUnique(TimelineAsset.LoadSynchronous());
	}
	LoadTimelineDataContent(GetTimelineData(), OutKeep);
}

const FGSTN_CombatTimelineData& UGSTN_GA_BaseCombat::GetRunningTimelineData() const
{
	// The entry is checked against the class: a table set of another revision may hold another move at that index
	const UGSTN_CombatSimComponent* Combat = RunningEntry.Value != INDEX_NONE ? GetCombatComponent() : nullptr;
	const UGSTN_CombatTableSet* Set = Combat ? Combat->GetActiveTableSet() : nullptr;
	if (Set && Set->TimelineRegistry.IsValidIndex(RunningEntry.Value) && Set->TimelineRegistry[RunningEntry.Value] == GetClass())
	{
		if (const FGSTN_CombatTimelineData* Data = Set->GetTimelineData(RunningEntry.Value))
		{
			return *Data;
		}
	}
	return GetTimelineData();
}

void UGSTN_GA_BaseCombat::LoadTimelineDataContent(const FGSTN_CombatTimelineData& Data, TArray<TObjectPtr<UObject>>& OutKeep)
{
	// The synced montage is played from a plain pointer: it has to be in memory before the first frame of the timeline
	if (!Data.Montage.IsNull())
	{
		OutKeep.AddUnique(Data.Montage.LoadSynchronous());
	}
	if (const FGSTN_RicochetResponse* Recoil = Data.GetRicochetResponse())
	{
		LoadRecoilContent(*Recoil, OutKeep);
	}
	// Each hitbox group's own recoil (plan precisione-weakpoint): played from a plain pointer like the move's
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Data.GetHitboxTrack(TrackIndex);
		if (Track && Track->bHasRicochetResponse && Track->RicochetResponse.IsAuthored())
		{
			LoadRecoilContent(Track->RicochetResponse, OutKeep);
		}
	}
}

void UGSTN_GA_BaseCombat::LoadRecoilContent(const FGSTN_RicochetResponse& Response, TArray<TObjectPtr<UObject>>& OutKeep)
{
	if (!Response.RecoilMontage.IsNull())
	{
		OutKeep.AddUnique(Response.RecoilMontage.LoadSynchronous());
	}
	if (Response.bHasMovement && !Response.Movement.VelocityCurve.IsNull())
	{
		OutKeep.AddUnique(Response.Movement.VelocityCurve.LoadSynchronous());
	}
}

bool UGSTN_GA_BaseCombat::CommitOrCancel(UAbilitySystemComponent& AbilitySystem, const FGameplayAbilitySpec& Spec)
{
	UGameplayAbility* Instance = Spec.IsActive() ? Spec.GetPrimaryInstance() : nullptr;
	if (!Instance)
	{
		return false;
	}
	if (Instance->CommitAbility(Spec.Handle, AbilitySystem.AbilityActorInfo.Get(), Instance->GetCurrentActivationInfo()))
	{
		return true;
	}
	AbilitySystem.CancelAbilityHandle(Spec.Handle);
	return false;
}

void UGSTN_GA_BaseCombat::PreloadTimelineContent()
{
	LoadedContent.Reset();
	LoadTimelineContent(LoadedContent);
}

void UGSTN_GA_BaseCombat::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	PreloadTimelineContent();
}

bool UGSTN_GA_BaseCombat::IsStillTheRunningTimeline() const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	int32 StateStart = INDEX_NONE;
	int32 StateEntry = INDEX_NONE;
	return Combat && Combat->IsTimelineRunning(GetClass(), StateStart, &StateEntry) && StateStart == StartFrame.Value
		&& (RunningEntry.Value == INDEX_NONE || StateEntry == RunningEntry.Value);
}

void UGSTN_GA_BaseCombat::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// A retrigger of an instance that still waits: its tag goes before the variables are rewritten
	ClearAwaitingAdoption();

	// Captures the activation context and applies the interrupt policy
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bStarted.Value = false;
	StartClock.Value = INDEX_NONE;
	StartFrame.Value = INDEX_NONE;
	RunningEntry.Value = INDEX_NONE;
	AwaitSinceFrame.Value = INDEX_NONE;
	bRecoilPlayed.Value = false;
	RecoilStartClock.Value = INDEX_NONE;
	RecoilLength.Value = 0;
	EvaluatedFrame.Value = INDEX_NONE;
	ReportedHitGroups.Value = 0;
	MoveEventsFrame.Value = INDEX_NONE;

	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (!Combat || !AbilitySystem)
	{
		UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: no combat component or ability system on the avatar: ended"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// A command: the combat tick wrote this timeline's start into its state in this frame, before the ability system ticked
	int32 StateStart = INDEX_NONE;
	if (Combat->IsTimelineRunning(GetClass(), StateStart) && StateStart == Combat->GetSimulationFrame())
	{
		BeginTimeline(StateStart);
		return;
	}

	// Anything else (a trigger, an effect, another ability, AI): wait for the combat tick of the next frame to adopt this
	// activation (combat-m3 7.4). The loose tag is what lets that tick skip the scan on every frame nothing waits
	bAwaitingAdoption.Value = true;
	AwaitSinceFrame.Value = Combat->GetSimulationFrame();
	AbilitySystem->AddLooseGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption);
}

void UGSTN_GA_BaseCombat::BeginTimeline(int32 StateStart)
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	bStarted.Value = true;
	StartFrame.Value = StateStart;
	StartClock.Value = Combat ? Combat->GetCombatClock() : INDEX_NONE;
	int32 EntryStart = INDEX_NONE;
	int32 Entry = INDEX_NONE;
	RunningEntry.Value = Combat && Combat->IsTimelineRunning(GetClass(), EntryStart, &Entry) ? Entry : INDEX_NONE;

	const FGSTN_CombatTimelineData& Data = GetRunningTimelineData();

	// Momentum slide (FGSTN_MomentumSlide): queued before the montage starts, so the slide move is ahead of this frame's root motion move
	// and wins the override mix (it adds the root motion itself). The move decides on its own first frame whether there is anything to slide
	if (Data.MomentumSlide.bEnabled)
	{
		if (UGSTN_CombatSimComponent* SlideCombat = GetCombatComponent())
		{
			SlideCombat->ExecuteMomentumSlide();
		}
	}

	UAnimMontage* Montage = Data.Montage.Get();
	TimelineMontagePlayed = Montage;
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (Montage && AbilitySystem)
	{
		AbilitySystem->PlaySyncedMontage(Montage, 0.f, Data.MontagePlayRate, Data.MontageSection, 1.f);
	}
	OnTimelineBegan(StateStart);

	// The Blueprint move events (CP1): the start, then what this first frame already holds (a key on frame 0). The cursors start empty; a
	// new run of the same ability (a cancel window into itself) starts them again
	EvaluatedFrame.Value = INDEX_NONE;
	ReportedHitGroups.Value = 0;
	MoveEventsFrame.Value = INDEX_NONE;
	if (CanRaiseMoveEvents())
	{
		K2_OnMoveStarted(StateStart);
		if (IsActive() && bStarted.Value)
		{
			RaiseMoveEvents();
		}
	}
}

bool UGSTN_GA_BaseCombat::CanRaiseMoveEvents() const
{
	const UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	return AbilitySystem && !AbilitySystem->GetIsRestoringFrame();
}

void UGSTN_GA_BaseCombat::RaiseMoveEvents()
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	if (!Combat || !CanRaiseMoveEvents())
	{
		return;
	}
	// Once per simulation frame: the activation and the tick of the start frame both get here, and the combat tick that wrote this frame's
	// state ran before both
	const int32 SimFrame = Combat->GetSimulationFrame();
	if (MoveEventsFrame.Value == SimFrame)
	{
		return;
	}
	MoveEventsFrame.Value = SimFrame;

	// Keys crossed since the last evaluated frame. The cursor moves before the handlers run, so a handler that ends or restarts the move
	// never sees a key twice
	const int32 Current = Combat->GetEvaluatedTimelineFrame();
	if (Current != INDEX_NONE && Current > EvaluatedFrame.Value)
	{
		const int32 Last = EvaluatedFrame.Value;
		EvaluatedFrame.Value = Current;
		const FGSTN_CombatTimelineData& Data = GetRunningTimelineData();
		FGSTN_MoveEventRules::FKeyList Keys;
		FGSTN_MoveEventRules::CollectKeys(Data, Last, Current, Keys);
		for (const int32 TrackIndex : Keys)
		{
			if (!IsActive() || !bStarted.Value)
			{
				return;
			}
			if (const FGSTN_TimelineTrack_Event* Key = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Event>())
			{
				RunKey(*Key, TrackIndex, Current);
			}
		}
	}

	// Hits the attacker side of this frame's combat tick confirmed (it runs before the ability system)
	const uint8 Groups = static_cast<uint8>(Combat->GetMoveHitConfirmGroups());
	const uint8 NewGroups = FGSTN_MoveEventRules::NewConfirmGroups(ReportedHitGroups.Value, Groups);
	ReportedHitGroups.Value = Groups;
	if (NewGroups != 0 && IsActive() && bStarted.Value)
	{
		OnMoveHitConfirmed(NewGroups, Groups);
	}

	if (bTickMoveInBlueprint && IsActive() && bStarted.Value)
	{
		OnMoveFrame(EvaluatedFrame.Value);
	}
}

void UGSTN_GA_BaseCombat::RunKey(const FGSTN_TimelineTrack_Event& Key, int32 TrackIndex, int32 TimelineFrame)
{
	// Until CP2 only the outbox part of a key ran (in the combat tick). The cue goes through the fork's frame-stamped cue executions (as the
	// recoil's cue), the event to this ability system as Send Move Event does; both inside the simulation, once per crossing like the event
	const auto IsRunning = [this]() { return IsActive() && bStarted.Value; };
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (AbilitySystem && Key.CueTag.IsValid())
	{
		AbilitySystem->ExecuteGameplayCue(Key.CueTag, AbilitySystem->MakeEffectContext());
	}
	if (Key.GameplayEventTag.IsValid())
	{
		SendMoveEvent(Key.GameplayEventTag, 0.f);
	}
	if (!IsRunning())
	{
		// The event started a move that replaced this one: its actions and its Blueprint event belong to a move that is over
		return;
	}
	FGSTN_MoveActionContext Context;
	Context.Ability = this;
	Context.KeyTag = Key.KeyTag;
	Context.TimelineFrame = TimelineFrame;
	FGSTN_MoveActionRules::RunActions(Key.Actions, Context, IsRunning);
	if (IsRunning())
	{
		OnMoveKey(Key, TrackIndex, TimelineFrame);
	}
}

bool UGSTN_GA_BaseCombat::CanRunMoveActions() const
{
	return IsActive() && bStarted.Value && CanRaiseMoveEvents();
}

bool UGSTN_GA_BaseCombat::SendMoveEvent(FGameplayTag EventTag, float Magnitude)
{
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (!EventTag.IsValid() || !AbilitySystem || !CanRunMoveActions())
	{
		return false;
	}
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Avatar;
	Payload.Target = Avatar;
	Payload.EventMagnitude = Magnitude;
	AbilitySystem->HandleGameplayEvent(EventTag, &Payload);
	return true;
}

bool UGSTN_GA_BaseCombat::ApplyMoveEffect(TSubclassOf<UGameplayEffect> Effect, float Level)
{
	if (!Effect || !CanRunMoveActions())
	{
		return false;
	}
	// The ability's own apply path (the one the dash and the move timelines use): the effect is part of the ability system's rolled back state
	ApplyGameplayEffectToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), Effect.GetDefaultObject(), Level);
	return true;
}

bool UGSTN_GA_BaseCombat::OpenMoveTagWindow(FGameplayTag Tag, int32 Frames)
{
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	const int32 Clock = GetCombatClock();
	if (!Tag.IsValid() || Frames < 1 || !AbilitySystem || Clock < 0 || !CanRunMoveActions())
	{
		return false;
	}
	FGameplayTag Tags[MaxTagWindows];
	int32 Remaining[MaxTagWindows];
	for (int32 Slot = 0; Slot < MaxTagWindows; ++Slot)
	{
		Tags[Slot] = TagWindowTag(Slot).Value;
		Remaining[Slot] = TagWindowFrames(Slot).Value - FGSTN_CombatClock::Elapsed(Clock, TagWindowStart(Slot).Value);
	}
	const int32 Slot = FGSTN_MoveActionRules::PickTagWindowSlot(MakeArrayView(Tags), MakeArrayView(Remaining), Tag);
	if (Slot == INDEX_NONE)
	{
		return false;
	}
	// A re-armed window keeps its loose tag (one count); a replaced one gives its own back first
	if (TagWindowTag(Slot).Value != Tag)
	{
		CloseTagWindowSlot(Slot);
		AbilitySystem->AddLooseGameplayTag(Tag);
		TagWindowTag(Slot).Value = Tag;
	}
	TagWindowStart(Slot).Value = Clock;
	TagWindowFrames(Slot).Value = Frames;
	return true;
}

bool UGSTN_GA_BaseCombat::CloseMoveTagWindow(FGameplayTag Tag)
{
	if (!Tag.IsValid() || !CanRunMoveActions())
	{
		return false;
	}
	for (int32 Slot = 0; Slot < MaxTagWindows; ++Slot)
	{
		if (TagWindowTag(Slot).Value == Tag)
		{
			CloseTagWindowSlot(Slot);
			return true;
		}
	}
	return false;
}

void UGSTN_GA_BaseCombat::CloseTagWindowSlot(int32 Slot)
{
	const FGameplayTag Tag = TagWindowTag(Slot).Value;
	if (Tag.IsValid())
	{
		if (UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent())
		{
			AbilitySystem->RemoveLooseGameplayTag(Tag);
		}
	}
	TagWindowTag(Slot).Value = FGameplayTag();
	TagWindowStart(Slot).Value = INDEX_NONE;
	TagWindowFrames(Slot).Value = 0;
}

void UGSTN_GA_BaseCombat::CloseTagWindows(bool bAll)
{
	const int32 Clock = GetCombatClock();
	for (int32 Slot = 0; Slot < MaxTagWindows; ++Slot)
	{
		if (TagWindowTag(Slot).Value.IsValid() && (bAll || FGSTN_MoveActionRules::IsTagWindowOver(Clock, TagWindowStart(Slot).Value, TagWindowFrames(Slot).Value)))
		{
			CloseTagWindowSlot(Slot);
		}
	}
}

bool UGSTN_GA_BaseCombat::RunMoveAction(const FInstancedStruct& Action)
{
	const UScriptStruct* Type = Action.GetScriptStruct();
	if (Type == nullptr || !Type->IsChildOf(FGSTN_MoveAction::StaticStruct()) || !CanRunMoveActions())
	{
		return false;
	}
	FGSTN_MoveActionContext Context;
	Context.Ability = this;
	Context.TimelineFrame = GetMoveTimelineFrame();
	return Action.Get<FGSTN_MoveAction>().Execute(Context);
}

float UGSTN_GA_BaseCombat::GetMoveRandomFloat(int32 Salt, float Min, float Max) const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	const uint32 Seed = FGSTN_MoveActionRules::MakeSeed(Combat ? Combat->GetSimulationFrame() : 0, StartFrame.Value, Salt);
	return FGSTN_MoveActionRules::RandomFloatInRange(Seed, Min, Max);
}

int32 UGSTN_GA_BaseCombat::GetMoveRandomInteger(int32 Salt, int32 Min, int32 Max) const
{
	const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	const uint32 Seed = FGSTN_MoveActionRules::MakeSeed(Combat ? Combat->GetSimulationFrame() : 0, StartFrame.Value, Salt);
	return FGSTN_MoveActionRules::RandomIntInRange(Seed, Min, Max);
}

void UGSTN_GA_BaseCombat::OnMoveKey(const FGSTN_TimelineTrack_Event& Key, int32 TrackIndex, int32 TimelineFrame)
{
	K2_OnMoveKey(Key.KeyTag, Key, TimelineFrame);
}

void UGSTN_GA_BaseCombat::OnMoveHitConfirmed(int32 NewHitGroups, int32 AllHitGroups)
{
	K2_OnMoveHitConfirmed(NewHitGroups, AllHitGroups);
}

void UGSTN_GA_BaseCombat::OnMoveEnded(EGSTN_MoveEndReason Reason)
{
	K2_OnMoveEnded(Reason);
}

void UGSTN_GA_BaseCombat::OnMoveFrame(int32 TimelineFrame)
{
	K2_OnMoveFrame(TimelineFrame);
}

int32 UGSTN_GA_BaseCombat::GetMoveTimelineFrame() const
{
	const UGSTN_CombatSimComponent* Combat = bStarted.Value ? GetCombatComponent() : nullptr;
	return Combat && IsStillTheRunningTimeline() ? Combat->GetEvaluatedTimelineFrame() : INDEX_NONE;
}

int32 UGSTN_GA_BaseCombat::GetMoveHitConfirmGroups() const
{
	const UGSTN_CombatSimComponent* Combat = bStarted.Value ? GetCombatComponent() : nullptr;
	return Combat && IsStillTheRunningTimeline() ? Combat->GetMoveHitConfirmGroups() : 0;
}

void UGSTN_GA_BaseCombat::BeginRecoil(const FGSTN_RicochetResponse& Response, float StepMs)
{
	UGSTN_CombatSimComponent* Combat = GetCombatComponent();
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	if (!Combat || !AbilitySystem)
	{
		return;
	}
	bRecoilPlayed.Value = true;
	RecoilStartClock.Value = Combat->GetCombatClock();
	RecoilLength.Value = Response.RecoilFrames;

	// The timeline's own montage gives way to the recoil's
	UAnimMontage* TimelineMontage = TimelineMontagePlayed ? TimelineMontagePlayed.Get() : GetRunningTimelineData().Montage.Get();
	float Time = 0.f;
	if (TimelineMontage && AbilitySystem->GetPlayingSyncedMontage(Time) == TimelineMontage)
	{
		AbilitySystem->StopSyncedMontage(TimelineMontage, true);
	}
	RecoilMontagePlayed = Response.RecoilMontage.Get();
	if (RecoilMontagePlayed)
	{
		AbilitySystem->PlaySyncedMontage(RecoilMontagePlayed, 0.f, 1.f, NAME_None, 1.f);
	}
	if (Response.CueTag.IsValid())
	{
		AbilitySystem->ExecuteGameplayCue(Response.CueTag, AbilitySystem->MakeEffectContext());
	}
	if (Response.bHasMovement)
	{
		Combat->ExecuteRecoilMove(Response.Movement, Response.RecoilFrames, StepMs);
	}
}

void UGSTN_GA_BaseCombat::StopRecoilMontage(bool bWasCancelled)
{
	UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	float Time = 0.f;
	if (RecoilMontagePlayed && AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(Time) == RecoilMontagePlayed)
	{
		AbilitySystem->StopSyncedMontage(RecoilMontagePlayed, bWasCancelled);
	}
}

void UGSTN_GA_BaseCombat::ClearAwaitingAdoption()
{
	if (bAwaitingAdoption.Value)
	{
		bAwaitingAdoption.Value = false;
		if (UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent())
		{
			AbilitySystem->RemoveLooseGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption);
		}
	}
}

void UGSTN_GA_BaseCombat::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);

	if (!IsActive())
	{
		return;
	}

	if (bAwaitingAdoption.Value)
	{
		const UGSTN_CombatSimComponent* Combat = GetCombatComponent();
		const int32 Frame = Combat ? Combat->GetSimulationFrame() : INDEX_NONE;
		int32 StateStart = INDEX_NONE;
		if (Combat && Combat->IsTimelineRunning(GetClass(), StateStart) && StateStart == Frame)
		{
			// The combat tick adopted this activation in this frame: from here it is a timeline like any other
			ClearAwaitingAdoption();
			SetActivationSource(EGSTN_ActivationSource::Adopted);
			BeginTimeline(StateStart);
		}
		else if (Frame - AwaitSinceFrame.Value > MaxAdoptionWaitFrames)
		{
			// Another ability was adopted in its place, or the combat model does not know this class
			UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: not adopted within %d frames (waiting since %d, frame %d): cancelled"), *GetName(), MaxAdoptionWaitFrames, AwaitSinceFrame.Value, Frame);
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
		}
		return;
	}

	if (!bStarted.Value)
	{
		return;
	}

	// A ricochet: the combat state ended the timeline into its recoil, which this ability plays from the frame the state says it begins on
	if (!bRecoilPlayed.Value)
	{
		UGSTN_CombatSimComponent* Combat = GetCombatComponent();
		int32 RecoilStart = INDEX_NONE;
		const FGSTN_RicochetResponse* Response = nullptr;
		if (Combat && IsStillTheRunningTimeline() && Combat->GetRecoil(RecoilStart, Response) && Combat->GetSimulationFrame() >= RecoilStart)
		{
			BeginRecoil(*Response, static_cast<float>(TimeStep.StepMs));
		}
	}

	// The move events of the frame (CP1), while the combat state still names this timeline; the tag windows that are over close first (CP2)
	if (IsStillTheRunningTimeline())
	{
		if (CanRaiseMoveEvents())
		{
			CloseTagWindows(false);
		}
		RaiseMoveEvents();
		return;
	}

	// The combat state no longer names this timeline: it ended, was replaced or was cancelled
	if (IsActive())
	{
		const UGSTN_CombatSimComponent* CombatNow = GetCombatComponent();
		int32 StateStartNow = INDEX_NONE;
		const bool bRunningNow = CombatNow && CombatNow->IsTimelineRunning(GetClass(), StateStartNow);
		UE_LOG(LogGSTNBaseCombat, Log, TEXT("%s: the combat state no longer names this timeline: ended (running %d, state start %d, own start %d, frame %d, step frame %d)"),
			*GetName(), bRunningNow, StateStartNow, StartFrame.Value, CombatNow ? CombatNow->GetSimulationFrame() : INDEX_NONE, TimeStep.ServerFrame);
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void UGSTN_GA_BaseCombat::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearAwaitingAdoption();
	bool bRaiseEnded = false;
	EGSTN_MoveEndReason EndReason = EGSTN_MoveEndReason::Interrupted;
	if (bStarted.Value)
	{
		// Why it ends, read before the cleanup below resets the recoil and the entry
		{
			const FGSTN_CombatTimelineData& Data = GetRunningTimelineData();
			EndReason = FGSTN_MoveEventRules::ResolveEndReason(bWasCancelled, bRecoilPlayed.Value, EvaluatedFrame.Value, Data.TotalFrames, Data.GetTailStartFrame());
			bRaiseEnded = CanRaiseMoveEvents() && IsActive();
		}

		// Stop the montage only if it is still this ability's: another timeline may have started its own in the same frame
		// The montage BeginTimeline played; after a rollback that restored a started state without it, the one of the running data
		UAnimMontage* Montage = TimelineMontagePlayed ? TimelineMontagePlayed.Get() : GetRunningTimelineData().Montage.Get();
		UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
		float Time = 0.f;
		const bool bOnSlot = Montage && AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(Time) == Montage;
		if (bOnSlot && ShouldReleaseMontageOnEnd(bWasCancelled))
		{
			// Owner rule (2026-11-15): the end of the timeline is not the end of its animation. The montage plays on to its own end and
			// blend-out; the next action stops it (StopReleasedMontages) or replaces it with its own
			bMontageReleased.Value = true;
			ReleasedMontage = Montage;
		}
		else if (bOnSlot)
		{
			// The data's blend-out on an end or a tail cancel (a recoil already took the slot, so it never gets here with the timeline's montage)
			float BlendOutSeconds = -1.f;
			bool bHoldPose = false;
			ResolveMontageStop(BlendOutSeconds, bHoldPose);
			AbilitySystem->StopSyncedMontageWithBlend(Montage, bWasCancelled, BlendOutSeconds, bHoldPose);
		}
		if (bRecoilPlayed.Value)
		{
			// The locked frames of the recoil are over when the clock moved that far: the montage then plays on as the cancellable tail. An
			// end before that (a hit, another ability) cuts the recoil short, and its montage with it
			const bool bLockedOver = !bWasCancelled && FGSTN_CombatClock::Elapsed(GetCombatClock(), RecoilStartClock.Value) >= RecoilLength.Value;
			if (!bLockedOver)
			{
				StopRecoilMontage(bWasCancelled);
			}
			bRecoilPlayed.Value = false;
		}
		bStarted.Value = false;
		RunningEntry.Value = INDEX_NONE;
		TimelineMontagePlayed = nullptr;
		EvaluatedFrame.Value = INDEX_NONE;
		ReportedHitGroups.Value = 0;
	}
	// The tag windows of Open Move Tag Window last at most as long as the move (CP2)
	CloseTagWindows(true);
	// After the cleanup (bStarted is false, so a handler that ends the ability again does not raise it twice), before the ability system's own
	// end: the ability is still active, so the handler may still apply an effect or send an event
	if (bRaiseEnded)
	{
		OnMoveEnded(EndReason);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UGSTN_GA_BaseCombat::ShouldReleaseMontageOnEnd(bool bWasCancelled) const
{
	const FGSTN_CombatTimelineData& Data = GetRunningTimelineData();
	// Same "ended by itself" test as ResolveMontageStop: the ability sees the end on the timeline's last frame or the one after
	return !bWasCancelled && Data.TotalFrames > 0 && GetTimelineFrame() >= Data.TotalFrames - 1 && Data.EndBlendOutFrames < 0;
}

void UGSTN_GA_BaseCombat::StopReleasedMontages(UAbilitySystemComponent& AbilitySystem)
{
	UNpAbilitySystemComponent* NpAbilitySystem = Cast<UNpAbilitySystemComponent>(&AbilitySystem);
	if (!NpAbilitySystem)
	{
		return;
	}
	float Time = 0.f;
	UAnimMontage* Playing = NpAbilitySystem->GetPlayingSyncedMontage(Time);
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UGSTN_GA_BaseCombat* Combat = Cast<UGSTN_GA_BaseCombat>(Instance);
			if (!Combat || !Combat->bMontageReleased.Value)
			{
				continue;
			}
			Combat->bMontageReleased.Value = false;
			// Only while it is still that montage on the slot: one that ended or was replaced since is left alone. Interrupted, so the
			// montage's end notifies say it did not finish; the presentation leaves with the montage's own blend-out
			if (Playing && Combat->ReleasedMontage == Playing)
			{
				NpAbilitySystem->StopSyncedMontage(Playing, true);
				Playing = nullptr;
			}
		}
	}
}

void UGSTN_GA_BaseCombat::ResolveMontageStop(float& OutBlendOutSeconds, bool& bOutHoldPose) const
{
	OutBlendOutSeconds = -1.f;
	bOutHoldPose = false;
	const FGSTN_CombatTimelineData& Data = GetRunningTimelineData();
	const int32 Frame = GetTimelineFrame();
	const int32 Total = Data.TotalFrames;
	int32 BlendFrames = INDEX_NONE;
	// The ability sees the end on the frame the combat state cleared the timeline: its last frame or the one after
	if (Total > 0 && Frame >= Total - 1)
	{
		BlendFrames = Data.EndBlendOutFrames;
	}
	else
	{
		const int32 TailStart = Data.Tail.ResolveStartFrame(Total, Data.FirstActionableFrame);
		if (TailStart != INDEX_NONE && Frame >= TailStart)
		{
			// The tail was cancelled (a stick move, a jump, a command): the blend its data names, until now not applied anywhere
			BlendFrames = Data.Tail.ResolveBlendOutFrames(Total, Data.FirstActionableFrame);
		}
	}
	if (BlendFrames < 0)
	{
		return;
	}
	// The fixed step of the simulation turns the data's frames into the blend's seconds
	const UNpAbilitySystemComponent* AbilitySystem = GetNpAbilitySystemComponent();
	const float StepMs = AbilitySystem != nullptr ? AbilitySystem->GetFixedStepMs() : 16.f;
	OutBlendOutSeconds = static_cast<float>(BlendFrames) * StepMs / 1000.f;
	bOutHoldPose = Data.bHoldPoseOnEnd;
}

int32 UGSTN_GA_BaseCombat::GetTimelineFrame() const
{
	return bStarted.Value ? FGSTN_CombatClock::Elapsed(GetCombatClock(), StartClock.Value) : 0;
}
