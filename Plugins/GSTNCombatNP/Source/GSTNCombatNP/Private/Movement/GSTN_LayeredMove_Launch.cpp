#include "Movement/GSTN_LayeredMove_Launch.h"

#include "AbilitySystemComponent.h"
#include "GSTN_CombatNPTags.h"

FGSTN_LayeredMove_Launch::FGSTN_LayeredMove_Launch()
{
	ForceMovementMode = TEXT("Falling");
}

FLayeredMoveBase* FGSTN_LayeredMove_Launch::Clone() const
{
	return new FGSTN_LayeredMove_Launch(*this);
}

void FGSTN_LayeredMove_Launch::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar.SerializeBits(&bResetAirJumps, 1);
	Ar.SerializeBits(&bResetCoyote, 1);
}

UScriptStruct* FGSTN_LayeredMove_Launch::GetScriptStruct() const
{
	return FGSTN_LayeredMove_Launch::StaticStruct();
}

FString FGSTN_LayeredMove_Launch::ToSimpleString() const
{
	return TEXT("SurfaceLaunch");
}

FEffectMoveContainer UGSTN_SurfaceLaunchMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_LayeredMove_Launch Move;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		const FGameplayEffectSpec& Spec = *EffectSpec.Data;
		Move.LaunchVelocity = FVector(Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, false, 0.f));
		Move.bResetAirJumps = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_LaunchResetAirJumps, false, 1.f) > 0.5f;
		Move.bResetCoyote = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_LaunchResetCoyote, false, 0.f) > 0.5f;
	}
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_SurfaceLaunch::UGSTN_GE_SurfaceLaunch()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_SurfaceLaunchMoveCreator* Creator = CreateDefaultSubobject<UGSTN_SurfaceLaunchMoveCreator>(TEXT("SurfaceLaunchMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
