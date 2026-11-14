// Ability-system layer of the combat framework.

#include "Cosmetics/GSTN_ReactionAnimComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNReactionAnim, Log, All);

namespace GSTNReactionAnim
{
	/** The rules count frames at 60 fps, as the clips are authored */
	constexpr float FrameSeconds = 1.f / 60.f;

	/** A looping stage plays as one long montage: enough loops for any reaction */
	constexpr int32 LoopCount = 10000;

	float Seconds(int32 Frames) { return FMath::Max(Frames, 0) * FrameSeconds; }
}

UGSTN_ReactionAnimComponent::UGSTN_ReactionAnimComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UGSTN_ReactionAnimComponent::BeginPlay()
{
	Super::BeginPlay();
	// Presentation: nothing to draw on a dedicated server
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	ApplySet(ReactionSet.IsNull() ? nullptr : ReactionSet.LoadSynchronous());
}

void UGSTN_ReactionAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindEvents();
	Super::EndPlay(EndPlayReason);
}

void UGSTN_ReactionAnimComponent::SetReactionSet(UGSTN_ReactionAnimSet* NewSet)
{
	ReactionSet = NewSet;
	if (HasBegunPlay() && GetNetMode() != NM_DedicatedServer)
	{
		ApplySet(NewSet);
	}
}

FName UGSTN_ReactionAnimComponent::GetActiveRuleName() const
{
	return LoadedSet && LoadedSet->Rules.IsValidIndex(ActiveRule) ? LoadedSet->Rules[ActiveRule].Name : NAME_None;
}

UGSTN_ReactionAnimComponent* UGSTN_ReactionAnimComponent::AddReactionComponent(AActor* Actor, UGSTN_ReactionAnimSet* Set)
{
	if (!Actor)
	{
		return nullptr;
	}
	UGSTN_ReactionAnimComponent* Component = Actor->FindComponentByClass<UGSTN_ReactionAnimComponent>();
	if (!Component)
	{
		Component = NewObject<UGSTN_ReactionAnimComponent>(Actor, TEXT("GSTN_ReactionAnim"));
		Component->ReactionSet = Set;
		Component->RegisterComponent();
		return Component;
	}
	Component->SetReactionSet(Set);
	return Component;
}

void UGSTN_ReactionAnimComponent::ApplySet(UGSTN_ReactionAnimSet* NewSet)
{
	StopActive(ResolveAnimInstance(), LoadedSet && LoadedSet->Rules.IsValidIndex(ActiveRule) ? GSTNReactionAnim::Seconds(LoadedSet->Rules[ActiveRule].BlendOutFrames) : 0.1f);
	UnbindEvents();
	LoadedSet = NewSet;
	LoadedAnimations.Reset();
	ActiveRule = INDEX_NONE;
	ConsumedRule = INDEX_NONE;
	PendingRestartRule = INDEX_NONE;
	if (!LoadedSet)
	{
		return;
	}
	for (const FGSTN_ReactionAnimRule& Rule : LoadedSet->Rules)
	{
		for (const TSoftObjectPtr<UAnimSequenceBase>* Soft : { &Rule.Animation, &Rule.ThenAnimation })
		{
			if (!Soft->IsNull())
			{
				if (UAnimSequenceBase* Loaded = Soft->LoadSynchronous())
				{
					LoadedAnimations.AddUnique(Loaded);
				}
				else
				{
					UE_LOG(LogGSTNReactionAnim, Warning, TEXT("%s: reaction %s cannot load %s"), *GetNameSafe(GetOwner()), *Rule.Name.ToString(), *Soft->ToString());
				}
			}
		}
	}
	if (UAbilitySystemComponent* ASC = ResolveAbilitySystem())
	{
		for (int32 Index = 0; Index < LoadedSet->Rules.Num(); ++Index)
		{
			const FGameplayTag Event = LoadedSet->Rules[Index].RestartEvent;
			if (Event.IsValid() && !EventHandles.ContainsByPredicate([&Event](const TPair<FGameplayTag, FDelegateHandle>& Pair) { return Pair.Key == Event; }))
			{
				EventHandles.Emplace(Event, ASC->GenericGameplayEventCallbacks.FindOrAdd(Event).AddUObject(this, &UGSTN_ReactionAnimComponent::OnRestartEvent, Event));
			}
		}
	}
}

void UGSTN_ReactionAnimComponent::UnbindEvents()
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : EventHandles)
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Pair.Key))
			{
				Delegate->Remove(Pair.Value);
			}
		}
	}
	EventHandles.Reset();
}

void UGSTN_ReactionAnimComponent::OnRestartEvent(const FGameplayEventData* Payload, FGameplayTag Tag)
{
	// Remembered for the tick, which restarts the rule only if it is the one that wins then (a hit during the hit reaction, or after its clip
	// ended while the state holds); a resimulated frame that fires the event again before the tick restarts once
	if (LoadedSet)
	{
		PendingRestartRule = LoadedSet->Rules.IndexOfByPredicate([&Tag](const FGSTN_ReactionAnimRule& Rule) { return Rule.RestartEvent == Tag; });
	}
}

UAbilitySystemComponent* UGSTN_ReactionAnimComponent::ResolveAbilitySystem()
{
	if (!AbilitySystem.IsValid())
	{
		AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	}
	return AbilitySystem.Get();
}

UAnimInstance* UGSTN_ReactionAnimComponent::ResolveAnimInstance()
{
	if (!Mesh.IsValid() && GetOwner())
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(GetOwner());
		for (USkeletalMeshComponent* Candidate : Meshes)
		{
			const bool bNamed = !MeshComponentName.IsNone() && Candidate->GetFName() == MeshComponentName;
			// A retargeted visual mesh follows a leader: the leader's anim instance is the one that plays
			const bool bFirstPlaying = MeshComponentName.IsNone() && Candidate->GetAnimInstance() && !Candidate->LeaderPoseComponent.IsValid();
			if (bNamed || bFirstPlaying)
			{
				Mesh = Candidate;
				break;
			}
		}
	}
	return Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
}

bool UGSTN_ReactionAnimComponent::IsOtherMontagePlaying(const UAnimInstance& Anim) const
{
	for (const FAnimMontageInstance* Instance : Anim.MontageInstances)
	{
		// Blending out does not count: an ability's montage that is leaving lets the reaction start under it, so nothing shows in between
		if (Instance && Instance->IsActive() && !Instance->IsStopped() && Instance->Montage != ActiveMontage)
		{
			return true;
		}
	}
	return false;
}

bool UGSTN_ReactionAnimComponent::PlayStage(UAnimInstance& Anim, const FGSTN_ReactionAnimRule& Rule, int32 Stage, float BlendSeconds)
{
	const TSoftObjectPtr<UAnimSequenceBase>& Soft = Stage == 0 ? Rule.Animation : Rule.ThenAnimation;
	UAnimSequenceBase* Asset = Soft.Get();
	if (!Asset)
	{
		return false;
	}
	const bool bLastStage = Stage == 1 || Rule.ThenAnimation.IsNull();
	const bool bLoop = bLastStage && Rule.EndBehaviour == EGSTN_ReactionAnimEnd::Loop;
	UAnimMontage* Montage = Cast<UAnimMontage>(Asset);
	if (!Montage)
	{
		// A sequence: a montage of our own, without the automatic blend out, so this component decides how the stage ends (hold, chain, blend)
		Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Asset, LoadedSet->SlotName, BlendSeconds, GSTNReactionAnim::Seconds(Rule.BlendOutFrames), Rule.PlayRate,
			bLoop ? GSTNReactionAnim::LoopCount : 1, -1.f, 0.f);
		if (!Montage)
		{
			return false;
		}
		Montage->bEnableAutoBlendOut = false;
	}
	// Playing on the slot's group stops what plays there with this blend in as its blend out: the cross-fade of a chain
	if (Anim.Montage_PlayWithBlendIn(Montage, FAlphaBlendArgs(BlendSeconds), Rule.PlayRate, EMontagePlayReturnType::MontageLength, 0.f, true) <= 0.f)
	{
		return false;
	}
	ActiveMontage = Montage;
	ActiveStage = Stage;
	return true;
}

void UGSTN_ReactionAnimComponent::StopActive(UAnimInstance* Anim, float BlendSeconds)
{
	if (Anim && ActiveMontage)
	{
		if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(ActiveMontage))
		{
			// The pose shown now is held through the blend: nothing of the clip past this frame is drawn
			Instance->SetPlayRate(0.f);
			FAlphaBlend Blend = Instance->GetBlend();
			Blend.SetBlendTime(BlendSeconds);
			Instance->Stop(Blend);
		}
	}
	ActiveMontage = nullptr;
	ActiveRule = INDEX_NONE;
	ActiveStage = 0;
}

void UGSTN_ReactionAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UAnimInstance* Anim = LoadedSet ? ResolveAnimInstance() : nullptr;
	UAbilitySystemComponent* ASC = Anim ? ResolveAbilitySystem() : nullptr;
	if (!Anim || !ASC)
	{
		return;
	}

	FGameplayTagContainer Tags;
	ASC->GetOwnedGameplayTags(Tags);
	const int32 Winner = LoadedSet->SelectRule(Tags);
	if (Winner != ConsumedRule)
	{
		ConsumedRule = INDEX_NONE;
	}

	// Our montage was replaced (an ability's montage on the same group, or anything that stopped it): the rule does not start again
	if (ActiveMontage && !Anim->GetActiveInstanceForMontage(ActiveMontage))
	{
		ConsumedRule = ActiveRule;
		ActiveMontage = nullptr;
		ActiveRule = INDEX_NONE;
	}

	if (ActiveRule != INDEX_NONE && Winner != ActiveRule)
	{
		// The state changed: a new rule cross-fades from our pose with its own blend in, no rule blends back with the old one's blend out
		const float Blend = Winner != INDEX_NONE ? GSTNReactionAnim::Seconds(LoadedSet->Rules[Winner].BlendInFrames)
			: GSTNReactionAnim::Seconds(LoadedSet->Rules[ActiveRule].BlendOutFrames);
		// The old pose is held while it fades, the new rule fades in over the same time below: a cross-fade with no frame of the game's pose
		StopActive(Anim, Blend);
	}

	if (Winner == INDEX_NONE)
	{
		PendingRestartRule = INDEX_NONE;
		return;
	}
	const FGSTN_ReactionAnimRule& Rule = LoadedSet->Rules[Winner];
	const bool bRestart = PendingRestartRule == Winner;
	PendingRestartRule = INDEX_NONE;

	if (ActiveRule != Winner || bRestart)
	{
		if (Winner == ConsumedRule && !bRestart)
		{
			return;
		}
		if (LoadedSet->bYieldToOtherMontages && IsOtherMontagePlaying(*Anim))
		{
			return;
		}
		if (PlayStage(*Anim, Rule, 0, GSTNReactionAnim::Seconds(Rule.BlendInFrames)))
		{
			ActiveRule = Winner;
			ConsumedRule = INDEX_NONE;
		}
		return;
	}

	// The rule plays on: chain, hold or blend out near the end of the stage
	FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(ActiveMontage);
	if (!Instance)
	{
		return;
	}
	const float Rate = FMath::Max(Rule.PlayRate, 0.01f);
	const float Remaining = (ActiveMontage->GetPlayLength() - Instance->GetPosition()) / Rate;
	const bool bHasNext = ActiveStage == 0 && !Rule.ThenAnimation.IsNull();
	// A frame and a half ahead: the next tick may already be past the end, and the character's own pose must never show for a frame
	const float Lead = FMath::Max(DeltaTime, GSTNReactionAnim::FrameSeconds) * 1.5f;
	if (bHasNext)
	{
		const float ChainBlend = GSTNReactionAnim::Seconds(Rule.ChainBlendFrames);
		if (Remaining <= ChainBlend + Lead && !PlayStage(*Anim, Rule, 1, ChainBlend))
		{
			// The second animation is missing: hold the first
			Instance->SetPlayRate(0.f);
		}
		return;
	}
	switch (Rule.EndBehaviour)
	{
	case EGSTN_ReactionAnimEnd::HoldLastPose:
		if (Remaining <= Lead && Instance->GetPlayRate() != 0.f)
		{
			Instance->SetPlayRate(0.f);
		}
		break;
	case EGSTN_ReactionAnimEnd::BlendOut:
	{
		const float BlendOut = GSTNReactionAnim::Seconds(Rule.BlendOutFrames);
		if (Remaining <= BlendOut + Lead)
		{
			StopActive(Anim, BlendOut);
			ConsumedRule = Winner;
		}
		break;
	}
	case EGSTN_ReactionAnimEnd::Loop:
	default:
		break;
	}
}
