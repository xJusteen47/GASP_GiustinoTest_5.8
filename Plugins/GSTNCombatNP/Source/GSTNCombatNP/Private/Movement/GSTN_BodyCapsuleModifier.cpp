// Combat simulation (GAS-NPP fork). Per-move movement capsule (owner 2026-11-15, dropkick).

#include "Movement/GSTN_BodyCapsuleModifier.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_BodyCapsuleModifier)

namespace GSTNBodyCapsule
{
	void SerializeShape(FArchive& Ar, FGSTN_BodyCapsuleShape& Shape)
	{
		Ar << Shape.HalfHeight;
		Ar << Shape.Radius;
		Ar << Shape.Offset;
	}

	/** Applied with HalfHeight 0 (never changed) is the standing capsule */
	FGSTN_BodyCapsuleShape Resolve(const FGSTN_BodyCapsuleShape& Applied, const FGSTN_BodyCapsuleShape& Standing)
	{
		return Applied.HalfHeight > 0.f ? Applied : Standing;
	}
}

FGSTN_BodyCapsuleModifier::FGSTN_BodyCapsuleModifier()
{
	// Never ends
	DurationMs = -1.f;
}

bool FGSTN_BodyCapsuleModifier::GetStanding(const UMoverComponent& MoverComp, FGSTN_BodyCapsuleShape& OutStanding)
{
	const UCapsuleComponent* Original = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(MoverComp.GetOwner());
	if (!Original || !Cast<UCapsuleComponent>(MoverComp.GetUpdatedComponent()))
	{
		return false;
	}
	OutStanding = FGSTN_BodyCapsuleShape::Standing(Original->GetUnscaledCapsuleHalfHeight(), Original->GetUnscaledCapsuleRadius());
	return true;
}

bool FGSTN_BodyCapsuleModifier::ApplyShape(UMoverComponent& MoverComp, const FGSTN_BodyCapsuleShape& Shape, const FGSTN_BodyCapsuleShape& Standing)
{
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(MoverComp.GetUpdatedComponent());
	if (!Capsule)
	{
		return false;
	}
	bool bChanged = false;
	if (!FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleHalfHeight(), Shape.HalfHeight, 0.01f)
		|| !FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), Shape.Radius, 0.01f))
	{
		Capsule->SetCapsuleSize(Shape.Radius, Shape.HalfHeight, /*bUpdateOverlaps*/ false);
		bChanged = true;
	}

	// Mesh root at actor - Offset: the archetype's relative location already holds the standing offset (root at the bottom)
	USceneComponent* Visual = MoverComp.GetPrimaryVisualComponent();
	const USceneComponent* Archetype = Visual ? Cast<USceneComponent>(Visual->GetArchetype()) : nullptr;
	if (Visual && Archetype && Archetype != Visual)
	{
		FTransform Base = MoverComp.GetBaseVisualComponentTransform();
		const FVector Wanted = Archetype->GetRelativeLocation() + (Standing.Offset - Shape.Offset);
		if (!Base.GetLocation().Equals(Wanted, 0.01))
		{
			const FVector BaseDelta = Wanted - Base.GetLocation();
			Base.SetLocation(Wanted);
			MoverComp.SetBaseVisualComponentTransform(Base);
			// The drawn mesh moves with its base now, as the stance modifier does (FStanceModifier::AdjustCapsule). Mover only re-places it from
			// the base in its next finalize, a frame after the clip already shows the new part (dropkick landing, v106: the land pose drawn
			// with the air offset, the body out of the picture). Shifting by the base's change keeps whatever smoothing offset the mesh has
			if (Visual != MoverComp.GetUpdatedComponent())
			{
				Visual->SetRelativeLocation(Visual->GetRelativeLocation() + BaseDelta, false, nullptr, ETeleportType::TeleportPhysics);
			}
			bChanged = true;
		}
	}
	return bChanged;
}

void FGSTN_BodyCapsuleModifier::ApplyPresentation(UMoverComponent& MoverComp)
{
	FGSTN_BodyCapsuleShape Standing;
	if (!GetStanding(MoverComp, Standing))
	{
		return;
	}
	const FGSTN_BodyCapsuleModifier* Modifier = MoverComp.FindMovementModifierByType<FGSTN_BodyCapsuleModifier>();
	const FGSTN_BodyCapsuleShape Shape = Modifier ? GSTNBodyCapsule::Resolve(Modifier->Applied, Standing) : Standing;
	ApplyShape(MoverComp, Shape, Standing);
	// With VisualComponentOffset smoothing Mover places the mesh from the base visual transform itself; otherwise the mesh only follows its
	// relative location, which nothing else rewrites
	USceneComponent* Visual = MoverComp.GetPrimaryVisualComponent();
	if (Visual && Visual != MoverComp.GetUpdatedComponent() && MoverComp.SmoothingMode != EMoverSmoothingMode::VisualComponentOffset)
	{
		const FVector Wanted = MoverComp.GetBaseVisualComponentTransform().GetLocation();
		if (!Visual->GetRelativeLocation().Equals(Wanted, 0.01))
		{
			Visual->SetRelativeLocation(Wanted, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void FGSTN_BodyCapsuleModifier::OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep)
{
	FGSTN_BodyCapsuleShape Standing;
	if (!MoverComp || !GetStanding(*MoverComp, Standing))
	{
		return;
	}
	const FGSTN_BodyCapsuleShape Current = GSTNBodyCapsule::Resolve(Applied, Standing);
	// The component properties follow the (maybe restored) state before anything moves this substep
	ApplyShape(*MoverComp, Current, Standing);

	int32 TimelineFrame = INDEX_NONE;
	const UGSTN_CombatSimComponent* Combat = MoverComp->GetOwner() ? MoverComp->GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;

	// The pushbox push of this frame (the combat tick computed it before Mover's tick), on its own substep like the capsule shift. Queued before
	// the capsule's early return: a fighter whose capsule does not change is pushed too
	const FVector Push = Combat ? Combat->ReadFramePush() : FVector::ZeroVector;
	if (!Push.IsNearlyZero(0.001))
	{
		TSharedPtr<FGSTN_PushboxShiftEffect> PushEffect = MakeShared<FGSTN_PushboxShiftEffect>();
		PushEffect->Move = Push;
		MoverComp->QueueInstantMovementEffectThisStep_Internal(TimeStep, PushEffect);
	}

	const FGSTN_CombatTimelineData* Data = Combat ? Combat->ReadEvaluatedTimeline(TimelineFrame) : nullptr;
	const FGSTN_BodyCapsuleTarget Target = FGSTN_BodyCapsule::ResolveTarget(Data, TimelineFrame, Standing);
	if (Target.Shape.NearlyEquals(Current))
	{
		return;
	}

	const FGSTN_BodyCapsuleStep Step = FGSTN_BodyCapsule::Step(Current, Target);
	TSharedPtr<FGSTN_BodyCapsuleShiftEffect> Effect = MakeShared<FGSTN_BodyCapsuleShiftEffect>();
	Effect->From = Current;
	Effect->To = Step.Applied;
	Effect->ShiftLocal = Step.ActorShiftLocal;
	Effect->bSweep = Target.Anchor == EGSTN_BodyCapsuleAnchor::Body;
	// Scheduled on this substep's own frame: applied after the modifiers, before the movement mode moves (MovementModeStateMachine).
	// QueueInstantMovementEffect_Internal would add Mover's network scheduling delay (0.3 s): the capsule then changed size frames before the
	// actor moved (dropkick end, v137: grown 35 -> 86 in place, under the floor, popped out; the +51 cm arrived at v154)
	MoverComp->QueueInstantMovementEffectThisStep_Internal(TimeStep, Effect);
	// Stored now (this instance is the frame's output state): a resimulated frame starts again from the restored value
	Applied = Step.Applied.NearlyEquals(Standing) ? FGSTN_BodyCapsuleShape() : Step.Applied;
}

FMovementModifierBase* FGSTN_BodyCapsuleModifier::Clone() const
{
	return new FGSTN_BodyCapsuleModifier(*this);
}

void FGSTN_BodyCapsuleModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	GSTNBodyCapsule::SerializeShape(Ar, Applied);
}

UScriptStruct* FGSTN_BodyCapsuleModifier::GetScriptStruct() const
{
	return FGSTN_BodyCapsuleModifier::StaticStruct();
}

FString FGSTN_BodyCapsuleModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Body Capsule hh %.1f r %.1f off %s"), Applied.HalfHeight, Applied.Radius, *Applied.Offset.ToCompactString());
}

bool FGSTN_BodyCapsuleShiftEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	USceneComponent* Updated = ApplyEffectParams.UpdatedComponent;
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Updated);
	FMoverDefaultSyncState* Sync = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	// The params hand a const Mover component (5.9); the effect only resizes the capsule and the mesh offset it owns, as the modifier does
	UMoverComponent* MoverComp = const_cast<UMoverComponent*>(ToRawPtr(ApplyEffectParams.MoverComp));
	if (!Capsule || !Sync || !MoverComp)
	{
		return false;
	}
	FGSTN_BodyCapsuleShape Standing;
	if (!FGSTN_BodyCapsuleModifier::GetStanding(*MoverComp, Standing))
	{
		return false;
	}

	const FRotator Orientation = Sync->GetOrientation_WorldSpace();
	const FVector ShiftWorld = FRotationMatrix(FRotator(0.0, Orientation.Yaw, 0.0)).TransformVector(ShiftLocal);
	const bool bGrowing = To.HalfHeight > From.HalfHeight || To.Radius > From.Radius;

	// Growing like the stance modifier's expand (FStanceModifier::CanExpand + its teleport): when the grown capsule fits at the destination
	// the actor is set there directly. A sweep of the small capsule can start touching the floor (the land capsule's bottom sits on or a
	// little in it) and then refuses to move at all. When it does not fit, the swept move (Body anchor) keeps walls and ceilings
	bool bSweepMove = bSweep;
	if (bGrowing && bSweep && !ShiftWorld.IsNearlyZero())
	{
		UPrimitiveComponent* Primitive = ApplyEffectParams.UpdatedPrimitive ? ApplyEffectParams.UpdatedPrimitive : Capsule;
		const FCollisionShape Grown = FCollisionShape::MakeCapsule(To.Radius, To.HalfHeight);
		const bool bEncroached = UMovementUtils::OverlapTest(Updated, Primitive, Updated->GetComponentLocation() + ShiftWorld,
			Updated->GetComponentQuat(), Primitive->GetCollisionObjectType(), Grown, MoverComp->GetOwner());
		bSweepMove = bEncroached;
	}
	auto Move = [&]()
	{
		if (ShiftWorld.IsNearlyZero())
		{
			return;
		}
		FHitResult Hit;
		Updated->SetWorldLocation(Updated->GetComponentLocation() + ShiftWorld, bSweepMove, bSweepMove ? &Hit : nullptr, ETeleportType::TeleportPhysics);
	};
	if (bGrowing)
	{
		Move();
		FGSTN_BodyCapsuleModifier::ApplyShape(*MoverComp, To, Standing);
	}
	else
	{
		FGSTN_BodyCapsuleModifier::ApplyShape(*MoverComp, To, Standing);
		Move();
	}

	// The modifier in this output state already holds To (it queued this effect on the same substep): nothing to write there
	Sync->SetTransforms_WorldSpace(Updated->GetComponentLocation(), Orientation, Sync->GetVelocity_WorldSpace(),
		Sync->GetAngularVelocityDegrees_WorldSpace(), Sync->GetMovementBase(), Sync->GetMovementBaseBoneName());
	// True = the sync state changed: Mover copies it into the state the movement mode starts from
	return true;
}

FInstantMovementEffect* FGSTN_BodyCapsuleShiftEffect::Clone() const
{
	return new FGSTN_BodyCapsuleShiftEffect(*this);
}

void FGSTN_BodyCapsuleShiftEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	GSTNBodyCapsule::SerializeShape(Ar, From);
	GSTNBodyCapsule::SerializeShape(Ar, To);
	Ar << ShiftLocal;
	Ar << bSweep;
}

UScriptStruct* FGSTN_BodyCapsuleShiftEffect::GetScriptStruct() const
{
	return FGSTN_BodyCapsuleShiftEffect::StaticStruct();
}

FString FGSTN_BodyCapsuleShiftEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Body Capsule shift %s hh %.1f->%.1f"), *ShiftLocal.ToCompactString(), From.HalfHeight, To.HalfHeight);
}

bool FGSTN_PushboxShiftEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	USceneComponent* Updated = ApplyEffectParams.UpdatedComponent;
	FMoverDefaultSyncState* Sync = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!Updated || !Sync || Move.IsNearlyZero())
	{
		return false;
	}
	// Swept: the world stops it. Other fighters are in the capsule's ignore list (UGSTN_CombatWorldSubsystem), so only the world can
	const FVector Start = Updated->GetComponentLocation();
	FHitResult Hit;
	Updated->SetWorldLocation(Start + Move, /*bSweep*/ true, &Hit, ETeleportType::None);
	if (Hit.bStartPenetrating)
	{
		// Started touching something (the floor under a grounded capsule sitting on it): once more from a hair above, then back down swept
		const FVector Lift(0.0, 0.0, 0.5);
		Updated->SetWorldLocation(Start + Lift, true, nullptr, ETeleportType::None);
		Updated->SetWorldLocation(Updated->GetComponentLocation() + Move, true, nullptr, ETeleportType::None);
		Updated->SetWorldLocation(Updated->GetComponentLocation() - Lift, true, nullptr, ETeleportType::None);
	}
	Sync->SetTransforms_WorldSpace(Updated->GetComponentLocation(), Sync->GetOrientation_WorldSpace(), Sync->GetVelocity_WorldSpace(),
		Sync->GetAngularVelocityDegrees_WorldSpace(), Sync->GetMovementBase(), Sync->GetMovementBaseBoneName());
	return true;
}

FInstantMovementEffect* FGSTN_PushboxShiftEffect::Clone() const
{
	return new FGSTN_PushboxShiftEffect(*this);
}

void FGSTN_PushboxShiftEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << Move;
}

UScriptStruct* FGSTN_PushboxShiftEffect::GetScriptStruct() const
{
	return FGSTN_PushboxShiftEffect::StaticStruct();
}

FString FGSTN_PushboxShiftEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Pushbox shift %s"), *Move.ToCompactString());
}