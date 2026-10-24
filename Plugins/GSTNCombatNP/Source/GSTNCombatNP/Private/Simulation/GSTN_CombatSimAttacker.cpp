// Combat simulation (GAS-NPP fork): the part of the combat component that runs the attacker side of the tick (combat-m4-hits 6).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Debug/GSTN_CombatDebugDraw.h"
#include "GameFramework/Actor.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "NetworkPredictionTickState.h"
#include "Rules/GSTN_CombatRules.h"
#include "World/GSTN_CombatWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatAttacker, Log, All);

void UGSTN_CombatSimComponent::SetCombatRules(UGSTN_CombatRules* Rules)
{
	CombatRules = Rules;
}

bool UGSTN_CombatSimComponent::ReadOwnPose(FTransform& OutTransform, FVector& OutUp)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	// The tracker holds the Mover state of the last simulated frame, which is the end of the previous one at this point of the tick
	const UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	const FMoverDefaultSyncState* Mover = Tracker ? Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
	if (Mover)
	{
		OutTransform = FTransform(Mover->GetOrientation_WorldSpace(), Mover->GetLocation_WorldSpace());
		OutUp = Tracker->GetUpDirection();
	}
	else
	{
		OutTransform = Owner->GetActorTransform();
		OutTransform.SetScale3D(FVector::OneVector);
		OutUp = OutTransform.GetUnitAxis(EAxis::Z);
	}
	return true;
}

void UGSTN_CombatSimComponent::StampMoveOrigin(FGSTN_CombatSyncCore& Core, int32 Frame)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame != Frame)
	{
		return;
	}
	FTransform Pose;
	FVector Up;
	if (ReadOwnPose(Pose, Up))
	{
		Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(Pose);
	}
}

void UGSTN_CombatSimComponent::RunAttackerStep(const FNetSimTimeStep& TimeStep, FGSTN_CombatSyncCore& Core, const FGSTN_StepSnapshot* Snapshot, const FGSTN_HitGroupMask& GroupMask)
{
	const int32 Frame = TimeStep.Frame;
	const bool bLog = GSTNCombatDebug::IsHitLogOn();
	const bool bDraw = GSTNCombatDebug::IsHitboxDrawOn();
	AttackerDebugShapes.Reset();
	AttackerHits.Reset();
	if (bLog)
	{
		HitLogByFrame.Remove(Frame);
		HitboxTraceByFrame.Remove(Frame);
	}

	FGSTN_AttackerTickInput In;
	In.Frame = Frame;
	In.AttackerId = RegisteredEntityId;
	In.AttackerTeam = ResolveTeam();
	In.Snapshot = Snapshot;
	In.Rules = CombatRules;
	In.GroupMask = GroupMask;
	In.StepSeconds = TimeStep.StepMS * 0.001f;
	In.DebugShapes = (bLog || bDraw) ? &AttackerDebugShapes : nullptr;
	if (Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		In.Timeline = GetRegistryTimeline(Tables(), Core.ActiveTimeline);
		In.TimelineFrame = FMath::Max(Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
	}
	if (!In.Timeline || !ReadOwnPose(In.Root, In.Up))
	{
		return;
	}

	FGSTN_CombatAttacker::Tick(In, Core, AttackerScratch, AttackerHits);

	UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	for (const FGSTN_AttackerHit& Hit : AttackerHits)
	{
		if (Hit.bReplacedTarget && !bLoggedTargetOverflow)
		{
			bLoggedTargetOverflow = true;
			UE_LOG(LogGSTNCombatAttacker, Warning, TEXT("%s: the hit target ring (%d) is full, the oldest target of the move was replaced; a hitbox group that hits more victims than that can hit the first ones again"),
				*GetPathName(), FGSTN_HitSyncState::MaxHitTargets);
		}
		// Also on a ricochet: the victim still gets the entry for its cue and event. A victim that does not tick here refuses it
		if (World)
		{
			World->PostHit(Hit.VictimId, Hit.Entry);
		}
	}

	if (bLog)
	{
		FString Trace;
		for (const FGSTN_AttackerDebugShape& Shape : AttackerDebugShapes)
		{
			const FVector Center = (Shape.Shape.Local * Shape.World).GetLocation();
			Trace += FString::Printf(TEXT("%d:%d:%d:%.2f,%.2f,%.2f;"), Frame, Shape.HitboxIndex, Shape.Space, Center.X, Center.Y, Center.Z);
		}
		if (!Trace.IsEmpty())
		{
			HitboxTraceByFrame.Add(Frame, Trace);
			HitboxTraceByFrame.Remove(Frame - 512);
		}
		FString Log;
		for (const FGSTN_AttackerHit& Hit : AttackerHits)
		{
			Log += FString::Printf(TEXT("%d:%d:%d:%d:%d:%d:%d;"), Frame, Hit.VictimId, Hit.HitGroup, Hit.Entry.AttackerHitboxIndex, Hit.Entry.HurtboxIndex, Hit.bRicochet ? 1 : 0, Hit.AttackerHitstopFrames);
		}
		if (!Log.IsEmpty())
		{
			HitLogByFrame.Add(Frame, Log);
			HitLogByFrame.Remove(Frame - 512);
		}
	}
}

FString UGSTN_CombatSimComponent::GetHitLog() const
{
	TArray<int32> Frames;
	HitLogByFrame.GetKeys(Frames);
	Frames.Sort();
	FString Out;
	for (const int32 Frame : Frames)
	{
		Out += HitLogByFrame[Frame];
	}
	return Out;
}

FString UGSTN_CombatSimComponent::GetHitboxTrace() const
{
	TArray<int32> Frames;
	HitboxTraceByFrame.GetKeys(Frames);
	Frames.Sort();
	FString Out;
	for (const int32 Frame : Frames)
	{
		Out += HitboxTraceByFrame[Frame];
	}
	return Out;
}
