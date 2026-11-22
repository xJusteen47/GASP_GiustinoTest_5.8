// Combat simulation (GAS-NPP fork). Per-move movement capsule (owner 2026-11-15, dropkick).

#include "Movement/GSTN_BodyCapsuleModifier.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "MoveLibrary/MovementUtils.h"
#include "Movement/GSTN_GravityFrame.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Rules/GSTN_GroundCrouch.h"
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

void FGSTN_BodyCapsuleModifier::ApplyPresentation(UMoverComponent& MoverComp, FVector* InOutCorrectedAt)
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
	USceneComponent* Updated = MoverComp.GetUpdatedComponent();
	if (Visual && Visual != Updated && MoverComp.SmoothingMode != EMoverSmoothingMode::VisualComponentOffset)
	{
		const FVector Wanted = MoverComp.GetBaseVisualComponentTransform().GetLocation();
		if (!Visual->GetRelativeLocation().Equals(Wanted, 0.01))
		{
			Visual->SetRelativeLocation(Wanted, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	else if (Visual && Updated && Visual != Updated && Modifier && !Modifier->PresentShift.IsNearlyZero(0.001))
	{
		// The smoothing drew the mesh at base offset x the interpolated actor: the shift of the newest frame is a change of collision, the mesh
		// takes all of it now (2026-11-17: dair V10 v88 sink, dropkick contact frames with the body under the floor or out of the picture)
		const FTransform VisualWorld = Visual->GetComponentTransform();
		if (InOutCorrectedAt && VisualWorld.GetLocation().Equals(*InOutCorrectedAt, 0.001))
		{
			return;  // nothing placed the mesh again since the last correction
		}
		const FTransform Presented = MoverComp.GetBaseVisualComponentTransform().Inverse() * VisualWorld;
		const FVector Correction = FGSTN_BodyCapsule::SmoothingCorrection(Presented.GetLocation(), Modifier->PresentFrameStart, Updated->GetComponentLocation(),
			Modifier->PresentShift);
		if (!Correction.IsNearlyZero(0.01))
		{
			Visual->SetWorldLocation(VisualWorld.GetLocation() + Correction, false, nullptr, ETeleportType::TeleportPhysics);
			if (InOutCorrectedAt)
			{
				*InOutCorrectedAt = Visual->GetComponentLocation();
			}
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
	// Presentation facts of this frame (ApplyPresentation): where it starts, no shift yet
	PresentFrameStart = MoverComp->GetUpdatedComponent() ? MoverComp->GetUpdatedComponent()->GetComponentLocation() : FVector::ZeroVector;
	PresentShift = FVector::ZeroVector;
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
	// Ground crouch: the combat tick of this frame decided it (none decided = keep the stance, e.g. no combat tick ran for the frame). Stored now,
	// before any early return: this instance is the frame's output state
	bool bCrouchNow = bCrouched;
	float CrouchHalfHeight = 0.f;
	float CrouchRadius = 0.f;
	if (Combat && !Combat->ReadFrameGroundCrouch(bCrouchNow, CrouchHalfHeight, CrouchRadius))
	{
		bCrouchNow = bCrouched;
	}
	bCrouched = bCrouchNow;
	FGSTN_BodyCapsuleShape CrouchShape;
	if (bCrouchNow && CrouchHalfHeight > 0.f)
	{
		CrouchShape = FGSTN_GroundCrouchKitExtension::MakeCapsule(CrouchHalfHeight, CrouchRadius, Standing);
	}
	// On the floor the standing capsule grows from the floor (GroundedRestore); the ground fact is the frame's input sync state, as every reader sees it,
	// or the touchdown this frame's combat tick anticipated (UGSTN_CombatRules::bAnticipateTouchdown: the move ended on the frame of contact)
	const bool bGrounded = MoverComp->HasGameplayTag(Mover_IsOnGround, true) || (Combat && Combat->ReadFrameTouchdown());
	const FGSTN_BodyCapsuleTarget Target = FGSTN_BodyCapsule::GroundedRestore(
		FGSTN_BodyCapsule::ResolveTarget(Data, TimelineFrame, Standing, CrouchShape.HalfHeight > 0.f ? &CrouchShape : nullptr), Standing, bGrounded);
	if (Target.Shape.NearlyEquals(Current))
	{
		return;
	}

	FGSTN_BodyCapsuleStep Step = FGSTN_BodyCapsule::Step(Current, Target);
	FGSTN_BodyCapsule::KeepBottomOnFloor(Step, Current, bGrounded);
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

namespace GSTNBodyCapsule
{
	/** A swept move of the updated component (only the world blocks: fighters ignore each other), once more from a hair above when it starts touching the floor */
	static void SweptHorizontalMove(USceneComponent& Updated, const FVector& Move)
	{
		const FVector Start = Updated.GetComponentLocation();
		FHitResult Hit;
		Updated.SetWorldLocation(Start + Move, /*bSweep*/ true, &Hit, ETeleportType::None);
		if (Hit.bStartPenetrating)
		{
			const FVector Lift(0.0, 0.0, 0.5);
			Updated.SetWorldLocation(Start + Lift, true, nullptr, ETeleportType::None);
			Updated.SetWorldLocation(Updated.GetComponentLocation() + Move, true, nullptr, ETeleportType::None);
			Updated.SetWorldLocation(Updated.GetComponentLocation() - Lift, true, nullptr, ETeleportType::None);
		}
	}
}

void FGSTN_BodyCapsuleModifier::OnPostMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	Super::OnPostMovement(MoverComp, TimeStep, SyncState, AuxState);
	const AActor* Owner = MoverComp ? MoverComp->GetOwner() : nullptr;
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	USceneComponent* Updated = MoverComp ? MoverComp->GetUpdatedComponent() : nullptr;
	// The frame's output state (MovementModeStateMachine passes OutputState.SyncState); writing it is what makes the correction part of the frame
	FMoverDefaultSyncState* Sync = const_cast<FMoverSyncState&>(SyncState).SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!Combat || !Updated || !Sync)
	{
		return;
	}
	FVector Velocity = Sync->GetVelocity_WorldSpace();
	const FVector OriginalVelocity = Velocity;
	const FVector Correction = Combat->ConstrainFrameMove(Updated->GetComponentLocation(), Velocity);
	if (Correction.IsNearlyZero(0.001) && Velocity.Equals(OriginalVelocity, 0.001))
	{
		return;
	}
	if (!Correction.IsNearlyZero(0.001))
	{
		GSTNBodyCapsule::SweptHorizontalMove(*Updated, Correction);
	}
	Sync->SetTransforms_WorldSpace(Updated->GetComponentLocation(), Sync->GetOrientation_WorldSpace(), Velocity,
		Sync->GetAngularVelocityDegrees_WorldSpace(), Sync->GetMovementBase(), Sync->GetMovementBaseBoneName());
}

FMovementModifierBase* FGSTN_BodyCapsuleModifier::Clone() const
{
	return new FGSTN_BodyCapsuleModifier(*this);
}

void FGSTN_BodyCapsuleModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	GSTNBodyCapsule::SerializeShape(Ar, Applied);
	uint8 CrouchedFlag = bCrouched ? 1 : 0;
	Ar << CrouchedFlag;
	bCrouched = CrouchedFlag != 0;
}

bool FGSTN_BodyCapsuleModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (!bCrouched)
	{
		return false;
	}
	const FGameplayTag Tag = Mover_IsCrouching.GetTag();
	return bExactMatch ? Tag.MatchesTagExact(TagToFind) : Tag.MatchesTag(TagToFind);
}

void FGSTN_BodyCapsuleModifier::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	if (bCrouched)
	{
		InOutTags.AddTag(Mover_IsCrouching.GetTag());
	}
}

bool FGSTN_BodyCapsuleModifier::IsCrouchedIn(const FMoverSyncState& SyncState)
{
	for (auto It = SyncState.MovementModifiers.GetActiveModifiersIterator(); It; ++It)
	{
		const FMovementModifierBase* Modifier = It->Get();
		if (Modifier && Modifier->GetScriptStruct() == FGSTN_BodyCapsuleModifier::StaticStruct())
		{
			return static_cast<const FGSTN_BodyCapsuleModifier*>(Modifier)->bCrouched;
		}
	}
	return false;
}

UScriptStruct* FGSTN_BodyCapsuleModifier::GetScriptStruct() const
{
	return FGSTN_BodyCapsuleModifier::StaticStruct();
}

FString FGSTN_BodyCapsuleModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Body Capsule hh %.1f r %.1f off %s%s"), Applied.HalfHeight, Applied.Radius, *Applied.Offset.ToCompactString(), bCrouched ? TEXT(" crouched") : TEXT(""));
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

	// The shift is in the capsule's upright frame: its yaw about the local up, on the gravity floor (gravity audit 2026-11-16; world up gives
	// the old yaw-only frame exactly)
	const FVector Up = MoverComp->GetUpDirection();
	const FRotator Orientation = Sync->GetOrientation_WorldSpace();
	const FVector ShiftWorld = FGSTN_GravityFrame::IsWorldUp(Up)
		? FVector(FRotationMatrix(FRotator(0.0, Orientation.Yaw, 0.0)).TransformVector(ShiftLocal))
		: FGSTN_GravityFrame::UprightRotation(Orientation.Quaternion(), Up).RotateVector(ShiftLocal);
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
	const FVector Before = Updated->GetComponentLocation();
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
	// The move really made (a sweep may have shortened it), for the presentation of this frame: the modifier of this output state
	for (auto It = OutputState.MovementModifiers.GetActiveModifiersIterator(); It; ++It)
	{
		FMovementModifierBase* Modifier = It->Get();
		if (Modifier && Modifier->GetScriptStruct() == FGSTN_BodyCapsuleModifier::StaticStruct())
		{
			static_cast<FGSTN_BodyCapsuleModifier*>(Modifier)->PresentShift += Updated->GetComponentLocation() - Before;
			break;
		}
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
	GSTNBodyCapsule::SweptHorizontalMove(*Updated, Move);
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