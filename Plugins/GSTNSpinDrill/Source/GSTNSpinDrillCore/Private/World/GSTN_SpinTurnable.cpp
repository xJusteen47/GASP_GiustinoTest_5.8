// Things in the world that a spin or a drill dive turns.

#include "World/GSTN_SpinTurnable.h"

void IGSTN_SpinTurnable::OnSpinTurned_Implementation(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source)
{
}

bool IGSTN_SpinTurnable::CanBeSpinTurned_Implementation(AActor* Turner) const
{
	return true;
}
