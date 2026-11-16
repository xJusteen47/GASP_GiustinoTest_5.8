#include "GSTN_PlatformPartGates.h"

uint16 FGSTN_PlatformPartGates::MakeGate(int32 PartIndex, TConstArrayView<int32> ParentIndices)
{
	uint16 Gate = 0;
	// At most 16 steps: a longer walk is a cycle, which the parts validation already reports
	for (int32 Step = 0; Step < 16 && PartIndex >= 0 && PartIndex < 16; ++Step)
	{
		const uint16 Bit = static_cast<uint16>(1u << PartIndex);
		if (Gate & Bit)
		{
			break;
		}
		Gate |= Bit;
		PartIndex = ParentIndices.IsValidIndex(PartIndex) ? ParentIndices[PartIndex] : INDEX_NONE;
	}
	return Gate;
}

uint8 FGSTN_PlatformPartGates::ComputeEnabledShapes(uint16 BrokenMask) const
{
	uint8 Enabled = 0xFF;
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		if (Gates[Id] & BrokenMask)
		{
			Enabled &= static_cast<uint8>(~(1u << Id));
		}
	}
	return Enabled;
}
