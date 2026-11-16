#include "Brain/GSTN_MinionBrain.h"

#include "Brain/GSTN_MinionSteering.h"

void FGSTN_MinionBrainActions::MoveToTarget(FGSTN_MinionBrainContext& Context, float Speed, float StopRadius, float SlowRadius, float SeparationRadius, float SeparationWeight)
{
	if (!Context.bHasTarget)
	{
		return;
	}
	// Speed and arrival follow the distance to the target itself; the direction follows MovePoint (a waypoint of a path, else the target)
	FVector2D ArriveDirection;
	float ArriveSpeed = 0.f;
	UGSTN_MinionSteeringUtils::Arrive(Context.Position2D(), FVector2D(Context.TargetPosition.X, Context.TargetPosition.Y), Speed, StopRadius, SlowRadius, ArriveDirection, ArriveSpeed);
	FGSTN_MinionIntent& Intent = Context.PendingIntent;
	Intent.TargetEntityId = Context.TargetId;
	if (ArriveSpeed <= 0.f)
	{
		return;
	}
	const FVector2D Seek = UGSTN_MinionSteeringUtils::Seek(Context.Position2D(), FVector2D(Context.MovePoint.X, Context.MovePoint.Y));
	const FVector2D Push = UGSTN_MinionSteeringUtils::Separation(Context.Position2D(), Context.Neighbours, SeparationRadius);
	Intent.MoveDir = UGSTN_MinionSteeringUtils::BlendSeparation(Seek, Push, SeparationWeight);
	Intent.DesiredSpeed = ArriveSpeed;
}

void FGSTN_MinionBrainActions::FaceTarget(FGSTN_MinionBrainContext& Context)
{
	if (!Context.bHasTarget)
	{
		return;
	}
	const FVector2D ToTarget(Context.TargetPosition.X - Context.Position.X, Context.TargetPosition.Y - Context.Position.Y);
	if (ToTarget.SizeSquared() < 1.0)
	{
		return;
	}
	FGSTN_MinionIntent& Intent = Context.PendingIntent;
	Intent.DesiredYaw = UGSTN_MinionSteeringUtils::YawOfDirection(ToTarget);
	Intent.bFaceRequested = true;
	Intent.TargetEntityId = Context.TargetId;
}

void FGSTN_MinionBrainActions::RequestAttack(FGSTN_MinionBrainContext& Context, const FGameplayTag& Command)
{
	if (!Context.bHasTarget)
	{
		return;
	}
	FGSTN_MinionIntent& Intent = Context.PendingIntent;
	Intent.Command = Command.IsValid() ? Command : Context.DefaultCommand;
	Intent.TargetEntityId = Context.TargetId;
}

bool FGSTN_MinionBrainActions::IsTargetInRange(const FGSTN_MinionBrainContext& Context, float MinRange, float MaxRange)
{
	return Context.bHasTarget && Context.TargetDistance >= MinRange && Context.TargetDistance <= MaxRange;
}

bool FGSTN_MinionBrainActions::IsLODAtLeast(const FGSTN_MinionBrainContext& Context, EGSTN_MinionLODTier Tier)
{
	return static_cast<uint8>(Context.Tier) <= static_cast<uint8>(Tier);
}

void FGSTN_MinionNativeBrain::RunPhase(FGSTN_MinionBrainContext& Context, const FGSTN_MinionChaseSwingParams& Params) const
{
	if (Phase == EPhase::Chase)
	{
		FGSTN_MinionBrainActions::MoveToTarget(Context, Params.Speed, Params.StopRadius, Params.SlowRadius, Params.SeparationRadius, Params.SeparationWeight);
		FGSTN_MinionBrainActions::FaceTarget(Context);
	}
	else
	{
		FGSTN_MinionBrainActions::FaceTarget(Context);
		FGSTN_MinionBrainActions::RequestAttack(Context, FGameplayTag());
	}
}

void FGSTN_MinionNativeBrain::Tick(FGSTN_MinionBrainContext& Context, const FGSTN_MinionChaseSwingParams& Params)
{
	Context.PendingIntent = FGSTN_MinionIntent();
	if (!bStarted)
	{
		bStarted = true;
		Phase = EPhase::Chase;
		RunPhase(Context, Params);
		return;
	}
	RunPhase(Context, Params);
	const EPhase Previous = Phase;
	if (Phase == EPhase::Chase && FGSTN_MinionBrainActions::IsTargetInRange(Context, 0.f, Params.EnterRange))
	{
		Phase = EPhase::Swing;
	}
	else if (Phase == EPhase::Swing && !FGSTN_MinionBrainActions::IsTargetInRange(Context, 0.f, Params.ExitRange))
	{
		Phase = EPhase::Chase;
	}
	if (Phase != Previous)
	{
		RunPhase(Context, Params);
	}
}

int32 FGSTN_MinionBrainBudget::GetInterval(EGSTN_MinionLODTier Tier) const
{
	switch (Tier)
	{
	case EGSTN_MinionLODTier::Combat:
		return 1;
	case EGSTN_MinionLODTier::Near:
		return FMath::Max(NearInterval, 1);
	case EGSTN_MinionLODTier::Far:
		return FMath::Max(FarInterval, 1);
	default:
		return 0;
	}
}

bool FGSTN_MinionBrainBudget::ShouldTick(EGSTN_MinionLODTier Tier, int32 Slot, int32 TickCounter, bool bWoken) const
{
	if (bWoken)
	{
		return true;
	}
	const int32 Interval = GetInterval(Tier);
	return Interval > 0 && ((TickCounter + Slot) % Interval) == 0;
}
