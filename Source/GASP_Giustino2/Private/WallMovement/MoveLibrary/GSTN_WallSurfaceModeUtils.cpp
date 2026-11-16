// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"

#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceUtils.h"
#include "MovementMode.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSurfaceModeUtils)

const FName UGSTN_WallSurfaceModeUtils::ClimbModeName(TEXT("WallClimbingV2"));
const FName UGSTN_WallSurfaceModeUtils::SlideModeName(TEXT("WallSlidingV2"));

FGSTN_WallSurfaceModeList UGSTN_WallSurfaceModeUtils::MakeDefaultList()
{
	FGSTN_WallSurfaceModeList List;

	// A tag that is not registered yet (a class default object built before the tag tree exists) leaves its entry out instead of adding a query that
	// matches nothing; the list then has fewer entries, never wrong ones
	if (GSTNSurfaceTags::Behaviour_WallClimb.GetTag().IsValid())
	{
		FGSTN_WallSurfaceModeEntry& Climb = List.Entries.AddDefaulted_GetRef();
		Climb.Behaviours = FGameplayTagQuery::MakeQuery_MatchTag(GSTNSurfaceTags::Behaviour_WallClimb);
		Climb.ModeName = ClimbModeName;
	}
	if (GSTNSurfaceTags::Behaviour_WallSlide.GetTag().IsValid())
	{
		FGSTN_WallSurfaceModeEntry& Slide = List.Entries.AddDefaulted_GetRef();
		Slide.Behaviours = FGameplayTagQuery::MakeQuery_MatchTag(GSTNSurfaceTags::Behaviour_WallSlide);
		Slide.ModeName = SlideModeName;
	}
	List.UnknownSurfaceModeName = SlideModeName;
	List.BlockedModeName = DefaultModeNames::Falling;
	List.HysteresisTicks = 6;
	return List;
}

FName UGSTN_WallSurfaceModeUtils::PickMode(const FGSTN_WallSurfaceModeList& List, const FGameplayTagContainer& Behaviours, bool bUnknownSurface,
	TFunctionRef<bool(FName)> IsRegistered, TFunctionRef<bool(const FGameplayTagQuery&)> IsBlocked, bool& bOutBlocked)
{
	bOutBlocked = false;

	if (bUnknownSurface)
	{
		// Nothing marked it: the settings' default offers every behaviour, so the entries would pick the first one for any unauthored wall
		return (!List.UnknownSurfaceModeName.IsNone() && IsRegistered(List.UnknownSurfaceModeName)) ? List.UnknownSurfaceModeName : NAME_None;
	}

	bool bAnyBlocked = false;
	for (const FGSTN_WallSurfaceModeEntry& Entry : List.Entries)
	{
		if (Entry.ModeName.IsNone() || !(Entry.Behaviours.IsEmpty() || Entry.Behaviours.Matches(Behaviours)) || !IsRegistered(Entry.ModeName))
		{
			continue;
		}
		if (IsBlocked(Entry.BlockedWhen))
		{
			bAnyBlocked = true;
			continue;
		}
		return Entry.ModeName;
	}
	bOutBlocked = bAnyBlocked;
	return NAME_None;
}

FName UGSTN_WallSurfaceModeUtils::ChooseModeForHit(const FGSTN_WallSurfaceModeList& List, const FHitResult& Hit, const UMoverComponent* MoverComp, const FMoverSyncState* SyncState)
{
	// Static configuration of the surface (components, settings, providers answering from replicated state), the same in a re-simulated frame
	const FGSTN_SurfaceView Surface = UGSTN_SurfaceUtils::Resolve(Hit);

	bool bBlocked = false;
	return PickMode(List, Surface.Behaviours, Surface.Source == EGSTN_SurfaceSource::Default,
		[MoverComp](FName ModeName) { return IsModeRegistered(MoverComp, ModeName); },
		[MoverComp, SyncState](const FGameplayTagQuery& Query) { return SyncState && IsBlockedByStateTags(MoverComp, *SyncState, Query); },
		bBlocked);
}

uint8 UGSTN_WallSurfaceModeUtils::StepVote(uint8 VoteTicks, bool bWantsOther, int32 HysteresisTicks, bool& bOutSwitch)
{
	bOutSwitch = false;
	if (!bWantsOther)
	{
		return 0;
	}
	const uint8 Next = static_cast<uint8>(FMath::Min<int32>(static_cast<int32>(VoteTicks) + 1, 255));
	bOutSwitch = Next >= FMath::Max(HysteresisTicks, 1);
	return Next;
}

bool UGSTN_WallSurfaceModeUtils::IsModeRegistered(const UMoverComponent* MoverComp, FName ModeName)
{
	if (ModeName.IsNone())
	{
		return false;
	}
	return !MoverComp || MoverComp->FindMode_Mutable(UBaseMovementMode::StaticClass(), ModeName) != nullptr;
}

bool UGSTN_WallSurfaceModeUtils::IsBlockedByStateTags(const UMoverComponent* MoverComp, const FMoverSyncState& SyncState, const FGameplayTagQuery& Query)
{
	if (!MoverComp || Query.IsEmpty())
	{
		return false;
	}

	// The query only looks at the tags it names: ask the state for those and match the answer, parents included (HasGameplayTagInState is hierarchical)
	FGameplayTagContainer Present;
	for (const FGameplayTag& Tag : Query.GetGameplayTagArray())
	{
		if (MoverComp->HasGameplayTagInState(SyncState, Tag, false))
		{
			Present.AddTag(Tag);
		}
	}
	return Query.Matches(Present);
}
