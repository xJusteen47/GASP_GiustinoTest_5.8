#include "Movement/GSTN_SetFacingEffect.h"

#include "Components/SceneComponent.h"
#include "GSTN_CombatNPTags.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SetFacingEffect)

FRotator FGSTN_SetFacingEffect::MakeOrientation(const FRotator& OrientationDeg, float YawDeg)
{
	return FRotator(OrientationDeg.Pitch, FRotator::NormalizeAxis(YawDeg), OrientationDeg.Roll);
}

bool FGSTN_SetFacingEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	FMoverDefaultSyncState* Sync = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!Sync)
	{
		return false;
	}
	const FRotator NewOrientation = MakeOrientation(Sync->GetOrientation_WorldSpace(), YawDeg);
	// The component turns too, like FTeleportEffect moves it. A walking step that neither moves nor turns from its start state skips the
	// component move and reads its final rotation back from the component, so a turn written only into the sync state was lost on a
	// frame at rest (the victim frozen in hitstop: react-hook-5, the turn showed only on the first frame the knockdown moved). A turn
	// about the up axis cannot collide with anything, so it is set without a sweep. On a resimulated frame the component is first put
	// back at the sync state of the frame before, and the same effect turns it the same way again
	if (USceneComponent* Updated = ApplyEffectParams.UpdatedComponent)
	{
		Updated->SetWorldRotation(NewOrientation, /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
	}
	// Everything but the yaw stays as the step found it, base included, so a character on a moving platform keeps riding it
	Sync->SetTransforms_WorldSpace(Sync->GetLocation_WorldSpace(), NewOrientation, Sync->GetVelocity_WorldSpace(),
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
}

UScriptStruct* FGSTN_SetFacingEffect::GetScriptStruct() const
{
	return FGSTN_SetFacingEffect::StaticStruct();
}

FString FGSTN_SetFacingEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("SetFacing %.1f"), YawDeg);
}

FEffectMoveContainer UGSTN_SetFacingMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_SetFacingEffect Effect;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		Effect.YawDeg = EffectSpec.Data->GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FacingYaw, false, 0.f);
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
