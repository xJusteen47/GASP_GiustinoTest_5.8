// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitShapeUtils.h"

namespace
{
	/** A shape resolved into world space: the carrier's transform and the shape's own scale folded in */
	struct FWorldShape
	{
		EGSTN_ShapeType Type = EGSTN_ShapeType::Sphere;
		FVector Center = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		/** Sphere and capsule radius */
		double Radius = 0.0;
		/** Capsule: half length of the core segment (half height minus the radius) */
		double HalfSegment = 0.0;
		/** Box: half extents along the rotated axes */
		FVector Half = FVector::ZeroVector;

		FVector CapsuleP0() const { return Center - Rotation.GetAxisZ() * HalfSegment; }
		FVector CapsuleP1() const { return Center + Rotation.GetAxisZ() * HalfSegment; }
	};

	FWorldShape Resolve(const FGSTN_HitShape& Shape, const FTransform& CarrierWorld)
	{
		const FTransform World = Shape.Local * CarrierWorld;
		const FVector Scale = World.GetScale3D().GetAbs();
		const FVector Extent = Shape.Extent.GetAbs();

		FWorldShape Out;
		Out.Type = Shape.Type;
		Out.Center = World.GetLocation();
		Out.Rotation = World.GetRotation();
		switch (Shape.Type)
		{
		case EGSTN_ShapeType::Sphere:
			Out.Radius = Extent.X * Scale.GetMax();
			break;
		case EGSTN_ShapeType::Capsule:
		{
			Out.Radius = Extent.X * FMath::Max(Scale.X, Scale.Y);
			const double HalfHeight = Extent.Z * Scale.Z;
			Out.HalfSegment = FMath::Max(HalfHeight - Out.Radius, 0.0);
			break;
		}
		case EGSTN_ShapeType::Box:
			Out.Half = Extent * Scale;
			break;
		}
		return Out;
	}

	double PointSegmentDistSq(const FVector& Point, const FVector& A, const FVector& B)
	{
		const FVector AB = B - A;
		const double LengthSq = AB | AB;
		const double T = LengthSq > 0.0 ? FMath::Clamp(((Point - A) | AB) / LengthSq, 0.0, 1.0) : 0.0;
		return FVector::DistSquared(Point, A + AB * T);
	}

	/** Closest squared distance between two segments (Ericson, Real-Time Collision Detection 5.1.9) */
	double SegmentSegmentDistSq(const FVector& P1, const FVector& Q1, const FVector& P2, const FVector& Q2)
	{
		constexpr double Epsilon = 1e-12;
		const FVector D1 = Q1 - P1;
		const FVector D2 = Q2 - P2;
		const FVector R = P1 - P2;
		const double A = D1 | D1;
		const double E = D2 | D2;
		const double F = D2 | R;

		double S = 0.0;
		double T = 0.0;
		if (A <= Epsilon && E <= Epsilon)
		{
			return FVector::DistSquared(P1, P2);
		}
		if (A <= Epsilon)
		{
			T = FMath::Clamp(F / E, 0.0, 1.0);
		}
		else
		{
			const double C = D1 | R;
			if (E <= Epsilon)
			{
				S = FMath::Clamp(-C / A, 0.0, 1.0);
			}
			else
			{
				const double B = D1 | D2;
				const double Denominator = A * E - B * B;
				S = Denominator > Epsilon * A * E ? FMath::Clamp((B * F - C * E) / Denominator, 0.0, 1.0) : 0.0;
				T = (B * S + F) / E;
				if (T < 0.0)
				{
					T = 0.0;
					S = FMath::Clamp(-C / A, 0.0, 1.0);
				}
				else if (T > 1.0)
				{
					T = 1.0;
					S = FMath::Clamp((B - C) / A, 0.0, 1.0);
				}
			}
		}
		return FVector::DistSquared(P1 + D1 * S, P2 + D2 * T);
	}

	/** Squared distance from a point to the box [-Half, Half] centred on the origin */
	double PointBoxDistSq(const FVector& Point, const FVector& Half)
	{
		const double X = FMath::Max(FMath::Abs(Point.X) - Half.X, 0.0);
		const double Y = FMath::Max(FMath::Abs(Point.Y) - Half.Y, 0.0);
		const double Z = FMath::Max(FMath::Abs(Point.Z) - Half.Z, 0.0);
		return X * X + Y * Y + Z * Z;
	}

	/**
	 * Exact squared distance from a segment to the box [-Half, Half]. The squared distance along the segment is convex and
	 * piecewise quadratic, with the pieces split where the segment crosses a face plane (at most six places), so the minimum
	 * is found in closed form on each piece and no iteration is involved.
	 */
	double SegmentBoxDistSq(const FVector& P0, const FVector& P1, const FVector& Half)
	{
		const FVector D = P1 - P0;
		double Breaks[8];
		int32 Count = 0;
		Breaks[Count++] = 0.0;
		Breaks[Count++] = 1.0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (D[Axis] != 0.0)
			{
				for (const double Sign : { -1.0, 1.0 })
				{
					const double T = (Sign * Half[Axis] - P0[Axis]) / D[Axis];
					if (T > 0.0 && T < 1.0)
					{
						Breaks[Count++] = T;
					}
				}
			}
		}
		for (int32 I = 1; I < Count; ++I)
		{
			const double Key = Breaks[I];
			int32 J = I - 1;
			while (J >= 0 && Breaks[J] > Key)
			{
				Breaks[J + 1] = Breaks[J];
				--J;
			}
			Breaks[J + 1] = Key;
		}

		auto DistSqAt = [&](double T) { return PointBoxDistSq(P0 + D * T, Half); };

		double Best = DistSqAt(0.0);
		for (int32 I = 0; I + 1 < Count; ++I)
		{
			const double T0 = Breaks[I];
			const double T1 = Breaks[I + 1];
			Best = FMath::Min(Best, DistSqAt(T1));
			const double Mid = 0.5 * (T0 + T1);
			Best = FMath::Min(Best, DistSqAt(Mid));

			// Inside a piece the faces the segment is outside of are fixed: minimise the sum of their squared gaps
			double Numerator = 0.0;
			double Denominator = 0.0;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double At = P0[Axis] + D[Axis] * Mid;
				if (FMath::Abs(At) > Half[Axis])
				{
					const double Face = At > 0.0 ? Half[Axis] : -Half[Axis];
					Numerator += D[Axis] * (P0[Axis] - Face);
					Denominator += D[Axis] * D[Axis];
				}
			}
			if (Denominator > 0.0)
			{
				Best = FMath::Min(Best, DistSqAt(FMath::Clamp(-Numerator / Denominator, T0, T1)));
			}
		}
		return Best;
	}

	bool SphereSphere(const FWorldShape& A, const FWorldShape& B)
	{
		const double Reach = A.Radius + B.Radius;
		return FVector::DistSquared(A.Center, B.Center) <= Reach * Reach;
	}

	bool SphereCapsule(const FWorldShape& Sphere, const FWorldShape& Capsule)
	{
		const double Reach = Sphere.Radius + Capsule.Radius;
		return PointSegmentDistSq(Sphere.Center, Capsule.CapsuleP0(), Capsule.CapsuleP1()) <= Reach * Reach;
	}

	bool CapsuleCapsule(const FWorldShape& A, const FWorldShape& B)
	{
		const double Reach = A.Radius + B.Radius;
		return SegmentSegmentDistSq(A.CapsuleP0(), A.CapsuleP1(), B.CapsuleP0(), B.CapsuleP1()) <= Reach * Reach;
	}

	bool SphereBox(const FWorldShape& Sphere, const FWorldShape& Box)
	{
		const FVector Local = Box.Rotation.UnrotateVector(Sphere.Center - Box.Center);
		return PointBoxDistSq(Local, Box.Half) <= Sphere.Radius * Sphere.Radius;
	}

	bool CapsuleBox(const FWorldShape& Capsule, const FWorldShape& Box)
	{
		const FVector P0 = Box.Rotation.UnrotateVector(Capsule.CapsuleP0() - Box.Center);
		const FVector P1 = Box.Rotation.UnrotateVector(Capsule.CapsuleP1() - Box.Center);
		return SegmentBoxDistSq(P0, P1, Box.Half) <= Capsule.Radius * Capsule.Radius;
	}

	/** Separating axis test of two oriented boxes: the 3 + 3 face axes and the 9 edge pairs (Ericson 4.4.1) */
	bool BoxBox(const FWorldShape& A, const FWorldShape& B)
	{
		constexpr double Epsilon = 1e-9;
		const FVector AxisA[3] = { A.Rotation.GetAxisX(), A.Rotation.GetAxisY(), A.Rotation.GetAxisZ() };
		const FVector AxisB[3] = { B.Rotation.GetAxisX(), B.Rotation.GetAxisY(), B.Rotation.GetAxisZ() };
		const double Ea[3] = { A.Half.X, A.Half.Y, A.Half.Z };
		const double Eb[3] = { B.Half.X, B.Half.Y, B.Half.Z };

		double R[3][3];
		double AbsR[3][3];
		for (int32 I = 0; I < 3; ++I)
		{
			for (int32 J = 0; J < 3; ++J)
			{
				R[I][J] = AxisA[I] | AxisB[J];
				AbsR[I][J] = FMath::Abs(R[I][J]) + Epsilon;
			}
		}

		const FVector Delta = B.Center - A.Center;
		const double T[3] = { Delta | AxisA[0], Delta | AxisA[1], Delta | AxisA[2] };

		for (int32 I = 0; I < 3; ++I)
		{
			const double Ra = Ea[I];
			const double Rb = Eb[0] * AbsR[I][0] + Eb[1] * AbsR[I][1] + Eb[2] * AbsR[I][2];
			if (FMath::Abs(T[I]) > Ra + Rb)
			{
				return false;
			}
		}
		for (int32 I = 0; I < 3; ++I)
		{
			const double Ra = Ea[0] * AbsR[0][I] + Ea[1] * AbsR[1][I] + Ea[2] * AbsR[2][I];
			const double Rb = Eb[I];
			if (FMath::Abs(T[0] * R[0][I] + T[1] * R[1][I] + T[2] * R[2][I]) > Ra + Rb)
			{
				return false;
			}
		}

		// A0 x B0 .. A2 x B2
		double Ra = Ea[1] * AbsR[2][0] + Ea[2] * AbsR[1][0];
		double Rb = Eb[1] * AbsR[0][2] + Eb[2] * AbsR[0][1];
		if (FMath::Abs(T[2] * R[1][0] - T[1] * R[2][0]) > Ra + Rb) { return false; }
		Ra = Ea[1] * AbsR[2][1] + Ea[2] * AbsR[1][1];
		Rb = Eb[0] * AbsR[0][2] + Eb[2] * AbsR[0][0];
		if (FMath::Abs(T[2] * R[1][1] - T[1] * R[2][1]) > Ra + Rb) { return false; }
		Ra = Ea[1] * AbsR[2][2] + Ea[2] * AbsR[1][2];
		Rb = Eb[0] * AbsR[0][1] + Eb[1] * AbsR[0][0];
		if (FMath::Abs(T[2] * R[1][2] - T[1] * R[2][2]) > Ra + Rb) { return false; }

		Ra = Ea[0] * AbsR[2][0] + Ea[2] * AbsR[0][0];
		Rb = Eb[1] * AbsR[1][2] + Eb[2] * AbsR[1][1];
		if (FMath::Abs(T[0] * R[2][0] - T[2] * R[0][0]) > Ra + Rb) { return false; }
		Ra = Ea[0] * AbsR[2][1] + Ea[2] * AbsR[0][1];
		Rb = Eb[0] * AbsR[1][2] + Eb[2] * AbsR[1][0];
		if (FMath::Abs(T[0] * R[2][1] - T[2] * R[0][1]) > Ra + Rb) { return false; }
		Ra = Ea[0] * AbsR[2][2] + Ea[2] * AbsR[0][2];
		Rb = Eb[0] * AbsR[1][1] + Eb[1] * AbsR[1][0];
		if (FMath::Abs(T[0] * R[2][2] - T[2] * R[0][2]) > Ra + Rb) { return false; }

		Ra = Ea[0] * AbsR[1][0] + Ea[1] * AbsR[0][0];
		Rb = Eb[1] * AbsR[2][2] + Eb[2] * AbsR[2][1];
		if (FMath::Abs(T[1] * R[0][0] - T[0] * R[1][0]) > Ra + Rb) { return false; }
		Ra = Ea[0] * AbsR[1][1] + Ea[1] * AbsR[0][1];
		Rb = Eb[0] * AbsR[2][2] + Eb[2] * AbsR[2][0];
		if (FMath::Abs(T[1] * R[0][1] - T[0] * R[1][1]) > Ra + Rb) { return false; }
		Ra = Ea[0] * AbsR[1][2] + Ea[1] * AbsR[0][2];
		Rb = Eb[0] * AbsR[2][1] + Eb[1] * AbsR[2][0];
		if (FMath::Abs(T[1] * R[0][2] - T[0] * R[1][2]) > Ra + Rb) { return false; }
		return true;
	}

	bool OverlapResolved(const FWorldShape& A, const FWorldShape& B)
	{
		switch (A.Type)
		{
		case EGSTN_ShapeType::Sphere:
			switch (B.Type)
			{
			case EGSTN_ShapeType::Sphere: return SphereSphere(A, B);
			case EGSTN_ShapeType::Capsule: return SphereCapsule(A, B);
			case EGSTN_ShapeType::Box: return SphereBox(A, B);
			}
			break;
		case EGSTN_ShapeType::Capsule:
			switch (B.Type)
			{
			case EGSTN_ShapeType::Sphere: return SphereCapsule(B, A);
			case EGSTN_ShapeType::Capsule: return CapsuleCapsule(A, B);
			case EGSTN_ShapeType::Box: return CapsuleBox(A, B);
			}
			break;
		case EGSTN_ShapeType::Box:
			switch (B.Type)
			{
			case EGSTN_ShapeType::Sphere: return SphereBox(B, A);
			case EGSTN_ShapeType::Capsule: return CapsuleBox(B, A);
			case EGSTN_ShapeType::Box: return BoxBox(A, B);
			}
			break;
		}
		return false;
	}

	FTransform InterpolateCarrier(const FTransform& From, const FTransform& To, double Alpha)
	{
		return FTransform(
			FQuat::Slerp(From.GetRotation(), To.GetRotation(), Alpha).GetNormalized(),
			FMath::Lerp(From.GetLocation(), To.GetLocation(), Alpha),
			FMath::Lerp(From.GetScale3D(), To.GetScale3D(), Alpha));
	}
}

double FGSTN_HitShape::GetSmallestWorldExtent(const FTransform& CarrierWorld) const
{
	const FWorldShape Resolved = Resolve(*this, CarrierWorld);
	switch (Resolved.Type)
	{
	case EGSTN_ShapeType::Sphere:
	case EGSTN_ShapeType::Capsule:
		return Resolved.Radius;
	case EGSTN_ShapeType::Box:
		return Resolved.Half.GetMin();
	}
	return 0.0;
}

bool UGSTN_HitShapeUtils::Overlap(const FGSTN_HitShape& A, const FTransform& AWorld, const FGSTN_HitShape& B, const FTransform& BWorld)
{
	return OverlapResolved(Resolve(A, AWorld), Resolve(B, BWorld));
}

int32 UGSTN_HitShapeUtils::GetSweepSubsteps(const FGSTN_HitShape& Shape, const FTransform& PreviousWorld, const FTransform& CurrentWorld)
{
	const FVector From = (Shape.Local * PreviousWorld).GetLocation();
	const FVector To = (Shape.Local * CurrentWorld).GetLocation();
	const double Distance = FVector::Dist(From, To);
	const double Smallest = Shape.GetSmallestWorldExtent(CurrentWorld);
	if (Smallest <= 0.0)
	{
		return Distance > 0.0 ? MaxSweepSubsteps : 1;
	}
	// The quotient is far below the int range for any real input; the clamp keeps a NaN or an infinity at the cap
	const double Steps = FMath::CeilToDouble(Distance / Smallest);
	return Steps >= static_cast<double>(MaxSweepSubsteps) ? MaxSweepSubsteps : FMath::Max(static_cast<int32>(Steps), 1);
}

bool UGSTN_HitShapeUtils::SweptOverlap(const FGSTN_HitShape& A, const FTransform& APreviousWorld, const FTransform& ACurrentWorld, const FGSTN_HitShape& B, const FTransform& BWorld)
{
	const FWorldShape Victim = Resolve(B, BWorld);
	const int32 Steps = GetSweepSubsteps(A, APreviousWorld, ACurrentWorld);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const double Alpha = static_cast<double>(Step) / static_cast<double>(Steps);
		const FTransform Carrier = Step == 0 ? APreviousWorld : Step == Steps ? ACurrentWorld : InterpolateCarrier(APreviousWorld, ACurrentWorld, Alpha);
		if (OverlapResolved(Resolve(A, Carrier), Victim))
		{
			return true;
		}
	}
	return false;
}

FGSTN_HitShape UGSTN_HitShapeUtils::ScaleShape(const FGSTN_HitShape& Shape, float Scale)
{
	FGSTN_HitShape Result = Shape;
	Result.Extent = Shape.Extent * static_cast<double>(Scale);
	return Result;
}

FTransform UGSTN_HitShapeUtils::EvaluateKeys(const TArray<FGSTN_HitboxKey>& Keys, int32 Frame)
{
	if (Keys.IsEmpty())
	{
		return FTransform::Identity;
	}
	auto KeyTransform = [](const FGSTN_HitboxKey& Key) { return FTransform(Key.Rotation.Quaternion(), Key.Offset, Key.Scale); };

	if (Frame <= Keys[0].Frame)
	{
		return KeyTransform(Keys[0]);
	}
	if (Frame >= Keys.Last().Frame)
	{
		return KeyTransform(Keys.Last());
	}
	for (int32 Index = 0; Index + 1 < Keys.Num(); ++Index)
	{
		const FGSTN_HitboxKey& From = Keys[Index];
		const FGSTN_HitboxKey& To = Keys[Index + 1];
		if (Frame >= From.Frame && Frame < To.Frame)
		{
			const double Alpha = static_cast<double>(Frame - From.Frame) / static_cast<double>(To.Frame - From.Frame);
			return InterpolateCarrier(KeyTransform(From), KeyTransform(To), Alpha);
		}
	}
	return KeyTransform(Keys.Last());
}

int32 UGSTN_HitShapeUtils::PickHurtbox(const TArray<int32>& Overlapped, const UGSTN_HurtboxSetDefinition* Set)
{
	return Set ? PickHurtboxIndex(Overlapped, Set->Hurtboxes) : INDEX_NONE;
}

int32 UGSTN_HitShapeUtils::PickHurtboxIndex(TConstArrayView<int32> Overlapped, TConstArrayView<FGSTN_Hurtbox> Hurtboxes)
{
	int32 Best = INDEX_NONE;
	for (const int32 Index : Overlapped)
	{
		if (!Hurtboxes.IsValidIndex(Index))
		{
			continue;
		}
		if (Best == INDEX_NONE || Hurtboxes[Index].Priority > Hurtboxes[Best].Priority || (Hurtboxes[Index].Priority == Hurtboxes[Best].Priority && Index < Best))
		{
			Best = Index;
		}
	}
	return Best;
}
