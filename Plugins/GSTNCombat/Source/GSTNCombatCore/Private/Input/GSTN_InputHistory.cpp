// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputHistory.h"

namespace
{
	/** Delta value standing for "AncientFrame" in the wire format; also the saturation of every delta */
	constexpr uint8 AncientDelta = 255;

	constexpr uint8 FlagHasWorldYaw = 1 << 5;
	constexpr uint8 FlagFrameOffset = 1 << 6;
	constexpr uint8 CountMask = 0x1F;

	bool IsAged(int32 EventFrame, int32 Now)
	{
		return EventFrame == FGSTN_InputHistory::AncientFrame || Now - EventFrame > FGSTN_InputHistory::MaxEventAge;
	}

	void PackStick(const FGSTN_StickSample& Stick, uint8 (&Out)[3])
	{
		Out[0] = static_cast<uint8>(Stick.StickDir) | (static_cast<uint8>(Stick.FacingDir) << 4);
		Out[1] = static_cast<uint8>(Stick.WorldDir) | (static_cast<uint8>(Stick.Zone) << 4) | (Stick.bFlick ? 0x40 : 0);
		Out[2] = Stick.WorldYaw;
	}

	/** False if the bytes do not describe a valid stick sample */
	bool UnpackStick(const uint8 (&In)[3], FGSTN_StickSample& Out)
	{
		const uint8 StickDir = In[0] & 0x0F;
		const uint8 FacingDir = In[0] >> 4;
		const uint8 WorldDir = In[1] & 0x0F;
		const uint8 Zone = (In[1] >> 4) & 0x03;
		if (StickDir >= GSTN_NumStickDirections || FacingDir >= GSTN_NumStickDirections || WorldDir >= GSTN_NumStickDirections
			|| Zone > static_cast<uint8>(EGSTN_StickZone::Tilt) || (In[1] & 0x80) != 0)
		{
			return false;
		}
		Out.StickDir = static_cast<EGSTN_StickDirection>(StickDir);
		Out.FacingDir = static_cast<EGSTN_StickDirection>(FacingDir);
		Out.WorldDir = static_cast<EGSTN_StickDirection>(WorldDir);
		Out.Zone = static_cast<EGSTN_StickZone>(Zone);
		Out.bFlick = (In[1] & 0x40) != 0;
		Out.WorldYaw = In[2];
		return true;
	}
}

void FGSTN_InputHistory::Append(const FGSTN_InputSample& Sample)
{
	checkf(Sample.Frame >= Current.Frame, TEXT("Samples must be appended in frame order (%d after %d)"), Sample.Frame, Current.Frame);

	// Press events first, then releases, each in button order: a tap leaves a press and a release on the same frame.
	for (uint32 Mask = Sample.Pressed; Mask != 0; Mask &= Mask - 1)
	{
		PushButtonEvent(Sample.Frame, FMath::CountTrailingZeros(Mask), false);
	}
	for (uint32 Mask = Sample.Released; Mask != 0; Mask &= Mask - 1)
	{
		PushButtonEvent(Sample.Frame, FMath::CountTrailingZeros(Mask), true);
	}
	HeldButtons = Sample.Held;

	if (!Sample.Stick.HasSameDirections(Current.Stick) || Sample.Stick.bFlick)
	{
		FGSTN_DirectionEvent Event;
		Event.Frame = Sample.Frame;
		Event.Stick = Sample.Stick;
		PushDirectionEvent(Event);
	}
	Current.Frame = Sample.Frame;
	Current.Stick = Sample.Stick;

	DropOldEvents(Sample.Frame);
}

void FGSTN_InputHistory::Consume(int32 Slot)
{
	check(Slot >= 0 && Slot < MaxEvents);
	ButtonEvents[Slot].bConsumed = 1;
}

void FGSTN_InputHistory::FlushPresses(uint32 ButtonMask, int32 UpToFrame)
{
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		FGSTN_ButtonEvent& Event = ButtonEvents[GetButtonSlot(Age)];
		if (!Event.bRelease && Event.Frame <= UpToFrame && (ButtonMask & (1u << Event.Button)) != 0)
		{
			Event.bConsumed = 1;
		}
	}
}

bool FGSTN_InputHistory::IsHeldContinuouslySince(int32 Button, int32 PressFrame) const
{
	if ((HeldButtons & (1u << Button)) == 0)
	{
		return false;
	}
	// A release on the press frame itself belongs to a tap of that frame, so it counts as "after the press" too.
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		const FGSTN_ButtonEvent& Event = ButtonEvents[GetButtonSlot(Age)];
		if (Event.Frame < PressFrame)
		{
			break;
		}
		if (Event.bRelease && Event.Button == Button)
		{
			return false;
		}
	}
	return true;
}

FGSTN_StickSample FGSTN_InputHistory::GetStickAt(int32 Frame) const
{
	if (Frame >= Current.Frame)
	{
		FGSTN_StickSample Stick = Current.Stick;
		Stick.bFlick = Stick.bFlick && Frame == Current.Frame;
		return Stick;
	}
	for (int32 Age = 0; Age < DirectionCount; ++Age)
	{
		const FGSTN_DirectionEvent& Event = GetDirectionEvent(Age);
		if (Event.Frame <= Frame)
		{
			FGSTN_StickSample Stick = Event.Stick;
			Stick.bFlick = Stick.bFlick && Event.Frame == Frame;
			return Stick;
		}
	}
	return FGSTN_StickSample();
}

void FGSTN_InputHistory::PushButtonEvent(int32 Frame, int32 Button, bool bRelease)
{
	FGSTN_ButtonEvent& Event = ButtonEvents[ButtonHead];
	Event = FGSTN_ButtonEvent();
	Event.Frame = Frame;
	Event.Button = Button;
	Event.bRelease = bRelease ? 1 : 0;
	ButtonHead = (ButtonHead + 1) & (MaxEvents - 1);
	ButtonCount = FMath::Min<int32>(ButtonCount + 1, MaxEvents);
}

void FGSTN_InputHistory::PushDirectionEvent(const FGSTN_DirectionEvent& Event)
{
	DirectionEvents[DirectionHead] = Event;
	DirectionHead = (DirectionHead + 1) & (MaxEvents - 1);
	DirectionCount = FMath::Min<int32>(DirectionCount + 1, MaxEvents);
}

void FGSTN_InputHistory::DropOldEvents(int32 Now)
{
	while (ButtonCount > 0 && IsAged(ButtonEvents[GetButtonSlot(ButtonCount - 1)].Frame, Now))
	{
		--ButtonCount;
	}

	// The newest stick event is the current stick state: it never leaves, it only becomes ancient.
	while (DirectionCount > 1 && IsAged(GetDirectionEvent(DirectionCount - 1).Frame, Now))
	{
		--DirectionCount;
	}
	if (DirectionCount == 1 && IsAged(GetDirectionEvent(0).Frame, Now))
	{
		DirectionEvents[(DirectionHead - 1) & (MaxEvents - 1)].Frame = AncientFrame;
	}
}

void FGSTN_InputHistory::NetSerialize(FArchive& Ar, int32 CurrentFrame)
{
	if (Ar.IsSaving())
	{
		const FGSTN_DirectionEvent* Newest = DirectionCount > 0 ? &GetDirectionEvent(0) : nullptr;
		checkf(Current.Stick.HasSameDirections(Newest ? Newest->Stick : FGSTN_StickSample()), TEXT("Current stick must equal the newest stick event"));

		uint8 ButtonHeader = static_cast<uint8>(ButtonCount);
		uint8 DirectionHeader = static_cast<uint8>(DirectionCount);
		if (Current.Stick.WorldYaw != 0)
		{
			DirectionHeader |= FlagHasWorldYaw;
		}
		if (Current.Frame != CurrentFrame)
		{
			DirectionHeader |= FlagFrameOffset;
		}
		Ar << ButtonHeader;
		Ar << DirectionHeader;
		Ar.SerializeIntPacked(HeldButtons);
		if (Current.Stick.WorldYaw != 0)
		{
			Ar << Current.Stick.WorldYaw;
		}
		if (Current.Frame != CurrentFrame)
		{
			// Only an owner that serializes a history it did not append to this frame gets here
			int32 Frame = Current.Frame;
			Ar << Frame;
		}

		for (int32 Age = 0; Age < ButtonCount; ++Age)
		{
			const FGSTN_ButtonEvent& Event = ButtonEvents[GetButtonSlot(Age)];
			const int32 Delta = CurrentFrame - Event.Frame;
			checkf(Delta >= 0 && Delta <= MaxEventAge, TEXT("Button event %d frames old"), Delta);
			uint8 DeltaByte = static_cast<uint8>(Delta);
			uint8 Packed = Event.Button | (Event.bRelease << 5) | (Event.bConsumed << 6);
			Ar << DeltaByte;
			Ar << Packed;
		}
		for (int32 Age = 0; Age < DirectionCount; ++Age)
		{
			const FGSTN_DirectionEvent& Event = GetDirectionEvent(Age);
			const int32 Delta = Event.Frame == AncientFrame ? AncientDelta : CurrentFrame - Event.Frame;
			checkf(Delta >= 0 && Delta <= AncientDelta, TEXT("Stick event %d frames old"), Delta);
			uint8 DeltaByte = static_cast<uint8>(Delta);
			uint8 Bytes[3];
			PackStick(Event.Stick, Bytes);
			Ar << DeltaByte;
			Ar.Serialize(Bytes, sizeof(Bytes));
		}
		return;
	}

	*this = FGSTN_InputHistory();

	uint8 ButtonHeader = 0;
	uint8 DirectionHeader = 0;
	Ar << ButtonHeader;
	Ar << DirectionHeader;
	Ar.SerializeIntPacked(HeldButtons);

	const int32 NumButtons = ButtonHeader & CountMask;
	const int32 NumDirections = DirectionHeader & CountMask;
	if ((ButtonHeader & ~CountMask) != 0 || NumButtons > MaxEvents || NumDirections > MaxEvents || (DirectionHeader & 0x80) != 0)
	{
		Ar.SetError();
		return;
	}

	uint8 WorldYaw = 0;
	if ((DirectionHeader & FlagHasWorldYaw) != 0)
	{
		Ar << WorldYaw;
	}
	int32 LatestFrame = CurrentFrame;
	if ((DirectionHeader & FlagFrameOffset) != 0)
	{
		Ar << LatestFrame;
	}

	// Newest first on the wire; the rings are rebuilt oldest first so slot 0 is the oldest.
	for (int32 Age = 0; Age < NumButtons; ++Age)
	{
		uint8 DeltaByte = 0;
		uint8 Packed = 0;
		Ar << DeltaByte;
		Ar << Packed;
		if (DeltaByte > MaxEventAge || (Packed & 0x80) != 0)
		{
			Ar.SetError();
			return;
		}
		FGSTN_ButtonEvent& Event = ButtonEvents[NumButtons - 1 - Age];
		Event.Frame = CurrentFrame - DeltaByte;
		Event.Button = Packed & 0x1F;
		Event.bRelease = (Packed >> 5) & 1;
		Event.bConsumed = (Packed >> 6) & 1;
	}
	ButtonCount = NumButtons;
	ButtonHead = NumButtons & (MaxEvents - 1);

	for (int32 Age = 0; Age < NumDirections; ++Age)
	{
		uint8 DeltaByte = 0;
		uint8 Bytes[3] = {};
		Ar << DeltaByte;
		Ar.Serialize(Bytes, sizeof(Bytes));
		FGSTN_DirectionEvent& Event = DirectionEvents[NumDirections - 1 - Age];
		if (!UnpackStick(Bytes, Event.Stick))
		{
			Ar.SetError();
			return;
		}
		Event.Frame = DeltaByte == AncientDelta ? AncientFrame : CurrentFrame - DeltaByte;
	}
	DirectionCount = NumDirections;
	DirectionHead = NumDirections & (MaxEvents - 1);

	Current.Frame = LatestFrame;
	if (NumDirections > 0)
	{
		const FGSTN_DirectionEvent& Newest = GetDirectionEvent(0);
		Current.Stick = Newest.Stick;
		// A flick lasts one frame: only the frame of the event carries it
		Current.Stick.bFlick = Newest.Stick.bFlick && Newest.Frame == LatestFrame;
	}
	Current.Stick.WorldYaw = WorldYaw;
}

bool FGSTN_InputHistory::operator==(const FGSTN_InputHistory& Other) const
{
	if (HeldButtons != Other.HeldButtons || ButtonCount != Other.ButtonCount || DirectionCount != Other.DirectionCount
		|| !(Current == Other.Current))
	{
		return false;
	}
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		if (!(ButtonEvents[GetButtonSlot(Age)] == Other.ButtonEvents[Other.GetButtonSlot(Age)]))
		{
			return false;
		}
	}
	for (int32 Age = 0; Age < DirectionCount; ++Age)
	{
		if (!(GetDirectionEvent(Age) == Other.GetDirectionEvent(Age)))
		{
			return false;
		}
	}
	return true;
}
