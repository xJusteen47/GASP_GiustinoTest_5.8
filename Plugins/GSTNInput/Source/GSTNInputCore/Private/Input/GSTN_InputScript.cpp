// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputScript.h"
#include "Algo/StableSort.h"
#include "GSTN_InputLog.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputHistory.h"

namespace
{
	/** Angle of each numpad key; index = key, -1 = no direction */
	constexpr float NumpadAngles[10] = { -1.f, 225.f, 180.f, 135.f, 270.f, -1.f, 90.f, 315.f, 0.f, 45.f };

	TCHAR NumpadKey(EGSTN_StickDirection Direction)
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

	FString ButtonName(int32 Button)
	{
		return Button < 26 ? FString::Chr(static_cast<TCHAR>(TEXT('A') + Button)) : FString::Printf(TEXT("#%d"), Button);
	}

	FString FrameName(int32 Frame)
	{
		return Frame == FGSTN_InputHistory::AncientFrame ? FString(TEXT("?")) : FString::FromInt(Frame);
	}

	/** Stick magnitude of a zone: the middle of the Aim band, full for Tilt */
	float ZoneMagnitude(EGSTN_StickZone Zone, const UGSTN_InputSettings& Settings)
	{
		switch (Zone)
		{
		case EGSTN_StickZone::Neutral: return 0.f;
		case EGSTN_StickZone::Aim:     return 0.5f * (Settings.StickDeadZone + Settings.AimZone);
		default:                       return 1.f;
		}
	}
}

FGSTN_InputScript::FGSTN_InputScript(TArray<FGameplayTag> InButtonTags)
	: ButtonTags(MoveTemp(InButtonTags))
{
}

bool FGSTN_InputScript::Build(TConstArrayView<FGameplayTag> InButtonTags, const FString& Notation, const UGSTN_InputSettings& Settings,
	FGSTN_InputHistory& OutHistory, int32& OutLastFrame, FString* OutError)
{
	TArray<FGameplayTag> ButtonTagsCopy(InButtonTags);
	FGSTN_InputScript Script(MoveTemp(ButtonTagsCopy));
	FString Error;
	OutHistory = FGSTN_InputHistory();
	OutLastFrame = INDEX_NONE;

	const bool bOk = Script.Parse(Notation, Error) && Script.Advance(OutHistory, Settings, Script.GetLastFrame(), &Error);
	if (!bOk)
	{
		UE_LOG(LogGSTNInput, Error, TEXT("Input script \"%s\": %s"), *Notation, *Error);
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}
	OutLastFrame = Script.GetLastFrame();
	return true;
}

bool FGSTN_InputScript::Parse(const FString& Notation, FString& OutError)
{
	TArray<FString> Tokens;
	Notation.ParseIntoArrayWS(Tokens);

	TArray<FEvent> Parsed;
	for (const FString& Token : Tokens)
	{
		if (!ParseToken(Token, Parsed, OutError))
		{
			return false;
		}
	}
	Events.Append(Parsed);
	bSorted = false;
	return true;
}

bool FGSTN_InputScript::ParseToken(const FString& Token, TArray<FEvent>& OutEvents, FString& OutError) const
{
	const auto Fail = [&Token, &OutError](const TCHAR* Reason)
	{
		OutError = FString::Printf(TEXT("token \"%s\": %s"), *Token, Reason);
		return false;
	};

	FString FrameText, Item;
	if (!Token.Split(TEXT(":"), &FrameText, &Item) || Item.IsEmpty())
	{
		return Fail(TEXT("expected <frame>:<item>"));
	}
	int32 Frame = 0;
	if (!LexTryParseString(Frame, *FrameText) || Frame < 0)
	{
		return Fail(TEXT("the frame is not a non-negative integer"));
	}

	FEvent Event;
	Event.Frame = Frame;

	if (Item.StartsWith(TEXT("cam=")) || Item.StartsWith(TEXT("face=")))
	{
		const bool bCamera = Item.StartsWith(TEXT("cam="));
		float Yaw = 0.f;
		if (!LexTryParseString(Yaw, *Item.RightChop(Item.Find(TEXT("=")) + 1)))
		{
			return Fail(TEXT("the yaw is not a number"));
		}
		Event.Type = bCamera ? EEventType::CameraYaw : EEventType::FacingYaw;
		Event.Value = Yaw;
		OutEvents.Add(Event);
		return true;
	}

	const TCHAR Head = Item[0];
	if (Head == TEXT('+') || Head == TEXT('-') || Head == TEXT('*'))
	{
		const int32 Button = FindButton(Item.RightChop(1));
		if (Button == INDEX_NONE)
		{
			return Fail(TEXT("unknown button"));
		}
		Event.ButtonMask = 1u << Button;
		Event.Type = Head == TEXT('-') ? EEventType::Release : EEventType::Press;
		OutEvents.Add(Event);
		if (Head == TEXT('*'))
		{
			Event.Type = EEventType::Release;
			OutEvents.Add(Event);
		}
		return true;
	}

	// Stick: a numpad key or "@<deg>", then an optional zone letter, then an optional flick mark.
	FString Rest = Item;
	bool bFlick = false;
	if (Rest.EndsWith(TEXT("!")))
	{
		bFlick = true;
		Rest.LeftChopInline(1);
	}
	EGSTN_StickZone Zone = EGSTN_StickZone::Tilt;
	if (Rest.EndsWith(TEXT("a")) || Rest.EndsWith(TEXT("t")))
	{
		Zone = Rest.EndsWith(TEXT("a")) ? EGSTN_StickZone::Aim : EGSTN_StickZone::Tilt;
		Rest.LeftChopInline(1);
	}

	float Angle = 0.f;
	if (Rest.StartsWith(TEXT("@")))
	{
		if (!LexTryParseString(Angle, *Rest.RightChop(1)))
		{
			return Fail(TEXT("the angle is not a number"));
		}
	}
	else if (Rest.Len() == 1 && FChar::IsDigit(Rest[0]) && Rest[0] != TEXT('0'))
	{
		const int32 Key = Rest[0] - TEXT('0');
		if (Key == 5)
		{
			Zone = EGSTN_StickZone::Neutral;
		}
		else
		{
			Angle = NumpadAngles[Key];
		}
	}
	else
	{
		return Fail(TEXT("unknown item"));
	}

	if (bFlick)
	{
		if (Zone == EGSTN_StickZone::Neutral)
		{
			return Fail(TEXT("a neutral stick cannot flick"));
		}
		if (Frame == 0)
		{
			return Fail(TEXT("a flick needs a frame before it to leave Neutral"));
		}
		// The classifier decides what a flick is: release the stick on the previous frame so it sees the quick leave.
		FEvent Release;
		Release.Frame = Frame - 1;
		Release.Type = EEventType::Stick;
		Release.Zone = EGSTN_StickZone::Neutral;
		OutEvents.Add(Release);
	}

	Event.Type = EEventType::Stick;
	Event.Value = Angle;
	Event.Zone = Zone;
	OutEvents.Add(Event);
	return true;
}

int32 FGSTN_InputScript::FindButton(const FString& Name) const
{
	if (Name.Len() == 1 && FChar::IsAlpha(Name[0]))
	{
		return FChar::ToUpper(Name[0]) - TEXT('A');
	}
	if (Name.StartsWith(TEXT("#")))
	{
		int32 Index = 0;
		return LexTryParseString(Index, *Name.RightChop(1)) && Index >= 0 && Index < 32 ? Index : INDEX_NONE;
	}
	for (int32 Index = 0; Index < ButtonTags.Num(); ++Index)
	{
		FString Leaf = ButtonTags[Index].ToString();
		Leaf.RightChopInline(Leaf.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1);
		if (Leaf.Equals(Name, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FGSTN_InputScript::AddPress(int32 Frame, uint32 ButtonMask)
{
	FEvent Event;
	Event.Frame = Frame;
	Event.Type = EEventType::Press;
	Event.ButtonMask = ButtonMask;
	Events.Add(Event);
	bSorted = false;
}

void FGSTN_InputScript::AddRelease(int32 Frame, uint32 ButtonMask)
{
	FEvent Event;
	Event.Frame = Frame;
	Event.Type = EEventType::Release;
	Event.ButtonMask = ButtonMask;
	Events.Add(Event);
	bSorted = false;
}

void FGSTN_InputScript::AddTap(int32 Frame, uint32 ButtonMask)
{
	AddPress(Frame, ButtonMask);
	AddRelease(Frame, ButtonMask);
}

void FGSTN_InputScript::SetStick(int32 Frame, float AngleDeg, EGSTN_StickZone Zone)
{
	FEvent Event;
	Event.Frame = Frame;
	Event.Type = EEventType::Stick;
	Event.Value = AngleDeg;
	Event.Zone = Zone;
	Events.Add(Event);
	bSorted = false;
}

void FGSTN_InputScript::SetCameraYaw(int32 Frame, float YawDeg)
{
	FEvent Event;
	Event.Frame = Frame;
	Event.Type = EEventType::CameraYaw;
	Event.Value = YawDeg;
	Events.Add(Event);
	bSorted = false;
}

void FGSTN_InputScript::SetFacingYaw(int32 Frame, float YawDeg)
{
	FEvent Event;
	Event.Frame = Frame;
	Event.Type = EEventType::FacingYaw;
	Event.Value = YawDeg;
	Events.Add(Event);
	bSorted = false;
}

int32 FGSTN_InputScript::GetLastFrame() const
{
	int32 Last = INDEX_NONE;
	for (const FEvent& Event : Events)
	{
		Last = FMath::Max(Last, Event.Frame);
	}
	return Last;
}

void FGSTN_InputScript::SortEvents()
{
	if (!bSorted)
	{
		// Only the events still to play: the played ones keep their positions. Events of one frame keep their order.
		TArrayView<FEvent> Pending = MakeArrayView(Events).Slice(EventCursor, Events.Num() - EventCursor);
		Algo::StableSort(Pending, [](const FEvent& A, const FEvent& B) { return A.Frame < B.Frame; });
		bSorted = true;
	}
}

bool FGSTN_InputScript::Advance(FGSTN_InputHistory& History, const UGSTN_InputSettings& Settings, int32 UntilFrame, FString* OutError)
{
	const auto Fail = [OutError](const FString& Reason)
	{
		UE_LOG(LogGSTNInput, Error, TEXT("Input script: %s"), *Reason);
		if (OutError)
		{
			*OutError = Reason;
		}
		return false;
	};

	SortEvents();

	for (; NextFrame <= UntilFrame; ++NextFrame)
	{
		uint32 Pressed = 0;
		uint32 Released = 0;

		for (; EventCursor < Events.Num() && Events[EventCursor].Frame <= NextFrame; ++EventCursor)
		{
			const FEvent& Event = Events[EventCursor];
			if (Event.Frame < NextFrame)
			{
				return Fail(FString::Printf(TEXT("event for frame %d added after that frame was played"), Event.Frame));
			}

			switch (Event.Type)
			{
			case EEventType::Press:
				if ((Held & Event.ButtonMask) != 0 || (Released & Event.ButtonMask) != 0)
				{
					return Fail(FString::Printf(TEXT("frame %d presses a button already held (or released this frame)"), NextFrame));
				}
				Held |= Event.ButtonMask;
				Pressed |= Event.ButtonMask;
				break;
			case EEventType::Release:
				if ((Held & Event.ButtonMask) != Event.ButtonMask)
				{
					return Fail(FString::Printf(TEXT("frame %d releases a button that is not held"), NextFrame));
				}
				Held &= ~Event.ButtonMask;
				Released |= Event.ButtonMask;
				break;
			case EEventType::Stick:
				StickAngle = Event.Value;
				StickZone = Event.Zone;
				break;
			case EEventType::CameraYaw:
				CameraYaw = Event.Value;
				break;
			case EEventType::FacingYaw:
				FacingYaw = Event.Value;
				break;
			}
		}

		FGSTN_InputSample Sample;
		Sample.Frame = NextFrame;
		Sample.Held = Held;
		Sample.Pressed = Pressed;
		Sample.Released = Released;

		const float Magnitude = ZoneMagnitude(StickZone, Settings);
		const float Radians = FMath::DegreesToRadians(StickAngle);
		const FVector2D Stick(FMath::Sin(Radians) * Magnitude, FMath::Cos(Radians) * Magnitude);
		FGSTN_ClassifierState NewState;
		Sample.Stick = FGSTN_DirectionClassifier::Classify(Stick, CameraYaw, FacingYaw, ClassifierState, NextFrame, Settings, NewState);
		ClassifierState = NewState;

		History.Append(Sample);
	}
	return true;
}

FString FGSTN_InputScript::Describe(const FGSTN_InputHistory& History, int32 MaxEvents)
{
	struct FItem
	{
		int32 Frame;
		FString Text;
	};
	TArray<FItem> Items;

	// Merge the two rings oldest first; on the same frame buttons come before the stick.
	int32 ButtonAge = History.GetNumButtonEvents() - 1;
	int32 DirectionAge = History.GetNumDirectionEvents() - 1;
	while (ButtonAge >= 0 || DirectionAge >= 0)
	{
		const bool bTakeButton = ButtonAge >= 0
			&& (DirectionAge < 0 || History.GetButtonEvent(History.GetButtonSlot(ButtonAge)).Frame <= History.GetDirectionEvent(DirectionAge).Frame);
		if (bTakeButton)
		{
			const FGSTN_ButtonEvent& Event = History.GetButtonEvent(History.GetButtonSlot(ButtonAge--));
			Items.Add({ Event.Frame, FString::Printf(TEXT("%s:%c%s"), *FrameName(Event.Frame), Event.bRelease ? TEXT('-') : TEXT('+'), *ButtonName(Event.Button)) });
		}
		else
		{
			const FGSTN_DirectionEvent& Event = History.GetDirectionEvent(DirectionAge--);
			FString Stick = FString::Chr(NumpadKey(Event.Stick.StickDir));
			if (Event.Stick.Zone != EGSTN_StickZone::Neutral)
			{
				Stick += Event.Stick.Zone == EGSTN_StickZone::Aim ? TEXT("a") : TEXT("t");
			}
			Stick += Event.Stick.bFlick ? TEXT("!") : TEXT("");
			Items.Add({ Event.Frame, FString::Printf(TEXT("%s:%s"), *FrameName(Event.Frame), *Stick) });
		}
	}

	FString Result;
	for (int32 Index = FMath::Max(0, Items.Num() - MaxEvents); Index < Items.Num(); ++Index)
	{
		Result += Result.IsEmpty() ? TEXT("") : TEXT(" ");
		Result += Items[Index].Text;
	}
	return Result;
}
