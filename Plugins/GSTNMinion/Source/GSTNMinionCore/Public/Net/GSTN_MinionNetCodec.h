// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"

/**
 * The wire layout of one minion item and the frame-stamp helpers (combat-m8-minions.md 8.2). Pure: no replication class, no
 * FastArray, no actor. The Runtime module's FFastArraySerializerItem forwards its NetSerialize to FGSTN_MinionNetCodec so the
 * layout is tested here without a world.
 *
 * Quantizer note: the shared quantizer of GSTNCore is not written yet (GSTNCore holds only the pose history), so the few helpers
 * this layout needs live in GSTNMinionQuant below. When a second user appears they move to GSTNCore/Net with a redirect.
 */
namespace GSTNMinion
{
	/** Bits bit 0..2 of FGSTN_MinionNetWireItem::Flags */
	namespace NetFlags
	{
		/** No interpolation across this sample (spawn, respawn, teleport) */
		constexpr uint8 Teleport = 1 << 0;
		/** The minion is being promoted to an elite or boss */
		constexpr uint8 Promoting = 1 << 1;
		constexpr uint8 Elite = 1 << 2;
	}

	/** Field widths of the item, in bits (8.2). The sum for the smallest item is checked by a test */
	namespace NetBits
	{
		constexpr int32 Id = 8;
		constexpr int32 TypeIndex = 8;
		constexpr int32 GlobalId = 8;
		constexpr int32 Position = 19;
		constexpr int32 Yaw = 8;
		constexpr int32 ActionState = 4;
		constexpr int32 MoveId = 6;
		constexpr int32 MoveStartFrame = 10;
		constexpr int32 FrameStamp = 8;
		constexpr int32 HitCounter = 3;
		constexpr int32 Health = 5;
		constexpr int32 ExtByte = 8;
		constexpr int32 Speed = 6;
		constexpr int32 Flags = 3;
	}
}

/**
 * Small quantization helpers. Position uses 1 cm steps, so the rounding error is at most 0.5 cm per axis; 19 bits give
 * 524288 steps, +-2621 m around the origin. Yaw uses 256 steps of 1.40625 degrees, so the error is at most 0.703 degrees.
 */
namespace GSTNMinionQuant
{
	/** Largest position error per axis: half a step */
	constexpr float PositionMaxError = 0.5f;
	/** Largest yaw error in degrees: half a step */
	constexpr float YawMaxError = 360.f / 256.f * 0.5f;

	/** Half the position range in steps: the stored value is (Offset cm) + this, so it is never negative */
	constexpr int32 PositionBias = 1 << (GSTNMinion::NetBits::Position - 1);
	constexpr int32 PositionMaxValue = (1 << GSTNMinion::NetBits::Position) - 1;

	/** Offset from the origin in cm, rounded to the nearest step and clamped to the 19-bit range */
	GSTNMINIONCORE_API uint32 QuantizePosition(float Value, float Origin);
	GSTNMINIONCORE_API float DequantizePosition(uint32 Quantized, float Origin);

	/** Any yaw in degrees (wrapped into [0, 360)) to a step of 1.40625 degrees */
	GSTNMINIONCORE_API uint32 QuantizeYaw(float Degrees);
	/** The yaw of a step in [0, 360) degrees */
	GSTNMINIONCORE_API float DequantizeYaw(uint32 Quantized);

	/** Health fraction in [0, 1] to 5 bits: 0 only when the fraction is 0, so a living minion never reads as dead; 31 = full */
	GSTNMINIONCORE_API uint32 QuantizeHealth(float Fraction);
	GSTNMINIONCORE_API float DequantizeHealth(uint32 Quantized);

	/** Speed in [0, MaxSpeed] cm/s to 6 bits (63 = MaxSpeed) */
	GSTNMINIONCORE_API uint32 QuantizeSpeed(float Speed, float MaxSpeed);
	GSTNMINIONCORE_API float DequantizeSpeed(uint32 Quantized, float MaxSpeed);
}

/**
 * Frame stamps on the wire carry only the low bits of a server frame (8 bits: wrap 4.1 s at 62.5 Hz; 10 bits: wrap 16.4 s).
 * The receiver rebuilds the full frame from a reference frame it already knows (its estimate of the server frame). The result
 * is exact as long as the real frame is within half a wrap of the reference (nearest) or within one wrap before it (past).
 */
namespace GSTNMinionFrame
{
	/** The low Bits bits of a frame (negative frames wrap like two's complement) */
	GSTNMINIONCORE_API uint32 Truncate(int32 Frame, int32 Bits);

	/** The frame closest to ReferenceFrame whose low Bits bits are Low: within [Ref - 2^(Bits-1), Ref + 2^(Bits-1) - 1] */
	GSTNMINIONCORE_API int32 ReconstructNearest(int32 ReferenceFrame, uint32 Low, int32 Bits);

	/** The latest frame at or before ReferenceFrame whose low bits are Low, for stamps that cannot be in the future (a move start) */
	GSTNMINIONCORE_API int32 ReconstructPast(int32 ReferenceFrame, uint32 Low, int32 Bits);
}

/**
 * What a sample of a minion means, before quantization: world position, yaw in degrees, the state and frames. This is what the
 * server fills from the store and the client's interpolator reads after Dequantize.
 */
struct GSTNMINIONCORE_API FGSTN_MinionNetState
{
	FVector3f Position = FVector3f::ZeroVector;
	float YawDegrees = 0.f;
	uint8 ActionState = 0;
	/** Index in the type's move and reaction list, 0 = none */
	uint8 MoveId = 0;
	/** Full server frame of the move start; INDEX_NONE when MoveId is 0 */
	int32 MoveStartFrame = INDEX_NONE;
	/** Full server frame of this sample */
	int32 FrameStamp = 0;
	uint8 HitCounter = 0;
	/** Health / MaxHealth in [0, 1] */
	float HealthFraction = 1.f;
	float Speed = 0.f;
	uint8 Flags = 0;
};

/** The numbers the quantizer needs and the wire does not carry: the registry's world origin and the type's top speed */
struct GSTNMINIONCORE_API FGSTN_MinionNetQuantization
{
	/** Registry WorldOrigin (8.2): positions are sent relative to it */
	FVector3f WorldOrigin = FVector3f::ZeroVector;

	/** Speed that the 6-bit field maps to 63 (the type's RunSpeed) */
	float MaxSpeed = 600.f;
};

/**
 * Which optional fields are on the wire. Known to both sides without being sent: the id field is active while the re-pack
 * fallback of 8.1 is, and the extension byte is a property of the minion type (a bridge plugin owns its meaning).
 */
struct GSTNMINIONCORE_API FGSTN_MinionNetFormat
{
	/** True while GlobalId may be sent (adds the one-bit bHasGlobalId flag); false = no flag bit at all */
	bool bIdFieldActive = false;

	/** True for a type with an extension byte: ExtByte is then always on the wire (8 bits) */
	bool bTypeHasExt = false;

	/**
	 * Mixed arrays: a pack holds items of several types, and the type is only known once the item's TypeIndex has been read. When set,
	 * bit TypeIndex says whether that type has the extension byte (the table of the project registry, same on every machine). Null =
	 * only bTypeHasExt decides. Must outlive the Serialize / GetBitSize call
	 */
	const TBitArray<>* ExtTypes = nullptr;

	/** Whether an item of this type carries the extension byte under this format */
	bool HasExtFor(uint8 TypeIndex) const
	{
		return bTypeHasExt || (ExtTypes && ExtTypes->IsValidIndex(TypeIndex) && (*ExtTypes)[TypeIndex]);
	}
};

/**
 * One item as it travels: every field already quantized to its wire width, so two items are equal exactly when their bytes are.
 * Id is pack-local; the global handle is PackId << 4 | Id (8.2). TypeIndex is sent on add only (bHasType), GlobalId on add only
 * while the format allows it (bHasGlobalId).
 */
struct GSTNMINIONCORE_API FGSTN_MinionNetWireItem
{
	uint8 Id = 0;
	bool bHasType = false;
	uint8 TypeIndex = 0;
	bool bHasGlobalId = false;
	uint8 GlobalId = 0;
	uint32 PositionQ[3] = { 0, 0, 0 };
	uint8 YawQ = 0;
	uint8 ActionState = 0;
	uint8 MoveId = 0;
	/** Low 10 bits of the move start frame */
	uint16 MoveStartLow = 0;
	/** Low 8 bits of the sample frame */
	uint8 FrameStampLow = 0;
	uint8 HitCounter = 0;
	uint8 HealthQ = 0;
	uint8 ExtByte = 0;
	uint8 SpeedQ = 0;
	uint8 Flags = 0;

	bool operator==(const FGSTN_MinionNetWireItem& Other) const;
	bool operator!=(const FGSTN_MinionNetWireItem& Other) const { return !(*this == Other); }
};

/**
 * Packs, unpacks and (de)serializes items. Stateless. Quantize and Dequantize are the lossy step (bounded by GSTNMinionQuant);
 * Serialize is lossless: what Serialize writes, a Serialize on a loading archive reads back bit for bit.
 */
struct GSTNMINIONCORE_API FGSTN_MinionNetCodec
{
	/** Fills the quantized fields of Item from State (Id, type, global id and ext byte are the caller's and stay as they are) */
	static void Quantize(const FGSTN_MinionNetState& State, const FGSTN_MinionNetQuantization& Quant, FGSTN_MinionNetWireItem& Item);

	/**
	 * Rebuilds a state. ReferenceFrame is the receiver's estimate of the server frame: it rebuilds FrameStamp (nearest, 8 bits) and
	 * then MoveStartFrame (latest at or before the stamp, 10 bits; INDEX_NONE when MoveId is 0)
	 */
	static FGSTN_MinionNetState Dequantize(const FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetQuantization& Quant, int32 ReferenceFrame);

	/**
	 * Writes the item to a saving archive or reads it from a loading one, field by field in the order of 8.2, at the widths of
	 * GSTNMinion::NetBits. Use FBitWriter and FBitReader for a packed stream. Returns false if the archive errored
	 */
	static bool Serialize(FArchive& Ar, FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetFormat& Format);

	/** Bits Serialize writes for this item under this format */
	static int32 GetBitSize(const FGSTN_MinionNetWireItem& Item, const FGSTN_MinionNetFormat& Format);
};
