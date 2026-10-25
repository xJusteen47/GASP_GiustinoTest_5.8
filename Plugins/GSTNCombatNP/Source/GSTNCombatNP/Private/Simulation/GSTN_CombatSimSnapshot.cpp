// Combat simulation (GAS-NPP fork): the part of the combat component that feeds the world subsystem's step snapshot (combat-m4-hits 5).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_HealthAttributeSet.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"

void UGSTN_CombatSimComponent::SetCombatWorldSubsystem(UGSTN_CombatWorldSubsystem* Subsystem)
{
	WorldSubsystem = Subsystem;
	TryRegisterWithWorld();
}

FGenericTeamId UGSTN_CombatSimComponent::ResolveTeam() const
{
	if (TestTeam >= 0)
	{
		return FGenericTeamId(static_cast<uint8>(FMath::Min(TestTeam, 254)));
	}
	const AActor* Owner = GetOwner();
	if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(Owner))
	{
		return Agent->GetGenericTeamId();
	}
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const IGenericTeamAgentInterface* Controller = Cast<IGenericTeamAgentInterface>(Pawn->GetController()))
		{
			return Controller->GetGenericTeamId();
		}
	}
	return FGenericTeamId::NoTeam;
}

void UGSTN_CombatSimComponent::TryRegisterWithWorld()
{
	UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get();
	if (!Subsystem)
	{
		return;
	}
	// The NP id of a client's pawn arrives with the replication of the proxy, which can be after BeginPlay
	const int32 EntityId = static_cast<int32>(NetworkPredictionProxy.GetID());
	if (EntityId < 0 || EntityId == RegisteredEntityId)
	{
		return;
	}
	if (RegisteredEntityId != INDEX_NONE)
	{
		Subsystem->UnregisterEntity(RegisteredEntityId);
		RegisteredEntityId = INDEX_NONE;
	}
	if (Subsystem->RegisterEntity(EntityId, *this, this, ResolveTeam(), ActiveHurtboxSet))
	{
		RegisteredEntityId = EntityId;
	}
}

bool UGSTN_CombatSimComponent::ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out)
{
	const bool bTicks = TicksOnThisMachine();
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(bTicks ? ENetworkPredictionStateRead::Simulation : ENetworkPredictionStateRead::Presentation);
	const AActor* Owner = GetOwner();
	if (!Sync || !Owner)
	{
		return false;
	}
	const FGSTN_CombatSyncCore& Core = Sync->Core;

	// Where and how fast: the tracker holds the Mover state of the last simulated frame, which at the first tick of a step is the end of the
	// previous one. An interpolated proxy has no steps of its own: it contributes the pose it shows
	const UGSTN_MoverSimStateTracker* Tracker = bTicks ? GetJumpTracker() : nullptr;
	const FMoverDefaultSyncState* Mover = Tracker ? Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
	if (Mover)
	{
		Out.Transform = FTransform(Mover->GetOrientation_WorldSpace(), Mover->GetLocation_WorldSpace());
		Out.Velocity = Tracker->GetVelocity();
		Out.Up = Tracker->GetUpDirection();
	}
	else
	{
		// No tracked state (a proxy, or a pawn without Mover): the actor's pose, with up along its own up axis, never world Z
		Out.Transform = Owner->GetActorTransform();
		Out.Velocity = Owner->GetVelocity();
		Out.Up = Out.Transform.GetUnitAxis(EAxis::Z);
	}
	Out.Facing = Out.Transform.GetUnitAxis(EAxis::X);
	Out.Team = ResolveTeam();
	Out.bInHitstop = Core.IsInHitstop(Frame);
	Out.ActionState = Core.ActionState;

	// How each hurtbox takes hits this frame, derived from the timeline the state says is running. A frame inside the hitstop window
	// has not advanced the timeline, so it reads the frame before
	const UGSTN_HurtboxSetDefinition* Set = Out.Hurtboxes;
	Out.HurtStates.Reset();
	if (Set)
	{
		Out.HurtStates.SetNum(Set->Hurtboxes.Num());
		const UGSTN_CombatTableSet* Tables = FindTableSet(Core.TableRevision);
		const FGSTN_CombatTimelineData* Timeline = Core.ActionState == EGSTN_ActionState::InTimeline && Tables ? GetRegistryTimeline(*Tables, Core.ActiveTimeline) : nullptr;
		if (Timeline)
		{
			const int32 EvaluatedFrame = Core.IsInHitstop(Frame) ? Frame - 1 : Frame;
			const int32 TimelineFrame = FMath::Max(EvaluatedFrame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
			FGSTN_TimelineFrameResult Result;
			FGSTN_TimelinePlayer::EvaluateHits(*Timeline, TimelineFrame, Result);
			for (int32 Index = 0; Index < Set->Hurtboxes.Num(); ++Index)
			{
				Out.HurtStates[Index] = FGSTN_TimelinePlayer::ResolveHurtState(Result, Set->Hurtboxes[Index]);
			}
		}
	}

	// The compact reaction state: the damage percent is the health set's, the weight scale the identity until the victim side writes one
	uint8 Flags = 0;
	FGSTN_StaggerThreshold Threshold = Set ? Set->StaggerThreshold : FGSTN_StaggerThreshold();
	bool bAllIntangible = Out.HurtStates.Num() > 0;
	bool bAllInvincible = Out.HurtStates.Num() > 0;
	for (const FGSTN_HurtboxFrameState& Hurt : Out.HurtStates)
	{
		bAllIntangible &= Hurt.State == EGSTN_HurtState::Intangible;
		bAllInvincible &= Hurt.State == EGSTN_HurtState::Intangible || Hurt.State == EGSTN_HurtState::Invincible;
		if (Hurt.State == EGSTN_HurtState::Armor)
		{
			Flags |= GSTNVictimReactionFlags::Armor;
			Threshold = FGSTN_StaggerThreshold::Max(Threshold, Hurt.ArmorThreshold);
		}
	}
	// A knocked out combatant cannot be hit any more: attackers see it as intangible
	bAllIntangible |= Core.ActionState == EGSTN_ActionState::KO;
	bAllInvincible |= Core.ActionState == EGSTN_ActionState::KO;
	Flags |= bAllIntangible ? GSTNVictimReactionFlags::Intangible : 0;
	Flags |= bAllInvincible ? GSTNVictimReactionFlags::Invincible : 0;
	Flags |= Out.bInHitstop ? GSTNVictimReactionFlags::InHitstop : 0;
	Flags |= Core.ActionState == EGSTN_ActionState::Tumble ? GSTNVictimReactionFlags::Tumble : 0;

	double DamagePercent = 0.0;
	if (AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UGSTN_HealthAttributeSet::GetDamagePercentAttribute()))
	{
		DamagePercent = AbilitySystem->GetNumericAttribute(UGSTN_HealthAttributeSet::GetDamagePercentAttribute());
	}
	Out.HealthFraction = 1.f;
	if (AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(UGSTN_HealthAttributeSet::GetHealthAttribute()))
	{
		const float MaxHealth = AbilitySystem->GetNumericAttribute(UGSTN_HealthAttributeSet::GetMaxHealthAttribute());
		Out.HealthFraction = MaxHealth > 0.f ? FMath::Clamp(AbilitySystem->GetNumericAttribute(UGSTN_HealthAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f) : 1.f;
	}
	// The weight the health weight curve gives the victim now: the attacker's machine reads it from the snapshot like the rest of the reaction state
	const double WeightScale = FGSTN_CombatVictim::ComputeWeightScale(CombatRules, Set, DamagePercent, Out.HealthFraction);
	Out.Reaction = FGSTN_VictimReactionState::Make(DamagePercent, WeightScale, Flags, Threshold);
	return true;
}
