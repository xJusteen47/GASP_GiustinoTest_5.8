// Combat simulation (GAS-NPP fork): the hit bounce (owner 2026-11-23, the Cr.L Aerial stomp).

#include "Simulation/GSTN_HitBounceSim.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_CombatNPTags.h"
#include "HAL/IConsoleManager.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Timeline/GSTN_TimelineTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNHitBounce, Log, All);

namespace GSTNHitBounce
{
	// The owner tunes these live from the GSTN Debug window (Tools > Debug > GSTN Debug Options), floats so the window shows a number field.
	// Simulation inputs: in a networked session every machine must hold the same values. -1 = the move's data (its Hit Bounce track)
	TAutoConsoleVariable<float> CVarMax(TEXT("gstn.Combat.Debug.HitBounceMax"), -1.f,
		TEXT("Hit bounce: bounces in a row before the move stops bouncing (owner default 2; 0 = never). -1 = the move's data. Reset on landing and on being hit"));
	TAutoConsoleVariable<float> CVarApexCm(TEXT("gstn.Combat.Debug.HitBounceApexCm"), -1.f,
		TEXT("Hit bounce: apex height above the launch point, cm (data default 150). -1 = the move's data"));
	TAutoConsoleVariable<float> CVarRiseFrames(TEXT("gstn.Combat.Debug.HitBounceRiseFrames"), -1.f,
		TEXT("Hit bounce: frames from the launch to the apex, 62.5 per second (data default 18). -1 = the move's data"));
	TAutoConsoleVariable<float> CVarBackCm(TEXT("gstn.Combat.Debug.HitBounceBackCm"), -1.f,
		TEXT("Hit bounce: distance back (against the facing) during the rise, cm (data default 40). -1 = the move's data"));
	TAutoConsoleVariable<float> CVarActionableFrames(TEXT("gstn.Combat.Debug.HitBounceActionableFrames"), -1.f,
		TEXT("Hit bounce: frames from the launch before the character can act again (aerial, air jump, air dodge; data default 24). -1 = the move's data"));
	TAutoConsoleVariable<bool> CVarOff(TEXT("gstn.Combat.Debug.HitBounceOff"), false,
		TEXT("Hit bounce: on = no move bounces (they play as normal moves)"));

	namespace Presence
	{
		constexpr uint8 Count = 1 << 0;
		constexpr uint8 Armed = 1 << 1;
		constexpr uint8 Launch = 1 << 2;
		constexpr uint8 All = Count | Armed | Launch;
	}
}

// ---- Fragment

bool FGSTN_HitBounceFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_HitBounceFragment>(GSTNFeatureIds::HitBounce);
}

void FGSTN_HitBounceFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::HitBounce);
}

bool FGSTN_HitBounceFragment::IsDefault() const
{
	return *this == FGSTN_HitBounceFragment();
}

bool FGSTN_HitBounceFragment::operator==(const FGSTN_HitBounceFragment& Other) const
{
	return Count == Other.Count && ArmedFrame == Other.ArmedFrame && ArmedTimelineStart == Other.ArmedTimelineStart && LaunchFrame == Other.LaunchFrame
		&& LockFrames == Other.LockFrames;
}

void FGSTN_HitBounceFragment::NetSerialize(FArchive& Ar)
{
	using namespace GSTNHitBounce;

	uint8 Mask = 0;
	if (Ar.IsSaving())
	{
		if (IsDefault())
		{
			return;
		}
		Mask |= Count != 0 ? Presence::Count : 0;
		Mask |= (ArmedFrame != INDEX_NONE || ArmedTimelineStart != INDEX_NONE) ? Presence::Armed : 0;
		Mask |= (LaunchFrame != INDEX_NONE || LockFrames != 0) ? Presence::Launch : 0;
	}
	else
	{
		*this = FGSTN_HitBounceFragment();
		// The registry reads into a default instance: a payload of zero bytes is the default fragment
		if (Ar.TotalSize() == 0)
		{
			return;
		}
	}

	Ar << Mask;
	if (Ar.IsLoading() && (Mask == 0 || (Mask & ~Presence::All) != 0))
	{
		// No group at all, or a bit this build does not know: a corrupt or foreign payload
		Ar.SetError();
		*this = FGSTN_HitBounceFragment();
		return;
	}
	if (Mask & Presence::Count)
	{
		Ar << Count;
	}
	if (Mask & Presence::Armed)
	{
		Ar << ArmedFrame << ArmedTimelineStart;
	}
	if (Mask & Presence::Launch)
	{
		Ar << LaunchFrame << LockFrames;
	}
	if (Ar.IsLoading() && Ar.IsError())
	{
		*this = FGSTN_HitBounceFragment();
	}
}

bool FGSTN_HitBounceFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	return !(*this == static_cast<const FGSTN_HitBounceFragment&>(Authority));
}

void FGSTN_HitBounceFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("HitBounce n %d", static_cast<int32>(Count));
	if (ArmedFrame != INDEX_NONE)
	{
		Out.Appendf(" armed %d/%d", ArmedFrame, ArmedTimelineStart);
	}
	if (LaunchFrame != INDEX_NONE)
	{
		Out.Appendf(" launch %d+%d", LaunchFrame, static_cast<int32>(LockFrames));
	}
}

// ---- Rules

FGSTN_HitBounceOverrides FGSTN_HitBounceTickExtension::ReadOverrides()
{
	using namespace GSTNHitBounce;
	FGSTN_HitBounceOverrides Overrides;
	Overrides.MaxConsecutiveBounces = CVarMax.GetValueOnAnyThread();
	Overrides.ApexHeightCm = CVarApexCm.GetValueOnAnyThread();
	Overrides.RiseFrames = CVarRiseFrames.GetValueOnAnyThread();
	Overrides.BackDistanceCm = CVarBackCm.GetValueOnAnyThread();
	Overrides.ActionableFrames = CVarActionableFrames.GetValueOnAnyThread();
	Overrides.bDisabled = CVarOff.GetValueOnAnyThread();
	return Overrides;
}

int32 FGSTN_HitBounceTickExtension::FindBouncingHit(TConstArrayView<FGSTN_AttackerHit> Hits, const FGSTN_HitBounceTuning& Tuning)
{
	for (int32 Index = 0; Index < Hits.Num(); ++Index)
	{
		const FGSTN_AttackerHit& Hit = Hits[Index];
		if (FGSTN_HitBounceRules::IsBouncingHit(Hit.bRicochet, Hit.bBlocked, Hit.bParried, Hit.bClank, Tuning))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FGSTN_HitBounceTickExtension::TimelineFrameOf(const FGSTN_CombatSyncCore& Core, int32 Frame)
{
	return FMath::Max(Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
}

bool FGSTN_HitBounceTickExtension::TryArm(FGSTN_HitBounceFragment& Fragment, const FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_TimelineTrack_HitBounce& Track,
	const FGSTN_HitBounceTuning& Tuning, bool bHasBouncingHit)
{
	// A hit of a recoil (landing lag, ricochet) is no hit of the move's hitboxes; at the limit the move plays on as a normal move. A timeline that
	// started on this frame (a command of step 8, after the attacker step) did not make the frame's hits: the one it replaced did, and is gone
	if (!bHasBouncingHit || Core.ActionState != EGSTN_ActionState::InTimeline || Core.RecoilStartFrame != INDEX_NONE || Core.TimelineStartFrame == Frame
		|| !FGSTN_HitBounceRules::CanBounce(Fragment.Count, Tuning) || !Track.Contains(TimelineFrameOf(Core, Frame)))
	{
		return false;
	}
	// A second hit of the same run (another target) keeps the first arm: the bounce waits for the longest hitstop anyway
	if (Fragment.IsArmed() && Fragment.ArmedTimelineStart == Core.TimelineStartFrame)
	{
		return false;
	}
	Fragment.ArmedFrame = Frame;
	Fragment.ArmedTimelineStart = Core.TimelineStartFrame;
	return true;
}

FGSTN_HitBounceTickExtension::EArmedAction FGSTN_HitBounceTickExtension::ResolveArmed(const FGSTN_HitBounceFragment& Fragment, const FGSTN_CombatSyncCore& Core, int32 Frame)
{
	if (!Fragment.IsArmed())
	{
		return EArmedAction::None;
	}
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame != Fragment.ArmedTimelineStart)
	{
		return EArmedAction::Disarm;
	}
	// One frame after the hitstop window (HitstopEndFrame is exclusive and the freeze finishes on it), and never on the hit's own frame
	const int32 HitstopEnd = Core.HitstopEndFrame >= Fragment.ArmedFrame ? Core.HitstopEndFrame : Fragment.ArmedFrame;
	return Frame > HitstopEnd ? EArmedAction::Fire : EArmedAction::Wait;
}

void FGSTN_HitBounceTickExtension::StepBookkeeping(FGSTN_HitBounceFragment& Fragment, int32 Frame, bool bInReactionOrKO, bool bGrounded)
{
	if (bInReactionOrKO)
	{
		// Being hit ends the bounce, its lock and the count (owner 3A): the reaction rules the entity now
		Fragment = FGSTN_HitBounceFragment();
		return;
	}
	if (Fragment.LaunchFrame != INDEX_NONE && Frame >= Fragment.LaunchFrame + Fragment.LockFrames)
	{
		Fragment.LaunchFrame = INDEX_NONE;
		Fragment.LockFrames = 0;
	}
	// The ground of the end of the previous frame: on the launch frame and the one after, it may still be the ground the bounce left from
	const bool bJustLaunched = Fragment.LaunchFrame != INDEX_NONE && Frame <= Fragment.LaunchFrame + 1;
	if (bGrounded && !Fragment.IsArmed() && !bJustLaunched)
	{
		Fragment.Count = 0;
	}
}

void FGSTN_HitBounceTickExtension::MarkLaunched(FGSTN_HitBounceFragment& Fragment, int32 Frame, const FGSTN_HitBounceTuning& Tuning)
{
	Fragment.Count = static_cast<uint8>(FMath::Min<int32>(Fragment.Count + 1, 255));
	Fragment.ArmedFrame = INDEX_NONE;
	Fragment.ArmedTimelineStart = INDEX_NONE;
	Fragment.LaunchFrame = Frame;
	Fragment.LockFrames = static_cast<uint8>(FMath::Clamp(Tuning.ActionableFrames, 1, 255));
}

// ---- Hooks

void FGSTN_HitBounceTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	if (Point == EGSTN_CombatHookPoint::AfterClocks)
	{
		FGSTN_HitBounceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_HitBounceFragment>(Context.Out.Fragments);
		if (!Fragment)
		{
			return;
		}
		const FGSTN_CombatSyncCore& Core = Context.Out.Core;
		StepBookkeeping(*Fragment, Context.Frame, FGSTN_CombatVictim::IsInReaction(Core) || Core.ActionState == EGSTN_ActionState::KO, Context.bGrounded);
		switch (ResolveArmed(*Fragment, Core, Context.Frame))
		{
		case EArmedAction::Disarm:
			Fragment->ArmedFrame = INDEX_NONE;
			Fragment->ArmedTimelineStart = INDEX_NONE;
			break;
		case EArmedAction::Fire:
			Fire(Context, *Fragment);
			break;
		default:
			break;
		}
		if (Fragment->IsDefault())
		{
			Context.Out.Fragments.RemoveAll([](const TInstancedStruct<FGSTN_SyncFragment>& Any) { return Any.GetPtr<FGSTN_HitBounceFragment>() != nullptr; });
		}
		return;
	}

	if (Point == EGSTN_CombatHookPoint::AfterExecute)
	{
		const FGSTN_CombatTimelineData* Running = Context.RunningTimeline;
		if (Context.AttackerHits.IsEmpty() || !Running)
		{
			return;
		}
		const FGSTN_TimelineTrack_HitBounce* Track = FGSTN_HitBounceRules::FindTrack(*Running);
		if (!Track)
		{
			return;
		}
		const FGSTN_HitBounceTuning Tuning = FGSTN_HitBounceRules::Resolve(*Track, GetOverrides());
		const bool bHasBouncingHit = FindBouncingHit(Context.AttackerHits, Tuning) != INDEX_NONE;
		const FGSTN_HitBounceFragment* Existing = FGSTN_SyncFragmentList::Find<FGSTN_HitBounceFragment>(Context.Out.Fragments);
		FGSTN_HitBounceFragment Candidate = Existing ? *Existing : FGSTN_HitBounceFragment();
		if (TryArm(Candidate, Context.Out.Core, Context.Frame, *Track, Tuning, bHasBouncingHit))
		{
			// Added only when it arms, so a move that whiffs or hits at the limit leaves no fragment behind
			FGSTN_SyncFragmentList::FindOrAdd<FGSTN_HitBounceFragment>(Context.Out.Fragments) = Candidate;
		}
	}
}

bool FGSTN_HitBounceTickExtension::BlocksActions(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_HitBounceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_HitBounceFragment>(Context.Out.Fragments);
	return Fragment && Fragment->IsLocked(Context.Frame);
}

void FGSTN_HitBounceTickExtension::Fire(FGSTN_CombatTickContext& Context, FGSTN_HitBounceFragment& Fragment) const
{
	const FGSTN_CombatTimelineData* Running = Context.RunningTimeline;
	const FGSTN_TimelineTrack_HitBounce* Track = Running ? FGSTN_HitBounceRules::FindTrack(*Running) : nullptr;
	if (!Track)
	{
		// The armed timeline is the running one (ResolveArmed), so its data has the track; content that changed under it ends the arm
		Fragment.ArmedFrame = INDEX_NONE;
		Fragment.ArmedTimelineStart = INDEX_NONE;
		return;
	}
	const FGSTN_HitBounceTuning Tuning = FGSTN_HitBounceRules::Resolve(*Track, GetOverrides());

	// The move is over: its own backflip clip takes over when the kit grants it, else it simply ends (no hitbox, no landing of the move).
	// Only a class already in memory: the ability's content loader holds it (UGSTN_GA_BaseCombat::LoadTimelineDataContent)
	bool bHandedOver = false;
	if (Context.Services)
	{
		const UClass* BounceMove = Track->BounceMove.Get();
		bHandedOver = BounceMove && Context.Services->StartForcedTimeline(BounceMove);
		if (!bHandedOver)
		{
			Context.Services->CancelRunningTimeline();
		}
	}

	const FGSTN_HitBounceMoverFrame Mover = ReadMoverFrame(Context);
	const FGSTN_HitBounceLaunch Launch = FGSTN_HitBounceRules::ComputeLaunch(Tuning, Context.Up, Mover.Facing, Mover.Gravity, FrameSeconds);
	ApplyLaunch(Context, Launch);
	MarkLaunched(Fragment, Context.Frame, Tuning);
	UE_LOG(LogGSTNHitBounce, Verbose, TEXT("%s frame %d: hit bounce %d, launch (%.0f %.0f %.0f) gravity x%.2f for %d frames, lock %d, bounce move %d"),
		*GetNameSafe(Context.Owner), Context.Frame, static_cast<int32>(Fragment.Count), Launch.Velocity.X, Launch.Velocity.Y, Launch.Velocity.Z, Launch.GravityScale,
		Launch.Frames, static_cast<int32>(Fragment.LockFrames), bHandedOver ? 1 : 0);
}

FGSTN_HitBounceMoverFrame FGSTN_HitBounceTickExtension::ReadMoverFrame(const FGSTN_CombatTickContext& Context) const
{
	FGSTN_HitBounceMoverFrame Frame;
	UMoverComponent* Mover = Context.Owner ? Context.Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!Mover)
	{
		return Frame;
	}
	Frame.bValid = true;
	// The facing of the end of the previous frame from the Mover sim state (the same on every machine and in a resimulation), never the actor.
	// The gravity is the one the movement mode pulls with (the jump phases' scale included), which the knockback move scales the same way
	const UGSTN_MoverSimStateTracker* Tracker = UGSTN_MoverSimStateTracker::FindOrAdd(Mover);
	const FMoverDefaultSyncState* State = Tracker ? Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
	Frame.Facing = State ? State->GetOrientation_WorldSpace().Vector() : FVector::ForwardVector;
	Frame.Gravity = Mover->GetGravityAcceleration().Size();
	return Frame;
}

void FGSTN_HitBounceTickExtension::ApplyLaunch(FGSTN_CombatTickContext& Context, const FGSTN_HitBounceLaunch& Launch) const
{
	UAbilitySystemComponent* AbilitySystem = Context.AbilitySystem;
	const AActor* Avatar = AbilitySystem ? AbilitySystem->GetAvatarActor() : nullptr;
	if (!Avatar || !Avatar->FindComponentByClass<UMoverComponent>() || Launch.Frames <= 0)
	{
		return;
	}
	// Rule 2 of the project: the character moves through a Mover queued move, the core's knockback move (velocity replaced and Falling forced on its
	// first frame, the gravity scaled while it lives). Not a tumble: the combat model reads that move's contact probe only in the Tumble state
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_Knockback::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Launch.Velocity.X));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Launch.Velocity.Y));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Launch.Velocity.Z));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 0.f);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, Launch.GravityScale);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, static_cast<float>(Launch.Frames * FrameSeconds));
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}
