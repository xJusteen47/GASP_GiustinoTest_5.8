// Backpacks, Network Prediction half (M11 L7b).

#include "GSTN_BackpackTickExtension.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GSTN_BackpackSim.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Sync/GSTN_SyncFragment.h"
#include "World/GSTN_StepSnapshot.h"

uint8 FGSTN_BackpackTickExtension::GetTypeId() const
{
	return GSTNFeatureIds::Backpack;
}

bool FGSTN_BackpackTickExtension::IsDomeTimelineRunning(const FGSTN_BackpackFragment& Fragment, const FGSTN_CombatSyncCore& Core)
{
	return Fragment.HasDome() && Core.ActiveTimeline != INDEX_NONE && static_cast<int32>(Core.ActiveTimeline) == static_cast<int32>(Fragment.DomeTimeline)
		&& Core.TimelineStartFrame == Fragment.DomeStartFrame;
}

bool FGSTN_BackpackTickExtension::HasLanded(const FGSTN_CombatSyncCore& In, const FGSTN_CombatSyncCore& Out)
{
	// Airborne phases are Rising, Falling and Launched; the grounded jump state is the default (phase None)
	return In.Jump.Phase != EGSTN_JumpPhase::None && Out.Jump.Phase == EGSTN_JumpPhase::None;
}

void FGSTN_BackpackTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (FGSTN_BackpackFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_BackpackFragment>(Context.Out.Fragments))
		{
			FGSTN_BackpackStep::AfterClocks(*Fragment, Context.Frame, IsDomeTimelineRunning(*Fragment, Context.Out.Core));
		}
		break;

	case EGSTN_CombatHookPoint::AfterExecute:
	{
		if (HasLanded(Context.In.Core, Context.Out.Core))
		{
			if (FGSTN_BackpackFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_BackpackFragment>(Context.Out.Fragments))
			{
				FGSTN_BackpackStep::Landed(*Fragment);
			}
		}
		const FGSTN_CombatTimelineData* Started = ResolveStartedTimeline(Context);
		const FGSTN_BackpackChargeTrack* Charge = Started ? FGSTN_BackpackChargeTrack::Find(*Started) : nullptr;
		const FGSTN_BackpackDomeTrack* Dome = Started ? FGSTN_BackpackDomeTrack::Find(*Started) : nullptr;
		if (!Charge && !Dome)
		{
			break;
		}
		// Added only now: an entity that never ran a backpack move has no fragment and costs nothing
		FGSTN_BackpackFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_BackpackFragment>(Context.Out.Fragments);
		if (Charge)
		{
			FGSTN_BackpackStep::Spend(Fragment, Context.Frame, *Charge);
		}
		if (Dome)
		{
			// Where the entity stood at the end of the previous frame, as every machine's snapshot has it; no snapshot (a test) = the origin
			const FGSTN_SnapshotEntry* Self = Context.Snapshot ? Context.Snapshot->Find(Context.EntityId) : nullptr;
			const FVector Center = Self ? Self->Transform.GetLocation() : FVector::ZeroVector;
			FGSTN_BackpackStep::RaiseDome(Fragment, Context.Frame, *Dome, Center, Context.Out.Core.ActiveTimeline, Started->TotalFrames);
		}
		break;
	}

	case EGSTN_CombatHookPoint::BeforeTagProjection:
		ProjectTags(Context);
		// An idle fragment is dropped, so it costs nothing on the wire (its defaults are what a missing one reads as)
		Context.Out.Fragments.RemoveAll([](const TInstancedStruct<FGSTN_SyncFragment>& Entry)
		{
			const FGSTN_BackpackFragment* Fragment = Entry.GetPtr<FGSTN_BackpackFragment>();
			return Fragment && Fragment->IsDefault();
		});
		break;

	default:
		break;
	}
}

const FGSTN_CombatTimelineData* FGSTN_BackpackTickExtension::ResolveStartedTimeline(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_CombatSyncCore& Core = Context.Out.Core;
	// Almost every frame stops here: only the frame a timeline starts on looks further
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame != Context.Frame || !Context.Owner)
	{
		return nullptr;
	}
	const UGSTN_CombatSimComponent* Combat = Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>();
	const UGSTN_CombatTableSet* Set = Combat ? Combat->FindTableSet(Core.TableRevision) : nullptr;
	if (!Set || !Set->TimelineRegistry.IsValidIndex(Core.ActiveTimeline) || !Set->TimelineRegistry[Core.ActiveTimeline])
	{
		return nullptr;
	}
	const UGSTN_GA_BaseCombat* Default = Set->TimelineRegistry[Core.ActiveTimeline]->GetDefaultObject<UGSTN_GA_BaseCombat>();
	return Default ? &Default->GetTimelineData() : nullptr;
}

void FGSTN_BackpackTickExtension::ProjectTags(FGSTN_CombatTickContext& Context) const
{
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	if (!ASC)
	{
		return;
	}
	const FGameplayTagContainer& Projectable = FGSTN_BackpackStep::GetProjectableTags();
	const FGSTN_BackpackFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_BackpackFragment>(Context.Out.Fragments);
	// No fragment (never used, or a rollback to before the first use): clear only what a predicted future may have left
	if (!Fragment && !ASC->HasAnyMatchingGameplayTags(Projectable))
	{
		return;
	}
	FGameplayTagContainer Held;
	FGSTN_BackpackStep::GatherStateTags(Fragment, Held);
	// Set, never added: nothing else writes these, and a re-simulated frame must not inherit what a predicted future left
	for (const FGameplayTag& Tag : Projectable)
	{
		ASC->SetLooseGameplayTagCount(Tag, Held.HasTagExact(Tag) ? 1 : 0);
	}
}
