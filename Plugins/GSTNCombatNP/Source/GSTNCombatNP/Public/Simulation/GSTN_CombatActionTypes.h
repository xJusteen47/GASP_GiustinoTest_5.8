// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

/**
 * What the combatant is doing, as far as the combat model is concerned. The hit states are written by the victim side of M4 (H9):
 * Hitstun is the locked part of a stagger, Tumble a launch that cannot act until it lands, Splat the reaction to a wall impact
 */
enum class EGSTN_ActionState : uint8
{
	Actionable = 0,
	InTimeline = 1,
	Hitstun = 2,
	Tumble = 3,
	Splat = 4,

	Count UMETA(Hidden)
};

/** The combat clock (combat-m3 7.1): the simulation frame minus every hitstop frame so far, so it stands still in hitstop */
struct FGSTN_CombatClock
{
	/** INDEX_NONE before the first simulated frame (Frame < 0) */
	static int32 FromFrame(int32 Frame, int32 PausedFramesTotal)
	{
		return Frame < 0 ? INDEX_NONE : Frame - PausedFramesTotal;
	}

	/** Combat frames since StartClock, 0 for a start that is not valid or a clock that is not */
	static int32 Elapsed(int32 Clock, int32 StartClock)
	{
		return (Clock < 0 || StartClock < 0) ? 0 : FMath::Max(Clock - StartClock, 0);
	}
};

/** What the combat tick asks the ability system to do before it ticks, in the frame it asks (combat-m3 7.1) */
enum class EGSTN_RequestType : uint8
{
	None = 0,
	ActivateEntry = 1,
	CancelTimeline = 2,
	/** Arg = the registry index of the ability to cancel */
	CancelAbility = 3,
	CommitTimeline = 4,
	SwapWeaponStats = 5,
	SetFacing = 6,
	Cue = 7,

	// ---- The hit milestone (M4): what the victim side asks of the ability system when a hit resolves. Arg is an index the
	// executor resolves (an outcome slot, a table row), never a pointer or a tag
	ApplyDamage = 8,
	ApplyEffect = 9,
	Freeze = 10,
	Knockback = 11,
	PushBack = 12,
	MeterGain = 13,
	ImpactSurface = 14,

	Count UMETA(Hidden)
};

/** One request: what, in which simulation frame, and its argument (meaning depends on the type) */
struct FGSTN_CombatRequest
{
	EGSTN_RequestType Type = EGSTN_RequestType::None;
	int32 Frame = INDEX_NONE;
	int16 Arg = 0;

	bool IsSet() const { return Type != EGSTN_RequestType::None; }

	bool operator==(const FGSTN_CombatRequest& Other) const { return Type == Other.Type && Frame == Other.Frame && Arg == Other.Arg; }
	bool operator!=(const FGSTN_CombatRequest& Other) const { return !(*this == Other); }
};

/**
 * The requests of one frame, a fixed array so the state has a fixed shape. A request is only meaningful in the frame it was made
 * (the ability system executes it in that frame's pre-tick): the combat tick calls PruneBefore at the top of every frame, so a
 * state at the end of a frame holds that frame's requests and nothing older, and the wire never carries a dead one.
 *
 * Wire format: 3 bits count (0 to Capacity), then per request 4 bits type, the frame as a packed integer, 16 bits argument.
 */
struct FGSTN_CombatRequests
{
	static constexpr int32 Capacity = 4;

	FGSTN_CombatRequest Slots[Capacity];

	bool IsEmpty() const
	{
		for (const FGSTN_CombatRequest& Slot : Slots)
		{
			if (Slot.IsSet())
			{
				return false;
			}
		}
		return true;
	}

	int32 Num() const
	{
		int32 Count = 0;
		for (const FGSTN_CombatRequest& Slot : Slots)
		{
			Count += Slot.IsSet() ? 1 : 0;
		}
		return Count;
	}

	/** False when the array is full: the request is dropped (a full array is a bug in the caller, nothing makes four requests in one frame yet) */
	bool Add(EGSTN_RequestType Type, int32 Frame, int16 Arg = 0)
	{
		if (Type == EGSTN_RequestType::None)
		{
			return false;
		}
		for (FGSTN_CombatRequest& Slot : Slots)
		{
			if (!Slot.IsSet())
			{
				Slot.Type = Type;
				Slot.Frame = Frame;
				Slot.Arg = Arg;
				return true;
			}
		}
		return false;
	}

	/**
	 * The first request of the type made in Frame, or null. A frame can hold several of one type (a Commit for the timeline that
	 * was replaced and one for its replacement): a caller that cares which one asks with the argument
	 */
	const FGSTN_CombatRequest* Find(EGSTN_RequestType Type, int32 Frame) const
	{
		for (const FGSTN_CombatRequest& Slot : Slots)
		{
			if (Slot.Type == Type && Slot.Frame == Frame)
			{
				return &Slot;
			}
		}
		return nullptr;
	}

	/** The request of the type made in Frame with exactly this argument, or null */
	const FGSTN_CombatRequest* Find(EGSTN_RequestType Type, int32 Frame, int16 Arg) const
	{
		for (const FGSTN_CombatRequest& Slot : Slots)
		{
			if (Slot.Type == Type && Slot.Frame == Frame && Slot.Arg == Arg)
			{
				return &Slot;
			}
		}
		return nullptr;
	}

	/** Removes the request of the type, frame and argument and packs the rest to the front. @return true if one was removed */
	bool Remove(EGSTN_RequestType Type, int32 Frame, int16 Arg)
	{
		bool bRemoved = false;
		int32 Out = 0;
		for (int32 In = 0; In < Capacity; ++In)
		{
			if (!bRemoved && Slots[In].Type == Type && Slots[In].Frame == Frame && Slots[In].Arg == Arg)
			{
				bRemoved = true;
				continue;
			}
			Slots[Out++] = Slots[In];
		}
		for (; Out < Capacity; ++Out)
		{
			Slots[Out] = FGSTN_CombatRequest();
		}
		return bRemoved;
	}

	/** Drops every request made before Frame and packs the rest to the front, so equal sets are equal arrays */
	void PruneBefore(int32 Frame)
	{
		int32 Out = 0;
		for (int32 In = 0; In < Capacity; ++In)
		{
			if (Slots[In].IsSet() && Slots[In].Frame >= Frame)
			{
				Slots[Out++] = Slots[In];
			}
		}
		for (; Out < Capacity; ++Out)
		{
			Slots[Out] = FGSTN_CombatRequest();
		}
	}

	bool operator==(const FGSTN_CombatRequests& Other) const
	{
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			if (Slots[Index] != Other.Slots[Index])
			{
				return false;
			}
		}
		return true;
	}

	void NetSerialize(FArchive& Ar)
	{
		// On save the set slots go out in order; on load they come back packed at the front
		FGSTN_CombatRequest Local[Capacity];
		int32 LocalCount = 0;
		if (Ar.IsSaving())
		{
			for (const FGSTN_CombatRequest& Slot : Slots)
			{
				if (Slot.IsSet())
				{
					Local[LocalCount++] = Slot;
				}
			}
		}

		uint8 Count = static_cast<uint8>(LocalCount);
		Ar.SerializeBits(&Count, 3);
		if (Count > Capacity)
		{
			Ar.SetError();
			return;
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGSTN_CombatRequest& Request = Local[Index];
			uint8 Type = static_cast<uint8>(Request.Type);
			Ar.SerializeBits(&Type, 4);
			uint32 FramePlusOne = static_cast<uint32>(FMath::Max(Request.Frame + 1, 0));
			Ar.SerializeIntPacked(FramePlusOne);
			uint16 Arg = static_cast<uint16>(Request.Arg);
			Ar << Arg;
			if (Ar.IsLoading())
			{
				if (Type == 0 || Type >= static_cast<uint8>(EGSTN_RequestType::Count))
				{
					Ar.SetError();
					return;
				}
				Request.Type = static_cast<EGSTN_RequestType>(Type);
				Request.Frame = static_cast<int32>(FramePlusOne) - 1;
				Request.Arg = static_cast<int16>(Arg);
			}
		}

		if (Ar.IsLoading())
		{
			for (int32 Index = 0; Index < Capacity; ++Index)
			{
				Slots[Index] = Index < Count ? Local[Index] : FGSTN_CombatRequest();
			}
		}
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		for (const FGSTN_CombatRequest& Slot : Slots)
		{
			if (Slot.IsSet())
			{
				Out.Appendf(" Req(%d)@%d:%d", static_cast<int32>(Slot.Type), Slot.Frame, static_cast<int32>(Slot.Arg));
			}
		}
	}
};
