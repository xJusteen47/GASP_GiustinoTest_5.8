// Minion runtime: movement backends (authority batch stage 4, M8 G4).

#include "Movement/GSTN_MinionMovement_Lightweight.h"

#include "Algo/BinarySearch.h"
#include "Components/PrimitiveComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Store/GSTN_MinionStore.h"

namespace
{
	/** Distance a capsule is pulled back from a blocking hit, so the next sweep does not start touching the surface */
	constexpr float HitPullback = 0.1f;
	/** Moves shorter than this are skipped (a standing minion only probes its floor) */
	constexpr float MinMoveDistance = 0.01f;
	/** Depenetration passes when a capsule starts inside several shapes */
	constexpr int32 MaxDepenetrationPasses = 6;
	/** A floor contact farther from the axis than this share of the radius is on the rim (FindFloor) */
	constexpr float PerchRadiusFraction = 0.5f;
	/** A contact whose normal is within this of horizontal (dot with Up) is a side contact: it cannot hold the capsule up */
	constexpr double SideContactUpDot = 0.1;
	/** Sideways speed given to an airborne capsule that a non-walkable contact holds in place (it slides off the edge) */
	constexpr double EdgeSlideSpeed = 100.0;

	bool IsLightweightLocomotionState(uint8 State)
	{
		const EGSTN_MinionActionState S = static_cast<EGSTN_MinionActionState>(State);
		return S == EGSTN_MinionActionState::Idle || S == EGSTN_MinionActionState::Walk || S == EGSTN_MinionActionState::Move;
	}

	bool IsKnockbackState(uint8 State)
	{
		const EGSTN_MinionActionState S = static_cast<EGSTN_MinionActionState>(State);
		return S == EGSTN_MinionActionState::Launched || S == EGSTN_MinionActionState::PushedBack;
	}

	uint64 CellKey(int32 X, int32 Y)
	{
		return (static_cast<uint64>(static_cast<uint32>(X)) << 32) | static_cast<uint64>(static_cast<uint32>(Y));
	}

	/** Caps the speed along gravity (Up is minus the gravity direction) */
	void ClampFallSpeed(FVector& Velocity, const FVector& Up, float MaxFallSpeed)
	{
		const double Down = -(Velocity | Up);
		if (Down > MaxFallSpeed)
		{
			Velocity += Up * (Down - MaxFallSpeed);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Columns and context

void FGSTN_MinionMovementColumns::Reset(int32 Capacity)
{
	Velocity.Init(FVector3f::ZeroVector, Capacity);
	PendingLaunch.Init(FVector3f::ZeroVector, Capacity);
	bGrounded.Init(0, Capacity);
	KnockbackMode.Init(static_cast<uint8>(EGSTN_MinionKnockbackMode::None), Capacity);
}

void FGSTN_MinionMovementColumns::ResetSlot(int32 Slot)
{
	if (!Velocity.IsValidIndex(Slot))
	{
		return;
	}
	Velocity[Slot] = FVector3f::ZeroVector;
	PendingLaunch[Slot] = FVector3f::ZeroVector;
	// A new minion starts airborne: its first steps find the floor (or fall to it) instead of trusting the spawn height
	bGrounded[Slot] = 0;
	KnockbackMode[Slot] = static_cast<uint8>(EGSTN_MinionKnockbackMode::None);
}

FVector FGSTN_MinionMoveContext::GetGravityAt(const FVector& Position) const
{
	if (GravityAt)
	{
		return GravityAt(Position);
	}
	return FVector(0.0, 0.0, World ? World->GetGravityZ() : -980.0);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Batch

void FGSTN_MinionMovement_Lightweight::StepBatch(FGSTN_MinionMoveContext& Ctx, TArrayView<const int32> Slots)
{
	if (!Ctx.World || !Ctx.Store || !Ctx.Columns)
	{
		return;
	}
	ComputeSeparation(Ctx, Slots);

	const FGSTN_MinionStore& Store = *Ctx.Store;
	for (const int32 Slot : Slots)
	{
		const int32 TypeIndex = Store.TypeIndex[Slot];
		const UGSTN_MinionDefinition* Definition = Ctx.Definitions.IsValidIndex(TypeIndex) ? Ctx.Definitions[TypeIndex].Get() : nullptr;
		if (!Definition)
		{
			continue;
		}
		StepSlot(Ctx, Slot, *Definition, SeparationBySlot[Slot]);
		++Ctx.MinionFrames;
	}
}

void FGSTN_MinionMovement_Lightweight::ComputeSeparation(const FGSTN_MinionMoveContext& Ctx, TArrayView<const int32> Slots)
{
	const FGSTN_MinionStore& Store = *Ctx.Store;
	if (SeparationBySlot.Num() != Store.Capacity())
	{
		SeparationBySlot.Init(FVector2D::ZeroVector, Store.Capacity());
	}
	for (const int32 Slot : Slots)
	{
		SeparationBySlot[Slot] = FVector2D::ZeroVector;
	}
	if (!Ctx.Settings.bSeparation || Slots.Num() < 2)
	{
		return;
	}

	auto StatsOf = [&Ctx, &Store](int32 Slot) -> const FGSTN_MinionStats*
	{
		const int32 TypeIndex = Store.TypeIndex[Slot];
		const UGSTN_MinionDefinition* Definition = Ctx.Definitions.IsValidIndex(TypeIndex) ? Ctx.Definitions[TypeIndex].Get() : nullptr;
		return Definition ? &Definition->Stats : nullptr;
	};

	float MaxRadius = 1.f;
	for (const int32 Slot : Slots)
	{
		if (const FGSTN_MinionStats* Stats = StatsOf(Slot))
		{
			MaxRadius = FMath::Max(MaxRadius, Stats->CapsuleRadius);
		}
	}
	// Cells of twice the largest radius: two overlapping capsules are always in the same or adjacent cells. The hash is on the
	// world XY plane (minions stand on gravity Z in every level so far; a gravity-field level revisits it with G-gravity)
	const double CellSize = 2.0 * MaxRadius;
	Cells.Reset();
	for (const int32 Slot : Slots)
	{
		const FVector3f& P = Store.Position[Slot];
		Cells.Emplace(CellKey(FMath::FloorToInt32(P.X / CellSize), FMath::FloorToInt32(P.Y / CellSize)), Slot);
	}
	// Sorted by cell then slot: the neighbour visit order (and so the float sums) is the same on every run
	Cells.Sort([](const TPair<uint64, int32>& A, const TPair<uint64, int32>& B)
	{
		return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value);
	});

	for (const int32 Slot : Slots)
	{
		const FGSTN_MinionStats* Stats = StatsOf(Slot);
		if (!Stats)
		{
			continue;
		}
		const FVector3f& P = Store.Position[Slot];
		const int32 CellX = FMath::FloorToInt32(P.X / CellSize);
		const int32 CellY = FMath::FloorToInt32(P.Y / CellSize);
		FVector2D Push = FVector2D::ZeroVector;
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				const uint64 Key = CellKey(CellX + DX, CellY + DY);
				int32 Index = Algo::LowerBoundBy(Cells, Key, [](const TPair<uint64, int32>& Entry) { return Entry.Key; });
				for (; Index < Cells.Num() && Cells[Index].Key == Key; ++Index)
				{
					const int32 Other = Cells[Index].Value;
					const FGSTN_MinionStats* OtherStats = Other != Slot ? StatsOf(Other) : nullptr;
					if (!OtherStats)
					{
						continue;
					}
					const FVector3f& Q = Store.Position[Other];
					// Capsules at different heights (stairs, one in the air) do not touch
					if (FMath::Abs(P.Z - Q.Z) > Stats->CapsuleHalfHeight + OtherStats->CapsuleHalfHeight)
					{
						continue;
					}
					const FVector2D Delta(P.X - Q.X, P.Y - Q.Y);
					const double Distance = Delta.Size();
					const double MinDistance = Stats->CapsuleRadius + OtherStats->CapsuleRadius;
					if (Distance >= MinDistance)
					{
						continue;
					}
					// Two minions on the same spot: split them along X by slot order, the same way on every run
					const FVector2D Direction = Distance > 0.01 ? Delta / Distance : FVector2D(Slot < Other ? -1.0 : 1.0, 0.0);
					Push += Direction * ((MinDistance - Distance) * Ctx.Settings.SeparationStiffness);
				}
			}
		}
		SeparationBySlot[Slot] = Push.GetClampedToMaxSize(Ctx.Settings.MaxSeparationSpeed);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// One slot

void FGSTN_MinionMovement_Lightweight::StepSlot(FGSTN_MinionMoveContext& Ctx, int32 Slot, const UGSTN_MinionDefinition& Definition, const FVector2D& Separation)
{
	FGSTN_MinionStore& Store = *Ctx.Store;
	FGSTN_MinionMovementColumns& Columns = *Ctx.Columns;
	const FGSTN_MinionStats& Stats = Definition.Stats;
	const FGSTN_MinionMovementSettings& Settings = Ctx.Settings;
	const float DeltaSeconds = Ctx.DeltaSecondsOf(Slot);
	const float Skin = Settings.FloorSkin;

	FStepState State;
	State.Radius = Stats.CapsuleRadius;
	State.HalfHeight = FMath::Max(Stats.CapsuleHalfHeight, Stats.CapsuleRadius);
	State.StepHeight = Stats.StepHeight;
	State.WalkableUpDot = FMath::Cos(FMath::DegreesToRadians(Settings.WalkableSlopeDegrees));
	State.bGrounded = Columns.bGrounded[Slot] != 0;

	// Gravity at the feet decides Up for the whole step; the capsule is aligned to it
	const FVector Feet(Store.Position[Slot]);
	const FVector Gravity = Ctx.GetGravityAt(Feet) * Stats.GravityScale;
	const FVector GravityDirection = Gravity.GetSafeNormal();
	State.Up = GravityDirection.IsNearlyZero() ? FVector::UpVector : -GravityDirection;
	State.CapsuleRotation = FQuat::FindBetweenNormals(FVector::UpVector, State.Up);
	State.Centre = Feet + State.Up * State.HalfHeight;

	FVector Velocity(Columns.Velocity[Slot]);
	EGSTN_MinionKnockbackMode Mode = static_cast<EGSTN_MinionKnockbackMode>(Columns.KnockbackMode[Slot]);
	bool bKnockbackEnded = false;

	// 1. Velocity
	const FVector Launch(Columns.PendingLaunch[Slot]);
	if (!Launch.IsNearlyZero())
	{
		Columns.PendingLaunch[Slot] = FVector3f::ZeroVector;
		if (Mode == EGSTN_MinionKnockbackMode::PushedBack && State.bGrounded)
		{
			// A push back never leaves the ground: only its part on the ground plane counts
			Velocity = FVector::VectorPlaneProject(Launch, State.Up);
		}
		else
		{
			// A push back on an airborne minion flies like a launch (there is no ground to slide on)
			Mode = EGSTN_MinionKnockbackMode::Launched;
			Velocity = Launch;
			State.bGrounded = false;
		}
	}
	else if (Mode == EGSTN_MinionKnockbackMode::PushedBack && !State.bGrounded)
	{
		Mode = EGSTN_MinionKnockbackMode::Launched;
	}

	const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
	const bool bStale = Intent.IsStale(Ctx.Frame);
	const bool bFollowsIntent = Mode == EGSTN_MinionKnockbackMode::None && IsLightweightLocomotionState(Store.ActionState[Slot]) && Store.MoveId[Slot] == 0 && !bStale;
	FVector IntentVelocity = FVector::ZeroVector;
	if (bFollowsIntent)
	{
		FVector2D Direction = Intent.MoveDir;
		if (Direction.SizeSquared() > 1.0)
		{
			Direction.Normalize();
		}
		const float MaxSpeed = Stats.RunSpeed > 0.f ? Stats.RunSpeed : TNumericLimits<float>::Max();
		const float WantedSpeed = FMath::Clamp(Intent.DesiredSpeed, 0.f, MaxSpeed);
		IntentVelocity = FVector::VectorPlaneProject(FVector(Direction.X, Direction.Y, 0.0) * WantedSpeed, State.Up);
	}
	// G5: the movement track of the minion's timeline (FGSTN_TimelineTrack_Movement, layered move data) adds to the desired
	// velocity here; it needs the fighters' layered move evaluation, which is not trivial to share

	switch (Mode)
	{
	case EGSTN_MinionKnockbackMode::Launched:
		Velocity += Gravity * DeltaSeconds;
		ClampFallSpeed(Velocity, State.Up, Settings.MaxFallSpeed);
		break;

	case EGSTN_MinionKnockbackMode::PushedBack:
	{
		const FVector Ground = FVector::VectorPlaneProject(Velocity, State.Up);
		const double Speed = FMath::Max(0.0, Ground.Size() - Settings.PushBackDeceleration * DeltaSeconds);
		if (Speed <= Settings.PushBackStopSpeed)
		{
			Velocity = FVector::ZeroVector;
			Mode = EGSTN_MinionKnockbackMode::None;
			bKnockbackEnded = true;
		}
		else
		{
			Velocity = Ground.GetSafeNormal() * Speed;
		}
		break;
	}

	case EGSTN_MinionKnockbackMode::None:
	default:
		if (State.bGrounded)
		{
			const FVector Desired = IntentVelocity + FVector::VectorPlaneProject(FVector(Separation.X, Separation.Y, 0.0), State.Up);
			const FVector Ground = FVector::VectorPlaneProject(Velocity, State.Up);
			const float Rate = Desired.IsNearlyZero() ? Stats.Braking : Stats.Acceleration;
			Velocity = Ground + (Desired - Ground).GetClampedToMaxSize(Rate * DeltaSeconds);
		}
		else
		{
			// Walked off a ledge (or spawned in the air): gravity, the ground speed is kept, no air control
			Velocity += Gravity * DeltaSeconds;
			ClampFallSpeed(Velocity, State.Up, Settings.MaxFallSpeed);
		}
		break;
	}

	// 2. Move
	const FVector StartCentre = State.Centre;
	const FVector Delta = Velocity * DeltaSeconds;
	FHitResult FirstHit;
	bool bMoveBlocked = false;
	if (Delta.SizeSquared() > FMath::Square(MinMoveDistance))
	{
		const bool bWalking = Mode == EGSTN_MinionKnockbackMode::None && State.bGrounded;
		const bool bPushedBack = Mode == EGSTN_MinionKnockbackMode::PushedBack;
		bMoveBlocked = MoveWithCollision(Ctx, State, Delta, !bPushedBack, bWalking, FirstHit);
		if (bMoveBlocked)
		{
			if (bPushedBack && !IsWalkable(State, FirstHit))
			{
				// A push back stops at a wall
				Velocity = FVector::ZeroVector;
				Mode = EGSTN_MinionKnockbackMode::None;
				bKnockbackEnded = true;
			}
			else
			{
				// Lose the speed into the surface (a launch along a wall keeps its tangent part)
				const double Into = Velocity | FirstHit.Normal;
				if (Into < 0.0)
				{
					Velocity -= FirstHit.Normal * Into;
				}
			}
		}
	}

	// 3. Floor
	FHitResult Floor;
	if (State.bGrounded)
	{
		if (FindFloor(Ctx, State, State.StepHeight + 2.f * Skin, Floor))
		{
			State.Centre = Floor.Location + State.Up * Skin;
		}
		else if (Mode == EGSTN_MinionKnockbackMode::PushedBack)
		{
			// A push back stops at a ledge: back to where the frame started, still grounded
			State.Centre = StartCentre;
			Velocity = FVector::ZeroVector;
			Mode = EGSTN_MinionKnockbackMode::None;
			bKnockbackEnded = true;
		}
		else
		{
			// Walked off a ledge: falls from the next step
			State.bGrounded = false;
		}
	}
	else if ((Velocity | State.Up) <= 0.0 && FindFloor(Ctx, State, 2.f * Skin + 1.f, Floor))
	{
		// Landing (only while descending, so a launch does not stick to the floor it leaves)
		State.Centre = Floor.Location + State.Up * Skin;
		State.bGrounded = true;
		Velocity = FVector::VectorPlaneProject(Velocity, State.Up);
		if (Mode == EGSTN_MinionKnockbackMode::Launched)
		{
			// G5: a launch with the knockdown flag lands into Knockdown (and its GetUp timeline) instead of Idle
			Mode = EGSTN_MinionKnockbackMode::None;
			bKnockbackEnded = true;
		}
	}

	// Still airborne, held in place by a contact that blocks the fall but is no floor (a box edge under the rim): slide off it,
	// away from the contact on the ground plane, instead of hovering there
	if (!State.bGrounded && bMoveBlocked && (FirstHit.Normal | State.Up) > 0.0
		&& FVector::DistSquared(State.Centre, StartCentre) < FMath::Square(0.5))
	{
		const FVector Away = FVector::VectorPlaneProject(FirstHit.Normal, State.Up).GetSafeNormal();
		if (!Away.IsNearlyZero())
		{
			Velocity += Away * FMath::Max(0.0, EdgeSlideSpeed - (Velocity | Away));
		}
	}

	// 4. Write back
	const FVector NewFeet = State.Centre - State.Up * State.HalfHeight;
	Store.Position[Slot] = FVector3f(NewFeet);
	Columns.Velocity[Slot] = FVector3f(Velocity);
	Columns.bGrounded[Slot] = State.bGrounded ? 1 : 0;
	Columns.KnockbackMode[Slot] = static_cast<uint8>(Mode);
	const float GroundSpeed = static_cast<float>(FVector::VectorPlaneProject(Velocity, State.Up).Size());
	Store.Speed[Slot] = GroundSpeed;

	if (bKnockbackEnded && IsKnockbackState(Store.ActionState[Slot]))
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}

	if (Mode == EGSTN_MinionKnockbackMode::None && IsLightweightLocomotionState(Store.ActionState[Slot]) && Store.MoveId[Slot] == 0)
	{
		// Facing: the requested yaw, else the intent's direction; turned at the type's rate
		float Yaw = Store.YawDegrees[Slot];
		bool bHasTarget = false;
		float TargetYaw = Yaw;
		if (!bStale && Intent.bFaceRequested)
		{
			TargetYaw = Intent.DesiredYaw;
			bHasTarget = true;
		}
		else if (IntentVelocity.SizeSquared() > 1.0)
		{
			TargetYaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(IntentVelocity.Y, IntentVelocity.X)));
			bHasTarget = true;
		}
		if (bHasTarget)
		{
			const float MaxTurn = Stats.TurnRateDegPerSec * DeltaSeconds;
			const float Turn = FMath::FindDeltaAngleDegrees(Yaw, TargetYaw);
			Store.YawDegrees[Slot] = static_cast<float>(FRotator::NormalizeAxis(Yaw + FMath::Clamp(Turn, -MaxTurn, MaxTurn)));
		}
		Store.ActionState[Slot] = static_cast<uint8>(GroundSpeed < 1.f ? EGSTN_MinionActionState::Idle
			: (GroundSpeed <= Stats.WalkSpeed + 1.f ? EGSTN_MinionActionState::Walk : EGSTN_MinionActionState::Move));
	}

	// Kill Z is a world Z rule, whatever the gravity
	if (NewFeet.Z < Ctx.KillZ)
	{
		Ctx.FellOut.Add(Slot);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Collision

bool FGSTN_MinionMovement_Lightweight::IsWalkable(const FStepState& State, const FHitResult& Hit) const
{
	// The surface hit (ImpactNormal), not the capsule's contact normal: a box edge under the capsule is a floor, as for characters
	return Hit.bBlockingHit && (Hit.ImpactNormal | State.Up) >= State.WalkableUpDot - UE_KINDA_SMALL_NUMBER;
}

bool FGSTN_MinionMovement_Lightweight::Sweep(const FGSTN_MinionMoveContext& Ctx, const FStepState& State, const FVector& From, const FVector& To, FHitResult& OutHit, float RadiusOverride) const
{
	OutHit = FHitResult();
	const float Radius = RadiusOverride > 0.f ? RadiusOverride : State.Radius;
	return Ctx.World->SweepSingleByObjectType(OutHit, From, To, State.CapsuleRotation, Ctx.ObjectQuery,
		FCollisionShape::MakeCapsule(Radius, State.HalfHeight), Ctx.QueryParams);
}

bool FGSTN_MinionMovement_Lightweight::Depenetrate(const FGSTN_MinionMoveContext& Ctx, FStepState& State) const
{
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(State.Radius, State.HalfHeight);
	bool bMoved = false;
	TArray<FOverlapResult> Overlaps;
	for (int32 Pass = 0; Pass < MaxDepenetrationPasses; ++Pass)
	{
		Overlaps.Reset();
		if (!Ctx.World->OverlapMultiByObjectType(Overlaps, State.Centre, State.CapsuleRotation, Ctx.ObjectQuery, Shape, Ctx.QueryParams))
		{
			break;
		}
		// One shape per pass, the deepest first, then query again. Summing the translations of every shape (G4 first version)
		// over-pushes when shapes overlap each other (stacked stair boxes, a platform against a step): the sum can carry the
		// capsule sideways into a neighbouring box, which the next sweeps then start inside of. Ties go to the lower unique id
		FVector Push = FVector::ZeroVector;
		float Deepest = 0.f;
		uint32 DeepestId = MAX_uint32;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UPrimitiveComponent* Component = Overlap.GetComponent();
			FMTDResult MTD;
			if (Component && Component->ComputePenetration(MTD, Shape, State.Centre, State.CapsuleRotation) && MTD.Distance > 0.f
				&& (MTD.Distance > Deepest || (MTD.Distance == Deepest && Component->GetUniqueID() < DeepestId)))
			{
				Deepest = MTD.Distance;
				DeepestId = Component->GetUniqueID();
				Push = MTD.Direction * (MTD.Distance + HitPullback);
			}
		}
		if (Push.IsNearlyZero())
		{
			break;
		}
		State.Centre += Push;
		bMoved = true;
	}
	return bMoved;
}

bool FGSTN_MinionMovement_Lightweight::MoveWithCollision(const FGSTN_MinionMoveContext& Ctx, FStepState& State, const FVector& Delta, bool bAllowSlide, bool bAllowStepUp, FHitResult& OutFirstHit)
{
	// One sweep over the whole displacement: whatever the speed and the dt (LOD), nothing thinner than the capsule is crossed
	FHitResult Hit;
	Sweep(Ctx, State, State.Centre, State.Centre + Delta, Hit);
	if (Hit.bStartPenetrating)
	{
		// Started inside geometry (spawned in a wall, pushed by separation into a corner): out along the MTD, then retry once
		Depenetrate(Ctx, State);
		Sweep(Ctx, State, State.Centre, State.Centre + Delta, Hit);
		if (Hit.bStartPenetrating)
		{
			// Still inside: stay this frame, the next one tries again
			return false;
		}
	}
	if (!Hit.bBlockingHit)
	{
		State.Centre += Delta;
		return false;
	}

	OutFirstHit = Hit;
	const FVector FeetAtHit = Hit.Location - State.Up * State.HalfHeight;
	State.Centre = Hit.Location + Hit.Normal * HitPullback;
	const FVector Remainder = Delta * (1.f - Hit.Time);

	// A low obstacle in the way of a walking minion: step up onto it
	if (bAllowStepUp && !IsWalkable(State, Hit))
	{
		const double ImpactHeight = (Hit.ImpactPoint - FeetAtHit) | State.Up;
		const FVector Forward = FVector::VectorPlaneProject(Remainder, State.Up);
		if (ImpactHeight > 0.0 && ImpactHeight <= State.StepHeight && Forward.SizeSquared() > FMath::Square(MinMoveDistance)
			&& TryStepUp(Ctx, State, Forward, Hit))
		{
			return true;
		}
	}

	if (!bAllowSlide || Remainder.SizeSquared() <= FMath::Square(MinMoveDistance))
	{
		return true;
	}
	// Slide along the hit plane with the remainder. Seen from the ground, a wall is vertical: its normal is flattened so the
	// slide never climbs it
	FVector Normal = Hit.Normal;
	if (State.bGrounded && !IsWalkable(State, Hit))
	{
		const FVector Flat = FVector::VectorPlaneProject(Normal, State.Up).GetSafeNormal();
		Normal = Flat.IsNearlyZero() ? Normal : Flat;
	}
	const FVector Slide = FVector::VectorPlaneProject(Remainder, Normal);
	if (Slide.SizeSquared() <= FMath::Square(MinMoveDistance))
	{
		return true;
	}
	FHitResult SlideHit;
	Sweep(Ctx, State, State.Centre, State.Centre + Slide, SlideHit);
	if (SlideHit.bStartPenetrating)
	{
		return true;
	}
	State.Centre = SlideHit.bBlockingHit ? SlideHit.Location + SlideHit.Normal * HitPullback : State.Centre + Slide;
	return true;
}

bool FGSTN_MinionMovement_Lightweight::TryStepUp(const FGSTN_MinionMoveContext& Ctx, FStepState& State, const FVector& Forward, const FHitResult& BlockingHit) const
{
	const float Skin = Ctx.Settings.FloorSkin;
	FHitResult Hit;

	// Up (a ceiling shortens the rise)
	const FVector Start = State.Centre;
	Sweep(Ctx, State, Start, Start + State.Up * State.StepHeight, Hit);
	if (Hit.bStartPenetrating)
	{
		return false;
	}
	const FVector Raised = Hit.bBlockingHit ? Hit.Location : Start + State.Up * State.StepHeight;
	const double Rise = (Raised - Start) | State.Up;
	if (Rise < 1.0)
	{
		return false;
	}

	// Forward: it must make progress, or the obstacle was a wall (a tall wall reports its contact anywhere on the capsule side)
	Sweep(Ctx, State, Raised, Raised + Forward, Hit);
	if (Hit.bStartPenetrating || (Hit.bBlockingHit && Hit.Time <= UE_KINDA_SMALL_NUMBER))
	{
		return false;
	}
	const FVector Advanced = Hit.bBlockingHit ? Hit.Location + Hit.Normal * HitPullback : Raised + Forward;

	// Down onto the step: a walkable floor under the capsule, not on its rim
	Sweep(Ctx, State, Advanced, Advanced - State.Up * (Rise + 2.0 * Skin), Hit);
	if (!Hit.bBlockingHit || Hit.bStartPenetrating || !IsWalkable(State, Hit))
	{
		return false;
	}
	const FVector Lateral = FVector::VectorPlaneProject(Hit.ImpactPoint - Hit.Location, State.Up);
	if (Lateral.Size() > State.Radius - 0.15f)
	{
		return false;
	}
	State.Centre = Hit.Location + State.Up * Skin;
	return true;
}

bool FGSTN_MinionMovement_Lightweight::FindFloor(const FGSTN_MinionMoveContext& Ctx, FStepState& State, float Distance, FHitResult& OutHit) const
{
	Sweep(Ctx, State, State.Centre, State.Centre - State.Up * Distance, OutHit);
	if (OutHit.bStartPenetrating)
	{
		// Sunk into the floor (or a wall): push out and look again once
		Depenetrate(Ctx, State);
		Sweep(Ctx, State, State.Centre, State.Centre - State.Up * Distance, OutHit);
		if (OutHit.bStartPenetrating)
		{
			return false;
		}
	}
	if (!OutHit.bBlockingHit)
	{
		return false;
	}
	const float PerchRadius = State.Radius * PerchRadiusFraction;
	const FVector Lateral = FVector::VectorPlaneProject(OutHit.ImpactPoint - OutHit.Location, State.Up);
	if (IsWalkable(State, OutHit) && Lateral.SizeSquared() <= FMath::Square(PerchRadius))
	{
		return true;
	}
	// Rim contact (or an unwalkable one): grounded only if there is walkable ground under the axis within reach. The capsule
	// then rests on the edge (it sinks a little, as a character perched on a ledge) until it clears it
	FHitResult Inner;
	Sweep(Ctx, State, State.Centre, State.Centre - State.Up * Distance, Inner, PerchRadius);
	if (!Inner.bBlockingHit || Inner.bStartPenetrating || !IsWalkable(State, Inner))
	{
		return false;
	}
	// A side contact (a wall the capsule touches, reported at time 0 before the floor below) does not hold the capsule up: the
	// floor height is the inner probe's. Snapping to the wall contact instead left a minion pressed against a wall hovering in
	// Falling (bench fix 2). A contact that does block downward (an edge under the rim) keeps the full capsule's height
	if ((OutHit.Normal | State.Up) < SideContactUpDot)
	{
		OutHit = Inner;
		// The thin probe falls further than the full capsule can: at an overhang or a leaning face (the underside of a tilted
		// cylinder in the bench level) the full capsule at the probe's height overlaps the surface that held it, and the caller
		// snaps to this height. Push it out along the MTD here, and report the resulting height as the floor (M8 trap fix)
		FStepState AtProbe = State;
		AtProbe.Centre = Inner.Location;
		if (Depenetrate(Ctx, AtProbe))
		{
			OutHit.Location = AtProbe.Centre;
		}
	}
	return true;
}
