// Combat simulation (GAS-NPP fork).

#include "Movement/GSTN_MovementMixer.h"

#include "MoverComponent.h"
#include "MoveLibrary/MovementUtilsTypes.h"

void UGSTN_MovementMixer::MixProposedMoves(const FProposedMove& MoveToMix, FVector UpDirection, FProposedMove& OutCumulativeMove)
{
	Super::MixProposedMoves(MoveToMix, UpDirection, OutCumulativeMove);
	// OverrideAll and OverrideAllExceptVerticalVelocity already copy the whole move, mix mode included; only this one is lost
	if (MoveToMix.MixMode == EMoveMixMode::OverrideVelocity && OutCumulativeMove.MixMode == EMoveMixMode::AdditiveVelocity)
	{
		OutCumulativeMove.MixMode = EMoveMixMode::OverrideVelocity;
	}
}

bool UGSTN_MovementMixer::Install(UMoverComponent& MoverComp)
{
	UMovementMixer* Current = MoverComp.MovementMixer;
	if (Current && Current->IsA<UGSTN_MovementMixer>())
	{
		return true;
	}
	// The stock mixer has no state that outlives a substep (ResetMixerState runs before every mix), so replacing it loses nothing
	if (!Current || Current->GetClass() == UMovementMixer::StaticClass())
	{
		MoverComp.MovementMixer = NewObject<UGSTN_MovementMixer>(&MoverComp, TEXT("GSTN_MovementMixer"));
		return true;
	}
	return false;
}
