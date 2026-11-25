// Combat simulation (GAS-NPP fork): Mover's falling mode without the start-of-fall floor snap while an attack hitbox is active.

#include "Movement/GSTN_FallingMode.h"

#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Timeline/GSTN_TimelineTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_FallingMode)

bool UGSTN_FallingMode::IsAttackHitboxActive(const FGSTN_CombatTimelineData* Data, int32 Frame)
{
	if (!Data || Frame == INDEX_NONE)
	{
		return false;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Entry : Data->Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Hitbox = GSTNTimelineTracks::AsAttackHitbox(Entry);
		if (Hitbox && Hitbox->Contains(Frame))
		{
			return true;
		}
	}
	return false;
}

FVector UGSTN_FallingMode::NudgeStartVelocity(const FVector& Velocity, const FVector& Up, float NudgeSpeed)
{
	const double Along = Velocity.Dot(Up);
	// The parent's test: FMath::IsNearlyZero with its default tolerance
	if (!FMath::IsNearlyZero(Along))
	{
		return Velocity;
	}
	return Velocity - Along * Up - static_cast<double>(FMath::Max(NudgeSpeed, 0.001f)) * Up;
}

void UGSTN_FallingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* Mover = GetMoverComponent();
	const AActor* Owner = Mover ? Mover->GetOwner() : nullptr;
	const UGSTN_CombatSimComponent* Combat = (bNoFloorSnapWhileHitboxActive && Owner) ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	int32 TimelineFrame = INDEX_NONE;
	const FGSTN_CombatTimelineData* Data = Combat ? Combat->ReadEvaluatedTimeline(TimelineFrame) : nullptr;
	const FMoverDefaultSyncState* Start = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!Start || !IsAttackHitboxActive(Data, TimelineFrame))
	{
		Super::SimulationTick_Implementation(Params, OutputState);
		return;
	}
	const FVector Up = Mover->GetUpDirection();
	const FVector Velocity = Start->GetVelocity_WorldSpace();
	const FVector Nudged = NudgeStartVelocity(Velocity, Up, StartNudgeSpeed);
	if (Nudged.Equals(Velocity, 0.0))
	{
		Super::SimulationTick_Implementation(Params, OutputState);
		return;
	}
	// A copy of the tick parameters with the start velocity nudged: the parent reads it for the start-of-fall test only (and as the velocity
	// passed on when the frame used no time, where the nudge is below anything that moves). The real start state is not touched
	FSimulationTickParams Copy = Params;
	if (FMoverDefaultSyncState* CopyStart = Copy.StartState.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>())
	{
		CopyStart->SetTransforms_WorldSpace(Start->GetLocation_WorldSpace(), Start->GetOrientation_WorldSpace(), Nudged,
			Start->GetAngularVelocityDegrees_WorldSpace(), Start->GetMovementBase(), Start->GetMovementBaseBoneName());
	}
	Super::SimulationTick_Implementation(Copy, OutputState);
}
