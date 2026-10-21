// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"

namespace
{
	/** A stick event copied out of the history, oldest first, for the conditions that read sequences */
	struct FStickEvent
	{
		int32 Frame = 0;
		FGSTN_StickSample Stick;
	};
	using FStickEvents = TArray<FStickEvent, TInlineAllocator<FGSTN_InputHistory::MaxEvents>>;

	void CollectStickEvents(const FGSTN_InputHistory& History, FStickEvents& Out)
	{
		for (int32 Age = History.GetNumDirectionEvents() - 1; Age >= 0; --Age)
		{
			const FGSTN_DirectionEvent& Event = History.GetDirectionEvent(Age);
			Out.Add({ Event.Frame, Event.Stick });
		}
	}

	/** Sector 0..7 of a non-neutral direction: Up = 0, clockwise */
	int32 SectorOf(EGSTN_StickDirection Direction)
	{
		return static_cast<int32>(Direction) - 1;
	}

	float AngleOf(EGSTN_StickDirection Direction)
	{
		return SectorOf(Direction) * 45.f;
	}

	/** First non-neutral direction of a set in enum order, Neutral if the set has none */
	EGSTN_StickDirection PickDirection(FGSTN_DirectionMask Mask)
	{
		for (int32 Direction = 1; Direction < GSTN_NumStickDirections; ++Direction)
		{
			if (GSTN_DirectionMasks::Contains(Mask, static_cast<EGSTN_StickDirection>(Direction)))
			{
				return static_cast<EGSTN_StickDirection>(Direction);
			}
		}
		return EGSTN_StickDirection::Neutral;
	}

	/** First button of a container the map knows, as a mask (0 if none) */
	uint32 FirstKnownButton(const FGSTN_ButtonMap& Map, const FGameplayTagContainer& Buttons)
	{
		for (const FGameplayTag& Button : Buttons)
		{
			if (const uint32 Mask = Map.GetMask(Button))
			{
				return Mask;
			}
		}
		return 0;
	}

	void SetResult(FGSTN_ConditionResult& Out, int32 Frame, int32 Slot, int32 TerminalPressSlot)
	{
		Out = FGSTN_ConditionResult();
		Out.Frame = Frame;
		if (Slot != INDEX_NONE)
		{
			Out.UsedEvents.Add(static_cast<uint8>(Slot));
		}
		Out.TerminalPressSlot = TerminalPressSlot;
	}

	/** A press that can still be used: a press, unconsumed, not excluded */
	bool IsUsablePress(const FGSTN_ConditionContext& Ctx, int32 Slot, const FGSTN_ButtonEvent& Event)
	{
		return !Event.bRelease && !Event.bConsumed && !Ctx.IsExcluded(Slot);
	}

	/** Stick in a direction set and a zone range */
	bool IsStickInSet(const FGSTN_StickSample& Stick, EGSTN_DirectionSpace Space, FGSTN_DirectionMask Mask, EGSTN_StickZone MinZone, EGSTN_StickZone MaxZone)
	{
		return GSTN_DirectionMasks::Contains(Mask, Stick.GetDirection(Space)) && Stick.Zone >= MinZone && Stick.Zone <= MaxZone;
	}

	/** Neutral counts for any zone; other directions need the minimum zone */
	bool IsMotionElement(const FGSTN_StickSample& Stick, EGSTN_DirectionSpace Space, FGSTN_DirectionMask Mask, EGSTN_StickZone MinZone)
	{
		const EGSTN_StickDirection Direction = Stick.GetDirection(Space);
		return GSTN_DirectionMasks::Contains(Mask, Direction) && (Direction == EGSTN_StickDirection::Neutral || Stick.Zone >= MinZone);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Press
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Press::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const uint32 Mask = Ctx.Buttons.GetMask(Buttons);
	if (Mask == 0)
	{
		return false;
	}

	const FGSTN_InputHistory& History = Ctx.History;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 Slot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(Slot);
		if (Event.Frame > AnchorFrame)
		{
			continue;
		}
		if (Event.Frame < Ctx.EarliestFrame)
		{
			break;
		}
		if (IsUsablePress(Ctx, Slot, Event) && (Mask & (1u << Event.Button)) != 0)
		{
			SetResult(Out, Event.Frame, Slot, Slot);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Press::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	Script.AddTap(Frame, FirstKnownButton(Map, Buttons));
}

// ---------------------------------------------------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Release::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const int32 ButtonIndex = Ctx.Buttons.GetIndex(Button);
	if (ButtonIndex == INDEX_NONE)
	{
		return false;
	}

	const FGSTN_InputHistory& History = Ctx.History;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 Slot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(Slot);
		if (Event.Frame > AnchorFrame)
		{
			continue;
		}
		if (Event.Frame < Ctx.EarliestFrame)
		{
			break;
		}
		if (!Event.bRelease || Event.bConsumed || Ctx.IsExcluded(Slot) || Event.Button != ButtonIndex)
		{
			continue;
		}

		// The press this release ends: the next older press of the button. If it is not in the history any more the
		// button has been held for longer than the history reaches.
		int32 HeldFrames = MAX_int32;
		for (int32 OlderAge = Age + 1; OlderAge < History.GetNumButtonEvents(); ++OlderAge)
		{
			const FGSTN_ButtonEvent& Older = History.GetButtonEvent(History.GetButtonSlot(OlderAge));
			if (Older.Button == ButtonIndex)
			{
				if (!Older.bRelease)
				{
					HeldFrames = Event.Frame - Older.Frame;
				}
				break;
			}
		}
		if (HeldFrames >= MinHeldFrames)
		{
			SetResult(Out, Event.Frame, Slot, INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Release::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	const uint32 Mask = Map.GetMask(Button);
	Script.AddPress(Frame, Mask);
	Frame += MinHeldFrames;
	Script.AddRelease(Frame, Mask);
}

// ---------------------------------------------------------------------------------------------------------------------
// Hold
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Hold::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const int32 ButtonIndex = Ctx.Buttons.GetIndex(Button);
	if (ButtonIndex == INDEX_NONE)
	{
		return false;
	}

	// Held at some frame of the window around the anchor, for long enough at that frame. Frames after Now do not exist yet.
	const int32 Low = AnchorFrame - Ctx.SimultaneityFrames;
	const int32 High = FMath::Min(AnchorFrame + Ctx.SimultaneityFrames, Ctx.Now);
	for (int32 Frame = High; Frame >= Low; --Frame)
	{
		int32 HeldFrames = 0;
		if (Ctx.History.IsButtonHeldAt(ButtonIndex, Frame, HeldFrames) && HeldFrames >= MinFrames)
		{
			SetResult(Out, Frame, INDEX_NONE, INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Hold::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	Script.AddPress(Frame, Map.GetMask(Button));
	Frame += MinFrames;
}

// ---------------------------------------------------------------------------------------------------------------------
// Chord
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Chord::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const uint32 Mask = Ctx.Buttons.GetMask(Buttons);
	if (Mask == 0 || FMath::CountBits(Mask) != Buttons.Num())
	{
		return false;
	}

	const FGSTN_InputHistory& History = Ctx.History;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 NewestSlot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent& Newest = History.GetButtonEvent(NewestSlot);
		if (Newest.Frame > AnchorFrame)
		{
			continue;
		}
		if (Newest.Frame < Ctx.EarliestFrame)
		{
			break;
		}
		if (!IsUsablePress(Ctx, NewestSlot, Newest) || (Mask & (1u << Newest.Button)) == 0)
		{
			continue;
		}

		// The other buttons: their newest usable press no later than this one and within the chord window.
		uint32 UsedSlots = 1u << NewestSlot;
		uint32 Missing = Mask & ~(1u << Newest.Button);
		for (int32 OtherAge = 0; OtherAge < History.GetNumButtonEvents() && Missing != 0; ++OtherAge)
		{
			const int32 OtherSlot = History.GetButtonSlot(OtherAge);
			const FGSTN_ButtonEvent& Other = History.GetButtonEvent(OtherSlot);
			if (Other.Frame > Newest.Frame)
			{
				continue;
			}
			if (Other.Frame < Newest.Frame - WithinFrames)
			{
				break;
			}
			if (IsUsablePress(Ctx, OtherSlot, Other) && (Missing & (1u << Other.Button)) != 0)
			{
				Missing &= ~(1u << Other.Button);
				UsedSlots |= 1u << OtherSlot;
			}
		}

		if (Missing == 0)
		{
			Out = FGSTN_ConditionResult();
			Out.Frame = Newest.Frame;
			Out.TerminalPressSlot = NewestSlot;
			for (uint32 Remaining = UsedSlots; Remaining != 0; Remaining &= Remaining - 1)
			{
				Out.UsedEvents.Add(static_cast<uint8>(FMath::CountTrailingZeros(Remaining)));
			}
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Chord::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	Script.AddTap(Frame, Map.GetMask(Buttons));
}

// ---------------------------------------------------------------------------------------------------------------------
// Mash
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Mash::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const int32 ButtonIndex = Ctx.Buttons.GetIndex(Button);
	if (ButtonIndex == INDEX_NONE || Count < 1)
	{
		return false;
	}

	const FGSTN_InputHistory& History = Ctx.History;
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 NewestSlot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent& Newest = History.GetButtonEvent(NewestSlot);
		if (Newest.Frame > AnchorFrame)
		{
			continue;
		}
		if (Newest.Frame < Ctx.EarliestFrame)
		{
			break;
		}
		if (!IsUsablePress(Ctx, NewestSlot, Newest) || Newest.Button != ButtonIndex)
		{
			continue;
		}

		FGSTN_ConditionResult Candidate;
		Candidate.Frame = Newest.Frame;
		Candidate.TerminalPressSlot = NewestSlot;
		for (int32 OtherAge = Age; OtherAge < History.GetNumButtonEvents() && Candidate.UsedEvents.Num() < Count; ++OtherAge)
		{
			const int32 OtherSlot = History.GetButtonSlot(OtherAge);
			const FGSTN_ButtonEvent& Other = History.GetButtonEvent(OtherSlot);
			if (Other.Frame < Newest.Frame - WithinFrames)
			{
				break;
			}
			if (IsUsablePress(Ctx, OtherSlot, Other) && Other.Button == ButtonIndex)
			{
				Candidate.UsedEvents.Add(static_cast<uint8>(OtherSlot));
			}
		}
		if (Candidate.UsedEvents.Num() >= Count)
		{
			Out = MoveTemp(Candidate);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Mash::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	const uint32 Mask = Map.GetMask(Button);
	for (int32 Press = 0; Press < Count; ++Press)
	{
		Script.AddTap(Frame, Mask);
		if (Press < Count - 1)
		{
			++Frame;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Direction
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Direction::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const FGSTN_DirectionMask Mask = static_cast<FGSTN_DirectionMask>(Directions);
	const int32 Low = AnchorFrame - Ctx.SimultaneityFrames;
	const int32 High = FMath::Min(AnchorFrame + Ctx.SimultaneityFrames, Ctx.Now);
	for (int32 Frame = High; Frame >= Low; --Frame)
	{
		const FGSTN_StickSample Stick = Ctx.History.GetStickAt(Frame);
		if (IsStickInSet(Stick, Space, Mask, MinZone, MaxZone) && (!bRequireFlick || Stick.bFlick))
		{
			SetResult(Out, Frame, INDEX_NONE, INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Direction::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	// The script's camera and character both face 0 by default, so every space reads the same stick.
	const FGSTN_DirectionMask Mask = static_cast<FGSTN_DirectionMask>(Directions);
	const bool bNeutralAccepted = GSTN_DirectionMasks::Contains(Mask, EGSTN_StickDirection::Neutral) && MinZone == EGSTN_StickZone::Neutral;
	const EGSTN_StickDirection Direction = PickDirection(Mask);

	if ((bNeutralAccepted && !bRequireFlick) || Direction == EGSTN_StickDirection::Neutral)
	{
		Script.SetStickNeutral(Frame);
		return;
	}

	// Lowest zone that is a real push and inside the range; a flick needs the full push.
	const EGSTN_StickZone Zone = bRequireFlick ? EGSTN_StickZone::Tilt : FMath::Max(MinZone, EGSTN_StickZone::Aim);
	if (bRequireFlick)
	{
		Script.SetStickNeutral(Frame);
		++Frame;
	}
	Script.SetStick(Frame, AngleOf(Direction), Zone);
}

// ---------------------------------------------------------------------------------------------------------------------
// Motion
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	/**
	 * Places Element and the ones before it, all entered before the event at NextIndex. An element is entered at the first
	 * event of a run of events that satisfy it; the gaps are measured between entries.
	 */
	bool PlaceMotionElement(const FStickEvents& Events, const FGSTN_InputCondition_Motion& Motion, int32 Element, int32 NextIndex, int32 LastFrame)
	{
		const auto Satisfies = [&Events, &Motion](int32 Index, int32 Which)
		{
			return IsMotionElement(Events[Index].Stick, Motion.Space, Motion.Sequence[Which].GetMask(), Motion.MinZone);
		};

		for (int32 Index = NextIndex - 1; Index >= 0; --Index)
		{
			if (Events[NextIndex].Frame - Events[Index].Frame > Motion.MaxFramesBetween || LastFrame - Events[Index].Frame > Motion.MaxTotalFrames)
			{
				break;
			}
			const bool bEntry = Satisfies(Index, Element) && (Index == 0 || !Satisfies(Index - 1, Element));
			if (bEntry && (Element == 0 || PlaceMotionElement(Events, Motion, Element - 1, Index, LastFrame)))
			{
				return true;
			}
		}
		return false;
	}
}

bool FGSTN_InputCondition_Motion::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	if (Sequence.IsEmpty())
	{
		return false;
	}

	FStickEvents Events;
	CollectStickEvents(Ctx.History, Events);
	const int32 Last = Sequence.Num() - 1;

	const auto Satisfies = [this, &Events](int32 Index, int32 Element)
	{
		return IsMotionElement(Events[Index].Stick, Space, Sequence[Element].GetMask(), MinZone);
	};

	for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
	{
		if (Events[Index].Frame > AnchorFrame)
		{
			continue;
		}
		if (Events[Index].Frame < Ctx.EarliestFrame)
		{
			break;
		}
		const bool bEntry = Satisfies(Index, Last) && (Index == 0 || !Satisfies(Index - 1, Last));
		if (bEntry && (Last == 0 || PlaceMotionElement(Events, *this, Last - 1, Index, Events[Index].Frame)))
		{
			SetResult(Out, Events[Index].Frame, INDEX_NONE, INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Motion::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	for (int32 Element = 0; Element < Sequence.Num(); ++Element)
	{
		const EGSTN_StickDirection Direction = PickDirection(Sequence[Element].GetMask());
		if (Direction == EGSTN_StickDirection::Neutral)
		{
			Script.SetStickNeutral(Frame);
		}
		else
		{
			Script.SetStick(Frame, AngleOf(Direction), EGSTN_StickZone::Tilt);
		}
		if (Element < Sequence.Num() - 1)
		{
			++Frame;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Spin
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
	/** A stretch of the stick in one sector (or out of any), merged from consecutive events that do not change it */
	struct FSpinNode
	{
		int32 Frame = 0;

		/** 0..7, -1 when the stick is not pushed far enough to count */
		int32 Sector = -1;
	};
}

bool FGSTN_InputCondition_Spin::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	FStickEvents Events;
	CollectStickEvents(Ctx.History, Events);

	TArray<FSpinNode, TInlineAllocator<FGSTN_InputHistory::MaxEvents>> Nodes;
	for (const FStickEvent& Event : Events)
	{
		const EGSTN_StickDirection Direction = Event.Stick.GetDirection(Space);
		const bool bCounts = Direction != EGSTN_StickDirection::Neutral && Event.Stick.Zone >= MinZone;
		const int32 Sector = bCounts ? SectorOf(Direction) : -1;
		if (Nodes.IsEmpty() || Nodes.Last().Sector != Sector)
		{
			Nodes.Add({ Event.Frame, Sector });
		}
	}

	const auto TrySense = [this, &Nodes](int32 End, bool bClockwise)
	{
		float Degrees = 0.f;
		for (int32 Current = End; Current > 0; --Current)
		{
			const FSpinNode& Previous = Nodes[Current - 1];
			if (Previous.Sector < 0 || Nodes[End].Frame - Previous.Frame > MaxFrames)
			{
				return false;
			}
			const int32 Step = bClockwise ? (Nodes[Current].Sector - Previous.Sector + 8) % 8 : (Previous.Sector - Nodes[Current].Sector + 8) % 8;
			if (Step < 1 || Step > 1 + MaxSkippedSectors)
			{
				return false;
			}
			Degrees += 45.f * Step;
			if (Degrees >= MinDegrees)
			{
				return true;
			}
		}
		return false;
	};

	for (int32 End = Nodes.Num() - 1; End >= 0; --End)
	{
		if (Nodes[End].Frame > AnchorFrame)
		{
			continue;
		}
		if (Nodes[End].Frame < Ctx.EarliestFrame)
		{
			break;
		}
		if (Nodes[End].Sector < 0)
		{
			continue;
		}
		if ((Rotation != EGSTN_SpinRotation::CounterClockwise && TrySense(End, true))
			|| (Rotation != EGSTN_SpinRotation::Clockwise && TrySense(End, false)))
		{
			SetResult(Out, Nodes[End].Frame, INDEX_NONE, INDEX_NONE);
			return true;
		}
	}
	return false;
}

void FGSTN_InputCondition_Spin::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	const float Sign = Rotation == EGSTN_SpinRotation::CounterClockwise ? -1.f : 1.f;
	const int32 Steps = FMath::CeilToInt(MinDegrees / 45.f);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		Script.SetStick(Frame, Sign * 45.f * Step, EGSTN_StickZone::Tilt);
		if (Step < Steps)
		{
			++Frame;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Charge
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_Charge::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const FGSTN_DirectionMask Mask = static_cast<FGSTN_DirectionMask>(Directions);
	FStickEvents Events;
	CollectStickEvents(Ctx.History, Events);

	// Runs of consecutive stick events inside the set, newest first. A run lasts from its oldest event to the start of the
	// first event after it (or the latest frame, inclusive, for the run still going on).
	int32 RunEnd = INDEX_NONE;
	int32 RunStart = INDEX_NONE;
	const auto RunMatches = [&]()
	{
		return RunEnd >= AnchorFrame - MaxReleaseFrames && RunEnd - RunStart >= MinFrames;
	};

	for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
	{
		const bool bInSet = IsStickInSet(Events[Index].Stick, Space, Mask, MinZone, EGSTN_StickZone::Tilt);
		if (bInSet)
		{
			if (RunEnd == INDEX_NONE)
			{
				RunEnd = Index == Events.Num() - 1 ? Ctx.History.GetLatestFrame() + 1 : Events[Index + 1].Frame;
			}
			RunStart = Events[Index].Frame;
			continue;
		}
		if (RunEnd != INDEX_NONE)
		{
			if (RunMatches())
			{
				SetResult(Out, RunEnd, INDEX_NONE, INDEX_NONE);
				return true;
			}
			RunEnd = INDEX_NONE;
		}
	}
	if (RunEnd != INDEX_NONE && RunMatches())
	{
		SetResult(Out, RunEnd, INDEX_NONE, INDEX_NONE);
		return true;
	}
	return false;
}

void FGSTN_InputCondition_Charge::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	const EGSTN_StickDirection Direction = PickDirection(static_cast<FGSTN_DirectionMask>(Directions));
	if (Direction != EGSTN_StickDirection::Neutral)
	{
		Script.SetStick(Frame, AngleOf(Direction), EGSTN_StickZone::Tilt);
	}
	Frame += MinFrames;
}

// ---------------------------------------------------------------------------------------------------------------------
// AnyOf
// ---------------------------------------------------------------------------------------------------------------------

bool FGSTN_InputCondition_AnyOf::Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
{
	const bool bAnchor = IsAnchorCondition();
	bool bFound = false;
	FGSTN_ConditionResult Best;
	for (const TInstancedStruct<FGSTN_InputCondition>& Child : Conditions)
	{
		const FGSTN_InputCondition* Condition = Child.GetPtr();
		FGSTN_ConditionResult Candidate;
		if (Condition && Condition->Matches(Ctx, AnchorFrame, Candidate) && (!bFound || Candidate.Frame > Best.Frame))
		{
			Best = MoveTemp(Candidate);
			bFound = true;
			if (!bAnchor)
			{
				break;
			}
		}
	}
	if (bFound)
	{
		Out = MoveTemp(Best);
	}
	return bFound;
}

bool FGSTN_InputCondition_AnyOf::IsAnchorCondition() const
{
	const FGSTN_InputCondition* First = Conditions.IsEmpty() ? nullptr : Conditions[0].GetPtr();
	return First && First->IsAnchorCondition();
}

bool FGSTN_InputCondition_AnyOf::IsDirectionCondition() const
{
	const FGSTN_InputCondition* First = Conditions.IsEmpty() ? nullptr : Conditions[0].GetPtr();
	return First && First->IsDirectionCondition();
}

int32 FGSTN_InputCondition_AnyOf::GetSpecificity() const
{
	int32 Result = 0;
	for (const TInstancedStruct<FGSTN_InputCondition>& Child : Conditions)
	{
		if (const FGSTN_InputCondition* Condition = Child.GetPtr())
		{
			Result = FMath::Max(Result, Condition->GetSpecificity());
		}
	}
	return Result;
}

void FGSTN_InputCondition_AnyOf::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const
{
	if (!Conditions.IsEmpty())
	{
		if (const FGSTN_InputCondition* First = Conditions[0].GetPtr())
		{
			First->AppendCanonicalInput(Script, Map, Frame);
		}
	}
}

void FGSTN_InputCondition_AnyOf::GetReferencedButtons(FGameplayTagContainer& OutButtons) const
{
	for (const TInstancedStruct<FGSTN_InputCondition>& Child : Conditions)
	{
		if (const FGSTN_InputCondition* Condition = Child.GetPtr())
		{
			Condition->GetReferencedButtons(OutButtons);
		}
	}
}
