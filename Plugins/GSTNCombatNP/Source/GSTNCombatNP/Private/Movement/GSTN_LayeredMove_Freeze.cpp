#include "Movement/GSTN_LayeredMove_Freeze.h"

#include "AbilitySystemComponent.h"
#include "GSTN_CombatNPTags.h"
#include "MoverComponent.h"
#include "MoverTypes.h"

FGSTN_LayeredMove_Freeze::FGSTN_LayeredMove_Freeze()
{
	MixMode = EMoveMixMode::OverrideAll;
}

void FGSTN_LayeredMove_Freeze::Setup(const FVector& InIncomingVelocity, float InDurationMs, bool bInZeroVelocityOnFinish)
{
	IncomingVelocity = InIncomingVelocity;
	DurationMs = InDurationMs;
	bZeroVelocityOnFinish = bInZeroVelocityOnFinish;

	// The residual velocity is what Mover hands back to the movement mode on the frame the move ends
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::SetVelocity;
	FinishVelocitySettings.SetVelocity = bInZeroVelocityOnFinish ? FVector::ZeroVector : InIncomingVelocity;
}

bool FGSTN_LayeredMove_Freeze::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = EMoveMixMode::OverrideAll;
	OutProposedMove.LinearVelocity = DriftVelocity;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
	return true;
}

bool FGSTN_LayeredMove_Freeze::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	const FGameplayTag Tag = GSTNCombatNPTags::Mover_Frozen.GetTag();
	return bExactMatch ? Tag.MatchesTagExact(TagToFind) : Tag.MatchesTag(TagToFind);
}

void FGSTN_LayeredMove_Freeze::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(GSTNCombatNPTags::Mover_Frozen.GetTag());
}

FLayeredMoveBase* FGSTN_LayeredMove_Freeze::Clone() const
{
	return new FGSTN_LayeredMove_Freeze(*this);
}

void FGSTN_LayeredMove_Freeze::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << IncomingVelocity;
	uint8 bZero = bZeroVelocityOnFinish ? 1 : 0;
	Ar.SerializeBits(&bZero, 1);
	bZeroVelocityOnFinish = bZero != 0;
	uint8 bDrift = DriftVelocity.IsZero() ? 0 : 1;
	Ar.SerializeBits(&bDrift, 1);
	if (bDrift)
	{
		Ar << DriftVelocity;
	}
	else if (Ar.IsLoading())
	{
		DriftVelocity = FVector::ZeroVector;
	}
}

UScriptStruct* FGSTN_LayeredMove_Freeze::GetScriptStruct() const
{
	return FGSTN_LayeredMove_Freeze::StaticStruct();
}

FString FGSTN_LayeredMove_Freeze::ToSimpleString() const
{
	return TEXT("Freeze");
}

FEffectMoveContainer UGSTN_FreezeMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	// The spec's value is the window length; the creator's default is the fallback of an effect applied without one
	float Seconds = LayeredMoveDefaultParams.Duration;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		Seconds = EffectSpec.Data->GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FreezeSeconds, false, Seconds);
	}

	FGSTN_LayeredMove_Freeze Freeze;
	Freeze.Setup(TargetMoverComponent ? TargetMoverComponent->GetVelocity() : FVector::ZeroVector, Seconds * 1000.f, bZeroVelocityOnFinish);
	if (bDriftFromSetByCaller && EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		const FGameplayEffectSpec& Spec = *EffectSpec.Data.Get();
		Freeze.DriftVelocity = FVector(Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, false, 0.f), Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, false, 0.f));
		// Above the hitstop freeze it rides on (the higher override wins), and it leaves the velocity to that freeze
		Freeze.Priority = 1;
		Freeze.FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	}
	return FEffectMoveContainer(&Freeze);
}

UGSTN_GE_Freeze::UGSTN_GE_Freeze()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_FreezeMoveCreator* Creator = CreateDefaultSubobject<UGSTN_FreezeMoveCreator>(TEXT("FreezeMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}

UGSTN_GE_FreezeZeroVelocity::UGSTN_GE_FreezeZeroVelocity()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_FreezeMoveCreator* Creator = CreateDefaultSubobject<UGSTN_FreezeMoveCreator>(TEXT("FreezeZeroMoveCreator"));
	Creator->bZeroVelocityOnFinish = true;
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
