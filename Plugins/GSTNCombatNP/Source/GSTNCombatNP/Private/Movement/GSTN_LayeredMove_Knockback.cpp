#include "Movement/GSTN_LayeredMove_Knockback.h"

#include "AbilitySystemComponent.h"
#include "GSTN_CombatNPTags.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Simulation/GSTN_CombatSimComponent.h"

FGSTN_LayeredMove_Knockback::FGSTN_LayeredMove_Knockback()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	DurationMs = 0.f;
}

bool FGSTN_LayeredMove_Knockback::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const double StepMs = FMath::Max<double>(TimeStep.StepMs, UE_SMALL_NUMBER);
	ElapsedFrames = FMath::Max(FMath::RoundToInt((TimeStep.BaseSimTimeMs - StartSimTimeMs) / StepMs), 0);

	if (ElapsedFrames == 0)
	{
		// The preferred mode is honoured on the first tick of a move only, so the launch is made here: the combat tick starts the move on a
		// frame no hitstop freeze finishes on, because Mover applies a finishing move's velocity over whatever the tick proposes
		OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
		OutProposedMove.LinearVelocity = Velocity;
		OutProposedMove.PreferredMode = ForceMovementMode;
		ProbeContact(StartState, TimeStep, MoverComp, Velocity);
		return true;
	}

	// The frames after the first only bend what the movement mode is doing: drag on the horizontal speed, and the part of the gravity the scale adds
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FVector Up = MoverComp ? MoverComp->GetUpDirection() : FVector::UpVector;
	const FVector Current = Sync ? Sync->GetVelocity_WorldSpace() : FVector::ZeroVector;
	const FVector Horizontal = Current - Up * Current.Dot(Up);
	const double Dt = StepMs * 0.001;
	FVector Extra = -Horizontal * FMath::Clamp<double>(DecayPerFrame, 0.0, 1.0);
	if (MoverComp && !FMath::IsNearlyEqual(GravityScale, 1.f))
	{
		Extra += MoverComp->GetGravityAcceleration() * (GravityScale - 1.0) * Dt;
	}
	OutProposedMove.MixMode = EMoveMixMode::AdditiveVelocity;
	OutProposedMove.LinearVelocity = Extra;

	// The velocity the mode moves with this frame: what it started with, the horizontal drag of this move and one step of gravity
	FVector FrameVelocity = Current + Extra;
	if (MoverComp)
	{
		FrameVelocity += MoverComp->GetGravityAcceleration() * Dt;
	}
	ProbeContact(StartState, TimeStep, MoverComp, FrameVelocity);
	return true;
}

void FGSTN_LayeredMove_Knockback::ProbeContact(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, const FVector& FrameVelocity)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	UPrimitiveComponent* Primitive = MoverComp ? Cast<UPrimitiveComponent>(MoverComp->GetUpdatedComponent()) : nullptr;
	UWorld* World = Primitive ? Primitive->GetWorld() : nullptr;
	const double Dt = FMath::Max<double>(TimeStep.StepMs, UE_SMALL_NUMBER) * 0.001;
	const FVector Delta = FrameVelocity * Dt;
	if (!Sync || !World || Delta.SquaredLength() < 1.0)
	{
		return;
	}

	// The start pose is the sync state's, not the component's, so a resimulated frame sweeps from where the original did
	const FVector Start = Sync->GetLocation_WorldSpace();
	const FVector End = Start + Delta + Delta.GetSafeNormal() * ProbeMarginCm;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTNTumbleContact), false, MoverComp->GetOwner());
	Query.bReturnPhysicalMaterial = true;
	FCollisionResponseParams Response;
	Primitive->InitSweepCollisionParams(Query, Response);
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, End, Sync->GetOrientation_WorldSpace().Quaternion(), Primitive->GetCollisionObjectType(), Primitive->GetCollisionShape(), Query, Response)
		|| !Hit.bBlockingHit || Hit.bStartPenetrating)
	{
		return;
	}

	const FGSTN_TumbleContactSyncState Found = FGSTN_TumbleContactSyncState::FromHit(Hit, FrameVelocity, MoverComp->GetUpDirection(), TimeStep.ServerFrame);
	if (!Found.IsSet())
	{
		return;
	}
	Contact = Found;

	// The object the surface belongs to, for the break request the combat component executes outside the simulation. Local data: nothing decides from it
	if (AActor* Owner = MoverComp->GetOwner())
	{
		if (UGSTN_CombatSimComponent* Combat = Owner->FindComponentByClass<UGSTN_CombatSimComponent>())
		{
			Combat->NoteContactSurface(Contact.ContactFrame, Hit.GetComponent());
		}
	}
}

FLayeredMoveBase* FGSTN_LayeredMove_Knockback::Clone() const
{
	return new FGSTN_LayeredMove_Knockback(*this);
}

void FGSTN_LayeredMove_Knockback::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	Ar << DecayPerFrame;
	Ar << GravityScale;
	Ar << ElapsedFrames;
	Ar << ForceMovementMode;
	Contact.NetSerialize(Ar);
}

UScriptStruct* FGSTN_LayeredMove_Knockback::GetScriptStruct() const
{
	return FGSTN_LayeredMove_Knockback::StaticStruct();
}

FString FGSTN_LayeredMove_Knockback::ToSimpleString() const
{
	return TEXT("Knockback");
}

FEffectMoveContainer UGSTN_KnockbackMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_LayeredMove_Knockback Move;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		const FGameplayEffectSpec& Spec = *EffectSpec.Data;
		Move.Velocity = FVector(Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, false, 0.f),
			Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, false, 0.f));
		Move.DecayPerFrame = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, false, 0.f);
		Move.GravityScale = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, false, 1.f);
		Move.DurationMs = Spec.GetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, false, 0.f) * 1000.f;
	}
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_Knockback::UGSTN_GE_Knockback()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_KnockbackMoveCreator* Creator = CreateDefaultSubobject<UGSTN_KnockbackMoveCreator>(TEXT("KnockbackMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
