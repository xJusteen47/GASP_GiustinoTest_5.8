// Ability-system layer of the combat framework.

#include "Cosmetics/GSTN_ReactionAnimComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/GSTN_HitstopFeel.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "Events/GSTN_HitEvents.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "GSTN_CombatCoreTags.h"
#include "GSTN_CombatTags.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNReactionAnim, Log, All);

namespace GSTNReactionAnim
{
	/** The rules count frames at 60 fps, as the clips are authored */
	constexpr float FrameSeconds = 1.f / 60.f;

	/** A looping stage plays as one long montage: enough loops for any reaction */
	constexpr int32 LoopCount = 10000;

	float Seconds(int32 Frames) { return FMath::Max(Frames, 0) * FrameSeconds; }

	/**
	 * A hitstop left that is this much longer than the one being counted down is a new hit during the freeze (a multi-hit, a trade): the shake
	 * starts again from full. Covers the 60 Hz drawing of a 62.5 Hz simulation (a frame can show one more tick left than the clock expects)
	 */
	constexpr float HitstopRestartSlackSeconds = 0.035f;
}

UGSTN_ReactionAnimComponent::UGSTN_ReactionAnimComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// The mesh's group (it waits for this component, see ResolveAnimInstance): a later group would push the mesh's animation tick back with it
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
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
	ApplyAdvanceLead(0.f);
	DropCm = 0.f;
	UpdateEntryDrop(0.f);
	if (USkeletalMeshComponent* MeshComponent = Mesh.Get())
	{
		MeshComponent->RemoveTickPrerequisiteComponent(this);
	}
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
	// The rules of the old set are over for the cosmetic hooks; the new set's winner is announced on the next tick
	if (AnnouncedRule != INDEX_NONE)
	{
		OnReactionEnded.Broadcast(AnnouncedRuleName);
	}
	AnnouncedRule = INDEX_NONE;
	AnnouncedRuleName = NAME_None;
	HitstopElapsed = -1.f;
	HitstopDuration = 0.f;
	bAdvancing = false;
	AdvancePeakTime = -1.f;
	ApplyAdvanceLead(0.f);
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
		const FGSTN_ReactionSideAnimations& Sides = Rule.SideAnimations;
		for (const TSoftObjectPtr<UAnimSequenceBase>* Soft : { &Rule.Animation, &Rule.ThenAnimation, &Sides.Front, &Sides.Back, &Sides.Left, &Sides.Right })
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
	HitLayers[0] = HitLayers[1] = FHitLayer();
	bHitEventPending = false;
	const FGSTN_DirectionalHitSettings& Directional = LoadedSet->DirectionalHits;
	for (const FGSTN_DirectionalHitAdditives* Additives : { &Directional.Ground, &Directional.Air })
	{
		for (const TSoftObjectPtr<UAnimSequenceBase>* Soft : { &Additives->Front, &Additives->Back, &Additives->Left, &Additives->Right })
		{
			if (!Soft->IsNull())
			{
				if (UAnimSequenceBase* Loaded = Soft->LoadSynchronous())
				{
					LoadedAnimations.AddUnique(Loaded);
				}
				else
				{
					UE_LOG(LogGSTNReactionAnim, Warning, TEXT("%s: directional hit additive cannot load %s"), *GetNameSafe(GetOwner()), *Soft->ToString());
				}
			}
		}
	}
	if (UAbilitySystemComponent* ASC = ResolveAbilitySystem())
	{
		// The hit event feeds the directional additives and the per-side poses: bound when either is used
		const bool bSidePoses = LoadedSet->Rules.ContainsByPredicate([](const FGSTN_ReactionAnimRule& Rule) { return Rule.SideAnimations.HasAny(); });
		if (Directional.bEnabled || bSidePoses)
		{
			HitEventHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(GSTNCombatTags::Event_Hit_Received)
				.AddUObject(this, &UGSTN_ReactionAnimComponent::OnHitEvent);
		}
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
	UAbilitySystemComponent* HitASC = AbilitySystem.Get();
	if (HitASC && HitEventHandle.IsValid())
	{
		if (FGameplayEventMulticastDelegate* Delegate = HitASC->GenericGameplayEventCallbacks.Find(GSTNCombatTags::Event_Hit_Received))
		{
			Delegate->Remove(HitEventHandle);
		}
	}
	HitEventHandle.Reset();
}

void UGSTN_ReactionAnimComponent::OnHitEvent(const FGameplayEventData* Payload)
{
	const AActor* Owner = GetOwner();
	if (!Payload || !LoadedSet || !Owner)
	{
		return;
	}
	// The push: attacker to victim; an attacker on top of the victim pushes along its own facing
	FVector Push = -Owner->GetActorForwardVector();
	if (const AActor* Attacker = Payload->Instigator.Get())
	{
		Push = Owner->GetActorLocation() - Attacker->GetActorLocation();
		Push.Z = 0.0;
		if (Push.SizeSquared() < FMath::Square(5.0))
		{
			Push = Attacker->GetActorForwardVector();
		}
	}
	Push = Push.GetSafeNormal2D();
	// The per-side poses read every hit, a weak one included: the reaction it starts must still face the right way
	LastHitPush = Push;
	bHasHitPush = true;

	const FGSTN_DirectionalHitSettings& Settings = LoadedSet->DirectionalHits;
	FGSTN_HitEventPayload Hit;
	const bool bStaggered = FGSTN_HitEventPayload::FromEventData(*Payload, Hit) ? Hit.bStaggered : true;
	const float Weight = FGSTN_DirectionalHitMath::IntensityWeight(bStaggered, FMath::RoundToInt(Payload->EventMagnitude),
		Settings.FullWeightHitstunFrames, Settings.MinWeight, Settings.UnstaggeredWeight);
	if (Weight <= 0.f)
	{
		return;
	}
	// A resimulated frame can send the same hit again a moment later: it does not restart the flinch (a multi-hit 3+ frames apart does)
	const FHitLayer& Newest = HitLayers[0];
	if (Newest.Weight > 0.f && Newest.Draw.Time < 0.05f && FVector::DotProduct(Newest.Push, Push) > 0.95)
	{
		return;
	}
	PendingHit = FHitLayer();
	PendingHit.Push = Push;
	PendingHit.Weight = Weight;
	bHitEventPending = true;
}

void UGSTN_ReactionAnimComponent::ResolveLayer(FHitLayer& Layer) const
{
	const AActor* Owner = GetOwner();
	const FGSTN_DirectionalHitSettings& Settings = LoadedSet->DirectionalHits;
	const FGSTN_DirectionalHitAdditives& Set = Layer.bAir ? Settings.Air : Settings.Ground;
	const FVector Forward = Owner->GetActorForwardVector();
	const FVector Right = Owner->GetActorRightVector();
	const EGSTN_HitSide Side = FGSTN_DirectionalHitMath::SelectSide(Layer.Push, Forward, Right);
	const UAnimSequenceBase* Clip = Set.ForSide(Side).Get();
	Layer.Side = static_cast<uint8>(Side);
	Layer.Draw.Additive = Clip;
	Layer.Draw.PushDirection = FGSTN_DirectionalHitMath::SidePush(Side, Forward, Right);
	Layer.Draw.PlaceholderAngleDeg = Clip ? 0.f : Set.PlaceholderAngleDeg;
	Layer.Draw.PlaceholderPeakSeconds = Set.PlaceholderPeakSeconds;
	Layer.Draw.PlaceholderLengthSeconds = Set.PlaceholderLengthSeconds;
	Layer.Length = Clip ? Clip->GetPlayLength() : Set.PlaceholderLengthSeconds;
}

void UGSTN_ReactionAnimComponent::UpdateDirectionalHits(float DeltaTime, const FGameplayTagContainer& Tags)
{
	const FGSTN_DirectionalHitSettings& Settings = LoadedSet->DirectionalHits;
	if (bHitEventPending)
	{
		bHitEventPending = false;
		if (Settings.bEnabled)
		{
			HitLayers[1] = HitLayers[0];
			HitLayers[1].bFading = HitLayers[1].Weight > 0.f;
			HitLayers[0] = PendingHit;
			HitLayers[0].bAir = Tags.HasAny(Settings.AirTags);
			ResolveLayer(HitLayers[0]);
		}
	}
	const bool bBlocked = !Settings.bEnabled || Tags.HasAny(Settings.BlockedTags);
	const bool bHitstop = Tags.HasTagExact(GSTNCombatTags::State_Hitstop);
	const float FadeStep = DeltaTime / FMath::Max(Settings.ReplaceFadeSeconds, 0.001f);
	for (FHitLayer& Layer : HitLayers)
	{
		if (Layer.Weight <= 0.f)
		{
			continue;
		}
		float Time = Layer.Draw.Time;
		if (Time < Settings.SideSettleSeconds)
		{
			// The back-hit turn lands on the first reaction frame, after the event: the side follows the body as drawn until it settles
			ResolveLayer(Layer);
		}
		// The hit pose holds while the victim is frozen by the hitstop, then the flinch plays out
		if (!bHitstop)
		{
			Time += DeltaTime;
		}
		else if (Time < Settings.HitstopHoldSeconds)
		{
			Time = FMath::Min(Time + DeltaTime, Settings.HitstopHoldSeconds);
		}
		if (Layer.bFading || bBlocked)
		{
			Layer.Fade -= FadeStep;
		}
		if (Time >= Layer.Length || Layer.Fade <= 0.f)
		{
			Layer = FHitLayer();
			continue;
		}
		Layer.Draw.Time = Time;
		Layer.Draw.Weight = Layer.Weight * Layer.Fade;
	}
}

void UGSTN_ReactionAnimComponent::GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const
{
	for (int32 Index = 0; Index < FGSTN_AdditiveReactionRequest::MaxLayers; ++Index)
	{
		Out.Layers[Index] = HitLayers[Index].Weight > 0.f ? HitLayers[Index].Draw : FGSTN_AdditiveReactionLayer();
	}
}

EGSTN_HitSide UGSTN_ReactionAnimComponent::GetDirectionalHitSide() const
{
	return static_cast<EGSTN_HitSide>(HitLayers[0].Side);
}

EGSTN_HitSide UGSTN_ReactionAnimComponent::GetLastHitSide() const
{
	const AActor* Owner = GetOwner();
	// Read against the facing now, as the additives do: the back-hit turn rotates the victim after the event
	return bHasHitPush && Owner ? FGSTN_DirectionalHitMath::SelectSide(LastHitPush, Owner->GetActorForwardVector(), Owner->GetActorRightVector())
		: EGSTN_HitSide::Front;
}

bool UGSTN_ReactionAnimComponent::UpdatePoseSide(UAnimInstance& Anim, const FGSTN_ReactionAnimRule& Rule, float DeltaTime)
{
	if (ActiveStage != 0 || !Rule.SideAnimations.HasAny())
	{
		return false;
	}
	PoseSideAge += DeltaTime;
	if (PoseSideAge > GSTNReactionAnim::Seconds(Rule.SideAnimations.SideSettleFrames) + UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const EGSTN_HitSide Side = GetLastHitSide();
	if (Side == ActivePoseSide)
	{
		return false;
	}
	if (Rule.AnimationForSide(Side) == Rule.AnimationForSide(ActivePoseSide))
	{
		// Another side with the same clip (front and back both on the base): nothing to swap
		ActivePoseSide = Side;
		return false;
	}
	const FAnimMontageInstance* Old = ActiveMontage ? Anim.GetActiveInstanceForMontage(ActiveMontage) : nullptr;
	const float Position = Old ? Old->GetPosition() : 0.f;
	const bool bWasHeld = bHitstopPoseHeld;
	const bool bWasStartedInHitstop = bStartedInHitstop;
	const float Age = PoseSideAge;
	// No blend: the two clips are the same reaction on the other side, a cross-fade would draw both arms half way for a frame
	if (!PlayStage(Anim, Rule, 0, 0.f))
	{
		return false;
	}
	PoseSideAge = Age;
	bStartedInHitstop = bWasStartedInHitstop;
	if (FAnimMontageInstance* New = Anim.GetActiveInstanceForMontage(ActiveMontage))
	{
		New->SetPosition(Position);
		if (bWasHeld)
		{
			New->SetPlayRate(0.f);
			bHitstopPoseHeld = true;
		}
	}
	UE_LOG(LogGSTNReactionAnim, Verbose, TEXT("%s: reaction %s pose side -> %d at %.3f s"), *GetNameSafe(GetOwner()), *Rule.Name.ToString(), static_cast<int32>(Side), Position);
	return true;
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
				// The mesh ticks after this component: a montage started here is in the pose the mesh evaluates on the same frame, so the
				// reaction shows on the frame its state starts and the entry drop lifts the new pose, never the previous one (react-hook-3:
				// the drop lifted the Tumble pose ~85 cm for one frame, the GroundHit pose arrived a tick later)
				Candidate->AddTickPrerequisiteComponent(this);
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
	// The first stage is the clip of the last hit's side (the HitStand lead arm on the attacker's side); a side clip that did not load plays the base
	const EGSTN_HitSide Side = GetLastHitSide();
	UAnimSequenceBase* Asset = Stage == 0 ? Rule.AnimationForSide(Side).Get() : Rule.ThenAnimation.Get();
	if (!Asset && Stage == 0)
	{
		Asset = Rule.Animation.Get();
	}
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
	// Sakurai #6: a reaction that starts while the victim is frozen blends into its hit pose on the pose blend's curve
	const bool bPoseBlend = Stage == 0 && StartsInHitstopPoseBlend();
	FAlphaBlendArgs BlendIn(BlendSeconds);
	if (bPoseBlend)
	{
		BlendIn.BlendOption = LoadedSet->HitstopPoseBlend.BlendOption;
	}
	// Playing on the slot's group stops what plays there with this blend in as its blend out: the cross-fade of a chain
	if (Anim.Montage_PlayWithBlendIn(Montage, BlendIn, Rule.PlayRate, EMontagePlayReturnType::MontageLength, 0.f, true) <= 0.f)
	{
		return false;
	}
	ActiveMontage = Montage;
	ActiveStage = Stage;
	if (Stage == 0)
	{
		ActivePoseSide = Side;
		PoseSideAge = 0.f;
	}
	bStartedInHitstop = bPoseBlend;
	bHitstopPoseHeld = false;
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
	UpdateHitstopTimer(DeltaTime, Tags);
	ShakeOffset = ComputeHitstopShake(Tags);
	UpdateEntryDrop(DeltaTime);
	UpdateDirectionalHits(DeltaTime, Tags);
	UpdateHitstopPoseHold(*Anim);
	UpdateHitstopAdvance(Tags);

	const int32 Winner = LoadedSet->SelectRule(Tags);
	AnnounceReaction(Winner, Winner != INDEX_NONE && PendingRestartRule == Winner);
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
		const float Blend = Winner != INDEX_NONE ? BlendInSeconds(LoadedSet->Rules[Winner])
			: GSTNReactionAnim::Seconds(LoadedSet->Rules[ActiveRule].BlendOutFrames);
		// The old pose is held while it fades, the new rule fades in over the same time below: a cross-fade with no frame of the game's pose
		StopActive(Anim, Blend);
	}

	if (Winner == INDEX_NONE)
	{
		PendingRestartRule = INDEX_NONE;
		// The character's own pose is back: a later rule does not drop from the last reaction's height
		LastShownRule = NAME_None;
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
		if (PlayStage(*Anim, Rule, 0, BlendInSeconds(Rule)))
		{
			if (!bRestart)
			{
				BeginEntryDrop(Rule, LastShownRule);
			}
			ActiveRule = Winner;
			ConsumedRule = INDEX_NONE;
			LastShownRule = Rule.Name;
			UpdateEntryDrop(0.f);
		}
		return;
	}

	// The rule plays on: the side settles (per-side poses), then chain, hold or blend out near the end of the stage
	UpdatePoseSide(*Anim, Rule, DeltaTime);
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

void UGSTN_ReactionAnimComponent::BeginEntryDrop(const FGSTN_ReactionAnimRule& Rule, FName PreviousRule)
{
	if (Rule.EntryDropCm <= 0.f || (Rule.EntryDropAfterRules.Num() > 0 && !Rule.EntryDropAfterRules.Contains(PreviousRule)))
	{
		DropCm = 0.f;
		return;
	}
	DropCm = Rule.EntryDropCm;
	DropSeconds = GSTNReactionAnim::Seconds(FMath::Max(Rule.EntryDropFrames, 1));
	DropElapsed = 0.f;
}

void UGSTN_ReactionAnimComponent::UpdateEntryDrop(float DeltaTime)
{
	USkeletalMeshComponent* MeshComponent = Mesh.Get();
	if (!MeshComponent || (DropCm <= 0.f && AppliedOffset.IsZero() && ShakeOffset.IsZero()))
	{
		return;
	}
	float Height = 0.f;
	if (DropCm > 0.f)
	{
		DropElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(DropElapsed / FMath::Max(DropSeconds, UE_SMALL_NUMBER), 0.f, 1.f);
		// Accelerating, as a fall: slow at the top, fastest when it reaches the floor
		Height = DropCm * (1.f - Alpha * Alpha);
		if (Alpha >= 1.f)
		{
			DropCm = 0.f;
			Height = 0.f;
		}
	}
	// Mover writes the mesh's relative transform back to its base on its frames (FinalizeFrame) and its smoothing may move it too: the offset
	// is added on top of what is there now, and taken off first only when nobody wrote the location since this component did
	FVector Location = MeshComponent->GetRelativeLocation();
	if (Location.Equals(LastWrittenLocation, 0.01))
	{
		Location -= AppliedOffset;
	}
	const FVector Offset = ShakeOffset + FVector(0.f, 0.f, Height);
	Location += Offset;
	if (!Location.Equals(MeshComponent->GetRelativeLocation(), 0.01))
	{
		MeshComponent->SetRelativeLocation(Location);
	}
	LastWrittenLocation = Location;
	AppliedOffset = Offset;
}

void UGSTN_ReactionAnimComponent::UpdateHitstopTimer(float DeltaTime, const FGameplayTagContainer& Tags)
{
	if (!Tags.HasTagExact(GSTNCombatTags::State_Hitstop))
	{
		HitstopElapsed = -1.f;
		HitstopDuration = 0.f;
		bVictimInHitstop = false;
		return;
	}
	// The hit one is the one in hitstun (or tumbling); the attacker is frozen too, without either
	bVictimInHitstop = Tags.HasTagExact(GSTNCombatTags::State_Hitstun) || Tags.HasTagExact(GSTNCombatTags::State_Tumble);
	const float Left = FGSTN_HitstopPresentationHooks::GetHitstopSecondsLeft ? FGSTN_HitstopPresentationHooks::GetHitstopSecondsLeft(GetOwner()) : -1.f;
	if (HitstopElapsed < 0.f)
	{
		HitstopElapsed = 0.f;
		HitstopDuration = FMath::Max(Left, 0.f);
		return;
	}
	HitstopElapsed += DeltaTime;
	if (Left > 0.f)
	{
		if (HitstopDuration <= 0.f)
		{
			// The length became known after the first frame (the simulation's view arrived late): the clock keeps its start
			HitstopDuration = HitstopElapsed + Left;
		}
		else if (Left > HitstopDuration - HitstopElapsed + GSTNReactionAnim::HitstopRestartSlackSeconds)
		{
			// A new hit during the freeze: a new hitstop, counted from now
			HitstopElapsed = 0.f;
			HitstopDuration = Left;
		}
	}
}

FVector UGSTN_ReactionAnimComponent::ComputeHitstopShake(const FGameplayTagContainer& Tags)
{
	const FGSTN_HitstopShake* Shake = LoadedSet ? &LoadedSet->HitstopShake : nullptr;
	USkeletalMeshComponent* MeshComponent = Mesh.Get();
	if (!Shake || !Shake->bEnabled || !MeshComponent || HitstopElapsed < 0.f)
	{
		return FVector::ZeroVector;
	}
	const float Amplitude = bVictimInHitstop ? Shake->VictimAmplitudeCm : Shake->AttackerAmplitudeCm;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Amplitude <= 0.f || !Camera)
	{
		return FVector::ZeroVector;
	}
	// Sakurai #4: the amplitude converges to zero over the hitstop this hit really gives (DecaySeconds when its length is unknown)
	const float Duration = Shake->DecayMode == EGSTN_HitstopShakeDecay::OverHitstop && HitstopDuration > 0.f ? HitstopDuration : Shake->DecaySeconds;
	// Sakurai #8: a far camera scales the shake up
	const float Distance = FVector::Dist(Camera->GetCameraLocation(), MeshComponent->GetComponentLocation());
	const float Value = FGSTN_HitstopShake::Sample(Amplitude * Shake->DistanceScale(Distance), Shake->FrequencyHz, Duration, HitstopElapsed);
	if (Value == 0.f)
	{
		return FVector::ZeroVector;
	}
	// Sakurai #3: side to side on the ground (nothing along the owner's up), every direction of the screen plane in the air
	const FRotationMatrix CameraAxes(Camera->GetCameraRotation());
	const FVector ScreenRight = CameraAxes.GetScaledAxis(EAxis::Y).GetSafeNormal();
	FVector Direction;
	if (Shake->bAllAxesInAir && Tags.HasAny(Shake->AirTags))
	{
		const FVector2D Axis = FGSTN_HitstopFeelMath::AirShakeAxis(Shake->FrequencyHz, HitstopElapsed);
		Direction = ScreenRight * Axis.X + CameraAxes.GetScaledAxis(EAxis::Z).GetSafeNormal() * Axis.Y;
	}
	else
	{
		const AActor* Owner = GetOwner();
		Direction = FGSTN_HitstopFeelMath::GroundShakeDirection(ScreenRight, Owner ? Owner->GetActorUpVector() : FVector::UpVector);
	}
	// Sakurai #2: only the mesh moves (UpdateEntryDrop writes it as an offset over Mover's): capsule, hurtboxes and the simulation stay put
	const FVector World = Direction * Value;
	const USceneComponent* Parent = MeshComponent->GetAttachParent();
	return Parent ? Parent->GetComponentTransform().InverseTransformVectorNoScale(World) : World;
}

bool UGSTN_ReactionAnimComponent::StartsInHitstopPoseBlend() const
{
	return bVictimInHitstop && LoadedSet && LoadedSet->HitstopPoseBlend.bEnabled;
}

float UGSTN_ReactionAnimComponent::BlendInSeconds(const FGSTN_ReactionAnimRule& Rule) const
{
	return StartsInHitstopPoseBlend() ? GSTNReactionAnim::Seconds(LoadedSet->HitstopPoseBlend.BlendFrames) : GSTNReactionAnim::Seconds(Rule.BlendInFrames);
}

void UGSTN_ReactionAnimComponent::UpdateHitstopPoseHold(UAnimInstance& Anim)
{
	FAnimMontageInstance* Instance = ActiveMontage ? Anim.GetActiveInstanceForMontage(ActiveMontage) : nullptr;
	if (!Instance || !LoadedSet || !LoadedSet->Rules.IsValidIndex(ActiveRule))
	{
		bHitstopPoseHeld = false;
		bStartedInHitstop = false;
		return;
	}
	const FGSTN_HitstopPoseBlend& PoseBlend = LoadedSet->HitstopPoseBlend;
	if (bStartedInHitstop && bVictimInHitstop && PoseBlend.bEnabled && ActiveStage == 0)
	{
		// The clip runs into its hit pose and stops there while the freeze lasts. Not set back onto the frame: an update that went a little past
		// it holds where it is, a pose never jumps backwards
		if (!bHitstopPoseHeld && Instance->GetPosition() >= GSTNReactionAnim::Seconds(PoseBlend.HoldAtFrame))
		{
			Instance->SetPlayRate(0.f);
			bHitstopPoseHeld = true;
		}
		return;
	}
	if (bHitstopPoseHeld)
	{
		// The hitstop is over (or the setting went off): the reaction plays out from its hit pose
		Instance->SetPlayRate(LoadedSet->Rules[ActiveRule].PlayRate);
		bHitstopPoseHeld = false;
	}
	if (!bVictimInHitstop)
	{
		bStartedInHitstop = false;
	}
}

void UGSTN_ReactionAnimComponent::UpdateHitstopAdvance(const FGameplayTagContainer& Tags)
{
	const AActor* Owner = GetOwner();
	if (!LoadedSet || !Owner || !FGSTN_HitstopPresentationHooks::GetPresentedMontageTime || !FGSTN_HitstopPresentationHooks::SetMontagePresentationLead)
	{
		return;
	}
	const UObject* Montage = nullptr;
	float Time = 0.f;
	if (!FGSTN_HitstopPresentationHooks::GetPresentedMontageTime(Owner, Montage, Time) || !Montage)
	{
		bAdvancing = false;
		AdvancePeakTime = -1.f;
		ApplyAdvanceLead(0.f);
		return;
	}
	if (AdvanceMontage.Get() != Montage)
	{
		// Another move (or none before): nothing of the last one's advance carries over
		bAdvancing = false;
		AdvancePeakTime = -1.f;
		ApplyAdvanceLead(0.f);
	}
	const FGSTN_HitstopAdvance& Advance = LoadedSet->HitstopAdvance;
	float Lead = 0.f;
	if (HitstopElapsed >= 0.f && !bVictimInHitstop && Advance.AppliesTo(Tags))
	{
		if (!bAdvancing)
		{
			// Starts from what is drawn now, lead included (a second hit while the first one's lead is still absorbing)
			bAdvancing = true;
			AdvanceMontage = Montage;
			AdvanceBaseTime = Time + AppliedLead;
		}
		// The clock may still be catching up with the frozen simulation on the first frames: never draw behind the base
		AdvanceBaseTime = FMath::Max(AdvanceBaseTime, Time);
		const float Duration = HitstopDuration > 0.f ? HitstopDuration : LoadedSet->HitstopShake.DecaySeconds;
		const float Alpha = Duration > 0.f ? HitstopElapsed / Duration : 1.f;
		AdvancePeakTime = AdvanceBaseTime + FGSTN_HitstopFeelMath::AdvanceLead(Advance.AdvanceFrames * GSTNReactionAnim::FrameSeconds, Alpha);
		Lead = FGSTN_HitstopFeelMath::AbsorbLead(AdvancePeakTime, Time);
	}
	else
	{
		bAdvancing = false;
		// After the freeze the drawn pose waits for the simulation to pass the farthest time it showed (less than a frame), then nothing
		Lead = AdvancePeakTime >= 0.f ? FGSTN_HitstopFeelMath::AbsorbLead(AdvancePeakTime, Time) : 0.f;
		if (Lead <= 0.f)
		{
			AdvancePeakTime = -1.f;
		}
	}
	AdvanceMontage = Montage;
	ApplyAdvanceLead(Lead);
}

void UGSTN_ReactionAnimComponent::ApplyAdvanceLead(float Lead)
{
	if (FMath::IsNearlyEqual(Lead, AppliedLead, 1.e-6f))
	{
		return;
	}
	AppliedLead = Lead;
	if (FGSTN_HitstopPresentationHooks::SetMontagePresentationLead)
	{
		FGSTN_HitstopPresentationHooks::SetMontagePresentationLead(GetOwner(), Lead);
	}
}

void UGSTN_ReactionAnimComponent::AnnounceReaction(int32 Winner, bool bRestart)
{
	const FName WinnerName = LoadedSet && LoadedSet->Rules.IsValidIndex(Winner) ? LoadedSet->Rules[Winner].Name : NAME_None;
	if (Winner != AnnouncedRule)
	{
		const FName Ended = AnnouncedRuleName;
		const bool bHadRule = AnnouncedRule != INDEX_NONE;
		AnnouncedRule = Winner;
		AnnouncedRuleName = WinnerName;
		if (bHadRule)
		{
			OnReactionEnded.Broadcast(Ended);
		}
		if (Winner != INDEX_NONE)
		{
			OnReactionStarted.Broadcast(WinnerName, false);
		}
		return;
	}
	if (bRestart && Winner != INDEX_NONE)
	{
		OnReactionStarted.Broadcast(WinnerName, true);
	}
}
