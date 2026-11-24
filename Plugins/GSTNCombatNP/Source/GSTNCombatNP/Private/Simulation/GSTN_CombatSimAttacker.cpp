// Combat simulation (GAS-NPP fork): the part of the combat component that runs the attacker side of the tick (combat-m4-hits 6).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Components/CapsuleComponent.h"

#include "Debug/GSTN_CombatDebugDraw.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "Movement/GSTN_BodyCapsuleModifier.h"
#include "Timeline/GSTN_Landing.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "NetworkPredictionTickState.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "World/GSTN_CombatWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatAttacker, Log, All);

void UGSTN_CombatSimComponent::SetTestFriendlyFire(bool bEnabled)
{
	if (CombatRules)
	{
		CombatRules->bFriendlyFire = bEnabled;
	}
}

void UGSTN_CombatSimComponent::SetCombatRules(UGSTN_CombatRules* Rules)
{
	CombatRules = Rules;
	// The surface layer of the impacts is part of the rules asset, loaded here and never inside the simulation. A table set by hand stays
	if (CombatRules && !CombatRules->SurfaceImpactTable.IsNull())
	{
		ImpactTable = CombatRules->SurfaceImpactTable.LoadSynchronous();
	}
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

bool UGSTN_CombatSimComponent::ProbeFloorAhead(float AheadCm, float DepthCm)
{
	FTransform Pose;
	FVector Up;
	const UPrimitiveComponent* Body = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr;
	if (!Body || !Body->GetWorld() || !ReadOwnPose(Pose, Up))
	{
		// No body to probe with: no ledge, the move plays as it would without the track
		return true;
	}
	// The feet: the capsule's current half height (it may be shrunk by the body capsule modifier), else the bounds'
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Body);
	const double HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : Body->Bounds.BoxExtent.Z;
	const FVector Forward = FVector::VectorPlaneProject(Pose.GetRotation().GetForwardVector(), Up).GetSafeNormal();
	const FVector Feet = Pose.GetLocation() + Forward * AheadCm - Up * HalfHeight;
	// From a little above the feet (a slope going up still counts as floor) to DepthCm below them, on the body's own channel and responses
	const FVector Start = Feet + Up * 10.0;
	const FVector End = Feet - Up * DepthCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNLedgeProbe), false, GetOwner());
	FCollisionResponseParams Responses;
	Body->InitSweepCollisionParams(Params, Responses);
	return Body->GetWorld()->LineTraceTestByChannel(Start, End, Body->GetCollisionObjectType(), Params, Responses);
}

bool UGSTN_CombatSimComponent::ProbeTouchdownThisFrame(int32 Frame, float StepSeconds)
{
	if (!CombatRules || !CombatRules->bAnticipateTouchdown)
	{
		return false;
	}
	const UGSTN_MoverSimStateTracker* Tracker = GetJumpTracker();
	const UMoverComponent* Mover = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
	const UPrimitiveComponent* Body = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr;
	FTransform Pose;
	FVector Up;
	if (!Tracker || !Mover || !Body || !Body->GetWorld() || !ReadOwnPose(Pose, Up) || Tracker->HasGameplayTag(Mover_IsOnGround, true))
	{
		return false;
	}
	const double Fall = FGSTN_LandingRules::FallThisFrame(Tracker->GetUpSpeed(), Mover->GetGravityAcceleration().Size(), StepSeconds);
	if (Fall <= 0.0)
	{
		return false;
	}
	// The capsule of the end of the previous frame: the body capsule modifier's shape in the tracked state (the component may still hold another
	// one during a resimulation, the modifier re-applies it only in Mover's tick)
	FGSTN_BodyCapsuleShape Shape;
	if (!FGSTN_BodyCapsuleModifier::GetStanding(*Mover, Shape))
	{
		return false;
	}
	for (auto It = Tracker->GetSyncState().MovementModifiers.GetActiveModifiersIterator(); It; ++It)
	{
		const FMovementModifierBase* Modifier = It->Get();
		if (Modifier && Modifier->GetScriptStruct() == FGSTN_BodyCapsuleModifier::StaticStruct())
		{
			const FGSTN_BodyCapsuleShape& Applied = static_cast<const FGSTN_BodyCapsuleModifier*>(Modifier)->Applied;
			Shape = Applied.HalfHeight > 0.f ? Applied : Shape;
			break;
		}
	}
	const double Margin = CombatRules->TouchdownMarginCm;
	const FVector Start = Pose.GetLocation();
	const FVector End = Start - Up * (Fall + Margin);
	const FQuat Upright = FRotationMatrix::MakeFromZ(Up).ToQuat();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNTouchdownProbe), false, GetOwner());
	FCollisionResponseParams Responses;
	Body->InitSweepCollisionParams(Params, Responses);
	FHitResult Hit;
	const bool bHit = Body->GetWorld()->SweepSingleByChannel(Hit, Start, End, Upright, Body->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Shape.Radius, FMath::Max(Shape.HalfHeight, Shape.Radius)), Params, Responses);
	// Walkable: Mover's default slope limit (about 45 degrees); a face we start inside is Mover's to resolve, not a landing
	constexpr double WalkableCos = 0.71;
	if (!bHit || Hit.bStartPenetrating || Hit.ImpactNormal.Dot(Up) < WalkableCos
		|| !FGSTN_LandingRules::ReachesFloorThisFrame(Tracker->GetUpSpeed(), Mover->GetGravityAcceleration().Size(), StepSeconds, Hit.Distance, Margin))
	{
		return false;
	}
	FrameTouchdown = Frame;
	return true;
}

bool UGSTN_CombatSimComponent::ReadFrameTouchdown() const
{
	return FrameTouchdown != INDEX_NONE && FrameTouchdown == GetSimulationFrame();
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
	LastStepMs = static_cast<float>(TimeStep.StepMS);
	PredictedHits.RemoveAll([Frame](const FGSTN_PredictedHitView& View) { return View.Frame >= Frame || View.Frame < Frame - 240; });
	if (bLog)
	{
		HitLogByFrame.Remove(Frame);
		HitboxTraceByFrame.Remove(Frame);
	}

	FGSTN_AttackerTickInput In;
	In.Frame = Frame;
	In.AttackerId = RegisteredEntityId;
	In.AttackerTeam = ResolveTeam();
	// A remote player's pawn on the authority hits the victims as that player saw them (M4 H10); every other attacker the shared snapshot. The
	// view is built once per frame and shared with the tick extensions that ask for it (GetViewSnapshot)
	In.Snapshot = Core.ActionState == EGSTN_ActionState::InTimeline && Core.RecoilStartFrame == INDEX_NONE ? ResolveViewOnce(TimeStep, Snapshot) : Snapshot;
	In.Rules = CombatRules;
	In.GroupMask = GroupMask;
	In.StepSeconds = TimeStep.StepMS * 0.001f;
	In.DebugShapes = (bLog || bDraw) ? &AttackerDebugShapes : nullptr;
	// A timeline that ended into its recoil has no hitboxes any more
	if (Core.ActionState == EGSTN_ActionState::InTimeline && Core.RecoilStartFrame == INDEX_NONE)
	{
		In.Timeline = GetRegistryTimeline(Tables(), Core.ActiveTimeline);
		In.TimelineFrame = FMath::Max(Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
	}
	if (!In.Timeline || !ReadOwnPose(In.Root, In.Up))
	{
		return;
	}
	// A contributor placed this entity in the step snapshot (a parked link role riding its partner, M6 F12): its moves start from there, not from its
	// capsule, which stays where the link began. The placement of the end of the previous frame, the same bytes on every machine
	if (const FGSTN_SnapshotEntry* Own = In.Snapshot ? In.Snapshot->Find(In.AttackerId) : nullptr; Own && Own->bPlacedByContributor)
	{
		In.Root.SetLocation(Own->Transform.GetLocation());
		In.Root.SetRotation(Own->Transform.GetRotation());
	}

	FGSTN_CombatAttacker::Tick(In, Core, AttackerScratch, AttackerHits);

	UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	int16 BindingOfTimeline = INDEX_NONE;
	if (!AttackerHits.IsEmpty())
	{
		// The binding that started the running timeline, for the tags of the victim's events (the first one that names it)
		const UGSTN_CombatTableSet& Set = Tables();
		for (int32 Index = 0; Index < Set.BindingTimeline.Num(); ++Index)
		{
			if (Set.BindingTimeline[Index] == Core.ActiveTimeline)
			{
				BindingOfTimeline = static_cast<int16>(Index);
				break;
			}
		}
	}
	// A ricochet ends the timeline into its recoil (M4 H12). No hitstop, confirm or meter were given to the attacker (the tick wrote none).
	// A FinishThenRecoil timeline keeps swinging: the tick wrote the hard contact and AdvanceClock starts the recoil after the active frames
	// The behaviour is the hit's hitbox group's (plan precisione-weakpoint, owner 2026-11-09): a move can slash with one group and thrust with another
	for (const FGSTN_AttackerHit& Hit : AttackerHits)
	{
		if (!Hit.bRicochet || Core.RecoilStartFrame != INDEX_NONE)
		{
			continue;
		}
		const FGSTN_StrikeSettings Strike = In.Timeline->ResolveStrike(Hit.TrackIndex);
		if (Strike.Behaviour == EGSTN_HardSurfaceBehaviour::ImmediateRecoil)
		{
			const UGSTN_CombatTableSet& Set = Tables();
			const int16 PreviousStrike = Core.StrikeTrack;
			Core.StrikeTrack = static_cast<int16>(Hit.TrackIndex);
			if (!FGSTN_CombatTimelineLogic::StartRecoil(Core, Frame, GetRegistryTimeline(Set, Core.ActiveTimeline), Set.GetRicochetFallback(Core.ActiveWeaponIndex, Strike.Kind)))
			{
				Core.StrikeTrack = PreviousStrike;
			}
			break;
		}
	}
	for (const FGSTN_AttackerHit& Hit : AttackerHits)
	{
		if (Hit.bReplacedTarget && !bLoggedTargetOverflow)
		{
			bLoggedTargetOverflow = true;
			UE_LOG(LogGSTNCombatAttacker, Warning, TEXT("%s: the hit target ring (%d) is full, the oldest target of the move was replaced; a hitbox group that hits more victims than that can hit the first ones again"),
				*GetPathName(), FGSTN_HitSyncState::MaxHitTargets);
		}
		if (!Hit.bRicochet)
		{
			FGSTN_PredictedHitView& View = PredictedHits.AddDefaulted_GetRef();
			View.Frame = Frame;
			View.VictimId = Hit.VictimId;
			// The ring's key, which the presentation tracker reads back from the targets (a re-hit interval makes each interval its own effect)
			View.HitGroup = Hit.TargetKey;
			View.Timeline = Core.ActiveTimeline;
			View.HitboxIndex = Hit.Entry.AttackerHitboxIndex;
			View.AttackerHitstopFrames = Hit.AttackerHitstopFrames;
			View.bHasKnockback = Hit.bHasKnockback;
			View.Knockback = Hit.Knockback;
			View.CueTag = Hit.CueTag;
			View.HitLocation = Hit.HitLocation;
		}
		// Also on a ricochet: the victim still gets the entry for its cue and event. A victim that does not tick here refuses it
		if (World)
		{
			FGSTN_HitInboxEntry Entry = Hit.Entry;
			Entry.AttackerTimeline = Core.ActiveTimeline;
			Entry.AttackerBinding = BindingOfTimeline;
			World->PostHit(Hit.VictimId, Entry);
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

FString UGSTN_CombatSimComponent::GetInboxLog() const
{
	TArray<int32> Frames;
	InboxLogByFrame.GetKeys(Frames);
	Frames.Sort();
	FString Out;
	for (const int32 Frame : Frames)
	{
		Out += InboxLogByFrame[Frame];
	}
	return Out;
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

FString UGSTN_CombatSimComponent::GetDebugHitboxShapes() const
{
	if (!GSTNCombatDebug::IsHitboxDrawOn() || !GetWorld())
	{
		return TEXT("[]");
	}
	// Same choice as the draw in TickComponent: the open shapes, else the faded linger of the last ones
	const bool bLinger = AttackerDebugShapes.Num() == 0;
	if (bLinger && (LingerDebugShapesTime < 0.0 || GetWorld()->GetTimeSeconds() - LingerDebugShapesTime > GSTNCombatDebug::GetHitboxLingerSeconds()))
	{
		return TEXT("[]");
	}
	const TArray<FGSTN_AttackerDebugShape>& Shapes = bLinger ? LingerDebugShapes : AttackerDebugShapes;
	const FVector Offset = GSTNCombatDebug::GetPresentationOffset(GetOwner());
	TArray<FString> Items;
	for (const FGSTN_AttackerDebugShape& Debug : Shapes)
	{
		FTransform Carrier = Debug.World;
		if (Debug.Space == static_cast<uint8>(EGSTN_HitboxSpace::Root) || Debug.Space == static_cast<uint8>(EGSTN_HitboxSpace::BakedSocket))
		{
			Carrier.AddToTranslation(Offset);
		}
		const FTransform World = Debug.Shape.Local * Carrier;
		const FVector C = World.GetLocation();
		const FQuat Q = World.GetRotation();
		const FVector Scale = World.GetScale3D().GetAbs();
		// Scaled like GSTNCombatDebug::DrawShape: sphere radius by the largest axis, capsule radius by XY and half height by Z
		FVector E = Debug.Shape.Extent * Scale;
		if (Debug.Shape.Type == EGSTN_ShapeType::Sphere)
		{
			E = FVector(Debug.Shape.Extent.X * Scale.GetMax());
		}
		else if (Debug.Shape.Type == EGSTN_ShapeType::Capsule)
		{
			E = FVector(Debug.Shape.Extent.X * FMath::Max(Scale.X, Scale.Y), Debug.Shape.Extent.X * FMath::Max(Scale.X, Scale.Y), Debug.Shape.Extent.Z * Scale.Z);
		}
		Items.Add(FString::Printf(TEXT("{\"i\":%d,\"s\":%d,\"t\":%d,\"c\":[%.1f,%.1f,%.1f],\"q\":[%.4f,%.4f,%.4f,%.4f],\"e\":[%.1f,%.1f,%.1f],\"f\":%d}"),
			Debug.HitboxIndex, Debug.Space, static_cast<int32>(Debug.Shape.Type), C.X, C.Y, C.Z, Q.X, Q.Y, Q.Z, Q.W, E.X, E.Y, E.Z, bLinger ? 1 : 0));
	}
	return TEXT("[") + FString::Join(Items, TEXT(",")) + TEXT("]");
}
