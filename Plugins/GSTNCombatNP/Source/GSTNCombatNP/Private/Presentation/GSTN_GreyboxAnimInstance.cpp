#include "Presentation/GSTN_GreyboxAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "EngineLogs.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Presentation/GSTN_RunPresentationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_GreyboxAnimInstance)

void FGSTN_GreyboxSpring::Step(float Target, float DeltaSeconds, float FrequencyHz, float Damping)
{
	// Sub-steps keep the spring stable on a long frame (a hitch must not explode the capsule)
	const float Omega = 2.f * PI * FMath::Max(FrequencyHz, 0.01f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 120.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const float Accel = Omega * Omega * (Target - Value) - 2.f * Damping * Omega * Velocity;
		Velocity += Accel * Dt;
		Value += Velocity * Dt;
	}
}

float UGSTN_GreyboxAnimInstance::SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration)
{
	if (!bOnGround)
	{
		return FMath::Min(1.f + Settings.AirStretchPer1000 * FMath::Abs(VelocityZ) / 1000.f, Settings.MaxStretch);
	}
	// Only a speed gain squashes (the body gathers to push); braking reads through the lean
	return FMath::Max(1.f - Settings.AccelSquashPer1000 * FMath::Max(ForwardAcceleration, 0.f) / 1000.f, Settings.MinSquash);
}

float UGSTN_GreyboxAnimInstance::PelvisSquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float PelvisHeight,
	float ReferenceHeight)
{
	if (!bOnGround || ReferenceHeight <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	// Only lowering squashes: a pelvis above the reference (a hop inside a clip) keeps the capsule at rest height
	return FMath::Clamp(PelvisHeight / ReferenceHeight, FMath::Min(Settings.MinPelvisSquash, 1.f), 1.f);
}

float UGSTN_GreyboxAnimInstance::LimbScaleTarget(const FGSTN_GreyboxShapeSettings& Settings, float AttackWeight)
{
	const float Weight = FMath::Clamp(AttackWeight, 0.f, 1.f);
	const float Locomotion = Settings.bShowLimbsInLocomotion ? 1.f : 0.f;
	const float Moves = Settings.bShowLimbsInMoves ? 1.f : 0.f;
	return FMath::Lerp(Locomotion, Moves, Weight);
}

float UGSTN_GreyboxAnimInstance::FranticAlpha(const FGSTN_GreyboxShapeSettings& Settings, float TimeSinceStart)
{
	if (TimeSinceStart < 0.f)
	{
		return 0.f;
	}
	return 1.f - FMath::Clamp((TimeSinceStart - Settings.FranticSeconds) / FMath::Max(Settings.FranticFadeSeconds, 0.01f), 0.f, 1.f);
}

float UGSTN_GreyboxAnimInstance::SwayCadenceHz(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic)
{
	const float BySpeed = FMath::Max(Speed2D, 0.f) / FMath::Max(Settings.StrideLength, 1.f);
	return FMath::Lerp(BySpeed, FMath::Max(Settings.FranticCadenceHz, BySpeed), FMath::Clamp(Frantic, 0.f, 1.f));
}

FVector UGSTN_GreyboxAnimInstance::RunSway(const FGSTN_GreyboxShapeSettings& Settings, float Phase, float Amplitude, float& OutPitch)
{
	// Footfalls at phase 0 and PI: the bob dips and the nod tips forward there (|sin| shapes a bounce, not a sine wobble); the rock and
	// the twist swing once per cycle, toward the stepping side
	const float Footfall = 1.f - FMath::Abs(FMath::Sin(Phase));
	OutPitch = -Settings.NodPitchDeg * Footfall * Amplitude;
	return FVector(-Settings.BobSquash * Footfall * Amplitude, Settings.RockRollDeg * FMath::Cos(Phase) * Amplitude,
		Settings.TwistYawDeg * FMath::Sin(Phase) * Amplitude);
}

bool UGSTN_GreyboxAnimInstance::StartsRunStop(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float Speed2D,
	float ForwardAcceleration, bool bAlreadyKicked)
{
	return Settings.RunStopKickDeg > 0.f && bOnGround && !bAlreadyKicked && Speed2D >= Settings.RunStopMinSpeed
		&& ForwardAcceleration <= -Settings.RunStopDecel;
}

FRotator UGSTN_GreyboxAnimInstance::LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration)
{
	// Forward (start / brake) and sideways (curves) separately: a curve leans harder than a start, with its own cap
	const FVector2D Lean(FMath::Clamp(LocalAcceleration.X * Settings.LeanDegPer1000 / 1000.f, -Settings.MaxLeanDeg, Settings.MaxLeanDeg),
		FMath::Clamp(LocalAcceleration.Y * Settings.TurnLeanDegPer1000 / 1000.f, -Settings.MaxTurnLeanDeg, Settings.MaxTurnLeanDeg));
	// Pitch = minus the forward lean (Unreal pitch up = top back), roll = the lean to the right (+Y)
	return FRotator(-Lean.X, 0.f, Lean.Y);
}

void UGSTN_GreyboxAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const AActor* Owner = GetOwningActor();
	Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	SquashSpring.Value = 1.f;
	SquashSpring.Velocity = 0.f;
	bHasPrevious = false;
	Shape.LimbScale = ShapeSettings.bShowLimbsInLocomotion ? 1.f : 0.f;
}

void UGSTN_GreyboxAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const AActor* Owner = GetOwningActor();
	if (!Owner || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (!Mover.IsValid())
	{
		Mover = Owner->FindComponentByClass<UMoverComponent>();
	}
	const UMoverComponent* MoverComponent = Mover.Get();
	const FVector Velocity = MoverComponent ? MoverComponent->GetVelocity() : Owner->GetVelocity();
	const bool bOnGround = MoverComponent ? MoverComponent->HasGameplayTag(Mover_IsOnGround, true) : true;

	FVector Acceleration = FVector::ZeroVector;
	if (bHasPrevious)
	{
		Acceleration = (Velocity - PreviousVelocity) / DeltaSeconds;
		Acceleration.Z = 0.f;
	}
	PreviousVelocity = Velocity;
	bHasPrevious = true;

	// Touchdown: kick the squash spring by the fall speed, so it squashes and bounces back
	if (bOnGround && !bWasOnGround && LastAirVelocityZ < 0.f)
	{
		const float Omega = 2.f * PI * ShapeSettings.SpringFrequency;
		SquashSpring.Velocity -= ShapeSettings.LandSquashPer1000 * (-LastAirVelocityZ / 1000.f) * Omega;
	}
	if (!bOnGround)
	{
		LastAirVelocityZ = Velocity.Z;
	}
	bWasOnGround = bOnGround;

	// Skid: hold the old facing while the run presentation skids, then turn on a spring (the capsule's yaw offset from the actor)
	if (!RunPresentation.IsValid())
	{
		RunPresentation = Owner->FindComponentByClass<UGSTN_RunPresentationComponent>();
	}
	const bool bSkidding = ShapeSettings.bSkidHoldsFacing && RunPresentation.IsValid() && RunPresentation->IsSkidding();
	const float ActorYaw = Owner->GetActorRotation().Yaw;
	bool bSkidStarted = false;
	if (bSkidding && !bWasSkidding)
	{
		// The velocity still points along the old run on the skid's first frame (the actor may already face the new way)
		const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
		SkidHeldYaw = Horizontal.SizeSquared() > 100.f ? Horizontal.Rotation().Yaw : ActorYaw + TurnSpring.Value;
		bSkidStarted = true;
	}
	if (bSkidding)
	{
		TurnSpring.Value = FMath::FindDeltaAngleDegrees(ActorYaw, SkidHeldYaw);
		TurnSpring.Velocity = 0.f;
	}
	else
	{
		if (bWasSkidding)
		{
			TimeSinceStart = 0.f;  // the turn ends in a fresh (frantic) start
		}
		TurnSpring.Step(0.f, DeltaSeconds, ShapeSettings.SkidTurnFrequency, ShapeSettings.SkidTurnDamping);
	}
	bWasSkidding = bSkidding;
	// The body's frame: the actor turned by the offset; leans and the stop plant read the acceleration in it
	const FQuat BodyQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(TurnSpring.Value)) * Owner->GetActorQuat();

	const FVector Local = BodyQuat.UnrotateVector(Acceleration);
	const FRotator LeanGoal = bOnGround ? LeanTarget(ShapeSettings, Local) : FRotator::ZeroRotator;
	SquashSpring.Step(SquashTarget(ShapeSettings, bOnGround, Velocity.Z, Local.X), DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);
	PitchSpring.Step(LeanGoal.Pitch, DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);
	RollSpring.Step(LeanGoal.Roll, DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);

	// Run stop plant: one backward kick into the pitch spring (positive pitch = top back), re-armed once still or speeding up again
	const float GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	if ((bSkidStarted && !bRunStopKicked && ShapeSettings.RunStopKickDeg > 0.f) || StartsRunStop(ShapeSettings, bOnGround, GroundSpeed, Local.X, bRunStopKicked))
	{
		// For this damping the spring's first peak is about 0.6 of Velocity / Omega
		PitchSpring.Velocity += ShapeSettings.RunStopKickDeg * 2.f * PI * ShapeSettings.SpringFrequency / 0.6f;
		bRunStopKicked = true;
	}
	else if (GroundSpeed < 10.f || Local.X > 200.f)
	{
		bRunStopKicked = false;
	}

	// Run sway: cadence from the speed, or the frantic cadence right after leaving rest; amplitude from the speed, eased
	const float Speed2D = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	const bool bMoving = bOnGround && Speed2D > 10.f;
	TimeSinceStart = bMoving ? (TimeSinceStart < 0.f ? 0.f : TimeSinceStart + DeltaSeconds) : -1.f;
	if (bSkidding)
	{
		TimeSinceStart = -1.f;  // no frantic sway while sliding; it starts when the turn does
	}
	const float Frantic = FranticAlpha(ShapeSettings, TimeSinceStart);
	const float AmplitudeGoal = bMoving ? FMath::Max(FMath::Clamp(Speed2D / FMath::Max(ShapeSettings.FullSwaySpeed, 1.f), 0.f, 1.f),
		Frantic * ShapeSettings.FranticAmplitude) : 0.f;
	SwayAmplitude = FMath::FInterpTo(SwayAmplitude, AmplitudeGoal, DeltaSeconds, 10.f);
	SwayPhase = FMath::Fmod(SwayPhase + 2.f * PI * SwayCadenceHz(ShapeSettings, Speed2D, Frantic) * DeltaSeconds, 2.f * PI);
	float SwayPitch = 0.f;
	const FVector Sway = RunSway(ShapeSettings, SwayPhase, SwayAmplitude, SwayPitch);

	Shape.Squash = FMath::Clamp(SquashSpring.Value + Sway.X, ShapeSettings.MinSquash * 0.8f, ShapeSettings.MaxStretch * 1.1f);
	// The lean is in the component's frame; the mesh is rotated -90 yaw from the actor in GASP pawns, so turn the actor-frame lean into it
	const FQuat ActorToComponent = GetSkelMeshComponent() ? (GetSkelMeshComponent()->GetComponentQuat().Inverse() * BodyQuat) : FQuat::Identity;
	// Rotation axis scaled by degrees, actor frame: tipping the top toward +X is a turn around +Y by the forward lean (-Pitch), toward +Y a
	// turn around -X by the side lean (Roll)
	const float Roll = RollSpring.Value + Sway.Y;
	const float Pitch = PitchSpring.Value + SwayPitch;
	const FVector Axis = ActorToComponent.RotateVector(FVector(-Roll, -Pitch, 0.f));
	const FQuat Tip = Axis.IsNearlyZero() ? FQuat::Identity : FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(Axis.Size()));
	// Twist about the up axis (same in actor and component frames), under the tip
	Shape.Lean = (Tip * FQuat(FVector::UpVector, FMath::DegreesToRadians(TurnSpring.Value + Sway.Z))).Rotator();

	float PelvisHeight = 0.f;
	float PelvisReference = 0.f;
	const bool bHasPelvis = GetDriverPelvisHeight(PelvisHeight, PelvisReference);
	const float PelvisGoal = bHasPelvis ? PelvisSquashTarget(ShapeSettings, bOnGround, PelvisHeight, PelvisReference) : 1.f;
	// Follows the pose directly (no spring: the clip already eases), blended only where it switches on or off
	Shape.PelvisSquash = FMath::FInterpTo(Shape.PelvisSquash, PelvisGoal, DeltaSeconds, ShapeSettings.PelvisSquashBlendSpeed);

	UpdateMoveOverride();
	// Bones only where strictly needed, in attacks (owner 2026-11-24): the body follows the pose and the markers show only while an
	// attack's capsule clip plays; everything else (locomotion, hit reactions, rolls) is the rigid capsule swaying
	Shape.PoseWeight = Shape.OverrideWeight;
	Shape.LimbScale = FMath::FInterpTo(Shape.LimbScale, LimbScaleTarget(ShapeSettings, Shape.OverrideWeight), DeltaSeconds,
		ShapeSettings.LimbBlendSpeed);
}

bool UGSTN_GreyboxAnimInstance::GetDriverPelvisHeight(float& OutHeight, float& OutReference)
{
	static const FName PelvisName(TEXT("pelvis"));
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const USkeletalMeshComponent* Driver = Capsule ? Cast<USkeletalMeshComponent>(Capsule->GetAttachParent()) : nullptr;
	const USkeletalMesh* DriverMesh = Driver ? Driver->GetSkeletalMeshAsset() : nullptr;
	if (!DriverMesh)
	{
		return false;
	}
	if (PelvisReferenceMesh.Get() != DriverMesh)
	{
		// A skin swap changes the driver mesh: measure its reference pelvis again (component space = above the feet in GASP pawns)
		const FReferenceSkeleton& RefSkeleton = DriverMesh->GetRefSkeleton();
		const int32 Index = RefSkeleton.FindBoneIndex(PelvisName);
		PelvisReferenceMesh = DriverMesh;
		PelvisReferenceHeight = Index != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Index).GetLocation().Z : 0.f;
	}
	if (PelvisReferenceHeight <= KINDA_SMALL_NUMBER || Driver->GetBoneIndex(PelvisName) == INDEX_NONE)
	{
		return false;
	}
	OutHeight = Driver->GetSocketTransform(PelvisName, RTS_Component).GetLocation().Z;
	OutReference = PelvisReferenceHeight;
	return true;
}

void UGSTN_GreyboxAnimInstance::UpdateMoveOverride()
{
	Shape.OverrideClip = nullptr;
	Shape.OverrideWeight = 0.f;
	const USkeletalMeshComponent* Capsule = GetSkelMeshComponent();
	const USkeletalMeshComponent* Driver = Capsule ? Cast<USkeletalMeshComponent>(Capsule->GetAttachParent()) : nullptr;
	UAnimInstance* DriverAnim = Driver ? Driver->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = DriverAnim ? DriverAnim->GetCurrentActiveMontage() : nullptr;
	const FAnimMontageInstance* Instance = Montage ? DriverAnim->GetActiveInstanceForMontage(Montage) : nullptr;
	if (!Instance)
	{
		return;
	}
	if (MoveOverrides.IsEmpty() && SequenceMoveOverrides.IsEmpty())
	{
		return;
	}
	const float Position = Instance->GetPosition();

	// The segment under the montage's time: a move built from several sequences (the dropkick: startup, loop repeated in the air until
	// it lands, land) needs a capsule clip per sequence, because the loop's length in the montage is not the time it is really shown
	const FAnimTrack* Track = Montage->SlotAnimTracks.IsEmpty() ? nullptr : &Montage->SlotAnimTracks[0].AnimTrack;
	const int32 SegmentCount = Track ? Track->AnimSegments.Num() : 0;
	const FAnimSegment* Segment = nullptr;
	if (SegmentCount > 0)
	{
		const int32 Index = Track->GetSegmentIndexAtTime(Position);
		Segment = &Track->AnimSegments[Index != INDEX_NONE ? Index : (Position <= 0.f ? 0 : SegmentCount - 1)];
	}

	// 1) By montage asset for a one-sequence montage (resolved once per montage; a synchronous load of a small clip, presentation only)
	if (Montage->IsAsset() && SegmentCount <= 1 && ApplyMontageOverride(*Montage, *Instance, Position))
	{
		return;
	}
	// 2) By the sequence the montage plays at its time (dynamic montages, multi-sequence montages, a montage without its own entry)
	if (Segment && ApplySequenceOverride(*Montage, *Instance, *Segment, Position))
	{
		return;
	}
	// 3) A multi-sequence montage without per-sequence clips falls back to its whole-montage clip
	if (Montage->IsAsset() && SegmentCount > 1)
	{
		ApplyMontageOverride(*Montage, *Instance, Position);
	}
}

bool UGSTN_GreyboxAnimInstance::ApplyMontageOverride(UAnimMontage& Montage, const FAnimMontageInstance& Instance, float Position)
{
	TObjectPtr<UAnimSequenceBase>* Loaded = LoadedOverrides.Find(&Montage);
	if (!Loaded)
	{
		const TSoftObjectPtr<UAnimSequenceBase>* Soft = MoveOverrides.Find(TSoftObjectPtr<UAnimMontage>(&Montage));
		Loaded = &LoadedOverrides.Add(&Montage, Soft ? Soft->LoadSynchronous() : nullptr);
	}
	if (!*Loaded)
	{
		return false;
	}
	Shape.OverrideClip = *Loaded;
	Shape.OverrideTime = Position;
	Shape.OverrideWeight = FMath::Clamp(Instance.GetWeight(), 0.f, 1.f);
	return true;
}

bool UGSTN_GreyboxAnimInstance::ApplySequenceOverride(const UAnimMontage& Montage, const FAnimMontageInstance& Instance, const FAnimSegment& Segment,
	float Position)
{
	UAnimSequenceBase* Played = Segment.GetAnimReference();
	if (!Played)
	{
		return false;
	}
	TObjectPtr<UAnimSequenceBase>* Loaded = LoadedSequenceOverrides.Find(Played);
	if (!Loaded)
	{
		const TSoftObjectPtr<UAnimSequenceBase>* Soft = SequenceMoveOverrides.Find(TSoftObjectPtr<UAnimSequenceBase>(Played));
		Loaded = &LoadedSequenceOverrides.Add(Played, Soft ? Soft->LoadSynchronous() : nullptr);
		if (!*Loaded)
		{
			// Once per clip: names what to add to SequenceMoveOverrides (make_capsule_clips.py reads this line from the log)
			UE_LOG(LogAnimation, Log, TEXT("GSTN Greybox: no capsule override for montage %s playing %s"), *Montage.GetName(), *Played->GetPathName());
		}
	}
	if (!*Loaded)
	{
		return false;
	}
	Shape.OverrideClip = *Loaded;
	// The time inside the played sequence (a looping segment wraps), mapped onto the capsule clip's length
	const float SequenceTime = Segment.ConvertTrackPosToAnimPos(Position);
	const float SequenceLength = Played->GetPlayLength();
	Shape.OverrideTime = SequenceLength > KINDA_SMALL_NUMBER ? SequenceTime / SequenceLength * (*Loaded)->GetPlayLength() : SequenceTime;
	Shape.OverrideWeight = FMath::Clamp(Instance.GetWeight(), 0.f, 1.f);
	return true;
}
