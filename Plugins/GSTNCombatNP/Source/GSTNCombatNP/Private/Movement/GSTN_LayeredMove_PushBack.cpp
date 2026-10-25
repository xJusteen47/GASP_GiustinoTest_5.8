#include "Movement/GSTN_LayeredMove_PushBack.h"

#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GSTN_CombatNPTags.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"

FGSTN_LayeredMove_PushBack::FGSTN_LayeredMove_PushBack()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	DurationMs = 0.f;
}

bool FGSTN_LayeredMove_PushBack::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const double StepMs = FMath::Max<double>(TimeStep.StepMs, UE_SMALL_NUMBER);
	ElapsedFrames = FMath::Max(FMath::RoundToInt((TimeStep.BaseSimTimeMs - StartSimTimeMs) / StepMs), 0);

	OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
	const double Remaining = 1.0 - static_cast<double>(ElapsedFrames) * DecayPerFrame;
	if (Remaining <= 0.0)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		return true;
	}

	FVector Proposed = Velocity * Remaining;
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UPrimitiveComponent* Primitive = MoverComp ? Cast<UPrimitiveComponent>(MoverComp->GetUpdatedComponent()) : nullptr;
	if (Sync && Primitive && Primitive->GetWorld())
	{
		// The next step's feet position must have floor under it, or the victim stops on the edge
		const FVector Up = MoverComp->GetUpDirection();
		const double HalfHeight = Primitive->Bounds.BoxExtent.GetMax();
		const FVector Next = Sync->GetLocation_WorldSpace() + Proposed * (StepMs * 0.001);
		const FVector Start = Next - Up * (HalfHeight - 10.0);
		const FVector End = Next - Up * (HalfHeight + LedgeProbeDepth);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNPushBackLedge), false, Primitive->GetOwner());
		const ECollisionChannel Channel = ECC_Visibility;
		if (!Primitive->GetWorld()->LineTraceTestByChannel(Start, End, Channel, Params))
		{
			Proposed = FVector::ZeroVector;
		}
	}
	OutProposedMove.LinearVelocity = Proposed;
	return true;
}

FLayeredMoveBase* FGSTN_LayeredMove_PushBack::Clone() const
{
	return new FGSTN_LayeredMove_PushBack(*this);
}

void FGSTN_LayeredMove_PushBack::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	Ar << DecayPerFrame;
	Ar << ElapsedFrames;
}

UScriptStruct* FGSTN_LayeredMove_PushBack::GetScriptStruct() const
{
	return FGSTN_LayeredMove_PushBack::StaticStruct();
}

FString FGSTN_LayeredMove_PushBack::ToSimpleString() const
{
	return TEXT("PushBack");
}

FEffectMoveContainer UGSTN_PushBackMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_LayeredMove_PushBack Move;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		const FGameplayEffectSpec& Spec = *EffectSpec.Data;
		Move.Velocity = FVector(Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, false, 0.f));
		Move.DecayPerFrame = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, false, 0.f);
		Move.DurationMs = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, false, 0.f) * 1000.f;
	}
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_PushBack::UGSTN_GE_PushBack()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_PushBackMoveCreator* Creator = CreateDefaultSubobject<UGSTN_PushBackMoveCreator>(TEXT("PushBackMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
