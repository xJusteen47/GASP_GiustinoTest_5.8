// Defence, Network Prediction half.

#include "Sim/GSTN_DefenceTickExtension.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Effects/GSTN_GE_ShieldDamage.h"
#include "Effects/GSTN_GE_ShieldRestore.h"
#include "GameFramework/Actor.h"
#include "GSTNDefenceCoreModule.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_DefenceLibrary.h"
#include "GSTN_DefenceTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Math/GSTN_DefenceMath.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceTrackHooks.h"
#include "Pipeline/GSTN_DefenceHitOutcome.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Timeline/GSTN_TimelineTypes.h"

uint8 FGSTN_DefenceTickExtension::GetTypeId() const
{
	return GSTNFeatureIds::Defence;
}

FGSTN_ShieldRates FGSTN_DefenceTickExtension::ReadRates(const UAbilitySystemComponent* AbilitySystem, const UGSTN_DefenceRules& Rules)
{
	if (const UGSTN_DefenceAttributeSet* Set = UGSTN_DefenceLibrary::FindDefenceAttributeSet(AbilitySystem))
	{
		return Set->ReadShieldRates(Rules.Shield.RegenDelayFrames);
	}
	return FGSTN_ShieldRates::FromRules(Rules.Shield);
}

bool FGSTN_DefenceTickExtension::ShieldBlocksAt(const FGSTN_DefenceFragment& Fragment, int32 Frame, const UGSTN_DefenceRules& Rules, bool bAvailable)
{
	return bAvailable && Fragment.IsRaised() && !Fragment.IsBreaking() && Fragment.ShieldPressFrame != INDEX_NONE
		&& Frame >= Fragment.ShieldPressFrame + FMath::Max(Rules.Shield.StartupFrames, 0);
}

void FGSTN_DefenceTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	const FGSTN_DefenceEntityData* Data = FGSTN_DefenceAttach::Find(Context.Owner);
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (Data && Data->Rules)
		{
			StepClocks(Context, *Data);
		}
		break;
	case EGSTN_CombatHookPoint::AfterExecute:
		if (Data && Data->Rules)
		{
			ConsumeStamps(Context, *Data);
		}
		break;
	case EGSTN_CombatHookPoint::BeforeTagProjection:
		ProjectTags(Context, Data);
		break;
	default:
		break;
	}
}

bool FGSTN_DefenceTickExtension::BlocksActions(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	if (!Fragment)
	{
		return false;
	}
	const int32 Frame = Context.Frame;
	return Fragment->IsShieldStunned(Frame) || Fragment->IsBreaking() || Fragment->IsPunished(Frame) || Fragment->IsKnockedDown(Frame);
}

uint32 FGSTN_DefenceTickExtension::HeldCancelConditions(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	const FGSTN_CombatSyncCore& Core = Context.Out.Core;
	if (!Fragment || Fragment->LastParryFrame == INDEX_NONE || Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame == INDEX_NONE)
	{
		return 0;
	}
	// A parry of the running timeline (like a hit confirm: a new timeline starts without it)
	return Fragment->LastParryFrame >= Core.TimelineStartFrame ? (1u << static_cast<uint32>(EGSTN_CancelCondition::OnParry)) : 0;
}

void FGSTN_DefenceTickExtension::WriteMirror(UAbilitySystemComponent* AbilitySystem, FGSTN_DefenceFragment& Fragment, double Value, int32 Frame)
{
	if (AbilitySystem && UGSTN_DefenceLibrary::FindDefenceAttributeSet(AbilitySystem))
	{
		UGSTN_GE_ShieldDamage::ApplyMirror(*AbilitySystem, nullptr, static_cast<float>(Value));
	}
	Fragment.MirrorValueQ = FGSTN_DefenceFragment::QuantizeValue(Value);
	Fragment.MirrorFrame = Frame;
}

void FGSTN_DefenceTickExtension::ApplyPushback(UAbilitySystemComponent* AbilitySystem, const FGSTN_DefenceFragment& Fragment)
{
	if (!AbilitySystem || Fragment.PushbackFrames == 0)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_PushBack::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	const FVector Velocity = Fragment.GetPushbackVelocity();
	const float Frames = static_cast<float>(Fragment.PushbackFrames);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Velocity.Z));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 1.f / Frames);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, Frames * FrameSeconds);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void FGSTN_DefenceTickExtension::StepClocks(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data)
{
	FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	if (!Fragment)
	{
		return;
	}
	const UGSTN_DefenceRules& Rules = *Data.Rules;
	const int32 Frame = Context.Frame;
	UAbilitySystemComponent* AbilitySystem = Context.AbilitySystem;
	const FGSTN_ShieldRates Rates = ReadRates(AbilitySystem, Rules);
	bool bMirror = false;

	// A break that ran its course: the shield comes back at a fraction of the maximum, not raised
	if (Fragment->IsBreaking() && Fragment->BreakEndFrame != INDEX_NONE && Frame >= Fragment->BreakEndFrame)
	{
		FGSTN_ShieldView View = Fragment->MakeShieldView(Rates);
		View.Value = FMath::Clamp(static_cast<double>(Rules.Shield.RecoverFractionAfterBreak) * Rates.Max, 0.0, Rates.Max);
		View.StampFrame = Frame;
		View.bHeld = false;
		Fragment->SetShieldView(View);
		Fragment->ShieldFlags &= ~GSTNShieldFlags::Breaking;
		Fragment->BreakEndFrame = INDEX_NONE;
		bMirror = true;
	}

	// A restore request on the mirror (a pickup, a healer): the difference the model did not write itself, applied as a re-stamp
	if (AbilitySystem && Fragment->MirrorFrame != INDEX_NONE)
	{
		const double Delta = UGSTN_GE_ShieldRestore::RequestedDelta(*AbilitySystem, static_cast<float>(FGSTN_DefenceFragment::DequantizeValue(Fragment->MirrorValueQ)));
		if (FMath::Abs(Delta) >= 0.01)
		{
			FGSTN_ShieldView View = Fragment->MakeShieldView(Rates);
			View.Value = FMath::Clamp(UGSTN_DefenceMath::ShieldValueAt(View, Frame, Rates) + Delta, 0.0, Rates.Max);
			View.StampFrame = Frame;
			Fragment->SetShieldView(View);
			bMirror = true;
		}
	}

	// The value at the frame: zero from decay while raised ends the hold (the hook reads the flag next tick, the forced drop)
	const FGSTN_ShieldView View = Fragment->MakeShieldView(Rates);
	const double Value = UGSTN_DefenceMath::ShieldValueAt(View, Frame, Rates);
	if (Fragment->IsRaised())
	{
		if (Value <= 0.0)
		{
			Fragment->ShieldFlags |= GSTNShieldFlags::Empty;
		}
	}
	else
	{
		Fragment->ShieldFlags &= ~GSTNShieldFlags::Empty;
	}

	// The mirror: at every re-stamp since the last write, and every MirrorIntervalFrames while the value moves (held, or regenerating)
	const bool bChanging = (Fragment->IsRaised() && Value > 0.0) || (!Fragment->IsRaised() && !Fragment->IsBreaking() && Value < Rates.Max);
	const bool bRestamped = Fragment->ShieldStampFrame != INDEX_NONE && (Fragment->MirrorFrame == INDEX_NONE || Fragment->ShieldStampFrame >= Fragment->MirrorFrame);
	if (bMirror || bRestamped || (bChanging && (Fragment->MirrorFrame == INDEX_NONE || Frame - Fragment->MirrorFrame >= MirrorIntervalFrames)))
	{
		WriteMirror(AbilitySystem, *Fragment, UGSTN_DefenceMath::ShieldValueAt(Fragment->MakeShieldView(Rates), Frame, Rates), Frame);
	}

	// The pushback of a blocked hit starts at the end of the hit's hitstop, or on the first tick after it when the applier wrote it later
	// (a hit without hitstop: the applier runs after this hook on the frame after the hit). Given once, then cleared
	if (Fragment->PushbackStartFrame != INDEX_NONE && Frame >= Fragment->PushbackStartFrame)
	{
		ApplyPushback(AbilitySystem, *Fragment);
		Fragment->ClearPushback();
	}

	// A perfect shield's advantage (m6 spec 5.4, source Shield): the shield timeline ends when the parry recovery does, so the defender acts
	// before its release frames would have let it
	FGSTN_CombatSyncCore& Core = Context.Out.Core;
	if (Fragment->ParryRecoveryEndFrame != INDEX_NONE && Frame == Fragment->ParryRecoveryEndFrame && Core.ActionState == EGSTN_ActionState::InTimeline
		&& Fragment->ShieldPressFrame != INDEX_NONE && Core.TimelineStartFrame == Fragment->ShieldPressFrame)
	{
		FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
	}
}

void FGSTN_DefenceTickExtension::ResolveParryTrack(const FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, int32 TrackIndex, uint8& OutProfile, uint16& OutHurtboxMask)
{
	OutProfile = 0;
	OutHurtboxMask = 0;
	const UGSTN_CombatSimComponent* Combat = Context.Owner ? Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const UGSTN_CombatTableSet* Tables = Combat ? Combat->FindTableSet(Context.Out.Core.TableRevision) : nullptr;
	const int32 Running = Context.Out.Core.ActiveTimeline;
	if (!Tables || !Tables->TimelineRegistry.IsValidIndex(Running) || !Tables->TimelineRegistry[Running])
	{
		return;
	}
	const UGSTN_GA_BaseCombat* Ability = Tables->TimelineRegistry[Running]->GetDefaultObject<UGSTN_GA_BaseCombat>();
	const FGSTN_CombatTimelineData& Timeline = Ability->GetTimelineData();
	const FGSTN_ParryWindowTrack* Track = Timeline.Tracks.IsValidIndex(TrackIndex) ? Timeline.Tracks[TrackIndex].GetPtr<FGSTN_ParryWindowTrack>() : nullptr;
	if (!Track)
	{
		return;
	}
	// The profile: the baked index when the track was baked, else its name against the entity's rules (unknown names fall back to profile 0, the shield's)
	int32 Profile = Track->ProfileIndex;
	if (Profile == INDEX_NONE)
	{
		Profile = Data.Rules->FindProfileIndex(Track->ProfileName);
	}
	OutProfile = static_cast<uint8>(FMath::Clamp(Profile, 0, GSTNDefenceLimits::MaxParryProfiles - 1));
	if (Track->bCloseOnParry)
	{
		OutProfile |= FGSTN_DefenceStepUtils::CloseOnParryBit;
	}
	// The hurtbox selector against the entity's set: bit 15 says a selector exists, bits 0..14 the hurtboxes it covers; nothing = any hurtbox
	if (Track->AffectedHurtboxes.IsEmpty() && Track->AffectedTags.IsEmpty())
	{
		return;
	}
	OutHurtboxMask = 0x8000;
	if (const UGSTN_HurtboxSetDefinition* Set = Combat->GetHurtboxSet())
	{
		for (int32 Index = 0; Index < Set->Hurtboxes.Num() && Index < 15; ++Index)
		{
			if (Track->Affects(Set->Hurtboxes[Index].Name, Set->Hurtboxes[Index].Tags))
			{
				OutHurtboxMask |= static_cast<uint16>(1u << Index);
			}
		}
	}
}

void FGSTN_DefenceTickExtension::ConsumeStamps(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data)
{
	const int32 Frame = Context.Frame;
	const UGSTN_DefenceRules& Rules = *Data.Rules;
	bool bHoldSeen = false;
	if (Context.Timeline)
	{
		for (const FGSTN_StampedWindow& Stamp : Context.Timeline->StampedWindows)
		{
			if (Stamp.Profile == GSTNDefenceStamps::HoldMarker)
			{
				bHoldSeen = true;
				FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
				const FGSTN_ShieldRates Rates = ReadRates(Context.AbilitySystem, Rules);
				if (Fragment.ShieldPressFrame != Stamp.StartFrame && Stamp.StartFrame != INDEX_NONE)
				{
					// The first stamp of a shield timeline: the raise, at the frame the timeline started (the press)
					Fragment.SetShieldView(UGSTN_DefenceMath::RaiseShield(Fragment.MakeShieldView(Rates), Stamp.StartFrame, Rates));
					Fragment.ShieldFlags &= ~GSTNShieldFlags::Empty;
				}
				if (Stamp.EndFrame != INDEX_NONE && Fragment.IsRaised())
				{
					Fragment.SetShieldView(UGSTN_DefenceMath::ReleaseShield(Fragment.MakeShieldView(Rates), Stamp.EndFrame, Rates));
				}
			}
			else
			{
				// A parry window of the running move: the later track wins, one window at a time
				FGSTN_DefenceFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DefenceFragment>(Context.Out.Fragments);
				uint8 Profile = 0;
				uint16 Mask = 0;
				ResolveParryTrack(Context, Data, Stamp.Profile, Profile, Mask);
				Fragment.MoveParryStartFrame = Stamp.StartFrame;
				Fragment.MoveParryEndFrame = Stamp.EndFrame;
				Fragment.MoveParryProfile = Profile;
				Fragment.MoveParryHurtboxMask = Mask;
			}
		}
	}
	if (!bHoldSeen)
	{
		// A shield still raised with no hold stamp this frame: its timeline ended or was cancelled (a stagger, another command): it is down from this frame
		if (FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments))
		{
			if (Fragment->IsRaised() && !Fragment->IsBreaking())
			{
				const FGSTN_ShieldRates Rates = ReadRates(Context.AbilitySystem, Rules);
				Fragment->SetShieldView(UGSTN_DefenceMath::ReleaseShield(Fragment->MakeShieldView(Rates), Frame, Rates));
			}
		}
	}
}

void FGSTN_DefenceTickExtension::ProjectTags(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData* Data) const
{
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	const FGSTN_DefenceFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Context.Out.Fragments);
	if (!ASC || (!Data && !Fragment))
	{
		return;
	}
	const int32 Frame = Context.Frame;
	const UGSTN_DefenceRules* Rules = Data ? Data->Rules : nullptr;
	const bool bAvailable = Rules && UGSTN_DefenceMath::IsShieldAvailable(Rules, Data->bKitHasShield);
	const FGSTN_DefenceFragment Defaults;
	const FGSTN_DefenceFragment& Frag = Fragment ? *Fragment : Defaults;

	const bool bShielding = Rules && ShieldBlocksAt(Frag, Frame, *Rules, bAvailable);
	const bool bShieldWindow = bAvailable && Rules && UGSTN_DefenceMath::EvaluateParryWindow(Frag.ShieldPressFrame, Frame, Frag.ShieldReleaseFrame, Rules->Parry);
	// Set, never added: nothing else writes these, and a re-simulated frame must not inherit what a predicted future left
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_Shielding, bShielding ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_ShieldStun, Frag.IsShieldStunned(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_ShieldBroken, Frag.IsBreaking() ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_ParryWindow, bShieldWindow || Frag.IsMoveParryWindowOpen(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_ParryRecovery, Frag.IsInParryRecovery(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_Parried, Frag.IsPunished(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_Teching, Frag.IsTeching(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_Knockdown, Frag.IsKnockedDown(Frame) ? 1 : 0);
	ASC->SetLooseGameplayTagCount(GSTNDefenceTags::State_Tech_Lockout, Frag.IsTechLockedOut(Frame) ? 1 : 0);
}
