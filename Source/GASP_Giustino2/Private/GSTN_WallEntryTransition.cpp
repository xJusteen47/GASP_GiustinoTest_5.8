// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_WallEntryTransition.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagAssetInterface.h"
#include "GSTN_SimpleWallMovementMode.h"
#include "GSTN_WallMovementDebug.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallEntryTransition)

UGSTN_WallEntryTransition::UGSTN_WallEntryTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = false;
	MemoryResetModes.Add(DefaultModeNames::Walking);
}

FTransitionEvalResult UGSTN_WallEntryTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FEntryCandidate Candidate;
	return FindEntryWall(Params, Candidate) ? FTransitionEvalResult(TargetModeName) : FTransitionEvalResult::NoTransition;
}

void UGSTN_WallEntryTransition::Trigger_Implementation(const FSimulationTickParams& Params)
{
	// Hand the wall to the mode (consumed by its Activate), so it starts aligned to it instead of guessing from the facing
	FEntryCandidate Candidate;
	const UMoverComponent* MoverComp = GetMoverComponent();
	UMoverBlackboard* SimBlackboard = MoverComp ? MoverComp->GetSimBlackboard_Mutable() : nullptr;
	if (SimBlackboard && FindEntryWall(Params, Candidate))
	{
		FGSTN_WallEntryInfo EntryInfo;
		EntryInfo.Normal = Candidate.Normal;
		EntryInfo.Point = Candidate.Point;
		SimBlackboard->Set(GSTNWallBlackboard::EntryInfo, EntryInfo);
	}
}

bool UGSTN_WallEntryTransition::HasMemoryResetTag() const
{
	if (MemoryResetTags.IsEmpty())
	{
		return false;
	}

	const UMoverComponent* MoverComp = GetMoverComponent();
	if (!MoverComp)
	{
		return false;
	}

	// Mover's own tags: active mode, layered moves, movement modifiers
	for (const FGameplayTag& Tag : MemoryResetTags)
	{
		if (MoverComp->HasGameplayTag(Tag, false))
		{
			return true;
		}
	}

	// Owner tags: the actor itself, or a component exposing them (e.g. an AbilitySystemComponent)
	if (const AActor* Owner = MoverComp->GetOwner())
	{
		const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Owner);
		if (!TagInterface)
		{
			TagInterface = Cast<IGameplayTagAssetInterface>(Owner->FindComponentByInterface(UGameplayTagAssetInterface::StaticClass()));
		}
		if (TagInterface && TagInterface->HasAnyMatchingGameplayTags(MemoryResetTags))
		{
			return true;
		}
	}
	return false;
}

bool UGSTN_WallEntryTransition::SweepForWall(const FSimulationTickParams& Params, const FVector& Direction, const FVector& GravityUp, FHitResult& OutHit) const
{
	const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = Params.MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent ? UpdatedComponent->GetWorld() : nullptr;
	if (!World || !UpdatedPrimitive || Direction.IsNearlyZero())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GSTN_WallEntry), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams ResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + Direction * ProbeDistance;
	if (!World->SweepSingleByChannel(OutHit, Start, End, UpdatedComponent->GetComponentQuat(), UpdatedPrimitive->GetCollisionObjectType(),
		UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams) || OutHit.bStartPenetrating)
	{
		return false;
	}

	// The face hit (not the capsule's rounded normal) must be a grabbable surface facing the movement
	const FVector FaceNormal = OutHit.ImpactNormal;
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FaceNormal, GravityUp), -1.f, 1.f)));
	if (AngleDeg < MinSurfaceAngle || AngleDeg > MaxSurfaceAngle || FVector::DotProduct(Direction, FaceNormal) >= -0.1f)
	{
		return false;
	}

	// The capsule can touch just a lip (top edge, under the feet) that the mode's probes, cast from the capsule center,
	// don't see: grabbing it would bounce between this transition and the mode's "no wall" exit. Confirm at center height
	const FVector FlatNormal = FVector::VectorPlaneProject(FaceNormal, GravityUp).GetSafeNormal();
	if (FlatNormal.IsNearlyZero())
	{
		return false;
	}
	const float Radius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
	FHitResult CenterHit;
	const FVector CenterEnd = Start - FlatNormal * (Radius + ProbeDistance + 5.f);
	if (!World->LineTraceSingleByChannel(CenterHit, Start, CenterEnd, UpdatedPrimitive->GetCollisionObjectType(), QueryParams, ResponseParams)
		|| FVector::DotProduct(CenterHit.ImpactNormal, FlatNormal) < 0.5f)
	{
		GSTN_WALL_LOG(2, TEXT("ENTRY rejected: no wall at capsule center height"));
		return false;
	}
	return true;
}

bool UGSTN_WallEntryTransition::FindEntryWall(const FSimulationTickParams& Params, FEntryCandidate& OutCandidate) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const FMoverDefaultSyncState* SyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!MoverComp || !UpdatedComponent || !SyncState || TargetModeName.IsNone())
	{
		return false;
	}

	const FVector GravityUp = MoverComp->GetUpDirection();
	const FVector Velocity = SyncState->GetVelocity_WorldSpace();
	const double NowMs = Params.TimeStep.BaseSimTimeMs;

	// Time in the current mode, from Mover's own record of the last mode change (predicted, rolled back)
	FMovementModeChangeRecord ModeChange;
	const bool bHasModeChange = Params.SimContext.Blackboard.TryGet(CommonBlackboard::LastModeChangeRecord, ModeChange);
	if (bHasModeChange && MinAirTime > 0.f && (NowMs - ModeChange.SimTimeMs) < MinAirTime * 1000.0)
	{
		return false;
	}

	// Going up?
	const float VerticalSpeed = FVector::DotProduct(Velocity, GravityUp);
	if ((VerticalPolicy == EGSTN_WallEntryVertical::DescendingOnly && VerticalSpeed > 0.f)
		|| (VerticalPolicy == EGSTN_WallEntryVertical::BelowSpeed && VerticalSpeed > MaxAscendingSpeed))
	{
		return false;
	}

	// Look for the wall along the stick first, then along the horizontal velocity
	const FCharacterDefaultInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	FVector MoveInput = Inputs ? Inputs->GetMoveInput_WorldSpace() : FVector::ZeroVector;
#if GSTN_WALL_DEBUG
	FVector DebugValue;
	if (Inputs && GSTNWallDebug::GetInputOverride(DebugValue) == GSTNWallDebug::EInputOverride::Stick)
	{
		const FVector CamForward = FVector::VectorPlaneProject(Inputs->ControlRotation.Vector(), GravityUp).GetSafeNormal();
		MoveInput = FVector::CrossProduct(GravityUp, CamForward) * DebugValue.X + CamForward * DebugValue.Y;
	}
#endif
	const FVector FlatInput = FVector::VectorPlaneProject(MoveInput, GravityUp).GetClampedToMaxSize(1.f);
	const FVector InputDir = (FlatInput.SizeSquared() > FMath::Square(0.1f)) ? FlatInput.GetSafeNormal() : FVector::ZeroVector;
	const FVector FlatVelocity = FVector::VectorPlaneProject(Velocity, GravityUp);
	const FVector VelocityDir = (FlatVelocity.SizeSquared() > 1.f) ? FlatVelocity.GetSafeNormal() : FVector::ZeroVector;

	FHitResult Hit;
	if (!SweepForWall(Params, InputDir, GravityUp, Hit) && !SweepForWall(Params, VelocityDir, GravityUp, Hit))
	{
		return false;
	}
	const FVector WallNormal = Hit.ImpactNormal;
	const FVector FlatWallNormal = FVector::VectorPlaneProject(WallNormal, GravityUp).GetSafeNormal();

	// Pushing toward it?
	if (bRequireInputTowardWall && (InputDir.IsNearlyZero() || FlatWallNormal.IsNearlyZero()
		|| FVector::DotProduct(InputDir, -FlatWallNormal) < MinInputTowardWallDot))
	{
		GSTN_WALL_LOG(2, TEXT("ENTRY rejected: input not toward wall"));
		return false;
	}

	// Re-entry rules from the wall memory, unless it was reset after leaving the last wall:
	// by going through a reset mode (landing), by a reset tag, or manually (UGSTN_WallMovementLibrary::ResetWallMemory)
	const FGSTN_WallMovementSyncState* WallState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();
	UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	if (SimBlackboard && HasMemoryResetTag())
	{
		// Recorded as a time, so the reset outlives the tag
		SimBlackboard->Set(GSTNWallBlackboard::MemoryResetTimeMs, NowMs);
	}
	double ManualResetTimeMs = -1.0;
	if (SimBlackboard)
	{
		SimBlackboard->TryGet(GSTNWallBlackboard::MemoryResetTimeMs, ManualResetTimeMs);
	}
	const bool bMemoryWasReset = WallState
		&& ((bHasModeChange && MemoryResetModes.Contains(ModeChange.PrevModeName) && ModeChange.SimTimeMs >= WallState->LastWallTimeMs)
			|| ManualResetTimeMs >= WallState->LastWallTimeMs);
	if (WallState && WallState->bHasWallMemory && !bMemoryWasReset)
	{
		const FVector LastFlatNormal = FVector::VectorPlaneProject(WallState->LastWallNormal, GravityUp).GetSafeNormal();
		const bool bSameWall = !LastFlatNormal.IsNearlyZero() && !FlatWallNormal.IsNearlyZero()
			&& FVector::DotProduct(LastFlatNormal, FlatWallNormal) >= FMath::Cos(FMath::DegreesToRadians(SameWallAngle));

		if (bSameWall)
		{
			if (ReentryCooldown > 0.f && (NowMs - WallState->LastWallTimeMs) < ReentryCooldown * 1000.0)
			{
				GSTN_WALL_LOG(2, TEXT("ENTRY rejected: same wall, cooldown"));
				return false;
			}
			if (SameWallPolicy == EGSTN_SameWallPolicy::Deny)
			{
				GSTN_WALL_LOG(2, TEXT("ENTRY rejected: same wall denied"));
				return false;
			}
			const float Height = FVector::DotProduct(UpdatedComponent->GetComponentLocation(), GravityUp);
			if (SameWallPolicy == EGSTN_SameWallPolicy::HeightCap && Height > WallState->LastWallHeight + HeightCapTolerance)
			{
				GSTN_WALL_LOG(2, TEXT("ENTRY rejected: height cap (%.1f > %.1f)"), Height, WallState->LastWallHeight + HeightCapTolerance);
				return false;
			}
		}
	}

	OutCandidate.Normal = WallNormal;
	OutCandidate.Point = Hit.ImpactPoint;
	GSTN_WALL_LOG(1, TEXT("ENTRY -> %s wallN=%s vSpeed=%.0f memReset=%d"), *TargetModeName.ToString(),
		*GSTNWallDebug::Vec(WallNormal), VerticalSpeed, bMemoryWasReset ? 1 : 0);
	return true;
}
