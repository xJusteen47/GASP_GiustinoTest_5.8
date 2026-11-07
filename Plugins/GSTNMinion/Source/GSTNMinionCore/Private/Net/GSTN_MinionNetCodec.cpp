// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Net/GSTN_MinionNetCodec.h"

namespace GSTNMinionQuant
{
	uint32 QuantizePosition(float Value, float Origin)
	{
		// Clamp in float first: rounding a float beyond the int range is undefined
		const float Offset = FMath::Clamp(Value - Origin, -static_cast<float>(PositionBias), static_cast<float>(PositionBias - 1));
		return static_cast<uint32>(FMath::Clamp(FMath::RoundToInt(Offset) + PositionBias, 0, PositionMaxValue));
	}

	float DequantizePosition(uint32 Quantized, float Origin)
	{
		return static_cast<float>(static_cast<int32>(Quantized) - PositionBias) + Origin;
	}

	uint32 QuantizeYaw(float Degrees)
	{
		float Wrapped = FMath::Fmod(Degrees, 360.f);
		if (Wrapped < 0.f)
		{
			Wrapped += 360.f;
		}
		// 255.6 rounds to 256, which is step 0 again
		return static_cast<uint32>(FMath::RoundToInt(Wrapped * (256.f / 360.f))) & 0xFFu;
	}

	float DequantizeYaw(uint32 Quantized)
	{
		return static_cast<float>(Quantized & 0xFFu) * (360.f / 256.f);
	}

	uint32 QuantizeHealth(float Fraction)
	{
		if (!(Fraction > 0.f))
		{
			return 0;
		}
		// Round up so a minion with any health left never reads as 0
		return static_cast<uint32>(FMath::Clamp(FMath::CeilToInt(Fraction * 31.f), 1, 31));
	}

	float DequantizeHealth(uint32 Quantized)
	{
		return static_cast<float>(FMath::Min(Quantized, 31u)) / 31.f;
	}

	uint32 QuantizeSpeed(float Speed, float MaxSpeed)
	{
		if (!(MaxSpeed > 0.f) || !(Speed > 0.f))
		{
			return 0;
		}
		return static_cast<uint32>(FMath::Clamp(FMath::RoundToInt(Speed / MaxSpeed * 63.f), 0, 63));
	}

	float DequantizeSpeed(uint32 Quantized, float MaxSpeed)
	{
		return MaxSpeed > 0.f ? static_cast<float>(FMath::Min(Quantized, 63u)) / 63.f * MaxSpeed : 0.f;
	}
}

namespace GSTNMinionFrame
{
	namespace
	{
		uint32 MaskOf(int32 Bits)
		{
			return Bits >= 32 ? MAX_uint32 : ((1u << Bits) - 1u);
		}
	}

	uint32 Truncate(int32 Frame, int32 Bits)
	{
		return static_cast<uint32>(Frame) & MaskOf(Bits);
	}

	int32 ReconstructNearest(int32 ReferenceFrame, uint32 Low, int32 Bits)
	{
		const uint32 Mask = MaskOf(Bits);
		const int32 Half = static_cast<int32>((Mask >> 1) + 1u);
		int32 Delta = static_cast<int32>((Low - static_cast<uint32>(ReferenceFrame)) & Mask);
		if (Delta >= Half)
		{
			Delta -= static_cast<int32>(Mask + 1u);
		}
		return ReferenceFrame + Delta;
	}

	int32 ReconstructPast(int32 ReferenceFrame, uint32 Low, int32 Bits)
	{
		const uint32 Mask = MaskOf(Bits);
		const int32 Back = static_cast<int32>((static_cast<uint32>(ReferenceFrame) - Low) & Mask);
		return ReferenceFrame - Back;
	}
}

bool FGSTN_MinionNetWireItem::operator==(const FGSTN_MinionNetWireItem& Other) const
{
	return Id == Other.Id && bHasType == Other.bHasType && TypeIndex == Other.TypeIndex
		&& bHasGlobalId == Other.bHasGlobalId && GlobalId == Other.GlobalId
		&& PositionQ[0] == Other.PositionQ[0] && PositionQ[1] == Other.PositionQ[1] && PositionQ[2] == Other.PositionQ[2]
		&& YawQ == Other.YawQ && ActionState == Other.ActionState && MoveId == Other.MoveId
		&& MoveStartLow == Other.MoveStartLow && FrameStampLow == Other.FrameStampLow && HitCounter == Other.HitCounter
		&& HealthQ == Other.HealthQ && ExtByte == Other.ExtByte && SpeedQ == Other.SpeedQ && Flags == Other.Flags;
}

void FGSTN_MinionNetCodec::Quantize(const FGSTN_MinionNetState& State, const FGSTN_MinionNetQuantization& Quant, FGSTN_MinionNetWireItem& Item)
{
	Item.PositionQ[0] = GSTNMinionQuant::QuantizePosition(State.Position.X, Quant.WorldOrigin.X);
	Item.PositionQ[1] = GSTNMinionQuant::QuantizePosition(State.Position.Y, Quant.WorldOrigin.Y);
	Item.PositionQ[2] = GSTNMinionQuant::QuantizePosition(State.Position.Z, Quant.WorldOrigin.Z);
	Item.YawQ = static_cast<uint8>(GSTNMinionQuant::QuantizeYaw(State.YawDegrees));
	Item.ActionState = static_cast<uint8>(State.ActionState & 0xF);
	Item.MoveId = static_cast<uint8>(State.MoveId & 0x3F);
	Item.MoveStartLow = static_cast<uint16>(State.MoveId == 0 ? 0u : GSTNMinionFrame::Truncate(State.MoveStartFrame, GSTNMinion::NetBits::MoveStartFrame));
	Item.FrameStampLow = static_cast<uint8>(GSTNMinionFrame::Truncate(State.FrameStamp, GSTNMinion::NetBits::FrameStamp));
	Item.HitCounter = static_cast<uint8>(State.HitCounter & 0x7);
	Item.HealthQ = static_cast<uint8>(GSTNMinionQuant::QuantizeHealth(State.HealthFraction));
	Item.SpeedQ = static_cast<uint8>(GSTNMinionQuant::QuantizeSpeed(State.Speed, Quant.MaxSpeed));
	Item.Flags = static_cast<uint8>(State.Flags & 0x7);
}

FGSTN_MinionNetState FGSTN_MinionNetCodec::Dequantize(const FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetQuantization& Quant, int32 ReferenceFrame)
{
	FGSTN_MinionNetState State;
	State.Position = FVector3f(
		GSTNMinionQuant::DequantizePosition(Item.PositionQ[0], Quant.WorldOrigin.X),
		GSTNMinionQuant::DequantizePosition(Item.PositionQ[1], Quant.WorldOrigin.Y),
		GSTNMinionQuant::DequantizePosition(Item.PositionQ[2], Quant.WorldOrigin.Z));
	State.YawDegrees = GSTNMinionQuant::DequantizeYaw(Item.YawQ);
	State.ActionState = Item.ActionState;
	State.MoveId = Item.MoveId;
	State.FrameStamp = GSTNMinionFrame::ReconstructNearest(ReferenceFrame, Item.FrameStampLow, GSTNMinion::NetBits::FrameStamp);
	State.MoveStartFrame = Item.MoveId == 0
		? INDEX_NONE
		: GSTNMinionFrame::ReconstructPast(State.FrameStamp, Item.MoveStartLow, GSTNMinion::NetBits::MoveStartFrame);
	State.HitCounter = Item.HitCounter;
	State.HealthFraction = GSTNMinionQuant::DequantizeHealth(Item.HealthQ);
	State.Speed = GSTNMinionQuant::DequantizeSpeed(Item.SpeedQ, Quant.MaxSpeed);
	State.Flags = Item.Flags;
	return State;
}

namespace
{
	/** Serializes the low NumBits bits of Value; a loading archive starts from zero so the unread high bits are clean */
	void SerializeField(FArchive& Ar, uint32& Value, int32 NumBits)
	{
		if (Ar.IsLoading())
		{
			Value = 0;
		}
		Ar.SerializeBits(&Value, NumBits);
	}

	template <typename T>
	void SerializeNarrow(FArchive& Ar, T& Field, int32 NumBits)
	{
		uint32 Value = static_cast<uint32>(Field);
		SerializeField(Ar, Value, NumBits);
		if (Ar.IsLoading())
		{
			Field = static_cast<T>(Value);
		}
	}

	void SerializeFlag(FArchive& Ar, bool& bFlag)
	{
		uint32 Value = bFlag ? 1u : 0u;
		SerializeField(Ar, Value, 1);
		if (Ar.IsLoading())
		{
			bFlag = Value != 0;
		}
	}
}

bool FGSTN_MinionNetCodec::Serialize(FArchive& Ar, FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetFormat& Format)
{
	namespace Bits = GSTNMinion::NetBits;

	SerializeNarrow(Ar, Item.Id, Bits::Id);

	SerializeFlag(Ar, Item.bHasType);
	if (Item.bHasType)
	{
		SerializeNarrow(Ar, Item.TypeIndex, Bits::TypeIndex);
	}
	else if (Ar.IsLoading())
	{
		Item.TypeIndex = 0;
	}

	if (Format.bIdFieldActive)
	{
		SerializeFlag(Ar, Item.bHasGlobalId);
		if (Item.bHasGlobalId)
		{
			SerializeNarrow(Ar, Item.GlobalId, Bits::GlobalId);
		}
		else if (Ar.IsLoading())
		{
			Item.GlobalId = 0;
		}
	}
	else if (Ar.IsLoading())
	{
		Item.bHasGlobalId = false;
		Item.GlobalId = 0;
	}

	for (uint32& Axis : Item.PositionQ)
	{
		SerializeField(Ar, Axis, Bits::Position);
	}
	SerializeNarrow(Ar, Item.YawQ, Bits::Yaw);
	SerializeNarrow(Ar, Item.ActionState, Bits::ActionState);
	SerializeNarrow(Ar, Item.MoveId, Bits::MoveId);
	SerializeNarrow(Ar, Item.MoveStartLow, Bits::MoveStartFrame);
	SerializeNarrow(Ar, Item.FrameStampLow, Bits::FrameStamp);
	SerializeNarrow(Ar, Item.HitCounter, Bits::HitCounter);
	SerializeNarrow(Ar, Item.HealthQ, Bits::Health);

	if (Format.HasExtFor(Item.TypeIndex))
	{
		SerializeNarrow(Ar, Item.ExtByte, Bits::ExtByte);
	}
	else if (Ar.IsLoading())
	{
		Item.ExtByte = 0;
	}

	SerializeNarrow(Ar, Item.SpeedQ, Bits::Speed);
	SerializeNarrow(Ar, Item.Flags, Bits::Flags);
	return !Ar.IsError();
}

int32 FGSTN_MinionNetCodec::GetBitSize(const FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetFormat& Format)
{
	namespace Bits = GSTNMinion::NetBits;

	int32 Total = Bits::Id + 1;
	Total += Item.bHasType ? Bits::TypeIndex : 0;
	if (Format.bIdFieldActive)
	{
		Total += 1 + (Item.bHasGlobalId ? Bits::GlobalId : 0);
	}
	Total += 3 * Bits::Position + Bits::Yaw + Bits::ActionState + Bits::MoveId + Bits::MoveStartFrame + Bits::FrameStamp
		+ Bits::HitCounter + Bits::Health + Bits::Speed + Bits::Flags;
	Total += Format.HasExtFor(Item.TypeIndex) ? Bits::ExtByte : 0;
	return Total;
}
