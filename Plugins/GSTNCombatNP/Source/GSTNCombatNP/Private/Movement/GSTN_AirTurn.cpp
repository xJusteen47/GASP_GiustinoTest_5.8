// Combat simulation (GAS-NPP fork): the air turn at the kit profile's rate (polish M3, qa-movement-1117 defect 3).

#include "Movement/GSTN_AirTurn.h"

#include "Engine/NetSerialization.h"
#include "Movement/GSTN_AnalogWalkTypes.h"
#include "Movement/GSTN_GravityFrame.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AirTurn)

bool FGSTN_AirTurnInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	uint16 Rate = static_cast<uint16>(FGSTN_AirTurn::QuantizeRate(RateDegPerSec));
	Ar << Rate;
	RateDegPerSec = static_cast<float>(Rate);

	bOutSuccess = true;
	return true;
}

void FGSTN_AirTurnInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("AirTurnRate: %.0f\n", RateDegPerSec);
}

float FGSTN_AirTurn::QuantizeRate(float RateDegPerSec)
{
	return static_cast<float>(FMath::RoundToInt32(FMath::Clamp(RateDegPerSec, 0.f, MaxRateDegPerSec)));
}

bool FGSTN_AirTurn::Apply(const FMoverTickStartData& Start, bool bFallingMode, const FVector& Up, float DeltaSeconds, FProposedMove& InOutMove)
{
	if (!bFallingMode || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// A layered move that overrides the velocity owns the rotation too (Mover's mixer copies its angular velocity)
	if (InOutMove.MixMode == EMoveMixMode::OverrideVelocity || InOutMove.MixMode == EMoveMixMode::OverrideAll
		|| InOutMove.MixMode == EMoveMixMode::OverrideAllExceptVerticalVelocity)
	{
		return false;
	}
	const FGSTN_AirTurnInputs* AirTurn = Start.InputCmd.InputCollection.FindDataByType<FGSTN_AirTurnInputs>();
	const FCharacterDefaultInputs* Inputs = Start.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* Sync = Start.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!AirTurn || AirTurn->RateDegPerSec <= 0.f || !Inputs || Inputs->OrientationIntent.IsNearlyZero() || !Sync)
	{
		return false;
	}
	const FVector SafeUp = Up.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Up.GetSafeNormal();
	const FVector IntentDir = FVector::VectorPlaneProject(Inputs->GetOrientationIntentDir_WorldSpace(), SafeUp);
	if (IntentDir.IsNearlyZero())
	{
		return false;
	}
	// Yaws about the local up (gravity audit 2026-11-16): world up is the old yaw maths bit for bit
	const float CurrentYaw = FGSTN_GravityFrame::YawOfRotator(Sync->GetOrientation_WorldSpace(), SafeUp);
	const float TargetYaw = FGSTN_GravityFrame::YawOfRotator(FRotationMatrix::MakeFromXZ(IntentDir, SafeUp).Rotator(), SafeUp);
	const float YawRate = FGSTN_AnalogWalkLogic::AirTurnYawRate(AirTurn->RateDegPerSec, CurrentYaw, TargetYaw, DeltaSeconds);
	InOutMove.AngularVelocityDegrees = SafeUp * YawRate;
	return true;
}
