// Defence, Network Prediction half.

#include "Sim/GSTN_TumbleLanding.h"

#include "Abilities/GSTN_ShieldAbilityData.h"
#include "AbilitySystemComponent.h"
#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Events/GSTN_SimEvent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "GSTN_DefenceTags.h"
#include "GSTNDefenceCoreModule.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputHistory.h"
#include "Layout/GSTN_ButtonLayout.h"
#include "Math/GSTN_DefenceMath.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Movement/GSTN_TumbleContact.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Movement/GSTN_GravityFrame.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Simulation/GSTN_CombatVictim.h"

FVector FGSTN_TumbleLanding::ReadFacing(const AActor* Owner)
{
	// The facing of the end of the previous frame from the Mover sim state (the same on every machine and in a resimulation), never the actor
	UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!Mover)
	{
		return FVector::ForwardVector;
	}
	const UGSTN_MoverSimStateTracker* Tracker = UGSTN_MoverSimStateTracker::FindOrAdd(Mover);
	const FMoverDefaultSyncState* State = Tracker ? Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
	return State ? State->GetOrientation_WorldSpace().Vector() : FVector::ForwardVector;
}

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

bool FGSTN_TumbleLanding::DownedRollCarry(const FGSTN_GroundBounceRules& Rules, int32 DownedStartFrame, int32 DownedFreeFrame, int32 Frame, int32& OutFrom, int32& OutUntil)
{
	OutFrom = INDEX_NONE;
	OutUntil = INDEX_NONE;
	const int32 Buffer = FMath::Max(Rules.RollPressBufferFrames, 1);
	if (DownedStartFrame == INDEX_NONE || DownedFreeFrame == INDEX_NONE || DownedFreeFrame < DownedStartFrame || Frame >= DownedFreeFrame + Buffer)
	{
		return false;
	}
	OutFrom = DownedStartFrame - Buffer + 1;
	OutUntil = DownedFreeFrame;
	return OutUntil > OutFrom;
}

bool FGSTN_TumbleLanding::WantsDownedRoll(const FGSTN_GroundBounceRules& Rules, const FGSTN_InputHistory& History, int32 DodgeButton, int32 Frame, double StickSize,
	int32 CarryFrom, int32 CarryUntil)
{
	if (StickSize < HoldStickMagnitude)
	{
		return false;
	}
	if (Rules.RollInput == EGSTN_DownedRollInput::StickOnly)
	{
		return true;
	}
	if (DodgeButton < 0)
	{
		return false;
	}
	// Window [Frame - Buffer + 1, Frame]: a buffer of 1 is the press frame only
	const int32 Oldest = Frame - FMath::Max(Rules.RollPressBufferFrames, 1) + 1;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
		if (Event.bRelease || Event.Button != DodgeButton || Event.Frame > Frame)
		{
			continue;
		}
		if (Event.Frame >= Oldest || (CarryFrom != INDEX_NONE && Event.Frame >= CarryFrom && Event.Frame < CarryUntil))
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_TumbleLanding::AppliesAutoGetup(EGSTN_AutoGetupScope Scope, bool bPlayerControlled)
{
	return Scope == EGSTN_AutoGetupScope::Everyone || !bPlayerControlled;
}

bool FGSTN_TumbleLanding::IsPlayerEntity(const AActor* Owner)
{
	const APawn* Pawn = Cast<APawn>(Owner);
	return Pawn && Pawn->IsPlayerControlled();
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
	// IO-2: the layout's shield button, the same one the shield press and hold read
	const int32 Button = Context.Buttons->GetIndex(FGSTN_ShieldAbilitySpec::GetLayoutShieldButton());
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
	// The hit's say in the landing, read before the reaction ends: a flat landing, and the speed scale the impact is judged with
	const FGSTN_ImpactProfile Profile = FGSTN_ImpactProfile::Unpack(Context.Out.Core.Hit.ImpactProfileQ);
	const double ImpactSpeed = static_cast<double>(Contact.ImpactSpeedQ) * Profile.SpeedScale;
	FGSTN_CombatVictim::EndReaction(Context.Out.Core, Context.Frame);
	EnterDowned(Context, Data, Contact.ContactFrame, Data.bHasTechSet, ImpactSpeed, Profile.bNoTechBounce);
}

void FGSTN_TumbleLanding::EnterDowned(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, int32 LandingFrame, bool bMissedTech, double ImpactSpeed, bool bNoBounce, double FixedHeight)
{
	const int32 Frame = Context.Frame;
	const FGSTN_GroundBounceRules& Bounce = Data.Rules->GroundBounce;
	FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
	int32 DownedEnd = NeverEnds;
	if (bMissedTech)
	{
		// A missed tech of a kit that has one: the spec's knockdown, with the price of not teching
		DownedEnd = LandingFrame + FMath::Max(Data.Rules->Tech.KnockdownFrames, 1) + FMath::Max(Data.Rules->Tech.NoTechKnockdownBonusFrames, 0);
	}
	else if (Bounce.MaxDownedFrames > 0 && AppliesAutoGetup(Bounce.AutoGetupFor, IsPlayerEntity(Context.Owner)))
	{
		DownedEnd = LandingFrame + Bounce.MaxDownedFrames;
	}
	Fragment.KnockdownEndFrame = DownedEnd;
	// The carry of the get-up roll's dodge press starts here; the first free frame is stamped by AfterClocks
	Fragment.DownedStartFrame = Frame;
	Fragment.DownedFreeFrame = INDEX_NONE;

	// The entity's own gravity, not 980 (gravity audit 2026-11-16: a low-gravity zone floats longer, so the flight lasts longer); 980 without a
	// Mover. Near zero gravity the bounce is capped at 10 s rather than never ending
	const UGSTN_CombatSimComponent* GravitySim = Context.Owner ? Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const double Gravity = GravitySim ? GravitySim->GetBaseGravity().Size() : FGSTN_GravityFrame::NominalGravity;
	// The mini bounce ("tech bounce"), steered by the frame's stick: a short launch, the knockback move's flight. Its height follows the impact
	// speed (owner 2026-11-16: a soft landing barely bounces, a hard one bounces well); the hit may ask for none
	const double BounceSpeed = bNoBounce ? 0.0
		: FixedHeight >= 0.0 ? UGSTN_DefenceMath::BounceSpeedForHeight(Bounce, FixedHeight, Gravity)
		: UGSTN_DefenceMath::TechBounceSpeed(Bounce, ImpactSpeed, Gravity);
	// Momentum and control (owner 2026-11-24, A): the bounce keeps part of the landing's horizontal speed and the stick adds its own, so a
	// launch reads to the ground and the victim chooses where it falls. Both from the frame's inputs and the tracked velocity: deterministic
	const FVector Velocity = BounceSpeed > 0.0
		? UGSTN_DefenceMath::GroundBounceVelocityAtSpeed(Bounce, Context.Up, Context.StickOnPlane, BounceSpeed)
			+ UGSTN_DefenceMath::GroundBounceMomentum(Bounce, Context.Up, Context.StickOnPlane, Context.Velocity)
		: FVector::ZeroVector;
	// Heavier gravity makes it small AND quick (react-hook-1 review: 300 cm/s under normal gravity rose 44 cm and floated about 34 frames)
	const float GravityScale = FMath::Max(Bounce.GroundBounceGravityScale, 0.01f);
	const double Rise = FMath::Max(Velocity.Dot(Context.Up), 0.0);
	const int32 MaxFlightFrames = FMath::Max(FMath::CeilToInt32(10.0 / FrameSeconds), 1);
	const double FlightSeconds = Gravity > UE_KINDA_SMALL_NUMBER ? 2.0 * Rise / (Gravity * GravityScale) : 10.0;
	const int32 FlightFrames = FMath::Clamp(FMath::CeilToInt32(FMath::Min(FlightSeconds, 10.0) / FrameSeconds), 1, MaxFlightFrames);
	if (!Velocity.IsNearlyZero())
	{
		ApplyMoveEffect(Context.AbilitySystem, UGSTN_GE_Knockback::StaticClass(), Velocity, 0.f, GravityScale, FlightFrames);
	}
	if (Data.GroundBounce.IsValid())
	{
		Fragment.PendingStartFrame = Frame;
		Fragment.PendingStartKind = GSTNDefencePendingStart::GroundBounce;
	}
	AppendEvent(Context, GSTNDefenceTags::Event_KnockedDown, 0.f);
}

const FGSTN_SelfLandingBounceTrack* FGSTN_TumbleLanding::FindSelfLandingBounce(const FGSTN_CombatTickContext& Context)
{
	// The move the clock just landed out of first: on that frame RunningTimeline is gone, or is the move's follow-up
	const FGSTN_CombatTimelineData* Move = Context.LandedTimeline ? Context.LandedTimeline : Context.RunningTimeline;
	if (!Move)
	{
		return nullptr;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instanced : Move->Tracks)
	{
		if (const FGSTN_SelfLandingBounceTrack* Track = Instanced.GetPtr<FGSTN_SelfLandingBounceTrack>())
		{
			return Track;
		}
	}
	return nullptr;
}

bool FGSTN_TumbleLanding::IsSelfKnockdownWindowOpen(const FGSTN_CombatTickContext& Context)
{
	if (Context.Out.Core.ActionState != EGSTN_ActionState::InTimeline || !Context.Timeline || Context.Timeline->bRecoil)
	{
		return false;
	}
	for (const FGSTN_TimelineTrack_Tags* Window : Context.Timeline->TagWindows)
	{
		if (Window && Window->Tags.HasTagExact(GSTNDefenceTags::Move_KnockdownOnLanding))
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_TumbleLanding::SelfKnockdown(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, const UClass** OutBounceAbility)
{
	if (OutBounceAbility)
	{
		*OutBounceAbility = nullptr;
	}
	if (!Data.Rules)
	{
		return false;
	}
	FGSTN_DefenceFragment* Existing = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const bool bArmed = Existing && (Existing->HitFlags & GSTNDefenceHitFlags::SelfKnockdownArmed) != 0;
	// Armed (airborne inside the window on the previous frame) and the clock took the move's landing on this frame: the move's own Landing
	// window, follow-up or landing lag ended it before this hook (owner 2026-11-24: the dropkick's hop never ran, its Landing window
	// autocancelled the move on the touchdown frame and the next frame found no window). The landing is this one
	const bool bLandedOutOfMove = bArmed && Context.LandedTimeline != nullptr;
	if (bLandedOutOfMove)
	{
		UE_LOG(LogGSTNDefence, Log, TEXT("SelfKnockdown: frame %d the clock landed the armed move (state %d after it)"), Context.Frame,
			static_cast<int32>(Context.Out.Core.ActionState));
	}
	else if (!IsSelfKnockdownWindowOpen(Context))
	{
		// Outside the window (or the move is over) nothing stays armed: a later landing is an ordinary one
		if (bArmed)
		{
			UE_LOG(LogGSTNDefence, Log, TEXT("SelfKnockdown: frame %d disarmed, window closed (state %d, timeline output %s, tag windows %d)"), Context.Frame,
				static_cast<int32>(Context.Out.Core.ActionState), Context.Timeline ? TEXT("yes") : TEXT("no"), Context.Timeline ? Context.Timeline->TagWindows.Num() : -1);
			Existing->HitFlags &= ~GSTNDefenceHitFlags::SelfKnockdownArmed;
		}
		return false;
	}
	if (!bLandedOutOfMove && !Context.bGrounded)
	{
		if (!bArmed)
		{
			UE_LOG(LogGSTNDefence, Log, TEXT("SelfKnockdown: frame %d armed (airborne inside the window)"), Context.Frame);
			FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments).HitFlags |= GSTNDefenceHitFlags::SelfKnockdownArmed;
		}
		return false;
	}
	if (!bArmed)
	{
		// Grounded without having left the ground inside the window (the window opened on the takeoff frame): not a landing yet
		return false;
	}

	// Landed inside the window: the move ends here and its user is downed as a tumble landing downs a victim. No tech evaluation (no contact, no
	// reaction to tech out of): the way out is the owner's one, the get-up roll from the stick or the neutral get-up
	Existing->HitFlags &= ~GSTNDefenceHitFlags::SelfKnockdownArmed;
	UE_LOG(LogGSTNDefence, Log, TEXT("SelfKnockdown: frame %d landed inside the window: the move's user is downed"), Context.Frame);
	// The move's own hop (owner 2026-11-16, the dropkick like Big Band's), read before the move is cancelled; else the rules' at their speed
	const FGSTN_SelfLandingBounceTrack* Own = FindSelfLandingBounce(Context);
	const bool bFlat = Own && Own->Mode == EGSTN_SelfLandingBounce::None;
	const double Height = Own && Own->Mode == EGSTN_SelfLandingBounce::Height ? FMath::Max(static_cast<double>(Own->Height), 0.0) : -1.0;
	// The move's own bounce clip (its ability), resolved now: the move is cancelled below, so AfterClocks can no longer read its tracks
	const UClass* OwnBounce = Own && !Own->BounceAbility.IsNull() ? Own->BounceAbility.Get() : nullptr;
	FGSTN_CombatTimelineLogic::CancelForOtherCommand(Context.Out.Core, Context.Frame);
	EnterDowned(Context, Data, Context.Frame, false, static_cast<double>(Data.Rules->GroundBounce.SelfKnockdownImpactSpeed), bFlat, Height);
	if (OwnBounce)
	{
		// Pending even for a kit without a GroundBounce of its own: the move names the ability to start
		FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
		Fragment.PendingStartFrame = Context.Frame;
		Fragment.PendingStartKind = GSTNDefencePendingStart::GroundBounce;
	}
	if (OutBounceAbility)
	{
		*OutBounceAbility = OwnBounce;
	}
	return true;
}

void FGSTN_TumbleLanding::AfterClocks(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data)
{
	const int32 Frame = Context.Frame;
	const UClass* OwnBounce = nullptr;
	SelfKnockdown(Context, Data, &OwnBounce);
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
			// A move's own bounce ability first (Self Landing Bounce track); not registered by the kit = the kit's ground bounce as before
			const bool bOwnStarted = bNow && Kind == GSTNDefencePendingStart::GroundBounce && OwnBounce && Context.Services
				&& Context.Services->StartForcedTimeline(OwnBounce);
			if (!bOwnStarted && bNow && Class && Context.Services)
			{
				Context.Services->StartForcedTimeline(Class);
			}
		}
	}

	// The presses of a tumble between contacts: the lockout (and its tag) while airborne
	bool bSuccess = false;
	int32 PressFrame = INDEX_NONE;
	RegisterPresses(Context, Data, INDEX_NONE, bSuccess, PressFrame);

	// Downed, free to act (the bounce ability is over) and lying on the floor (the bounce has landed: never a roll from the air, react-hook-1
	// review): holding the stick rolls up on any such frame, toward the head when the stick points against the facing and the kit has a roll back
	FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	// Hit while lying (playtest 2026-11-17, Saved/PF1117c/diag strong_100.jsonl: a dummy still downed from an earlier knockdown was launched,
	// but the Knockdown tag outlived the hit, so the lying pose (Knockdown rule, priority above Tumble) showed through the whole flight and the
	// next bounce): a new hit's reaction ends the knockdown, the hit's own reaction takes over, and a later landing downs it again
	if (Fragment && Fragment->IsKnockedDown(Frame) && Context.Out.Core.Hit.HasPendingReaction())
	{
		Fragment->KnockdownEndFrame = Frame;
	}
	// The roll's input is the rules' (owner 2026-11-17: dodge + stick by default, so a stick held to walk away never rolls by accident). The
	// dodge button is the button layout's Movement.Dodge, never a fixed button
	const int32 DodgeButton = Context.Buttons
		? Context.Buttons->GetIndex(UGSTN_ButtonLayout::GetButton(TEXT("Movement.Dodge"), GSTNCombatButtonTags::Button_Dodge.GetTag())) : INDEX_NONE;
	const bool bDownedFree = Fragment && Fragment->IsKnockedDown(Frame) && Context.Out.Core.ActionState == EGSTN_ActionState::Actionable && Context.bGrounded;
	// The first frame the downed body is free: presses made before it (during the bounce and ground hit) are carried to it (owner 2026-11-17)
	if (bDownedFree && (Fragment->DownedFreeFrame == INDEX_NONE || Fragment->DownedFreeFrame < Fragment->DownedStartFrame))
	{
		Fragment->DownedFreeFrame = Frame;
	}
	int32 CarryFrom = INDEX_NONE;
	int32 CarryUntil = INDEX_NONE;
	if (bDownedFree && Data.Rules)
	{
		DownedRollCarry(Data.Rules->GroundBounce, Fragment->DownedStartFrame, Fragment->DownedFreeFrame, Frame, CarryFrom, CarryUntil);
	}
	if (bDownedFree
		&& Data.Rules && WantsDownedRoll(Data.Rules->GroundBounce, Context.Out.Core.Input.History, DodgeButton, Frame, Context.StickOnPlane.Size(), CarryFrom, CarryUntil)
		&& Data.GetupRoll.IsValid() && Context.Services)
	{
		const bool bBack = Data.GetupRollBack.IsValid() && Context.StickOnPlane.Dot(ReadFacing(Context.Owner)) < 0.0;
		if (Context.Services->StartForcedTimeline(bBack ? Data.GetupRollBack.Get() : Data.GetupRoll.Get()))
		{
			Fragment->KnockdownEndFrame = Frame;
		}
	}
	// A downed state with an end (rules MaxDownedFrames > 0) that runs out with nobody pressing anything: the neutral get-up plays on its last
	// frame, so the body gets up instead of popping from lying to idle (playtest 2026-11-17: dummies lay forever, no get-up ever seen)
	else if (Fragment && Fragment->KnockdownEndFrame != NeverEnds && Fragment->IsKnockedDown(Frame) && Frame + 1 >= Fragment->KnockdownEndFrame
		&& Context.Out.Core.ActionState == EGSTN_ActionState::Actionable && Context.bGrounded && Data.GetupNeutral.IsValid() && Context.Services)
	{
		if (Context.Services->StartForcedTimeline(Data.GetupNeutral.Get()))
		{
			Fragment->KnockdownEndFrame = Frame;
		}
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
