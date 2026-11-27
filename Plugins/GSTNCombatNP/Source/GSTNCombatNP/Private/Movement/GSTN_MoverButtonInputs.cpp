// Combat simulation (GAS-NPP fork): logical buttons Mover modes need, carried in the Mover command.

#include "Movement/GSTN_MoverButtonInputs.h"

bool FGSTN_MoverButtonInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);
	uint8 Bits = Ar.IsLoading() ? 0 : (Flags & 0xF);
	Ar.SerializeBits(&Bits, 4);
	Flags = Bits & 0xF;
	bOutSuccess = true;
	return true;
}

void FGSTN_MoverButtonInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("Buttons: jump %d/%d detach %d forced %d\n", Has(JumpPressed) ? 1 : 0, Has(JumpHeld) ? 1 : 0, Has(DetachHeld) ? 1 : 0, Has(DetachForced) ? 1 : 0);
}
