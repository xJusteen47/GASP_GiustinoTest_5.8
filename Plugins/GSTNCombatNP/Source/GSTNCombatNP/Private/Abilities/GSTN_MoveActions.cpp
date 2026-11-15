// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_MoveActions.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GameplayEffect.h"
#include "Math/RandomStream.h"
#include "Simulation/GSTN_CombatActionTypes.h"

#define LOCTEXT_NAMESPACE "GSTNMoveActions"

bool FGSTN_MoveAction_SendEvent::Execute(const FGSTN_MoveActionContext& Context) const
{
	return Context.Ability && Context.Ability->SendMoveEvent(EventTag, Magnitude);
}

FText FGSTN_MoveAction_SendEvent::Validate() const
{
	return EventTag.IsValid() ? FText::GetEmpty() : LOCTEXT("SendEventNoTag", "Send Event has no event tag: it sends nothing.");
}

bool FGSTN_MoveAction_ApplyEffect::Execute(const FGSTN_MoveActionContext& Context) const
{
	return Context.Ability && Context.Ability->ApplyMoveEffect(Effect, Level);
}

FText FGSTN_MoveAction_ApplyEffect::Validate() const
{
	return Effect ? FText::GetEmpty() : LOCTEXT("ApplyEffectNone", "Apply Effect has no effect class: it applies nothing.");
}

bool FGSTN_MoveAction_TagWindow::Execute(const FGSTN_MoveActionContext& Context) const
{
	return Context.Ability && Context.Ability->OpenMoveTagWindow(Tag, Frames);
}

FText FGSTN_MoveAction_TagWindow::Validate() const
{
	if (!Tag.IsValid())
	{
		return LOCTEXT("TagWindowNoTag", "Open Tag Window has no tag: it opens nothing.");
	}
	return Frames >= 1 ? FText::GetEmpty() : LOCTEXT("TagWindowFrames", "Open Tag Window lasts less than one frame: it opens nothing.");
}

int32 FGSTN_MoveActionRules::RunActions(TConstArrayView<FInstancedStruct> Actions, const FGSTN_MoveActionContext& Context, TFunctionRef<bool()> IsStillRunning)
{
	int32 Ran = 0;
	for (const FInstancedStruct& Instance : Actions)
	{
		if (!IsStillRunning())
		{
			break;
		}
		const UScriptStruct* Type = Instance.GetScriptStruct();
		if (Type == nullptr || !Type->IsChildOf(FGSTN_MoveAction::StaticStruct()))
		{
			continue;
		}
		Instance.Get<FGSTN_MoveAction>().Execute(Context);
		++Ran;
	}
	return Ran;
}

int32 FGSTN_MoveActionRules::PickTagWindowSlot(TConstArrayView<FGameplayTag> Tags, TConstArrayView<int32> Remaining, const FGameplayTag& Tag)
{
	const int32 Num = FMath::Min(Tags.Num(), Remaining.Num());
	if (Num == 0)
	{
		return INDEX_NONE;
	}
	for (int32 Slot = 0; Slot < Num; ++Slot)
	{
		if (Tags[Slot] == Tag)
		{
			return Slot;
		}
	}
	for (int32 Slot = 0; Slot < Num; ++Slot)
	{
		if (!Tags[Slot].IsValid())
		{
			return Slot;
		}
	}
	int32 Earliest = 0;
	for (int32 Slot = 1; Slot < Num; ++Slot)
	{
		if (Remaining[Slot] < Remaining[Earliest])
		{
			Earliest = Slot;
		}
	}
	return Earliest;
}

bool FGSTN_MoveActionRules::IsTagWindowOver(int32 Clock, int32 StartClock, int32 Frames)
{
	// The combat clock: a window stands still in hitstop like every combat frame count
	return StartClock < 0 || FGSTN_CombatClock::Elapsed(Clock, StartClock) >= Frames;
}

uint32 FGSTN_MoveActionRules::MakeSeed(int32 SimulationFrame, int32 MoveStartFrame, int32 Salt)
{
	// Integers only, the same hash on every platform: no address, no time, no global state
	return HashCombineFast(HashCombineFast(::GetTypeHash(SimulationFrame), ::GetTypeHash(MoveStartFrame)), ::GetTypeHash(Salt));
}

float FGSTN_MoveActionRules::RandomFloatInRange(uint32 Seed, float Min, float Max)
{
	const FRandomStream Stream(static_cast<int32>(Seed));
	return Stream.FRandRange(Min, Max);
}

int32 FGSTN_MoveActionRules::RandomIntInRange(uint32 Seed, int32 Min, int32 Max)
{
	const FRandomStream Stream(static_cast<int32>(Seed));
	return Stream.RandRange(Min, Max);
}

#undef LOCTEXT_NAMESPACE
