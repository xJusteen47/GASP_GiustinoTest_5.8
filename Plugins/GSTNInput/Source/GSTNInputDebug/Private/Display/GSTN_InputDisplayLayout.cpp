// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Display/GSTN_InputDisplayLayout.h"

TCHAR FGSTN_InputDisplayLayout::GetNumpadKey(EGSTN_StickDirection Direction)
{
	switch (Direction)
	{
	case EGSTN_StickDirection::Up:        return TEXT('8');
	case EGSTN_StickDirection::UpRight:   return TEXT('9');
	case EGSTN_StickDirection::Right:     return TEXT('6');
	case EGSTN_StickDirection::DownRight: return TEXT('3');
	case EGSTN_StickDirection::Down:      return TEXT('2');
	case EGSTN_StickDirection::DownLeft:  return TEXT('1');
	case EGSTN_StickDirection::Left:      return TEXT('4');
	case EGSTN_StickDirection::UpLeft:    return TEXT('7');
	default:                              return TEXT('5');
	}
}

FString FGSTN_InputDisplayLayout::DescribeStick(const FGSTN_InputDisplayStickCell& Cell)
{
	FString Item = FString::Chr(Cell.StickKey);
	if (Cell.Zone != EGSTN_StickZone::Neutral)
	{
		Item += Cell.Zone == EGSTN_StickZone::Aim ? TEXT("a") : TEXT("t");
	}
	if (Cell.bFlick)
	{
		Item += TEXT("!");
	}
	return Item;
}

FString FGSTN_InputDisplayLayout::GetDirectionName(EGSTN_StickDirection Direction)
{
	switch (Direction)
	{
	case EGSTN_StickDirection::Up:        return TEXT("up");
	case EGSTN_StickDirection::UpRight:   return TEXT("up right");
	case EGSTN_StickDirection::Right:     return TEXT("right");
	case EGSTN_StickDirection::DownRight: return TEXT("down right");
	case EGSTN_StickDirection::Down:      return TEXT("down");
	case EGSTN_StickDirection::DownLeft:  return TEXT("down left");
	case EGSTN_StickDirection::Left:      return TEXT("left");
	case EGSTN_StickDirection::UpLeft:    return TEXT("up left");
	default:                              return TEXT("neutral");
	}
}

FString FGSTN_InputDisplayLayout::DescribeStickForTooltip(const FGSTN_InputDisplayStickCell& Cell)
{
	const TCHAR* Zone = Cell.Zone == EGSTN_StickZone::Tilt ? TEXT(", tilt") : (Cell.Zone == EGSTN_StickZone::Aim ? TEXT(", aim") : TEXT(""));
	return FString::Printf(TEXT("%s (%s%s%s)"), *DescribeStick(Cell), *GetDirectionName(Cell.StickDirection), Zone, Cell.bFlick ? TEXT(", flick") : TEXT(""));
}

FString FGSTN_InputDisplayLayout::ToNotation() const
{
	if (Events.IsEmpty())
	{
		return FString();
	}
	const int32 Origin = Events[0].Frame;
	FString Result;
	for (const FGSTN_InputDisplayEvent& Event : Events)
	{
		Result += Result.IsEmpty() ? TEXT("") : TEXT(" ");
		Result += FString::Printf(TEXT("%d:%s"), Event.Frame - Origin, *Event.Item);
	}
	return Result;
}

FGSTN_InputDisplayLayout FGSTN_InputDisplayLayout::Build(const FGSTN_InputDisplayData& Data, const FGSTN_InputDisplayLayoutParams& Params)
{
	FGSTN_InputDisplayLayout Layout;
	const FGSTN_InputHistory& History = Data.History;

	const int32 Last = Params.NowFrame != INDEX_NONE ? Params.NowFrame : History.GetLatestFrame();
	if (Last < 0)
	{
		return Layout;
	}
	const int32 MaxColumns = FMath::Clamp(Params.MaxColumns, 1, FGSTN_InputHistory::MaxEventAge + 1);
	const int32 NumButtons = FMath::Clamp(Params.NumButtons, 0, 32);

	Layout.FirstFrame = FMath::Max(0, Last - MaxColumns + 1);
	Layout.NumColumns = Last - Layout.FirstFrame + 1;
	const int32 First = Layout.FirstFrame;

	Layout.Buttons.SetNum(NumButtons);
	for (TArray<FGSTN_InputDisplayCell>& Row : Layout.Buttons)
	{
		Row.SetNum(Layout.NumColumns);
	}
	Layout.Stick.SetNum(Layout.NumColumns);
	Layout.ColumnMarker.Init(INDEX_NONE, Layout.NumColumns);

	// Markers first: a consumed press looks for the command after it
	for (const FGSTN_InputDisplayMarker& Marker : Data.Markers)
	{
		if (Marker.Frame >= First && Marker.Frame <= Last)
		{
			Layout.Markers.Add(Marker);
		}
	}
	Layout.Markers.StableSort([](const FGSTN_InputDisplayMarker& A, const FGSTN_InputDisplayMarker& B) { return A.Frame < B.Frame; });
	for (int32 Index = 0; Index < Layout.Markers.Num(); ++Index)
	{
		int32& Slot = Layout.ColumnMarker[Layout.Markers[Index].Frame - First];
		if (Slot == INDEX_NONE)
		{
			Slot = Index;
		}
	}
	const auto FindMarkerFor = [&Layout, &Params](int32 PressFrame)
	{
		for (int32 Index = 0; Index < Layout.Markers.Num(); ++Index)
		{
			const int32 Frame = Layout.Markers[Index].Frame;
			if (Frame >= PressFrame && Frame - PressFrame <= Params.MarkerReachFrames)
			{
				return Index;
			}
		}
		return static_cast<int32>(INDEX_NONE);
	};

	// ---- Stick: the state of every column, and which columns have an event
	for (int32 Column = 0; Column < Layout.NumColumns; ++Column)
	{
		const FGSTN_StickSample Sample = History.GetStickAt(First + Column);
		FGSTN_InputDisplayStickCell& Cell = Layout.Stick[Column];
		Cell.StickDirection = Sample.StickDir;
		Cell.FacingDirection = Sample.FacingDir;
		Cell.StickKey = GetNumpadKey(Sample.StickDir);
		Cell.FacingKey = GetNumpadKey(Sample.FacingDir);
		Cell.Zone = Sample.Zone;
		Cell.bFlick = Sample.bFlick;
		Cell.bEvent = Column == 0;
	}

	// The two rings are in append order, oldest at the highest age. Events on one frame: stick, then buttons.
	struct FRawEvent
	{
		int32 Frame;
		int32 Order;
		FString Item;
	};
	TArray<FRawEvent> Raw;

	for (int32 Age = History.GetNumDirectionEvents() - 1; Age >= 0; --Age)
	{
		const int32 Frame = History.GetDirectionEvent(Age).Frame;
		if (Frame >= First && Frame <= Last)
		{
			FGSTN_InputDisplayStickCell& Cell = Layout.Stick[Frame - First];
			Cell.bEvent = true;
			Raw.Add({ Frame, 0, DescribeStick(Cell) });
		}
	}

	// ---- Buttons
	for (int32 Button = 0; Button < NumButtons; ++Button)
	{
		TArray<FGSTN_InputDisplayCell>& Row = Layout.Buttons[Button];
		const TCHAR Letter = GetButtonLetter(Button);

		const auto SetCell = [&Row, First](int32 Frame, EGSTN_InputCellKind Kind, bool bConsumed, int32 MarkerIndex)
		{
			const int32 Column = Frame - First;
			if (Row.IsValidIndex(Column))
			{
				Row[Column] = { Kind, bConsumed, MarkerIndex };
			}
		};
		const auto FillHold = [&SetCell](int32 FromFrame, int32 ToFrame, bool bConsumed)
		{
			for (int32 Frame = FromFrame; Frame <= ToFrame; ++Frame)
			{
				SetCell(Frame, EGSTN_InputCellKind::Hold, bConsumed, INDEX_NONE);
			}
		};

		// The state before the first event we know of: a button whose first event is a release was down already
		bool bHeld = false;
		int32 HeldFrom = INDEX_NONE;
		bool bHeldConsumed = false;
		int32 PressMarker = INDEX_NONE;
		bool bFirst = true;

		for (int32 Age = History.GetNumButtonEvents() - 1; Age >= 0; --Age)
		{
			const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(Age));
			if (Event.Button != Button)
			{
				continue;
			}
			if (bFirst && Event.bRelease)
			{
				bHeld = true;
				HeldFrom = First - 1;
			}
			bFirst = false;

			if (!Event.bRelease)
			{
				bHeld = true;
				HeldFrom = Event.Frame;
				bHeldConsumed = Event.bConsumed;
				PressMarker = Event.bConsumed ? FindMarkerFor(Event.Frame) : INDEX_NONE;
				SetCell(Event.Frame, EGSTN_InputCellKind::Press, bHeldConsumed, PressMarker);
				if (Event.Frame >= First && Event.Frame <= Last)
				{
					Raw.Add({ Event.Frame, 1 + Button * 2, FString::Printf(TEXT("+%c"), Letter) });
				}
			}
			else
			{
				if (bHeld && HeldFrom == Event.Frame)
				{
					// Pressed and released on one frame: one tap, which replaces the press item
					SetCell(Event.Frame, EGSTN_InputCellKind::Tap, bHeldConsumed, PressMarker);
					for (FRawEvent& Item : Raw)
					{
						if (Item.Frame == Event.Frame && Item.Order == 1 + Button * 2)
						{
							Item.Item = FString::Printf(TEXT("*%c"), Letter);
						}
					}
				}
				else
				{
					FillHold(HeldFrom + 1, Event.Frame - 1, bHeldConsumed);
					SetCell(Event.Frame, EGSTN_InputCellKind::Release, bHeldConsumed, INDEX_NONE);
					if (Event.Frame >= First && Event.Frame <= Last)
					{
						Raw.Add({ Event.Frame, 2 + Button * 2, FString::Printf(TEXT("-%c"), Letter) });
					}
				}
				bHeld = false;
			}
		}
		if (bHeld && (History.GetHeldButtons() & (1u << Button)) != 0)
		{
			FillHold(HeldFrom + 1, Last, bHeldConsumed);
		}
	}

	// A tap leaves only its '*' item; the release item of the same frame never existed (it was skipped above)
	Raw.StableSort([](const FRawEvent& A, const FRawEvent& B) { return A.Frame != B.Frame ? A.Frame < B.Frame : A.Order < B.Order; });
	int32 Previous = INDEX_NONE;
	for (const FRawEvent& Item : Raw)
	{
		FGSTN_InputDisplayEvent& Event = Layout.Events.AddDefaulted_GetRef();
		Event.Frame = Item.Frame;
		Event.Column = Item.Frame - First;
		Event.Gap = Previous == INDEX_NONE ? 0 : Item.Frame - Previous;
		Event.Item = Item.Item;
		Previous = Item.Frame;
	}
	return Layout;
}
