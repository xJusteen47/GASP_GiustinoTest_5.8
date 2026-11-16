// Combat simulation (GAS-NPP fork): the part of the combat component that feeds the world subsystem's step snapshot (combat-m4-hits 5).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_HealthAttributeSet.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GSTN_CombatTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverDataModelTypes.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"
#include "Movement/GSTN_BodyCapsuleModifier.h"
#include "Movement/GSTN_GravityFrame.h"
#include "Rules/GSTN_CombatRules.h"
#include "Timeline/GSTN_Pushbox.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** Diagnostics (pushbox P5, owner PIE bug 2026-11-16): logs every frame the body constraint or push changes a fighter's move */
	TAutoConsoleVariable<int32> CVarPushboxLog(TEXT("gstn.Pushbox.Log"), 0,
		TEXT("1: log (LogTemp) each frame a fighter's move is corrected by the pushbox approach clamp / rigid core or pushed, with the bodies it read."),
		ECVF_Default);
}

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
	Out.HurtPose.Reset();
	if (Set)
	{
		Out.HurtStates.SetNum(Set->Hurtboxes.Num());
		const UGSTN_CombatTableSet* Tables = FindTableSet(Core.TableRevision);
		const FGSTN_CombatTimelineData* Timeline = Core.ActionState == EGSTN_ActionState::InTimeline && Core.RecoilStartFrame == INDEX_NONE && Tables ? GetRegistryTimeline(*Tables, Core.ActiveTimeline) : nullptr;
		FGSTN_HurtboxPoseInput PoseInput;
		PoseInput.Set = Set;
		// The kit's movement hurtbox comes with the table of the loadout revision the state names, so a rollback across a kit swap reads the old kit's
		PoseInput.KitMovement = Tables && Tables->bHasKitMovementHurtbox ? &Tables->KitMovementHurtbox : nullptr;
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
			PoseInput.Timeline = Timeline;
			PoseInput.TimelineFrame = TimelineFrame;
		}
		// Where the hurtboxes are: the same timeline frame as the states (a hitstop frame holds the pose of the frame before, like the montage)
		FGSTN_HurtboxPose::Build(PoseInput, Out.HurtPose);
	}

	// The compact reaction state: the damage percent is the health set's, the weight scale the identity until the victim side writes one
	uint8 Flags = 0;
	FGSTN_StaggerThreshold Threshold = Set ? Set->StaggerThreshold : FGSTN_StaggerThreshold();
	bool bAllIntangible = Out.HurtStates.Num() > 0;
	bool bAllInvincible = Out.HurtStates.Num() > 0;
	for (int32 Index = 0; Index < Out.HurtStates.Num(); ++Index)
	{
		const FGSTN_HurtboxFrameState& Hurt = Out.HurtStates[Index];
		// A hurtbox left out by the detail level does not decide "all intangible" (unless something counts as it, it cannot be touched at all)
		if (!Out.IsHurtboxPresent(Index) && !Out.HurtPose.Extensions.ContainsByPredicate([Index](const FGSTN_PosedHurtboxExtension& Extension) { return Extension.HurtboxIndex == Index; }))
		{
			continue;
		}
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

	// The data optional plugins add (broken parts...): from the same end-of-previous-frame state, in ascending feature id order; nothing registered leaves it empty
	FGSTN_SnapshotContributorContext Contribution;
	Contribution.Frame = Frame;
	Contribution.Owner = Owner;
	Contribution.Fragments = &Sync->Fragments;
	Contribution.ActionState = Core.ActionState;
	Contribution.Transform = Out.Transform;
	FGSTN_SnapshotContributorRegistry::Get().Fill(Contribution, Out);

	// The pushbox, after the contributors: the states they report (knockdown) select its StateRules entry, with the core's own action state tag
	FGSTN_PushboxStateTags StateTags;
	if (Core.ActionState == EGSTN_ActionState::Tumble)
	{
		StateTags.Add(GSTNCombatTags::State_Tumble.GetTag().GetTagName());
	}
	else if (Core.ActionState == EGSTN_ActionState::Splat)
	{
		StateTags.Add(GSTNCombatTags::State_Splat.GetTag().GetTagName());
	}
	FGSTN_SnapshotContributorRegistry::Get().CollectStateTags(Contribution, StateTags);
	ResolveSnapshotPushbox(Frame, Core, Out.Transform, StateTags, Out.Pushbox);
	return true;
}

bool UGSTN_CombatSimComponent::ResolveSnapshotPushbox(int32 Frame, const FGSTN_CombatSyncCore& Core, const FTransform& CapsuleTransform, TConstArrayView<FName> StateTags, FGSTN_PushboxCylinder& Out)
{
	Out = FGSTN_PushboxCylinder();
	// A knocked out fighter is out of play: nothing pushes it, it pushes nothing
	if (Core.ActionState == EGSTN_ActionState::KO)
	{
		return false;
	}
	const AActor* Owner = GetOwner();
	const UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	FGSTN_BodyCapsuleShape Standing;
	if (!Mover || !FGSTN_BodyCapsuleModifier::GetStanding(*Mover, Standing))
	{
		return false;
	}

	// The movement capsule at the end of the previous frame: the body capsule modifier of the tracked Mover state (the tracker holds the last
	// simulated frame, as for the transform), or the presented one for a proxy, whose capsule ApplyPresentation keeps
	FGSTN_BodyCapsuleShape Applied;
	const UGSTN_MoverSimStateTracker* Tracker = TicksOnThisMachine() ? GetJumpTracker() : nullptr;
	if (Tracker)
	{
		for (auto It = Tracker->GetSyncState().MovementModifiers.GetActiveModifiersIterator(); It; ++It)
		{
			const FMovementModifierBase* Modifier = It->Get();
			if (Modifier && Modifier->GetScriptStruct() == FGSTN_BodyCapsuleModifier::StaticStruct())
			{
				Applied = static_cast<const FGSTN_BodyCapsuleModifier*>(Modifier)->Applied;
				break;
			}
		}
	}
	else if (const FGSTN_BodyCapsuleModifier* Presented = Mover->FindMovementModifierByType<FGSTN_BodyCapsuleModifier>())
	{
		Applied = Presented->Applied;
	}
	const FGSTN_BodyCapsuleShape Capsule = Applied.HalfHeight > 0.f ? Applied : Standing;

	// The running move's frame, as the hurtbox states read it
	const FGSTN_CombatTimelineData* Timeline = nullptr;
	int32 TimelineFrame = INDEX_NONE;
	const UGSTN_CombatTableSet* Tables = FindTableSet(Core.TableRevision);
	if (Core.ActionState == EGSTN_ActionState::InTimeline && Core.RecoilStartFrame == INDEX_NONE && Tables)
	{
		Timeline = GetRegistryTimeline(*Tables, Core.ActiveTimeline);
		const int32 EvaluatedFrame = Core.IsInHitstop(Frame) ? Frame - 1 : Frame;
		TimelineFrame = FMath::Max(EvaluatedFrame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
	}

	const FGSTN_PushboxSettings Settings = CombatRules ? CombatRules->Pushbox : FGSTN_PushboxSettings();
	FGSTN_BodyCapsuleShape Local;
	if (BodyCollision.Mode == EGSTN_BodyCollisionMode::None || !FGSTN_Pushbox::ResolveLocal(Timeline, TimelineFrame, Capsule, Settings, StateTags, Local))
	{
		return false;
	}
	// The core is the body, not the pose: from the standing capsule (a Body Capsule track grows the pushbox, never the core), and only while the
	// frame has a pushbox at all (a disabled track or a downed state crosses both)
	if (BodyCollision.Mode == EGSTN_BodyCollisionMode::PushboxAndCore)
	{
		Out.CoreRadius = BodyCollision.CoreRadius > 0.f ? BodyCollision.CoreRadius : Standing.Radius * Settings.CoreRadiusScale;
	}
	// The capsule centre is the actor; the mesh root sits at actor - Capsule.Offset in the yaw frame (FGSTN_BodyCapsuleShiftEffect's frame)
	// Upright frame = yaw about the local up (gravity audit 2026-11-16; world up gives the old yaw-only frame exactly)
	const FVector Up = Mover->GetUpDirection();
	const bool bWorldUp = FGSTN_GravityFrame::IsWorldUp(Up);
	const FRotator Yaw(0.0, CapsuleTransform.Rotator().Yaw, 0.0);
	const FQuat Upright = bWorldUp ? FQuat::Identity : FGSTN_GravityFrame::UprightRotation(CapsuleTransform.GetRotation(), Up);
	auto ToWorld = [&](const FVector& V) { return bWorldUp ? Yaw.RotateVector(V) : Upright.RotateVector(V); };
	const FVector MeshRoot = CapsuleTransform.GetLocation() - ToWorld(Capsule.Offset);
	Out.Center = MeshRoot + ToWorld(Local.Offset);
	Out.Radius = Local.Radius;
	Out.HalfHeight = Local.HalfHeight;
	return true;
}

void UGSTN_CombatSimComponent::ComputeFramePush(int32 Frame, const FGSTN_StepSnapshot* Snapshot)
{
	FramePush.Frame = Frame;
	FramePush.Move = FVector::ZeroVector;
	FramePush.bHasBodies = false;
	FramePush.Others.Reset();
	const FGSTN_SnapshotEntry* Self = Snapshot ? Snapshot->Find(RegisteredEntityId) : nullptr;
	// A parked link role rides its partner (placed by a contributor): its pushbox is not where it is, and the pair must not push apart
	if (!Self || !Self->Pushbox.IsActive() || Self->bPlacedByContributor)
	{
		return;
	}
	const FGSTN_PushboxSettings& Settings = CombatRules ? CombatRules->Pushbox : GetDefault<UGSTN_CombatRules>()->Pushbox;
	FGSTN_PushboxBody& SelfBody = FramePush.Self;
	SelfBody.EntityId = Self->EntityId;
	SelfBody.Cylinder = Self->Pushbox;
	SelfBody.bFrozen = Self->bInHitstop;
	FramePush.ActorStart = Self->Transform.GetLocation();
	FramePush.Up = Self->Up;

	TArray<FGSTN_PushboxBody, TInlineAllocator<8>>& Others = FramePush.Others;
	for (const FGSTN_SnapshotEntry& Entry : Snapshot->Entries)
	{
		if (Entry.EntityId == Self->EntityId || !Entry.Pushbox.IsActive() || Entry.bPlacedByContributor)
		{
			continue;
		}
		// P5, a crowd of minions in the snapshot: only bodies overlapping at the start of the frame can push or block (PairShare and
		// ConstrainMove skip the rest), so the far ones are dropped here and the per-frame lists stay short. Same result, less work
		const FVector Delta = Entry.Pushbox.Center - Self->Pushbox.Center;
		const double Vertical = FVector::DotProduct(Delta, Self->Up);
		const double Reach = FMath::Max(static_cast<double>(Entry.Pushbox.Radius) + Self->Pushbox.Radius, static_cast<double>(Entry.Pushbox.CoreRadius) + Self->Pushbox.CoreRadius);
		if (FMath::Abs(Vertical) >= static_cast<double>(Entry.Pushbox.HalfHeight) + Self->Pushbox.HalfHeight
			|| (Delta - Self->Up * Vertical).SizeSquared() >= FMath::Square(Reach))
		{
			continue;
		}
		FGSTN_PushboxBody& Body = Others.AddDefaulted_GetRef();
		Body.EntityId = Entry.EntityId;
		Body.Cylinder = Entry.Pushbox;
		Body.bFrozen = Entry.bInHitstop;
	}
	FramePush.Move = FGSTN_Pushbox::ComputePush(SelfBody, Others, Settings, Self->Up);
	FramePush.bHasBodies = Others.Num() > 0;
}

FVector UGSTN_CombatSimComponent::ConstrainFrameMove(const FVector& ActorEnd, FVector& InOutVelocity) const
{
	if (!FramePush.bHasBodies || FramePush.Frame == INDEX_NONE || FramePush.Frame != GetSimulationFrame())
	{
		return FVector::ZeroVector;
	}
	const FGSTN_PushboxSettings& Settings = CombatRules ? CombatRules->Pushbox : GetDefault<UGSTN_CombatRules>()->Pushbox;
	const FVector Move = ActorEnd - FramePush.ActorStart;
	const FVector Constrained = FGSTN_Pushbox::ConstrainMove(FramePush.Self, Move, FramePush.Others, Settings, FramePush.Up, &InOutVelocity);
	if (CVarPushboxLog.GetValueOnAnyThread() != 0 && (!(Constrained - Move).IsNearlyZero(0.05) || !FramePush.Move.IsNearlyZero(0.05)))
	{
		const AActor* Owner = GetOwner();
		FString Others;
		for (const FGSTN_PushboxBody& Other : FramePush.Others)
		{
			Others += FString::Printf(TEXT(" [%d c=%s r=%.1f core=%.1f hh=%.1f fz=%d]"), Other.EntityId, *Other.Cylinder.Center.ToCompactString(), Other.Cylinder.Radius,
				Other.Cylinder.CoreRadius, Other.Cylinder.HalfHeight, Other.bFrozen ? 1 : 0);
		}
		UE_LOG(LogTemp, Log, TEXT("PushboxLog f=%d %s role=%d id=%d start=%s end=%s self_c=%s r=%.1f core=%.1f move=%s corr=%s push=%s others:%s"),
			FramePush.Frame, *GetNameSafe(Owner), Owner ? static_cast<int32>(Owner->GetLocalRole()) : -1, FramePush.Self.EntityId,
			*FramePush.ActorStart.ToCompactString(), *ActorEnd.ToCompactString(), *FramePush.Self.Cylinder.Center.ToCompactString(),
			FramePush.Self.Cylinder.Radius, FramePush.Self.Cylinder.CoreRadius, *Move.ToCompactString(), *(Constrained - Move).ToCompactString(),
			*FramePush.Move.ToCompactString(), *Others);
	}
	return Constrained - Move;
}

FVector UGSTN_CombatSimComponent::ReadFramePush() const
{
	return FramePush.Frame != INDEX_NONE && FramePush.Frame == GetSimulationFrame() ? FramePush.Move : FVector::ZeroVector;
}