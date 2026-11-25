// Combat simulation (GAS-NPP fork): logical buttons Mover modes need, carried in the Mover command.

#include "Movement/GSTN_MoverButtonInputs.h"

bool FGSTN_MoverButtonInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);
	uint8 Bits = Ar.IsLoading() ? 0 : (Flags & 0x7);
	Ar.SerializeBits(&Bits, 3);
	Flags = Bits & 0x7;
	bOutSuccess = true;
	return true;
}

void FGSTN_MoverButtonInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("Buttons: jump %d/%d detach %d\n", Has(JumpPressed) ? 1 : 0, Has(JumpHeld) ? 1 : 0, Has(DetachHeld) ? 1 : 0);
}
