// Pure combat logic (no GAS, no Network Prediction).

#include "Jump/GSTN_JumpTypes.h"

void FGSTN_JumpState::NetSerialize(FArchive& Ar)
{
	uint8 bPresent = IsDefault() ? 0 : 1;
	Ar.SerializeBits(&bPresent, 1);
	if (!bPresent)
	{
		if (Ar.IsLoading())
		{
			*this = FGSTN_JumpState();
		}
		return;
	}

	uint8 PhaseBits = static_cast<uint8>(Phase);
	Ar.SerializeBits(&PhaseBits, 2);
	uint8 Flags = (bJumpCut ? 1 : 0) | (bFastFalling ? 2 : 0) | (bTakeoffWasAir ? 4 : 0);
	Ar.SerializeBits(&Flags, 3);
	// +1 so that INDEX_NONE (-1) is 0 and the packed form stays small
	uint32 StartPlusOne = static_cast<uint32>(FMath::Max(StartFrame + 1, 0));
	Ar.SerializeIntPacked(StartPlusOne);
	uint8 ButtonPlusOne = static_cast<uint8>(FMath::Clamp(static_cast<int32>(Button) + 1, 0, 255));
	Ar << ButtonPlusOne;
	Ar << AirJumpsUsed;
	Ar << CoyoteFramesLeft;

	if (Ar.IsLoading())
	{
		Phase = static_cast<EGSTN_JumpPhase>(PhaseBits);
		bJumpCut = (Flags & 1) != 0;
		bFastFalling = (Flags & 2) != 0;
		bTakeoffWasAir = (Flags & 4) != 0;
		StartFrame = static_cast<int32>(StartPlusOne) - 1;
		Button = static_cast<int8>(static_cast<int32>(ButtonPlusOne) - 1);
	}
}

void FGSTN_JumpState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf(" Jump: %d@%d btn %d cut %d fast %d air %d used %d coyote %d", static_cast<int32>(Phase), StartFrame, static_cast<int32>(Button), bJumpCut, bFastFalling, bTakeoffWasAir, AirJumpsUsed, CoyoteFramesLeft);
}
