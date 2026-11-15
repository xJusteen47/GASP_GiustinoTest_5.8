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
	const USceneComponent* Visual = MoverComp.GetPrimaryVisualComponent();
	const USceneComponent* Archetype = Visual ? Cast<USceneComponent>(Visual->GetArchetype()) : nullptr;
	if (Visual && Archetype && Archetype != Visual)
	{
		FTransform Base = MoverComp.GetBaseVisualComponentTransform();
		const FVector Wanted = Archetype->GetRelativeLocation() + (Standing.Offset - Shape.Offset);
		if (!Base.GetLocation().Equals(Wanted, 0.01))
		{
			Base.SetLocation(Wanted);
			MoverComp.SetBaseVisualComponentTransform(Base);
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
	// Scheduled on this substep's frame: applied after the modifiers, before the movement mode moves (MovementModeStateMachine)
	MoverComp->QueueInstantMovementEffect_Internal(TimeStep, Effect);
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

	auto Move = [&]()
	{
		if (ShiftWorld.IsNearlyZero())
		{
			return;
		}
		FHitResult Hit;
		Updated->SetWorldLocation(Updated->GetComponentLocation() + ShiftWorld, bSweep, bSweep ? &Hit : nullptr, ETeleportType::TeleportPhysics);
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
