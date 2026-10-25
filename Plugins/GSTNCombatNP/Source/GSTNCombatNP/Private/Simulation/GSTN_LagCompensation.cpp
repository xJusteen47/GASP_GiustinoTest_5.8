// Combat simulation (GAS-NPP fork): lag compensation of the attacker side (combat-m4-hits 9, M4 H10).

#include "Simulation/GSTN_LagCompensation.h"

#include "NetworkPredictionLagCompensation.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionWorldManager.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

bool GSTNLagComp::ShouldRewind(ENetRole LocalRole, ENetRole RemoteRole, const UGSTN_CombatRules* Rules)
{
	if (!Rules || !Rules->bUseLagCompensation || Rules->MaxRewindMs <= 0)
	{
		return false;
	}
	return LocalRole == ROLE_Authority && RemoteRole == ROLE_AutonomousProxy;
}

float GSTNLagComp::ClampRewindTime(float CurrentMs, float RequestedMs, int32 MaxRewindMs)
{
	const float Oldest = CurrentMs - static_cast<float>(FMath::Max(MaxRewindMs, 0));
	return FMath::Clamp(RequestedMs, FMath::Min(Oldest, CurrentMs), CurrentMs);
}

void GSTNLagComp::BuildOverlay(const FGSTN_StepSnapshot& Source, int32 AttackerId, float RewindMs, const UGSTN_CombatWorldSubsystem& World, FPoseProvider Pose, FGSTN_StepSnapshot& Out)
{
	Out = Source;
	for (FGSTN_SnapshotEntry& Entry : Out.Entries)
	{
		if (Entry.EntityId == AttackerId)
		{
			continue;
		}
		FGSTN_EntityHistoryRecord Record;
		if (!World.SampleHistory(Entry.EntityId, RewindMs, Record, true))
		{
			continue;
		}
		FTransform Rewound = Record.Transform;
		FTransform Provided;
		if (Pose(Entry.EntityId, Provided))
		{
			Rewound = Provided;
		}
		Rewound.SetScale3D(Entry.Transform.GetScale3D());
		Entry.Transform = Rewound;
		Entry.Facing = Rewound.GetUnitAxis(EAxis::X);
		Entry.Velocity = Record.Velocity;
		Entry.Reaction = Record.Reaction;
		Entry.bInHitstop = Record.Reaction.HasFlag(GSTNVictimReactionFlags::InHitstop);
	}
	Out.BuildGrid(UGSTN_CombatWorldSubsystem::DefaultCellSize);
}

FGSTN_LagCompRewindScope::FGSTN_LagCompRewindScope(UNetworkPredictionWorldManager* InManager, AActor* Requester, float RewindMs)
	: Manager(InManager)
{
	if (Manager && Requester)
	{
		bRewound = Manager->RewindActors(Requester, RewindMs);
	}
}

FGSTN_LagCompRewindScope::~FGSTN_LagCompRewindScope()
{
	if (Manager && bRewound)
	{
		Manager->UnwindActors();
	}
}

const FGSTN_StepSnapshot* UGSTN_CombatSimComponent::ResolveAttackerSnapshot(const FNetSimTimeStep& TimeStep, const FGSTN_StepSnapshot* Shared)
{
	AActor* Owner = GetOwner();
	UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	// A pawn that is not a remote player's on the authority hits the present, which is what it shows. No interpolation time means the input
	// carried none (the first frames of a connection): rewinding to 0 would put every victim as far back as the cap
	if (!Shared || !Owner || !World || !GSTNLagComp::ShouldRewind(Owner->GetLocalRole(), Owner->GetRemoteRole(), CombatRules) || TimeStep.InterpolationTimeMS <= 0.f)
	{
		return Shared;
	}
	UNetworkPredictionWorldManager* Manager = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	if (!Manager)
	{
		return Shared;
	}

	const float Current = Manager->GetCurrentLagCompensationTimeMS(Owner);
	const float Applied = GSTNLagComp::ClampRewindTime(Current, TimeStep.InterpolationTimeMS, CombatRules->MaxRewindMs);
	{
		// The actors are back in the present when the scope ends, before the overlay is used
		FGSTN_LagCompRewindScope Scope(Manager, Owner, Applied);
		GSTNLagComp::BuildOverlay(*Shared, RegisteredEntityId, Applied, *World,
			[World](int32 EntityId, FTransform& OutPose)
			{
				const AActor* Victim = World->FindOwnerActor(EntityId);
				if (!Victim || !Victim->FindComponentByClass<UNetworkPredictionLagCompensation>())
				{
					return false;
				}
				OutPose = Victim->GetActorTransform();
				return true;
			}, LagCompOverlay);
	}
	++LagCompRewinds;
	LagCompLastRequestedMs = TimeStep.InterpolationTimeMS;
	LagCompLastAppliedMs = Applied;
	LagCompLastCurrentMs = Current;
	return &LagCompOverlay;
}

FString UGSTN_CombatSimComponent::GetLagCompInfo() const
{
	return FString::Printf(TEXT("%d,%.1f,%.1f,%.1f"), LagCompRewinds, LagCompLastRequestedMs, LagCompLastAppliedMs, LagCompLastCurrentMs);
}