#include "Movement/GSTN_SetFacingEffect.h"

#include "Components/SceneComponent.h"
#include "GSTN_CombatNPTags.h"
#include "Movement/GSTN_GravityFrame.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SetFacingEffect)

FRotator FGSTN_SetFacingEffect::MakeOrientation(const FRotator& OrientationDeg, float YawDeg, const FVector& Up)
{
	// The yaw about the local up (gravity audit 2026-11-16); world up gives FRotator(Pitch, Yaw, Roll) as before
	return FGSTN_GravityFrame::WithYaw(OrientationDeg, YawDeg, Up);
}

FVector FGSTN_SetFacingEffect::TurnVelocity(const FVector& Velocity, float FromYawDeg, float ToYawDeg, const FVector& Up)
{
	const FVector Axis = Up.GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return Velocity;
	}
	const float Delta = FMath::FindDeltaAngleDegrees(FromYawDeg, ToYawDeg);
	return FQuat(Axis, FMath::DegreesToRadians(static_cast<double>(Delta))).RotateVector(Velocity);
}

bool FGSTN_SetFacingEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	FMoverDefaultSyncState* Sync = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!Sync)
	{
		return false;
	}
	const FVector Up = ApplyEffectParams.MoverComp ? ApplyEffectParams.MoverComp->GetUpDirection() : FVector::UpVector;
	const FRotator NewOrientation = MakeOrientation(Sync->GetOrientation_WorldSpace(), YawDeg, Up);
	// The component turns too, like FTeleportEffect moves it. A walking step that neither moves nor turns from its start state skips the
	// component move and reads its final rotation back from the component, so a turn written only into the sync state was lost on a
	// frame at rest (the victim frozen in hitstop: react-hook-5, the turn showed only on the first frame the knockdown moved). A turn
	// about the up axis cannot collide with anything, so it is set without a sweep. On a resimulated frame the component is first put
	// back at the sync state of the frame before, and the same effect turns it the same way again
	if (USceneComponent* Updated = ApplyEffectParams.UpdatedComponent)
	{
		Updated->SetWorldRotation(NewOrientation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
	// A reverse with TurnWithFacing momentum (owner 2026-11-16): the drift turns with the move, a 180 flips it. The turn is about the local up, so the
	// vertical speed (a jump, a fall) is untouched
	const FVector Velocity = bTurnVelocity
		? TurnVelocity(Sync->GetVelocity_WorldSpace(), FGSTN_GravityFrame::YawOfRotator(Sync->GetOrientation_WorldSpace(), Up), YawDeg, Up)
		: Sync->GetVelocity_WorldSpace();
	// Everything but the yaw (and, on a reverse, the velocity) stays as the step found it, base included, so a character on a moving platform keeps riding it
	Sync->SetTransforms_WorldSpace(Sync->GetLocation_WorldSpace(), NewOrientation, Velocity,
		Sync->GetAngularVelocityDegrees_WorldSpace(), Sync->GetMovementBase(), Sync->GetMovementBaseBoneName());
	// True = the sync state changed: Mover copies it into the state the movement mode starts from
	return true;
}

FInstantMovementEffect* FGSTN_SetFacingEffect::Clone() const
{
	return new FGSTN_SetFacingEffect(*this);
}

void FGSTN_SetFacingEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << YawDeg;
	uint8 TurnVelocityBit = bTurnVelocity ? 1 : 0;
	Ar << TurnVelocityBit;
	bTurnVelocity = TurnVelocityBit != 0;
}

UScriptStruct* FGSTN_SetFacingEffect::GetScriptStruct() const
{
	return FGSTN_SetFacingEffect::StaticStruct();
}

FString FGSTN_SetFacingEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("SetFacing %.1f%s"), YawDeg, bTurnVelocity ? TEXT(" +velocity") : TEXT(""));
}

FEffectMoveContainer UGSTN_SetFacingMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_SetFacingEffect Effect;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		Effect.YawDeg = EffectSpec.Data->GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FacingYaw, false, 0.f);
		Effect.bTurnVelocity = EffectSpec.Data->GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FacingTurnVelocity, false, 0.f) > 0.5f;
	}
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_SetFacing::UGSTN_GE_SetFacing()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_SetFacingMoveCreator* Creator = CreateDefaultSubobject<UGSTN_SetFacingMoveCreator>(TEXT("SetFacingMoveCreator"));
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
