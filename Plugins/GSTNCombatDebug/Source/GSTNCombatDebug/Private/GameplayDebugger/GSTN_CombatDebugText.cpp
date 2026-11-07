#include "GameplayDebugger/GSTN_CombatDebugText.h"

#include "Events/GSTN_SimEvent.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimTypes.h"

namespace
{
	FString LeafName(const FGameplayTag& Tag)
	{
		const FString Full = Tag.ToString();
		int32 Dot = INDEX_NONE;
		return Full.FindLastChar(TEXT('.'), Dot) ? Full.RightChop(Dot + 1) : Full;
	}

	/** Frames left before End, "-" when the window is closed at Frame */
	FString Remaining(int32 End, int32 Frame)
	{
		return End != INDEX_NONE && Frame < End ? FString::FromInt(End - Frame) : FString(TEXT("-"));
	}
}

FGSTN_CombatHitRecord FGSTN_CombatHitRecord::FromEvent(const FGSTN_SimEvent& Event, bool bInDealt)
{
	FGSTN_CombatHitRecord Record;
	Record.Frame = Event.Frame;
	Record.bDealt = bInDealt;
	const AActor* OtherActor = bInDealt ? Event.Target.Get() : Event.Instigator.Get();
	Record.Other = OtherActor ? OtherActor->GetName() : FString(TEXT("?"));
	Record.Damage = Event.Magnitude;
	TArray<FString> Names;
	for (const FGameplayTag& Tag : Event.Tags)
	{
		Names.Add(LeafName(Tag));
	}
	Record.Tags = FString::Join(Names, TEXT(" "));
	return Record;
}

FString FGSTN_CombatHitRecord::ToLine() const
{
	return FString::Printf(TEXT("%d %s %s  dmg %.1f  %s"), Frame, bDealt ? TEXT("dealt to") : TEXT("taken from"), *Other, Damage, *Tags);
}

void FGSTN_CombatHitRing::Add(FGSTN_CombatHitRecord&& Record)
{
	if (Records.Num() < Capacity)
	{
		Records.Add(MoveTemp(Record));
	}
	else
	{
		Records[Head] = MoveTemp(Record);
	}
	Head = (Head + 1) % Capacity;
}

FString GSTNCombatDebugText::ActionStateName(uint8 ActionState)
{
	switch (static_cast<EGSTN_ActionState>(ActionState))
	{
	case EGSTN_ActionState::Actionable: return TEXT("Actionable");
	case EGSTN_ActionState::InTimeline: return TEXT("InTimeline");
	case EGSTN_ActionState::Hitstun: return TEXT("Hitstun");
	case EGSTN_ActionState::Tumble: return TEXT("Tumble");
	case EGSTN_ActionState::Splat: return TEXT("Splat");
	case EGSTN_ActionState::KO: return TEXT("KO");
	default: return FString::Printf(TEXT("State %d"), ActionState);
	}
}

void GSTNCombatDebugText::DescribeCore(const FGSTN_CombatSyncCore& Core, TArray<FString>& OutLines)
{
	const int32 Frame = Core.Input.History.GetLatestFrame();
	OutLines.Add(FString::Printf(TEXT("frame %d  combat clock %d  {yellow}%s"), Frame, Frame - Core.PausedFramesTotal,
		*ActionStateName(static_cast<uint8>(Core.ActionState))));
	if (Core.ActiveTimeline != INDEX_NONE && Core.TimelineStartFrame != INDEX_NONE)
	{
		OutLines.Add(FString::Printf(TEXT("timeline %d  frame %d (paused %d)  source %d"), Core.ActiveTimeline,
			Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, Core.TimelinePausedFrames, static_cast<int32>(Core.ActiveSource)));
	}
	else
	{
		OutLines.Add(TEXT("timeline -"));
	}
	OutLines.Add(FString::Printf(TEXT("hitstop %s  hitstun %s  weapon %d  loadout rev %d  table rev %d"), *Remaining(Core.HitstopEndFrame, Frame),
		*Remaining(Core.Hit.HitstunEndFrame, Frame), Core.ActiveWeaponIndex, Core.LoadoutRevision, Core.TableRevision));
}
