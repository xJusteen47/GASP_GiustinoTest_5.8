// Interaction focus: the pure maths (interaction.md §2). No GAS, no Network Prediction, no Mover.

#include "GSTN_InteractMath.h"

#include "Algo/Sort.h"
#include "Misc/Crc.h"

namespace GSTNInteractMathPrivate
{
	const FGameplayTagContainer EmptyTags;

	FVector SafeUp(const FVector& Up)
	{
		const FVector Normal = Up.GetSafeNormal();
		return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
	}

	FVector Planar(const FVector& V, const FVector& Up)
	{
		return V - Up * (V | Up);
	}

	/** Reach to the surface of a candidate under a scale, honouring the override and the hard maximum */
	float EffectiveReach(const FGSTN_InteractCandidate& Candidate, const FGSTN_InteractRules& Rules)
	{
		const float Base = Candidate.ReachOverrideCm > 0 ? static_cast<float>(Candidate.ReachOverrideCm) : Rules.ReachCm;
		return FMath::Max(FMath::Min(Base, Rules.HardMaxReachCm), 1.f);
	}

	int32 EffectivePriority(const FGSTN_InteractQuery& Query, const FGSTN_InteractCandidate& Candidate)
	{
		if (Candidate.Priority >= 0)
		{
			return FMath::Clamp(Candidate.Priority, 0, 255);
		}
		return Query.Meanings ? Query.Meanings->DefaultPriorityOf(Candidate.MeaningIndex) : 0;
	}

	bool RequirementHolds(const FGSTN_InteractCandidate& Candidate, const FGameplayTagContainer* Tags, TConstArrayView<FGameplayTagQuery> Requirements)
	{
		if (Candidate.QueryIndex < 0 || !Requirements.IsValidIndex(Candidate.QueryIndex))
		{
			return true;
		}
		return Requirements[Candidate.QueryIndex].Matches(Tags ? *Tags : EmptyTags);
	}

	int32 TieEpsilonFixed(const FGSTN_InteractRules& Rules)
	{
		return FMath::Max(1, FMath::RoundToInt(Rules.TieEpsilon * UGSTN_InteractMath::CostScale));
	}
}

using namespace GSTNInteractMathPrivate;

FVector UGSTN_InteractMath::QuantiseYaw(const FVector& PlanarDirection, const FVector& Up)
{
	const FVector NormalUp = SafeUp(Up);
	const FQuat Frame = FQuat::FindBetweenNormals(FVector::UpVector, NormalUp);
	const FVector AxisX = Frame.RotateVector(FVector::ForwardVector);
	const FVector AxisY = Frame.RotateVector(FVector::RightVector);
	const double Angle = FMath::Atan2(PlanarDirection | AxisY, PlanarDirection | AxisX);
	// The command carries yaw in 16 bits, so the pointing is rounded to the same grid
	const double Step = UE_DOUBLE_TWO_PI / 65536.0;
	const double Snapped = FMath::RoundToDouble(Angle / Step) * Step;
	return AxisX * FMath::Cos(Snapped) + AxisY * FMath::Sin(Snapped);
}

FGSTN_InteractPointing UGSTN_InteractMath::ComputePointing(const FGSTN_InteractQuery& Query)
{
	check(Query.Rules);
	const FGSTN_InteractRules& Rules = *Query.Rules;
	const FVector Up = SafeUp(Query.Up);
	FGSTN_InteractPointing Result;

	if (Rules.PointingMode == EGSTN_InteractPointingMode::CameraAim && Query.bHasAim)
	{
		const bool bAimActive = Rules.CameraAimWhen.IsEmpty() || Rules.CameraAimWhen.Matches(Query.Tags ? *Query.Tags : EmptyTags);
		const FVector Aim = Query.AimDirection.GetSafeNormal();
		if (bAimActive && !Aim.IsNearlyZero())
		{
			constexpr double Grid = 32768.0;
			const FVector Snapped(FMath::RoundToDouble(Aim.X * Grid) / Grid, FMath::RoundToDouble(Aim.Y * Grid) / Grid, FMath::RoundToDouble(Aim.Z * Grid) / Grid);
			Result.Direction = Snapped.GetSafeNormal();
			Result.b3D = true;
			Result.Source = EGSTN_InteractPointingSource::Aim;
			return Result;
		}
	}

	if (Rules.PointingMode != EGSTN_InteractPointingMode::Facing && Query.StickMagnitude >= Rules.StickThreshold)
	{
		const FVector Stick = Planar(Query.StickDirection, Up).GetSafeNormal();
		if (!Stick.IsNearlyZero())
		{
			Result.Direction = QuantiseYaw(Stick, Up);
			Result.Source = EGSTN_InteractPointingSource::Stick;
			return Result;
		}
	}

	FVector Facing = Planar(Query.Facing, Up).GetSafeNormal();
	if (Facing.IsNearlyZero())
	{
		// Facing along Up has no planar direction: any fixed axis keeps the answer deterministic
		Facing = FQuat::FindBetweenNormals(FVector::UpVector, Up).RotateVector(FVector::ForwardVector);
	}
	Result.Direction = QuantiseYaw(Facing, Up);
	Result.Source = EGSTN_InteractPointingSource::Facing;
	return Result;
}

bool UGSTN_InteractMath::ScoreCandidate(const FGSTN_InteractQuery& Query, const FGSTN_InteractPointing& Pointing, const FGSTN_InteractCandidate& Candidate,
	float ReachScale, float ArcScale, FGSTN_InteractScored& OutScored)
{
	check(Query.Rules);
	const FGSTN_InteractRules& Rules = *Query.Rules;
	if (!Candidate.IsLiveAt(Query.Frame))
	{
		return false;
	}

	const bool bRequirementOk = RequirementHolds(Candidate, Query.Tags, Query.Requirements);
	if (!bRequirementOk && !Candidate.FocusesWhenUnactionable())
	{
		return false;
	}

	const FVector Up = SafeUp(Query.Up);
	const FVector Delta = Candidate.GetPosition() - FGSTN_InteractCandidate::SnapCm(Query.Origin);
	const double Distance = Delta.Size();
	const double SurfaceDistance = FMath::Max(0.0, Distance - Candidate.RadiusCm);
	const float Reach = EffectiveReach(Candidate, Rules);
	if (SurfaceDistance > Reach * ReachScale)
	{
		return false;
	}

	if (!Pointing.b3D)
	{
		const double Height = FMath::Max(0.0, FMath::Abs(Delta | Up) - Candidate.RadiusCm);
		if (Height > Rules.MaxHeightDeltaCm)
		{
			return false;
		}
	}

	double AngleDeg = 0.0;
	if (Pointing.b3D)
	{
		if (Distance >= 1.0)
		{
			AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((Delta / Distance) | Pointing.Direction, -1.0, 1.0)));
		}
	}
	else
	{
		const FVector Flat = Planar(Delta, Up);
		const double FlatSize = Flat.Size();
		if (FlatSize >= 1.0)
		{
			AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((Flat / FlatSize) | Pointing.Direction, -1.0, 1.0)));
		}
	}

	// The arc narrows from ArcNear (just outside the near radius) to ArcFar (full reach); inside the near radius the angle is ignored
	double ArcHalf;
	if (Pointing.b3D)
	{
		ArcHalf = Rules.AimArcDeg;
	}
	else
	{
		const double Span = FMath::Max<double>(Reach - Rules.NearRadiusCm, 1.0);
		const double Along = FMath::Clamp((SurfaceDistance - Rules.NearRadiusCm) / Span, 0.0, 1.0);
		ArcHalf = FMath::Lerp<double>(Rules.ArcNearDeg, Rules.ArcFarDeg, Along);
	}
	const bool bNear = SurfaceDistance <= Rules.NearRadiusCm;
	if (!bNear && AngleDeg > ArcHalf * ArcScale)
	{
		return false;
	}

	const int32 Priority = EffectivePriority(Query, Candidate);
	const double AngleTerm = ArcHalf > 0.0 ? FMath::Pow(AngleDeg / ArcHalf, Rules.AnglePower) : 0.0;
	const double Cost = Rules.AngleWeight * AngleTerm + Rules.DistanceWeight * (SurfaceDistance / Reach) + Rules.PriorityWeight * (1.0 - Priority / 255.0);

	OutScored.Candidate = Candidate;
	OutScored.CostFixed = FMath::Clamp(FMath::RoundToInt32(Cost * CostScale), 0, 100000 * CostScale);
	OutScored.TieBucket = OutScored.CostFixed / TieEpsilonFixed(Rules);
	OutScored.Priority = Priority;
	OutScored.DistanceCm = static_cast<float>(SurfaceDistance);
	OutScored.AngleDeg = static_cast<float>(AngleDeg);
	OutScored.bUnactionable = !bRequirementOk || Query.Frame < Candidate.ActionableFrom;
	return true;
}

bool UGSTN_InteractMath::SortsBefore(const FGSTN_InteractScored& A, const FGSTN_InteractScored& B, EGSTN_InteractPriorityMode Mode)
{
	if (Mode == EGSTN_InteractPriorityMode::Lexicographic && A.Priority != B.Priority)
	{
		return A.Priority > B.Priority;
	}
	if (A.TieBucket != B.TieBucket)
	{
		return A.TieBucket < B.TieBucket;
	}
	if (A.Priority != B.Priority)
	{
		return A.Priority > B.Priority;
	}
	return A.Candidate.Id < B.Candidate.Id;
}

namespace GSTNInteractMathPrivate
{
	/** Every live candidate near the origin: registry cells plus providers. Order is unspecified and never relied on */
	void GatherRaw(const FGSTN_InteractQuery& Query, TArray<FGSTN_InteractCandidate>& OutRaw)
	{
		if (Query.Registry)
		{
			Query.Registry->QueryAround(Query.Origin, Query.Frame, [&OutRaw](const FGSTN_InteractCandidate& Record) { OutRaw.Add(Record); });
		}
		const int32 FirstProvided = OutRaw.Num();
		for (const IGSTN_InteractCandidateProvider* Provider : Query.Providers)
		{
			if (Provider)
			{
				Provider->GatherCandidates(Query, OutRaw);
			}
		}
		for (int32 Index = OutRaw.Num() - 1; Index >= FirstProvided; --Index)
		{
			if (!OutRaw[Index].IsLiveAt(Query.Frame))
			{
				OutRaw.RemoveAtSwap(Index);
			}
		}
	}

	/** Caps the raw set by a distance-only prefilter under a total order, scores the survivors, applies the filter, sorts best first */
	void BuildScored(const FGSTN_InteractQuery& Query, const FGSTN_InteractPointing& Pointing, const TArray<FGSTN_InteractCandidate>& Raw,
		const FGSTN_InteractCandidateId& ExcludeA, const FGSTN_InteractCandidateId& ExcludeB, TArray<FGSTN_InteractScored>& OutSorted)
	{
		const FGSTN_InteractRules& Rules = *Query.Rules;
		const FVector Up = SafeUp(Query.Up);
		const FVector OriginQ = FGSTN_InteractCandidate::SnapCm(Query.Origin);

		struct FPrefilter
		{
			int32 DistanceKey;
			const FGSTN_InteractCandidate* Candidate;
		};
		TArray<FPrefilter> Kept;
		Kept.Reserve(Raw.Num());
		for (const FGSTN_InteractCandidate& Candidate : Raw)
		{
			if (Candidate.Id == ExcludeA || Candidate.Id == ExcludeB)
			{
				continue;
			}
			const FVector Delta = Candidate.GetPosition() - OriginQ;
			const double Surface = FMath::Max(0.0, Delta.Size() - Candidate.RadiusCm);
			if (Surface > EffectiveReach(Candidate, Rules))
			{
				continue;
			}
			if (!Pointing.b3D && FMath::Max(0.0, FMath::Abs(Delta | Up) - Candidate.RadiusCm) > Rules.MaxHeightDeltaCm)
			{
				continue;
			}
			Kept.Add({ FMath::RoundToInt32(Surface), &Candidate });
		}

		// Never "first K found": registration order differs between machines, a cap over the set under a total order does not
		Algo::Sort(Kept, [](const FPrefilter& A, const FPrefilter& B)
		{
			return A.DistanceKey != B.DistanceKey ? A.DistanceKey < B.DistanceKey : A.Candidate->Id < B.Candidate->Id;
		});
		if (Kept.Num() > Rules.MaxCandidatesPerProbe)
		{
			Kept.SetNum(Rules.MaxCandidatesPerProbe);
		}

		OutSorted.Reset();
		for (const FPrefilter& Entry : Kept)
		{
			FGSTN_InteractScored Scored;
			if (UGSTN_InteractMath::ScoreCandidate(Query, Pointing, *Entry.Candidate, 1.f, 1.f, Scored))
			{
				OutSorted.Add(Scored);
			}
		}
		const EGSTN_InteractPriorityMode Mode = Rules.PriorityMode;
		auto Order = [Mode](const FGSTN_InteractScored& A, const FGSTN_InteractScored& B) { return UGSTN_InteractMath::SortsBefore(A, B, Mode); };
		Algo::Sort(OutSorted, Order);

		if (Query.Filter && Rules.MaxFilterCalls > 0)
		{
			const int32 Calls = FMath::Min(Rules.MaxFilterCalls, OutSorted.Num());
			TArray<FGSTN_InteractScored> Filtered;
			Filtered.Reserve(OutSorted.Num());
			for (int32 Index = 0; Index < OutSorted.Num(); ++Index)
			{
				FGSTN_InteractScored Scored = OutSorted[Index];
				if (Index < Calls)
				{
					FGSTN_InteractCandidateView View;
					View.Id = Scored.Candidate.Id;
					View.MeaningIndex = Scored.Candidate.MeaningIndex;
					View.Cost = static_cast<float>(Scored.CostFixed) / UGSTN_InteractMath::CostScale;
					View.DistanceCm = Scored.DistanceCm;
					View.AngleDeg = Scored.AngleDeg;
					bool bAccept = true;
					float Bias = 0.f;
					Query.Filter->Evaluate(View, bAccept, Bias);
					if (!bAccept)
					{
						continue;
					}
					Scored.CostFixed = FMath::Max(0, Scored.CostFixed + FMath::RoundToInt(Bias * UGSTN_InteractMath::CostScale));
					Scored.TieBucket = Scored.CostFixed / TieEpsilonFixed(Rules);
				}
				Filtered.Add(Scored);
			}
			OutSorted = MoveTemp(Filtered);
			Algo::Sort(OutSorted, Order);
		}
	}
}

void UGSTN_InteractMath::GatherAndScore(const FGSTN_InteractQuery& Query, const FGSTN_InteractPointing& Pointing, TArray<FGSTN_InteractScored>& OutSorted)
{
	TArray<FGSTN_InteractCandidate> Raw;
	GatherRaw(Query, Raw);
	BuildScored(Query, Pointing, Raw, FGSTN_InteractCandidateId(), FGSTN_InteractCandidateId(), OutSorted);
}

FGSTN_InteractFocusResult UGSTN_InteractMath::UpdateFocus(const FGSTN_InteractQuery& Query, FGSTN_InteractFocusState& State)
{
	check(Query.Rules);
	const FGSTN_InteractRules& Rules = *Query.Rules;
	const int32 Frame = Query.Frame;
	const bool bHadFocus = State.FocusId.IsValid();

	FGSTN_InteractFocusResult Out;
	Out.FocusId = State.FocusId;
	Out.FocusSinceFrame = State.FocusSinceFrame;

	auto Lose = [&]()
	{
		Out.FocusId.Reset();
		Out.MeaningIndex = 0;
		Out.Reason = bHadFocus ? EGSTN_InteractFocusReason::Lost : EGSTN_InteractFocusReason::None;
		State.FocusId.Reset();
		State.bGhost = false;
	};

	if (!Rules.BlockedWhen.IsEmpty() && Rules.BlockedWhen.Matches(Query.Tags ? *Query.Tags : EmptyTags))
	{
		Lose();
		return Out;
	}

	const FGSTN_InteractPointing Pointing = ComputePointing(Query);
	TArray<FGSTN_InteractCandidate> Raw;
	GatherRaw(Query, Raw);

	if (Rules.CycleSkipFrames > 0 && Query.bCycleRequested && bHadFocus)
	{
		State.CycleSkipId = State.FocusId;
		State.CycleSkipUntilFrame = Frame + Rules.CycleSkipFrames;
	}
	const FGSTN_InteractCandidateId SkipId = (State.CycleSkipId.IsValid() && Frame < State.CycleSkipUntilFrame) ? State.CycleSkipId : FGSTN_InteractCandidateId();

	TArray<FGSTN_InteractScored> Others;
	BuildScored(Query, Pointing, Raw, SkipId, State.FocusId, Others);
	Out.CandidatesScored = Others.Num();

	// The incumbent is judged on its own, with the relaxed limits (a Schmitt trigger on the geometry)
	FGSTN_InteractScored Incumbent;
	bool bIncumbentKnown = false;
	bool bIncumbentValid = false;
	if (bHadFocus)
	{
		if (const FGSTN_InteractCandidate* Found = Raw.FindByPredicate([&State](const FGSTN_InteractCandidate& C) { return C.Id == State.FocusId; }))
		{
			bIncumbentKnown = true;
			bIncumbentValid = !(SkipId.IsValid() && SkipId == State.FocusId)
				&& ScoreCandidate(Query, Pointing, *Found, Rules.StickyReachScale, Rules.StickyArcScale, Incumbent);
		}
	}

	int32 Traces = 0;
	auto IsVisible = [&](const FGSTN_InteractCandidate& Candidate) -> bool
	{
		if (Candidate.LosPolicy != EGSTN_InteractLosPolicy::StaticGeometry || !Query.Visibility)
		{
			return true;
		}
		if (Traces >= Rules.MaxLosTraces)
		{
			// Cannot confirm it: not focusable (the budget, not a guess that it is clear)
			return false;
		}
		++Traces;
		return !Query.Visibility->IsSegmentBlocked(FGSTN_InteractCandidate::SnapCm(Query.Origin), Candidate.GetPosition());
	};

	auto Take = [&](const FGSTN_InteractScored& Scored, EGSTN_InteractFocusReason Reason, bool bNewFocus)
	{
		if (bNewFocus)
		{
			State.FocusId = Scored.Candidate.Id;
			State.FocusSinceFrame = Frame;
		}
		State.bGhost = false;
		Out.FocusId = State.FocusId;
		Out.FocusSinceFrame = State.FocusSinceFrame;
		Out.MeaningIndex = Scored.bUnactionable ? 0 : Scored.Candidate.MeaningIndex;
		Out.Flags = Scored.bUnactionable ? FGSTN_InteractFocusResult::FlagUnactionable : 0;
		Out.Cost = static_cast<float>(Scored.CostFixed) / CostScale;
		Out.Reason = Reason;
		Out.TracesUsed = Traces;
	};

	auto FirstVisible = [&]() -> const FGSTN_InteractScored*
	{
		for (const FGSTN_InteractScored& Candidate : Others)
		{
			if (IsVisible(Candidate.Candidate))
			{
				return &Candidate;
			}
		}
		return nullptr;
	};

	const int32 AbsMargin = FMath::RoundToInt(Rules.SwitchMarginAbs * CostScale);
	auto Beats = [&](const FGSTN_InteractScored& Challenger) -> bool
	{
		const int32 Margin = FMath::Max(AbsMargin, FMath::RoundToInt(Rules.SwitchMarginRatio * Incumbent.CostFixed));
		const bool bCheaper = Challenger.CostFixed + Margin < Incumbent.CostFixed;
		if (Rules.PriorityMode == EGSTN_InteractPriorityMode::Lexicographic)
		{
			return Challenger.Priority > Incumbent.Priority || (Challenger.Priority == Incumbent.Priority && bCheaper);
		}
		return bCheaper;
	};

	// A ghost (an id the server holds and this machine has no record for) waits for a known candidate to qualify after the dwell
	if (State.bGhost)
	{
		if (bIncumbentKnown)
		{
			State.bGhost = false;
		}
		else
		{
			if (Frame - State.FocusSinceFrame >= Rules.MinDwellFrames)
			{
				if (const FGSTN_InteractScored* Best = FirstVisible())
				{
					Take(*Best, EGSTN_InteractFocusReason::Switched, true);
					return Out;
				}
			}
			Out.MeaningIndex = 0;
			Out.Flags = FGSTN_InteractFocusResult::FlagGhost;
			Out.Reason = EGSTN_InteractFocusReason::Kept;
			Out.TracesUsed = Traces;
			return Out;
		}
	}

	if (bIncumbentValid && !IsVisible(Incumbent.Candidate))
	{
		bIncumbentValid = false;
	}

	if (bIncumbentValid)
	{
		if (Query.bInteractHeld && Rules.bHoldLocksFocus)
		{
			Take(Incumbent, EGSTN_InteractFocusReason::Locked, false);
			return Out;
		}
		if (Frame - State.FocusSinceFrame >= Rules.MinDwellFrames)
		{
			for (const FGSTN_InteractScored& Challenger : Others)
			{
				if (Beats(Challenger) && IsVisible(Challenger.Candidate))
				{
					Take(Challenger, EGSTN_InteractFocusReason::Switched, true);
					return Out;
				}
			}
		}
		Take(Incumbent, EGSTN_InteractFocusReason::Kept, false);
		return Out;
	}

	// No incumbent, or it is gone (removed, outside the relaxed limits, requirement lost): the best candidate at once, no dwell
	if (const FGSTN_InteractScored* Best = FirstVisible())
	{
		Take(*Best, bHadFocus ? EGSTN_InteractFocusReason::Switched : EGSTN_InteractFocusReason::Acquired, true);
		return Out;
	}
	Lose();
	Out.TracesUsed = Traces;
	return Out;
}

bool UGSTN_InteractMath::IsStillInteractable(const FGSTN_InteractCandidate& Candidate, const FVector& InteractorLocation, const FGameplayTagContainer* Tags,
	TConstArrayView<FGameplayTagQuery> Requirements, int32 Frame, const FGSTN_InteractRules& Rules)
{
	if (!Candidate.IsLiveAt(Frame) || Frame < Candidate.ActionableFrom || !RequirementHolds(Candidate, Tags, Requirements))
	{
		return false;
	}
	const FVector Delta = Candidate.GetPosition() - FGSTN_InteractCandidate::SnapCm(InteractorLocation);
	const double Surface = FMath::Max(0.0, Delta.Size() - Candidate.RadiusCm);
	return Surface <= EffectiveReach(Candidate, Rules) + Rules.ServerReachSlackCm;
}

FGSTN_InteractCandidateId UGSTN_InteractMath::MakeCandidateId(int32 Provider, int32 Handle)
{
	return FGSTN_InteractCandidateId(static_cast<uint8>(FMath::Clamp(Provider, 0, 255)), static_cast<uint32>(Handle));
}

int32 UGSTN_InteractMath::HashStableKey(const FString& StableKey)
{
	return static_cast<int32>(FCrc::StrCrc32(*StableKey));
}
