// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputHistory.h"
#include "GSTN_InputLog.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Delta value standing for "AncientFrame" in the wire format; also the saturation of every delta */
	constexpr uint8 AncientDelta = 255;

	// Button header: count in the low 5 bits (0..16), then two flags; bit 7 is never set.
	constexpr uint8 ButtonCountMask = 0x1F;

	/** A byte with the age of the last consumed stick anchor follows */
	constexpr uint8 FlagHasConsumedStick = 1 << 5;

	/** The latest frame is not the serialized frame: it follows as an int32 */
	constexpr uint8 FlagFrameOffset = 1 << 6;

	// Stick header: count in the low 6 bits (0..32), then one flag; bit 7 is never set.
	constexpr uint8 DirectionCountMask = 0x3F;
	constexpr uint8 FlagHasWorldYaw = 1 << 6;

	bool IsAged(int32 EventFrame, int32 Now)
	{
		return EventFrame == FGSTN_InputHistory::AncientFrame || Now - EventFrame > FGSTN_InputHistory::MaxEventAge;
	}

	void PackStickBytes(const FGSTN_StickSample& Stick, uint8 (&Out)[3])
	{
		Out[0] = static_cast<uint8>(Stick.StickDir) | (static_cast<uint8>(Stick.FacingDir) << 4);
		Out[1] = static_cast<uint8>(Stick.WorldDir) | (static_cast<uint8>(Stick.Zone) << 4) | (Stick.bFlick ? 0x40 : 0);
		Out[2] = Stick.WorldYaw;
	}

	/** False if the bytes do not describe a valid stick sample */
	bool UnpackStickBytes(const uint8 (&In)[3], FGSTN_StickSample& Out)
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
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_InputHistory_Append);
	if (Sample.Frame < Current.Frame)
	{
		// The frame counter went back without a restore of this state: Network Prediction re-based the frame numbers (a
		// ReconcileFromInputOffset after a hitch or an input gap moved them 50+ frames back in a playtest). The stored events
		// are dated on the old numbering and would read as future frames, so the history starts over from this sample. Only
		// the held buttons survive: a button still down is not pressed again. A rollback never gets here, it restores first
		UE_LOG(LogGSTNInput, Warning, TEXT("Input history: frame %d after %d without a restore (frame numbers re-based), history cleared"), Sample.Frame, Current.Frame);
		const uint32 StillHeld = HeldButtons;
		*this = FGSTN_InputHistory();
		HeldButtons = StillHeld;
	}

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
		PushDirectionEvent(Sample.Frame, Sample.Stick);
	}
	Current.Frame = Sample.Frame;
	Current.Stick = Sample.Stick;

	DropOldEvents(Sample.Frame);
}

void FGSTN_InputHistory::Consume(int32 Slot)
{
	check(Slot >= 0 && Slot < MaxButtonEvents);
	ButtonBits[Slot] |= ButtonBitConsumed;
}

void FGSTN_InputHistory::FlushPresses(uint32 ButtonMask, int32 UpToFrame)
{
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		const int32 Slot = GetButtonSlot(Age);
		const uint8 Bits = ButtonBits[Slot];
		if ((Bits & ButtonBitRelease) == 0 && UnwrapFrame(ButtonFrames[Slot]) <= UpToFrame && (ButtonMask & (1u << (Bits & ButtonBitsButton))) != 0)
		{
			ButtonBits[Slot] = static_cast<uint8>(Bits | ButtonBitConsumed);
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
		const int32 Slot = GetButtonSlot(Age);
		if (UnwrapFrame(ButtonFrames[Slot]) < PressFrame)
		{
			break;
		}
		const uint8 Bits = ButtonBits[Slot];
		if ((Bits & ButtonBitRelease) != 0 && (Bits & ButtonBitsButton) == Button)
		{
			return false;
		}
	}
	return true;
}

bool FGSTN_InputHistory::IsButtonHeldAt(int32 Button, int32 Frame, int32& OutHeldFrames) const
{
	// Newest first: events after Frame are skipped (the oldest of them tells how the button was before), the first event
	// at or before Frame gives the state.
	bool bHasLater = false;
	bool bOldestLaterIsRelease = false;
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		const int32 Slot = GetButtonSlot(Age);
		const uint8 Bits = ButtonBits[Slot];
		if ((Bits & ButtonBitsButton) != Button)
		{
			continue;
		}
		const int32 EventFrame = UnwrapFrame(ButtonFrames[Slot]);
		const bool bRelease = (Bits & ButtonBitRelease) != 0;
		if (EventFrame > Frame)
		{
			bHasLater = true;
			bOldestLaterIsRelease = bRelease;
			continue;
		}
		OutHeldFrames = Frame - EventFrame;
		return !bRelease;
	}

	OutHeldFrames = MAX_int32;
	if (bHasLater)
	{
		// Nothing before Frame in the ring: a later release means the button was already down.
		return bOldestLaterIsRelease;
	}
	return (HeldButtons & (1u << Button)) != 0;
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
		// Only the frame is unpacked until the event is found
		const int32 Slot = GetDirectionSlot(Age);
		const int32 EventFrame = GetDirectionFrameAtSlot(Slot);
		if (EventFrame <= Frame)
		{
			FGSTN_StickSample Stick = UnpackStick(DirectionSticks[Slot], DirectionYaws[Slot]);
			Stick.bFlick = Stick.bFlick && EventFrame == Frame;
			return Stick;
		}
	}
	return FGSTN_StickSample();
}

void FGSTN_InputHistory::PushButtonEvent(int32 Frame, int32 Button, bool bRelease)
{
	ButtonFrames[ButtonHead] = static_cast<uint16>(Frame);
	ButtonBits[ButtonHead] = static_cast<uint8>((Button & ButtonBitsButton) | (bRelease ? ButtonBitRelease : 0));
	ButtonHead = (ButtonHead + 1) & (MaxButtonEvents - 1);
	ButtonCount = FMath::Min<int32>(ButtonCount + 1, MaxButtonEvents);
}

void FGSTN_InputHistory::PushDirectionEvent(int32 Frame, const FGSTN_StickSample& Stick)
{
	DirectionFrames[DirectionHead] = static_cast<uint16>(Frame);
	DirectionSticks[DirectionHead] = PackStick(Stick);
	DirectionYaws[DirectionHead] = Stick.WorldYaw;
	DirectionHead = (DirectionHead + 1) & (MaxDirectionEvents - 1);
	DirectionCount = FMath::Min<int32>(DirectionCount + 1, MaxDirectionEvents);
}

void FGSTN_InputHistory::DropOldEvents(int32 Now)
{
	// Every anchor still matchable is newer than this, so an aged mark has nothing left to block
	if (IsAged(LastConsumedStickFrame, Now))
	{
		LastConsumedStickFrame = AncientFrame;
	}

	while (ButtonCount > 0 && IsAged(UnwrapFrame(ButtonFrames[GetButtonSlot(ButtonCount - 1)]), Now))
	{
		--ButtonCount;
	}

	// The newest stick event is the current stick state: it never leaves, it only becomes ancient.
	while (DirectionCount > 1 && IsAged(GetDirectionFrameAtSlot(GetDirectionSlot(DirectionCount - 1)), Now))
	{
		--DirectionCount;
	}
	if (DirectionCount == 1)
	{
		const int32 Slot = GetDirectionSlot(0);
		if (IsAged(GetDirectionFrameAtSlot(Slot), Now))
		{
			// The stored frame no longer means anything: zero it so equal histories hold equal bytes
			DirectionSticks[Slot] = static_cast<uint16>(DirectionSticks[Slot] | PackedStickAncient);
			DirectionFrames[Slot] = 0;
		}
	}
}

void FGSTN_InputHistory::NetSerialize(FArchive& Ar, int32 CurrentFrame)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_InputHistory_NetSerialize);
	if (Ar.IsSaving())
	{
		const FGSTN_StickSample NewestStick = DirectionCount > 0 ? GetDirectionEvent(0).Stick : FGSTN_StickSample();
		checkf(Current.Stick.HasSameDirections(NewestStick), TEXT("Current stick must equal the newest stick event"));

		uint8 ButtonHeader = static_cast<uint8>(ButtonCount);
		uint8 DirectionHeader = static_cast<uint8>(DirectionCount);
		if (Current.Stick.WorldYaw != 0)
		{
			DirectionHeader |= FlagHasWorldYaw;
		}
		if (Current.Frame != CurrentFrame)
		{
			ButtonHeader |= FlagFrameOffset;
		}
		if (LastConsumedStickFrame != AncientFrame)
		{
			ButtonHeader |= FlagHasConsumedStick;
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
		if (LastConsumedStickFrame != AncientFrame)
		{
			const int32 Delta = CurrentFrame - LastConsumedStickFrame;
			checkf(Delta >= 0 && Delta <= MaxEventAge, TEXT("Consumed stick anchor %d frames old"), Delta);
			uint8 DeltaByte = static_cast<uint8>(Delta);
			Ar << DeltaByte;
		}

		for (int32 Age = 0; Age < ButtonCount; ++Age)
		{
			const int32 Slot = GetButtonSlot(Age);
			const int32 Delta = CurrentFrame - UnwrapFrame(ButtonFrames[Slot]);
			checkf(Delta >= 0 && Delta <= MaxEventAge, TEXT("Button event %d frames old"), Delta);
			uint8 DeltaByte = static_cast<uint8>(Delta);
			// The stored bits are the wire bits: button 0-4, release 5, consumed 6
			uint8 Packed = ButtonBits[Slot];
			Ar << DeltaByte;
			Ar << Packed;
		}
		for (int32 Age = 0; Age < DirectionCount; ++Age)
		{
			const FGSTN_DirectionEvent Event = GetDirectionEvent(Age);
			const int32 Delta = Event.Frame == AncientFrame ? AncientDelta : CurrentFrame - Event.Frame;
			checkf(Delta >= 0 && Delta <= AncientDelta, TEXT("Stick event %d frames old"), Delta);
			uint8 DeltaByte = static_cast<uint8>(Delta);
			uint8 Bytes[3];
			PackStickBytes(Event.Stick, Bytes);
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

	const int32 NumButtons = ButtonHeader & ButtonCountMask;
	const int32 NumDirections = DirectionHeader & DirectionCountMask;
	if ((ButtonHeader & ~(ButtonCountMask | FlagHasConsumedStick | FlagFrameOffset)) != 0 || (DirectionHeader & ~(DirectionCountMask | FlagHasWorldYaw)) != 0
		|| NumButtons > MaxButtonEvents || NumDirections > MaxDirectionEvents)
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
	if ((ButtonHeader & FlagFrameOffset) != 0)
	{
		Ar << LatestFrame;
	}
	if ((ButtonHeader & FlagHasConsumedStick) != 0)
	{
		uint8 DeltaByte = 0;
		Ar << DeltaByte;
		if (DeltaByte > MaxEventAge)
		{
			Ar.SetError();
			return;
		}
		LastConsumedStickFrame = CurrentFrame - DeltaByte;
	}
	// Set first: the stored 16-bit frames are read back relative to it
	Current.Frame = LatestFrame;

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
		const int32 Slot = NumButtons - 1 - Age;
		ButtonFrames[Slot] = static_cast<uint16>(CurrentFrame - DeltaByte);
		ButtonBits[Slot] = Packed;
	}
	ButtonCount = NumButtons;
	ButtonHead = NumButtons & (MaxButtonEvents - 1);

	for (int32 Age = 0; Age < NumDirections; ++Age)
	{
		uint8 DeltaByte = 0;
		uint8 Bytes[3] = {};
		Ar << DeltaByte;
		Ar.Serialize(Bytes, sizeof(Bytes));
		FGSTN_StickSample Stick;
		if (!UnpackStickBytes(Bytes, Stick))
		{
			Ar.SetError();
			return;
		}
		const int32 Slot = NumDirections - 1 - Age;
		DirectionSticks[Slot] = PackStick(Stick);
		DirectionYaws[Slot] = Stick.WorldYaw;
		if (DeltaByte == AncientDelta)
		{
			DirectionSticks[Slot] = static_cast<uint16>(DirectionSticks[Slot] | PackedStickAncient);
			DirectionFrames[Slot] = 0;
		}
		else
		{
			DirectionFrames[Slot] = static_cast<uint16>(CurrentFrame - DeltaByte);
		}
	}
	DirectionCount = NumDirections;
	DirectionHead = NumDirections & (MaxDirectionEvents - 1);

	if (NumDirections > 0)
	{
		const FGSTN_DirectionEvent Newest = GetDirectionEvent(0);
		Current.Stick = Newest.Stick;
		// A flick lasts one frame: only the frame of the event carries it
		Current.Stick.bFlick = Newest.Stick.bFlick && Newest.Frame == LatestFrame;
	}
	Current.Stick.WorldYaw = WorldYaw;
}

bool FGSTN_InputHistory::operator==(const FGSTN_InputHistory& Other) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_InputHistory_Equals);
	if (HeldButtons != Other.HeldButtons || LastConsumedStickFrame != Other.LastConsumedStickFrame || ButtonCount != Other.ButtonCount || DirectionCount != Other.DirectionCount
		|| !(Current == Other.Current))
	{
		return false;
	}
	// Same newest frame, so the 16-bit frames compare like the full ones and the packed words like the unpacked events
	for (int32 Age = 0; Age < ButtonCount; ++Age)
	{
		const int32 Slot = GetButtonSlot(Age);
		const int32 OtherSlot = Other.GetButtonSlot(Age);
		if (ButtonFrames[Slot] != Other.ButtonFrames[OtherSlot] || ButtonBits[Slot] != Other.ButtonBits[OtherSlot])
		{
			return false;
		}
	}
	for (int32 Age = 0; Age < DirectionCount; ++Age)
	{
		const int32 Slot = GetDirectionSlot(Age);
		const int32 OtherSlot = Other.GetDirectionSlot(Age);
		if (DirectionFrames[Slot] != Other.DirectionFrames[OtherSlot] || DirectionSticks[Slot] != Other.DirectionSticks[OtherSlot]
			|| DirectionYaws[Slot] != Other.DirectionYaws[OtherSlot])
		{
			return false;
		}
	}
	return true;
}
