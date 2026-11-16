// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_GizmoMath.h"

#include "Hits/GSTN_HitShapeUtils.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	/** The component of largest magnitude, sign kept: the axis the user dragged furthest */
	double Dominant(double A, double B, double C)
	{
		double Best = A;
		if (FMath::Abs(B) > FMath::Abs(Best))
		{
			Best = B;
		}
		if (FMath::Abs(C) > FMath::Abs(Best))
		{
			Best = C;
		}
		return Best;
	}

	double ScaleFactor(double Units)
	{
		return FMath::Max(0.01, 1.0 + Units * GSTNGizmo::ScalePerWidgetUnit);
	}

	/** Entry distance of a ray (unit direction) into a sphere it starts outside of; false if it misses or the sphere is behind */
	bool RaySphere(const FVector& O, const FVector& D, const FVector& Center, double Radius, double& OutT)
	{
		const FVector M = O - Center;
		const double B = FVector::DotProduct(M, D);
		const double C = M.SizeSquared() - Radius * Radius;
		if (C > 0.0 && B > 0.0)
		{
			return false;
		}
		const double Disc = B * B - C;
		if (Disc < 0.0)
		{
			return false;
		}
		OutT = FMath::Max(-B - FMath::Sqrt(Disc), 0.0);
		return true;
	}
}

FTransform GSTNGizmo::SpaceTransform(EGSTN_HitboxSpace Space, const FTransform& Root, const TOptional<FTransform>& SocketRootRelative, const FTransform& MoveOrigin)
{
	switch (Space)
	{
	case EGSTN_HitboxSpace::Root:
		return Root;
	case EGSTN_HitboxSpace::BakedSocket:
		return SocketRootRelative.IsSet() ? SocketRootRelative.GetValue() * Root : Root;
	case EGSTN_HitboxSpace::WorldAtMoveStart:
		return MoveOrigin;
	case EGSTN_HitboxSpace::Projectile:
	default:
		return FTransform::Identity;
	}
}

void GSTNGizmo::FDragDelta::Accumulate(const FVector& InTranslation, const FQuat& InRotation, const FVector& InScale)
{
	Translation += InTranslation;
	Rotation = (InRotation * Rotation).GetNormalized();
	Scale += InScale;
}

FVector GSTNGizmo::WorldTranslationToKey(const FVector& WorldDelta, const FTransform& Space)
{
	return Space.InverseTransformVector(WorldDelta);
}

FQuat GSTNGizmo::WorldRotationToKey(const FQuat& KeyRotation, const FQuat& SpaceRotation, const FQuat& WorldDelta)
{
	// World rotation of the key is Space x Key; the gizmo turns it by WorldDelta on the world side; the key is what is left once the space is taken off again
	return (SpaceRotation.Inverse() * WorldDelta * SpaceRotation * KeyRotation).GetNormalized();
}

FTransform GSTNGizmo::ApplyDragToKey(const FTransform& StartKey, const FTransform& Space, const FDragDelta& Delta)
{
	FTransform Result = StartKey;
	Result.SetLocation(StartKey.GetLocation() + WorldTranslationToKey(Delta.Translation, Space));
	Result.SetRotation(WorldRotationToKey(StartKey.GetRotation(), Space.GetRotation(), Delta.Rotation));
	return Result;
}

FVector GSTNGizmo::ScaleExtent(const FGSTN_HitShape& StartShape, const FVector& ScaleUnits)
{
	const FVector& Start = StartShape.Extent;
	switch (StartShape.Type)
	{
	case EGSTN_ShapeType::Sphere:
		{
			const double Radius = FMath::Max(Start.X * ScaleFactor(Dominant(ScaleUnits.X, ScaleUnits.Y, ScaleUnits.Z)), MinExtent);
			return FVector(Radius, Radius, Radius);
		}
	case EGSTN_ShapeType::Capsule:
		{
			const double Radius = FMath::Max(Start.X * ScaleFactor(Dominant(ScaleUnits.X, ScaleUnits.Y, 0.0)), MinExtent);
			// The half height includes the hemispheres: below the radius the capsule would not exist
			const double HalfHeight = FMath::Max(Start.Z * ScaleFactor(ScaleUnits.Z), Radius);
			return FVector(Radius, Radius, HalfHeight);
		}
	case EGSTN_ShapeType::Box:
	default:
		return FVector(FMath::Max(Start.X * ScaleFactor(ScaleUnits.X), MinExtent), FMath::Max(Start.Y * ScaleFactor(ScaleUnits.Y), MinExtent),
			FMath::Max(Start.Z * ScaleFactor(ScaleUnits.Z), MinExtent));
	}
}

FTransform GSTNGizmo::MirrorKeyAcrossRootYZ(const FTransform& Key, const FTransform& Space, const FTransform& Root)
{
	FTransform InRoot = (Key * Space) * Root.Inverse();
	FVector Location = InRoot.GetLocation();
	Location.X = -Location.X;
	// A reflection across the plane whose normal is X keeps the X part of a rotation's axis and flips the other two
	const FQuat Q = InRoot.GetRotation();
	InRoot.SetLocation(Location);
	InRoot.SetRotation(FQuat(Q.X, -Q.Y, -Q.Z, Q.W).GetNormalized());
	return (InRoot * Root) * Space.Inverse();
}

GSTNGizmo::FDragKeyResolution GSTNGizmo::ResolveDragKey(TConstArrayView<FGSTN_HitboxKey> Keys, int32 RelativeFrame, bool bAutoKey)
{
	FDragKeyResolution Result;
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (Keys[Index].Frame == RelativeFrame)
		{
			Result.Action = EDragKeyAction::UseKeyAtFrame;
			Result.KeyInLane = Index;
			return Result;
		}
	}
	if (bAutoKey)
	{
		Result.Action = EDragKeyAction::CreateAtFrame;
		return Result;
	}
	const int32 Nearest = FindKeyForPick(Keys, RelativeFrame);
	if (Nearest != INDEX_NONE)
	{
		Result.Action = EDragKeyAction::EditNearest;
		Result.KeyInLane = Nearest;
	}
	return Result;
}

int32 GSTNGizmo::FindKeyForPick(TConstArrayView<FGSTN_HitboxKey> Keys, int32 RelativeFrame)
{
	int32 Best = INDEX_NONE;
	int32 BestDistance = MAX_int32;
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		const int32 Distance = FMath::Abs(Keys[Index].Frame - RelativeFrame);
		// Strictly less: of two equally near keys the earlier stays
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Index;
		}
	}
	return Best;
}

bool GSTNGizmo::RayVsShape(const FVector& Origin, const FVector& Direction, const FGSTN_HitShape& Shape, const FTransform& Carrier, double& OutDistance)
{
	const double DirectionLength = Direction.Size();
	if (DirectionLength < 1e-12)
	{
		return false;
	}
	// Placed as DrawHitShape places it: rigid, the Local scale lives in the extent only
	FTransform World = Shape.Local * Carrier;
	World.SetScale3D(FVector::OneVector);
	World.NormalizeRotation();
	const FVector Extent = GSTNPreviewDrawing::GetEffectiveExtent(Shape);
	const FVector O = World.InverseTransformPositionNoScale(Origin);
	const FVector D = World.InverseTransformVectorNoScale(Direction / DirectionLength);

	switch (Shape.Type)
	{
	case EGSTN_ShapeType::Sphere:
		return RaySphere(O, D, FVector::ZeroVector, Extent.X, OutDistance);

	case EGSTN_ShapeType::Box:
		{
			double TNear = 0.0;
			double TFar = TNumericLimits<double>::Max();
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double Half = Extent[Axis];
				if (FMath::Abs(D[Axis]) < 1e-12)
				{
					// Parallel to this pair of faces: it either runs between them or never touches the box
					if (FMath::Abs(O[Axis]) > Half)
					{
						return false;
					}
					continue;
				}
				double T1 = (-Half - O[Axis]) / D[Axis];
				double T2 = (Half - O[Axis]) / D[Axis];
				if (T1 > T2)
				{
					Swap(T1, T2);
				}
				TNear = FMath::Max(TNear, T1);
				TFar = FMath::Min(TFar, T2);
				if (TNear > TFar)
				{
					return false;
				}
			}
			OutDistance = TNear;
			return true;
		}

	case EGSTN_ShapeType::Capsule:
		{
			const double Radius = Extent.X;
			const double Half = FMath::Max(Extent.Z - Radius, 0.0);
			const double ClampedZ = FMath::Clamp(O.Z, -Half, Half);
			if (FVector(O.X, O.Y, O.Z - ClampedZ).SizeSquared() <= Radius * Radius)
			{
				OutDistance = 0.0;
				return true;
			}
			double Best = TNumericLimits<double>::Max();
			double T = 0.0;
			// The two hemispheres, then the side between them
			if (RaySphere(O, D, FVector(0.0, 0.0, Half), Radius, T))
			{
				Best = FMath::Min(Best, T);
			}
			if (RaySphere(O, D, FVector(0.0, 0.0, -Half), Radius, T))
			{
				Best = FMath::Min(Best, T);
			}
			const double A = D.X * D.X + D.Y * D.Y;
			if (A > 1e-12)
			{
				const double B = O.X * D.X + O.Y * D.Y;
				const double C = O.X * O.X + O.Y * O.Y - Radius * Radius;
				const double Disc = B * B - A * C;
				if (Disc >= 0.0)
				{
					const double Side = (-B - FMath::Sqrt(Disc)) / A;
					if (Side >= 0.0 && FMath::Abs(O.Z + Side * D.Z) <= Half)
					{
						Best = FMath::Min(Best, Side);
					}
				}
			}
			if (Best == TNumericLimits<double>::Max())
			{
				return false;
			}
			OutDistance = Best;
			return true;
		}
	}
	return false;
}

int32 GSTNGizmo::PickNearest(const FVector& Origin, const FVector& Direction, TConstArrayView<FPickCandidate> Candidates, double* OutDistance)
{
	int32 Best = INDEX_NONE;
	double BestDistance = 0.0;
	double BestSize = 0.0;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		double Distance = 0.0;
		if (!RayVsShape(Origin, Direction, Candidates[Index].Shape, Candidates[Index].Carrier, Distance))
		{
			continue;
		}
		const FVector Extent = GSTNPreviewDrawing::GetEffectiveExtent(Candidates[Index].Shape);
		const double Size = Extent.X * Extent.Y * Extent.Z;
		const bool bCloser = Best == INDEX_NONE || Distance < BestDistance - 1e-6;
		const bool bTieAndSmaller = Best != INDEX_NONE && FMath::Abs(Distance - BestDistance) <= 1e-6 && Size < BestSize;
		if (bCloser || bTieAndSmaller)
		{
			Best = Index;
			BestDistance = Distance;
			BestSize = Size;
		}
	}
	if (Best != INDEX_NONE && OutDistance != nullptr)
	{
		*OutDistance = BestDistance;
	}
	return Best;
}

void GSTNGizmo::CollectActiveHitboxes(const FGSTN_CombatTimelineData& Data, int32 Frame, const FTransform& Root, const FSocketResolver& LiveSocket, TArray<FActiveHitbox>& Out)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track == nullptr || !Track->Contains(Frame))
		{
			continue;
		}
		for (int32 Lane = 0; Lane < Track->Hitboxes.Num(); ++Lane)
		{
			const FGSTN_HitboxSpec& Spec = Track->Hitboxes[Lane];
			const FTransform Key = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, Frame - Track->StartFrame);

			TOptional<FTransform> Socket;
			if (Spec.Space == EGSTN_HitboxSpace::BakedSocket)
			{
				FTransform Found;
				if (Data.FindBakedSocket(Spec.Socket, Frame, Found) || LiveSocket(Spec.Socket, Found))
				{
					Socket = Found;
				}
			}

			FActiveHitbox& Active = Out.AddDefaulted_GetRef();
			Active.Track = TrackIndex;
			Active.Lane = Lane;
			Active.HitGroup = Spec.HitGroup;
			Active.Shape = Spec.Shape;
			Active.Shape.Local = Spec.Shape.Local * Key;
			Active.Space = SpaceTransform(Spec.Space, Root, Socket);
		}
	}
}
