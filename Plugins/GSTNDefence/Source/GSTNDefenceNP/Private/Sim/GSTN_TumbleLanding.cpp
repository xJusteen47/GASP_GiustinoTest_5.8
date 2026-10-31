// Defence, Network Prediction half.

#include "Sim/GSTN_TumbleLanding.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "Events/GSTN_SimEvent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "GSTN_DefenceTags.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputHistory.h"
#include "Math/GSTN_DefenceMath.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Movement/GSTN_TumbleContact.h"
#include "MoverComponent.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatVictim.h"

bool FGSTN_TumbleLanding::CanQueueMoves(const UAbilitySystemComponent* AbilitySystem)
{
	// The move effects queue a layered move on the avatar's Mover: an actor without one (a test rig, a non-Mover entity) gets none
	const AActor* Avatar = AbilitySystem ? AbilitySystem->GetAvatarActor() : nullptr;
	return Avatar && Avatar->FindComponentByClass<UMoverComponent>() != nullptr;
}

namespace
{
	/** A move through one of the core's move effects: the velocity, decay and gravity as SetByCaller values, the lifetime in frames */
	void ApplyMoveEffect(UAbilitySystemComponent* AbilitySystem, TSubclassOf<UGameplayEffect> Effect, const FVector& Velocity, float DecayPerFrame, float GravityScale, int32 Frames)
	{
		if (!FGSTN_TumbleLanding::CanQueueMoves(AbilitySystem) || !Effect || Frames <= 0)
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Effect, 1.f, AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Velocity.Z));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, DecayPerFrame);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, GravityScale);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, Frames * FGSTN_TumbleLanding::FrameSeconds);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	/** Holds the entity still for Frames (the core's at-rest freeze): an in-place or wall tech */
	void ApplyHold(UAbilitySystemComponent* AbilitySystem, int32 Frames)
	{
		if (!FGSTN_TumbleLanding::CanQueueMoves(AbilitySystem) || Frames <= 0)
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_FreezeZeroVelocity::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FreezeSeconds, Frames * FGSTN_TumbleLanding::FrameSeconds);
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	void AppendEvent(FGSTN_CombatTickContext& Context, const FGameplayTag& Type, float Magnitude)
	{
		if (!Context.Outbox)
		{
			return;
		}
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Frame = Context.Frame;
		Event.Instigator = Context.Owner;
		Event.Target = Context.Owner;
		Event.Magnitude = Magnitude;
		Context.Outbox->Add(MoveTemp(Event));
	}

	/** The fragment's tech stamps as the math reads them (a lockout of INDEX_NONE is "none", 0 for the math) */
	FGSTN_TechState ReadTechState(const FGSTN_DefenceFragment* Fragment)
	{
		FGSTN_TechState State;
		if (Fragment)
		{
			State.LastTechPressFrame = Fragment->LastTechPressFrame;
			State.TechLockoutEndFrame = Fragment->TechLockoutEndFrame == INDEX_NONE ? 0 : Fragment->TechLockoutEndFrame;
		}
		return State;
	}
}

void FGSTN_TumbleLanding::CollectPresses(const FGSTN_InputHistory& History, int32 Button, int32 After, TArray<int32, TInlineAllocator<8>>& OutFrames)
{
	OutFrames.Reset();
	// Oldest first: the ring is walked from the oldest age down to the newest
	for (int32 Age = History.GetNumButtonEvents() - 1; Age >= 0; --Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (!Event.bRelease && Event.Button == Button && (After == INDEX_NONE || Event.Frame > After))
		{
			OutFrames.Add(Event.Frame);
		}
	}
}

int32 FGSTN_TumbleLanding::TechFrames(uint8 Type, const FGSTN_TechRules& Rules)
{
	switch (static_cast<EGSTN_TechType>(Type))
	{
	case EGSTN_TechType::RollLeft:
	case EGSTN_TechType::RollRight:
		return FMath::Max(Rules.RollFrames, 1);
	case EGSTN_TechType::WallTech:
	case EGSTN_TechType::WallJumpTech:
		return FMath::Max(Rules.WallTechFrames, 1);
	default:
		return FMath::Max(Rules.InPlaceFrames, 1);
	}
}

uint8 FGSTN_TumbleLanding::PickTechType(EGSTN_TumbleContactKind Kind, const FVector& Normal, const FVector2D& Stick, const FVector& StickOnPlane, const FGSTN_TechRules& Rules, const FGSTN_DefenceEntityData& Data)
{
	const auto Has = [&Data](EGSTN_TechType Type) { return Data.GetTechAbility(static_cast<uint8>(Type)) != nullptr; };
	if (Kind == EGSTN_TumbleContactKind::Wall)
	{
		if (!Rules.bAllowWallTech)
		{
			return static_cast<uint8>(EGSTN_TechType::None);
		}
		// Away from the wall: the stick points along the face's normal
		const bool bAway = StickOnPlane.Size() >= HoldStickMagnitude && StickOnPlane.GetSafeNormal().Dot(Normal) > 0.5;
		if (bAway && Has(EGSTN_TechType::WallJumpTech))
		{
			return static_cast<uint8>(EGSTN_TechType::WallJumpTech);
		}
		return static_cast<uint8>(Has(EGSTN_TechType::WallTech) ? EGSTN_TechType::WallTech : EGSTN_TechType::None);
	}
	if (Kind != EGSTN_TumbleContactKind::Floor)
	{
		return static_cast<uint8>(EGSTN_TechType::None);
	}
	EGSTN_TechType Wanted = EGSTN_TechType::InPlace;
	if (Stick.Size() >= HoldStickMagnitude)
	{
		Wanted = Stick.X >= 0.0 ? EGSTN_TechType::RollRight : EGSTN_TechType::RollLeft;
	}
	if (Has(Wanted))
	{
		return static_cast<uint8>(Wanted);
	}
	return static_cast<uint8>(Has(EGSTN_TechType::InPlace) ? EGSTN_TechType::InPlace : EGSTN_TechType::None);
}

bool FGSTN_TumbleLanding::RegisterPresses(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, int32 ContactFrame, bool& bOutSuccess, int32& OutPressFrame)
{
	bOutSuccess = false;
	OutPressFrame = INDEX_NONE;
	if (!Data.bHasTechSet || !Data.Rules || !Context.Buttons)
	{
		// No tech in the kit: the button does nothing in tumble and starts no lockout (m6 spec 6, owner revision)
		return false;
	}
	FGSTN_CombatSyncCore& Core = Context.Out.Core;
	const FGSTN_TechRules& Rules = Data.Rules->Tech;
	const bool bTumble = Core.ActionState == EGSTN_ActionState::Tumble;
	if (!bTumble && !(Core.ActionState == EGSTN_ActionState::Hitstun && Rules.bTechInHitstunNotTumble))
	{
		return false;
	}
	const int32 Button = Context.Buttons->GetIndex(GSTNCombatButtonTags::Button_Shield);
	if (Button < 0)
	{
		return false;
	}
	FGSTN_DefenceFragment* Existing = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const FGSTN_TechState State = ReadTechState(Existing);
	TArray<int32, TInlineAllocator<8>> Presses;
	CollectPresses(Core.Input.History, Button, State.LastTechPressFrame, Presses);
	if (Presses.IsEmpty() && ContactFrame == INDEX_NONE)
	{
		return false;
	}
	const FGSTN_TechEvaluation Evaluation = UGSTN_DefenceMath::EvaluateTech(State, Presses, ContactFrame, Core.HitstopEndFrame, bTumble, Rules);
	const int32 Lockout = Evaluation.State.TechLockoutEndFrame <= 0 ? INDEX_NONE : Evaluation.State.TechLockoutEndFrame;
	const int32 LastPress = Evaluation.State.LastTechPressFrame;
	if (Existing || Lockout != INDEX_NONE || LastPress != INDEX_NONE)
	{
		FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
		Fragment.TechLockoutEndFrame = Lockout;
		Fragment.LastTechPressFrame = LastPress;
	}
	bOutSuccess = Evaluation.bContactEvaluated && Evaluation.bSuccess;
	OutPressFrame = Evaluation.PressFrame;
	return true;
}

bool FGSTN_TumbleLanding::ConsumeContact(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, const FGSTN_TumbleContactSyncState& Contact)
{
	if (!Data.bHasTechSet || !Data.Rules || Contact.Kind == EGSTN_TumbleContactKind::Ceiling)
	{
		return false;
	}
	FGSTN_CombatSyncCore& Core = Context.Out.Core;
	// The hit says no tech (M4 7b bNoTech): straight to the impact
	if (FGSTN_ImpactProfile::Unpack(Core.Hit.ImpactProfileQ).bNoTech)
	{
		return false;
	}
	bool bSuccess = false;
	int32 PressFrame = INDEX_NONE;
	if (!RegisterPresses(Context, Data, Contact.ContactFrame, bSuccess, PressFrame) || !bSuccess)
	{
		return false;
	}
	const FGSTN_TechRules& Rules = Data.Rules->Tech;
	const FVector Normal = Contact.GetNormal();
	const uint8 Type = PickTechType(Contact.Kind, Normal, Context.Stick, Context.StickOnPlane, Rules, Data);
	if (Type == static_cast<uint8>(EGSTN_TechType::None))
	{
		// The kit cannot tech this contact (a wall where the rules forbid it, no ability of the kind): a failure, the impact resolves
		return false;
	}

	const int32 Frame = Context.Frame;
	const int32 Frames = TechFrames(Type, Rules);
	FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
	Fragment.TechType = Type;
	Fragment.TechEndFrame = Frame + Frames;
	Fragment.PendingStartFrame = Frame;
	Fragment.PendingStartKind = GSTNDefencePendingStart::Tech;

	// The tech cancels the impact outcome entirely: the reaction ends here, the tech's own move replaces the flight
	FGSTN_CombatVictim::EndReaction(Core, Frame);
	switch (static_cast<EGSTN_TechType>(Type))
	{
	case EGSTN_TechType::RollLeft:
	case EGSTN_TechType::RollRight:
	{
		const FVector Along = (Context.StickOnPlane - Normal * Context.StickOnPlane.Dot(Normal)).GetSafeNormal();
		const float Speed = Rules.RollDistance / (Frames * FrameSeconds);
		ApplyMoveEffect(Context.AbilitySystem, UGSTN_GE_PushBack::StaticClass(), Along * Speed, 0.f, 1.f, Frames);
		break;
	}
	case EGSTN_TechType::WallJumpTech:
	{
		const float Speed = Rules.RollDistance / (Frames * FrameSeconds);
		ApplyMoveEffect(Context.AbilitySystem, UGSTN_GE_Knockback::StaticClass(), (Normal + Context.Up).GetSafeNormal() * Speed, 0.f, 1.f, Frames);
		break;
	}
	default:
		ApplyHold(Context.AbilitySystem, Frames);
		break;
	}
	AppendEvent(Context, GSTNDefenceTags::Event_Teched, static_cast<float>(Type));
	return true;
}

void FGSTN_TumbleLanding::OnContactResolved(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, const FGSTN_TumbleContactSyncState& Contact, EGSTN_ImpactOutcome Outcome)
{
	// A surface impact (splat, bounce, break) replaces the landing; a wall without an outcome goes on as M4 has it
	if (!Data.Rules || Outcome != EGSTN_ImpactOutcome::None || Contact.Kind != EGSTN_TumbleContactKind::Floor)
	{
		return;
	}
	FGSTN_CombatSyncCore& Core = Context.Out.Core;
	const int32 Frame = Context.Frame;
	const FGSTN_GroundBounceRules& Bounce = Data.Rules->GroundBounce;
	FGSTN_CombatVictim::EndReaction(Core, Frame);

	FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
	int32 DownedEnd = NeverEnds;
	if (Data.bHasTechSet)
	{
		// A missed tech of a kit that has one: the spec's knockdown, with the price of not teching
		DownedEnd = Contact.ContactFrame + FMath::Max(Data.Rules->Tech.KnockdownFrames, 1) + FMath::Max(Data.Rules->Tech.NoTechKnockdownBonusFrames, 0);
	}
	else if (Bounce.MaxDownedFrames > 0)
	{
		DownedEnd = Contact.ContactFrame + Bounce.MaxDownedFrames;
	}
	Fragment.KnockdownEndFrame = DownedEnd;

	// The mini bounce, steered by the frame's stick: a short launch, the knockback move's flight
	const FVector Velocity = UGSTN_DefenceMath::GroundBounceVelocity(Bounce, Context.Up, Context.StickOnPlane);
	const double Rise = FMath::Max(Velocity.Dot(Context.Up), 0.0);
	const int32 FlightFrames = FMath::Max(FMath::CeilToInt32(2.0 * Rise / 980.0 / FrameSeconds), 1);
	if (!Velocity.IsNearlyZero())
	{
		ApplyMoveEffect(Context.AbilitySystem, UGSTN_GE_Knockback::StaticClass(), Velocity, 0.f, 1.f, FlightFrames);
	}
	if (Data.GroundBounce.IsValid())
	{
		Fragment.PendingStartFrame = Frame;
		Fragment.PendingStartKind = GSTNDefencePendingStart::GroundBounce;
	}
	AppendEvent(Context, GSTNDefenceTags::Event_KnockedDown, 0.f);
}

void FGSTN_TumbleLanding::AfterClocks(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data)
{
	const int32 Frame = Context.Frame;
	if (FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments))
	{
		// What the victim step of this frame decided to start: the model starts it now that the frame's tables are known
		if (Fragment->PendingStartFrame != INDEX_NONE)
		{
			const uint8 Kind = Fragment->PendingStartKind;
			const bool bNow = Fragment->PendingStartFrame == Frame;
			Fragment->PendingStartFrame = INDEX_NONE;
			Fragment->PendingStartKind = GSTNDefencePendingStart::None;
			UClass* Class = Kind == GSTNDefencePendingStart::GroundBounce ? Data.GroundBounce.Get() : Data.GetTechAbility(Fragment->TechType);
			if (bNow && Class && Context.Services)
			{
				Context.Services->StartForcedTimeline(Class);
			}
		}
	}

	// The presses of a tumble between contacts: the lockout (and its tag) while airborne
	bool bSuccess = false;
	int32 PressFrame = INDEX_NONE;
	RegisterPresses(Context, Data, INDEX_NONE, bSuccess, PressFrame);

	// Downed and free to act (the bounce is over): holding the stick rolls up
	FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	if (Fragment && Fragment->IsKnockedDown(Frame) && Context.Out.Core.ActionState == EGSTN_ActionState::Actionable && Context.StickOnPlane.Size() >= HoldStickMagnitude
		&& Data.GetupRoll.IsValid() && Context.Services && Context.Services->StartForcedTimeline(Data.GetupRoll.Get()))
	{
		Fragment->KnockdownEndFrame = Frame;
	}
}

void FGSTN_TumbleLanding::AfterExecute(FGSTN_CombatTickContext& Context)
{
	FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const FGSTN_CombatSyncCore& Core = Context.Out.Core;
	// A get-up command (the only commands the gate admitted while downed) started this frame: the entity is up
	if (Fragment && Fragment->IsKnockedDown(Context.Frame) && Core.ActionState == EGSTN_ActionState::InTimeline && Core.TimelineStartFrame == Context.Frame
		&& Core.ActiveSource == EGSTN_ActivationSource::Command)
	{
		Fragment->KnockdownEndFrame = Context.Frame;
	}
}

void FGSTN_TumbleLanding::SyncDownedLock(FGSTN_CombatTickContext& Context)
{
	UAbilitySystemComponent* AbilitySystem = Context.AbilitySystem;
	if (!AbilitySystem)
	{
		return;
	}
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const bool bDowned = Fragment && Fragment->IsKnockedDown(Context.Frame);
	// The effect is part of the ability system's rolled back state, so the count read here is the state of this frame's timeline
	const int32 Count = AbilitySystem->GetGameplayEffectCount(UGSTN_GE_DownedLock::StaticClass(), nullptr);
	if (bDowned && Count == 0)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_DownedLock::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
		if (Spec.IsValid())
		{
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	else if (!bDowned && Count > 0)
	{
		AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(UGSTN_GE_DownedLock::StaticClass(), nullptr);
	}
}

FGameplayTag FGSTN_TumbleLanding::AdmitsOnly(const FGSTN_CombatTickContext& Context)
{
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	return Fragment && Fragment->IsKnockedDown(Context.Frame) ? GSTNDefenceTags::State_Knockdown.GetTag() : FGameplayTag();
}

UGSTN_GE_DownedLock::UGSTN_GE_DownedLock()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNCombatTags::Lock_Movement);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}
