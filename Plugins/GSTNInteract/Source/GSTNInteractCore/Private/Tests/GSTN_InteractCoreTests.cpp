// Automation tests of the pure interaction focus core (interaction.md §10, GSTNInteract.Core.*). No world.

#include "GSTN_InteractMath.h"
#include "GSTN_InteractTags.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNInteractCoreTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Several Up vectors, so a hidden world-Z fails: gravity can point anywhere (game-design-direction §2d) */
	static TArray<FVector> UpVectors()
	{
		return { FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(1, 1, 1).GetSafeNormal(), FVector(-2, 1, 3).GetSafeNormal() };
	}

	struct FBasis
	{
		FVector Up, Forward, Right;
		explicit FBasis(const FVector& InUp)
		{
			const FQuat Frame = FQuat::FindBetweenNormals(FVector::UpVector, InUp);
			Up = InUp;
			Forward = Frame.RotateVector(FVector::ForwardVector);
			Right = Frame.RotateVector(FVector::RightVector);
		}
	};

	/** A mock static-geometry query: blocks segments that end near a listed point, counts calls */
	struct FMockVisibility : IGSTN_InteractVisibilityQuery
	{
		TArray<FVector> BlockedEnds;
		mutable int32 Calls = 0;
		virtual bool IsSegmentBlocked(const FVector& From, const FVector& To) const override
		{
			++Calls;
			return BlockedEnds.ContainsByPredicate([&To](const FVector& End) { return FVector::DistSquared(End, To) < 4.f; });
		}
	};

	struct FRig
	{
		FGSTN_InteractRules Rules;
		FGSTN_InteractRegistry Registry{ 800.f, 4096 };
		FGSTN_InteractMeaningTable Meanings;
		FGSTN_InteractFocusState State;
		FBasis Basis;
		FVector Origin = FVector(1234.f, -567.f, 89.f);
		FVector Stick = FVector::ZeroVector;
		float StickMagnitude = 0.f;
		bool bHeld = false;
		bool bCycle = false;
		int32 Frame = 100;
		const IGSTN_InteractVisibilityQuery* Visibility = nullptr;
		FGameplayTagContainer Tags;
		TArray<FGameplayTagQuery> Requirements;

		explicit FRig(const FVector& Up = FVector::UpVector) : Basis(Up)
		{
			FGSTN_InteractMeaning Meaning;
			Meaning.Tag = GSTNInteractTags::Root_Target.GetTag();
			Meaning.DefaultPriority = 100;
			Meanings.Build({ Meaning });
		}

		/** Position relative to the interactor: distance and angle in the plane, then height along Up */
		FVector PlaceAt(float Distance, float AngleDeg, float Height = 0.f) const
		{
			const float Radians = FMath::DegreesToRadians(AngleDeg);
			return Origin + Basis.Forward * (Distance * FMath::Cos(Radians)) + Basis.Right * (Distance * FMath::Sin(Radians)) + Basis.Up * Height;
		}

		FGSTN_InteractCandidate Make(int32 Handle, const FVector& Location, int32 Priority = FGSTN_InteractCandidate::DefaultPriority) const
		{
			FGSTN_InteractCandidate Candidate;
			Candidate.Id = FGSTN_InteractCandidateId(GSTNInteractProviders::Registry, Handle);
			Candidate.PositionCm = FGSTN_InteractCandidate::QuantiseCm(Location);
			Candidate.MeaningIndex = 1;
			Candidate.Priority = Priority;
			return Candidate;
		}

		bool Add(int32 Handle, float Distance, float AngleDeg, float Height = 0.f, int32 Priority = FGSTN_InteractCandidate::DefaultPriority)
		{
			return Registry.Add(Make(Handle, PlaceAt(Distance, AngleDeg, Height), Priority));
		}

		FGSTN_InteractCandidateId Id(int32 Handle) const { return FGSTN_InteractCandidateId(GSTNInteractProviders::Registry, Handle); }

		FGSTN_InteractQuery MakeQuery() const
		{
			FGSTN_InteractQuery Query;
			Query.Frame = Frame;
			Query.Origin = Origin;
			Query.Up = Basis.Up;
			Query.Facing = Basis.Forward;
			Query.StickDirection = Stick;
			Query.StickMagnitude = StickMagnitude;
			Query.bInteractHeld = bHeld;
			Query.bCycleRequested = bCycle;
			Query.Tags = &Tags;
			Query.Rules = &Rules;
			Query.Registry = &Registry;
			Query.Meanings = &Meanings;
			Query.Requirements = Requirements;
			Query.Visibility = Visibility;
			return Query;
		}

		FGSTN_InteractFocusResult Probe() { return UGSTN_InteractMath::UpdateFocus(MakeQuery(), State); }

		TArray<FGSTN_InteractScored> Scored() const
		{
			const FGSTN_InteractQuery Query = MakeQuery();
			TArray<FGSTN_InteractScored> Out;
			UGSTN_InteractMath::GatherAndScore(Query, UGSTN_InteractMath::ComputePointing(Query), Out);
			return Out;
		}
	};

	template <typename TFunc>
	static void ForEachUp(TFunc Func)
	{
		for (const FVector& Up : UpVectors())
		{
			Func(Up);
		}
	}
}

using namespace GSTNInteractCoreTest;

#define GSTN_INTERACT_CORE_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNInteractCoreTest_##Name, "GSTNInteract.Core." #Name, GSTNInteractCoreTest::Flags) \
	bool FGSTNInteractCoreTest_##Name::RunTest(const FString& Parameters)

GSTN_INTERACT_CORE_TEST(StickBeatsFacing)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 0.f);
		Rig.Add(2, 150.f, 45.f);

		TestTrue(TEXT("no stick: the facing points at the item ahead"), Rig.Probe().FocusId == Rig.Id(1));

		FRig Stick(Up);
		Stick.Add(1, 150.f, 0.f);
		Stick.Add(2, 150.f, 45.f);
		Stick.Stick = Stick.Basis.Right;
		Stick.StickMagnitude = 1.f;
		TestTrue(TEXT("the stick points sideways: the item ahead is outside the arc, the one to the side is chosen"), Stick.Probe().FocusId == Stick.Id(2));

		FRig Weak(Up);
		Weak.Add(1, 150.f, 0.f);
		Weak.Add(2, 150.f, 45.f);
		Weak.Stick = Weak.Basis.Right;
		Weak.StickMagnitude = 0.1f;
		TestTrue(TEXT("a stick below the threshold falls back to the facing"), Weak.Probe().FocusId == Weak.Id(1));

		FRig FacingOnly(Up);
		FacingOnly.Rules.PointingMode = EGSTN_InteractPointingMode::Facing;
		FacingOnly.Add(1, 150.f, 0.f);
		FacingOnly.Add(2, 150.f, 45.f);
		FacingOnly.Stick = FacingOnly.Basis.Right;
		FacingOnly.StickMagnitude = 1.f;
		TestTrue(TEXT("Facing mode ignores the stick"), FacingOnly.Probe().FocusId == FacingOnly.Id(1));
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(CameraAimUsesPitch)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Rules.PointingMode = EGSTN_InteractPointingMode::CameraAim;
		// A shelf item above and a low item below, both straight ahead
		Rig.Registry.Add(Rig.Make(1, Rig.Origin + Rig.Basis.Forward * 100.f + Rig.Basis.Up * 80.f));
		Rig.Registry.Add(Rig.Make(2, Rig.Origin + Rig.Basis.Forward * 100.f - Rig.Basis.Up * 80.f));

		FGSTN_InteractQuery Query = Rig.MakeQuery();
		Query.bHasAim = true;
		Query.AimDirection = (Rig.Basis.Forward * 100.f + Rig.Basis.Up * 80.f).GetSafeNormal();
		FGSTN_InteractFocusState State;
		TestTrue(TEXT("aiming up picks the shelf item"), UGSTN_InteractMath::UpdateFocus(Query, State).FocusId == Rig.Id(1));

		Query.AimDirection = (Rig.Basis.Forward * 100.f - Rig.Basis.Up * 80.f).GetSafeNormal();
		State.Reset();
		TestTrue(TEXT("aiming down picks the low item: pitch matters, the height band does not apply in 3D"), UGSTN_InteractMath::UpdateFocus(Query, State).FocusId == Rig.Id(2));

		Query.AimDirection = Rig.Basis.Forward;
		State.Reset();
		TestFalse(TEXT("aiming level at neither: the narrow aim arc rejects both"), UGSTN_InteractMath::UpdateFocus(Query, State).HasFocus());

		Query.bHasAim = false;
		State.Reset();
		TestTrue(TEXT("without aim values the mode falls back to the planar pointing"), UGSTN_InteractMath::UpdateFocus(Query, State).HasFocus());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(SidewaysUpPlanarAngle)
{
	// With Up along world X the interactor's "ground" is a wall: planar angles and the height band must follow Up, not world Z
	FRig Rig(FVector(1, 0, 0));
	Rig.Add(1, 150.f, 0.f);
	Rig.Registry.Add(Rig.Make(2, Rig.Origin - Rig.Basis.Forward * 150.f));
	Rig.Registry.Add(Rig.Make(3, Rig.Origin + Rig.Basis.Up * 100.f + Rig.Basis.Forward * 20.f));
	const FGSTN_InteractFocusResult Result = Rig.Probe();
	TestTrue(TEXT("the item ahead in the Up-perpendicular plane is chosen"), Result.FocusId == Rig.Id(1) || Result.FocusId == Rig.Id(3));

	TArray<FGSTN_InteractScored> Scored = Rig.Scored();
	TestFalse(TEXT("the item behind is outside the arc even though it lies on world Z"), Scored.ContainsByPredicate([&Rig](const FGSTN_InteractScored& S) { return S.Candidate.Id == Rig.Id(2); }));
	const FGSTN_InteractScored* Above = Scored.FindByPredicate([&Rig](const FGSTN_InteractScored& S) { return S.Candidate.Id == Rig.Id(3); });
	TestTrue(TEXT("100 cm along Up is inside the height band"), Above != nullptr);
	if (Above)
	{
		TestTrue(TEXT("its planar angle is measured around Up"), Above->AngleDeg < 5.f);
	}

	FRig Tilted(FVector(1, 1, 1).GetSafeNormal());
	Tilted.Add(1, 150.f, 30.f, 0.f);
	TArray<FGSTN_InteractScored> TiltedScored = Tilted.Scored();
	TestTrue(TEXT("a tilted Up still measures 30 degrees"), TiltedScored.Num() == 1 && FMath::IsNearlyEqual(TiltedScored[0].AngleDeg, 30.f, 0.1f));
	return true;
}

GSTN_INTERACT_CORE_TEST(OutsideArcRejected)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 190.f, 80.f);
		TestFalse(TEXT("80 degrees at near full reach is outside the far arc"), Rig.Probe().HasFocus());

		FRig Inside(Up);
		Inside.Add(1, 190.f, 40.f);
		TestTrue(TEXT("40 degrees is inside"), Inside.Probe().HasFocus());

		FRig Reach(Up);
		Reach.Add(1, 260.f, 0.f);
		TestFalse(TEXT("beyond the reach"), Reach.Probe().HasFocus());

		FRig Radius(Up);
		FGSTN_InteractCandidate Big = Radius.Make(1, Radius.PlaceAt(260.f, 0.f));
		Big.RadiusCm = 80;
		Radius.Registry.Add(Big);
		TestTrue(TEXT("reach is measured to the surface: radius 80 at 260 cm is 180 cm away"), Radius.Probe().HasFocus());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(NearRadiusIgnoresAngle)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 40.f, 170.f);
		TestTrue(TEXT("an item under the feet is eligible whichever way the character faces"), Rig.Probe().HasFocus());

		FRig Out(Up);
		Out.Add(1, 100.f, 170.f);
		TestFalse(TEXT("outside the near radius the angle counts"), Out.Probe().HasFocus());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(HeightBandRejectsOtherFloor)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 50.f, 0.f, 200.f);
		TestFalse(TEXT("200 cm up is another floor"), Rig.Probe().HasFocus());

		FRig Below(Up);
		Below.Add(1, 50.f, 0.f, -200.f);
		TestFalse(TEXT("and 200 cm down"), Below.Probe().HasFocus());

		FRig Same(Up);
		Same.Add(1, 50.f, 0.f, 100.f);
		TestTrue(TEXT("100 cm up is inside the band"), Same.Probe().HasFocus());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(PriorityBiasAndLexicographic)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Biased(Up);
		Biased.Add(1, 150.f, 0.f, 0.f, 10);
		Biased.Add(2, 150.f, 30.f, 0.f, 250);
		TestTrue(TEXT("biased: the aligned item wins, priority only nudges the cost"), Biased.Probe().FocusId == Biased.Id(1));

		FRig Lexicographic(Up);
		Lexicographic.Rules.PriorityMode = EGSTN_InteractPriorityMode::Lexicographic;
		Lexicographic.Add(1, 150.f, 0.f, 0.f, 10);
		Lexicographic.Add(2, 150.f, 30.f, 0.f, 250);
		TestTrue(TEXT("lexicographic: the higher priority beats the better cost"), Lexicographic.Probe().FocusId == Lexicographic.Id(2));

		FRig Default(Up);
		Default.Add(1, 150.f, 0.f);
		const TArray<FGSTN_InteractScored> Scored = Default.Scored();
		TestTrue(TEXT("a candidate with no priority takes its meaning's default"), Scored.Num() == 1 && Scored[0].Priority == 100);
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(TieBreakByIdTotal)
{
	ForEachUp([this](const FVector& Up)
	{
		const int32 Orders[2][3] = { { 5, 3, 9 }, { 9, 5, 3 } };
		for (const auto& Order : Orders)
		{
			FRig Rig(Up);
			for (const int32 Handle : Order)
			{
				Rig.Add(Handle, 120.f, 10.f);
			}
			TestTrue(TEXT("equal candidates: the lower id wins whatever the registration order"), Rig.Probe().FocusId == Rig.Id(3));
		}

		FRig Priority(Up);
		Priority.Rules.PriorityWeight = 0.f;
		Priority.Add(1, 120.f, 10.f, 0.f, 10);
		Priority.Add(2, 120.f, 10.f, 0.f, 20);
		TestTrue(TEXT("equal cost: the higher priority wins before the id"), Priority.Probe().FocusId == Priority.Id(2));
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(CapIsOrderIndependent)
{
	TArray<FGSTN_InteractCandidate> All;
	FRig Source;
	FRandomStream Random(1234);
	for (int32 Handle = 1; Handle <= 240; ++Handle)
	{
		All.Add(Source.Make(Handle, Source.PlaceAt(Random.FRandRange(10.f, 190.f), Random.FRandRange(-60.f, 60.f), Random.FRandRange(-100.f, 100.f))));
	}
	TArray<FGSTN_InteractCandidate> Shuffled = All;
	for (int32 Index = Shuffled.Num() - 1; Index > 0; --Index)
	{
		Shuffled.Swap(Index, Random.RandRange(0, Index));
	}

	FRig A;
	FRig B;
	for (const FGSTN_InteractCandidate& Candidate : All)
	{
		A.Registry.Add(Candidate);
	}
	for (const FGSTN_InteractCandidate& Candidate : Shuffled)
	{
		B.Registry.Add(Candidate);
	}
	A.Rules.MaxCandidatesPerProbe = 64;
	B.Rules.MaxCandidatesPerProbe = 64;
	const TArray<FGSTN_InteractScored> ScoredA = A.Scored();
	const TArray<FGSTN_InteractScored> ScoredB = B.Scored();
	TestTrue(TEXT("at most the cap survives"), ScoredA.Num() <= 64 && ScoredA.Num() > 10);
	bool bSame = ScoredA.Num() == ScoredB.Num();
	for (int32 Index = 0; bSame && Index < ScoredA.Num(); ++Index)
	{
		bSame = ScoredA[Index].Candidate.Id == ScoredB[Index].Candidate.Id && ScoredA[Index].CostFixed == ScoredB[Index].CostFixed;
	}
	TestTrue(TEXT("shuffled registration gives the same top set in the same order"), bSame);
	TestTrue(TEXT("and the same focus"), A.Probe().FocusId == B.Probe().FocusId);
	return true;
}

GSTN_INTERACT_CORE_TEST(QuantisationStable)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 20.f);
		const double Step = UE_DOUBLE_TWO_PI / 65536.0;
		const double Angle = 1000.0 * Step;
		const FVector OnGrid = Rig.Basis.Forward * FMath::Cos(Angle) + Rig.Basis.Right * FMath::Sin(Angle);

		auto Pointing = [&Rig](const FVector& Stick)
		{
			Rig.Stick = Stick;
			Rig.StickMagnitude = 1.f;
			return UGSTN_InteractMath::ComputePointing(Rig.MakeQuery()).Direction;
		};
		const FVector Reference = Pointing(OnGrid);
		const double Wobble = FMath::DegreesToRadians(0.0005);
		const FVector Wobbled = Rig.Basis.Forward * FMath::Cos(Angle + Wobble) + Rig.Basis.Right * FMath::Sin(Angle + Wobble);
		TestTrue(TEXT("a stick wobble below the 16 bit quantum gives the identical direction"), Pointing(Wobbled) == Reference);

		// The same holds for the origin: whole centimetres
		const TArray<FGSTN_InteractScored> Before = Rig.Scored();
		Rig.Origin += FVector(0.3f, -0.2f, 0.1f);
		const TArray<FGSTN_InteractScored> After = Rig.Scored();
		TestTrue(TEXT("a sub-centimetre move gives the identical fixed point cost"), Before.Num() == 1 && After.Num() == 1 && Before[0].CostFixed == After[0].CostFixed);
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(StickyKeepsIncumbent)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 10.f);
		Rig.Add(2, 150.f, 0.f);
		Rig.State.AdoptAuthoritative(Rig.Id(1), 0, false);
		const FGSTN_InteractFocusResult Result = Rig.Probe();
		TestTrue(TEXT("a slightly better neighbour does not take the focus: it is inside the switch margin"), Result.FocusId == Rig.Id(1) && Result.Reason == EGSTN_InteractFocusReason::Kept);

		FRig Edge(Up);
		Edge.Add(1, 225.f, 0.f);
		TestFalse(TEXT("a new item at 225 cm is beyond the reach"), Edge.Probe().HasFocus());
		Edge.State.AdoptAuthoritative(Edge.Id(1), 0, false);
		TestTrue(TEXT("but the incumbent keeps it up to the relaxed reach (x1.15)"), Edge.Probe().FocusId == Edge.Id(1));
		Edge.Origin += Edge.Basis.Forward * -30.f;
		TestFalse(TEXT("and loses it beyond that"), Edge.Probe().HasFocus());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(SwitchNeedsMarginAndDwell)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 40.f);
		Rig.Add(2, 150.f, 0.f);
		Rig.State.AdoptAuthoritative(Rig.Id(1), 95, false);
		Rig.Frame = 100;
		TestTrue(TEXT("a clearly better challenger waits for the dwell"), Rig.Probe().FocusId == Rig.Id(1));
		Rig.Frame = 105;
		const FGSTN_InteractFocusResult Result = Rig.Probe();
		TestTrue(TEXT("and takes the focus once the dwell is over"), Result.FocusId == Rig.Id(2) && Result.Reason == EGSTN_InteractFocusReason::Switched);
		TestEqual(TEXT("the focus restarts its dwell"), Result.FocusSinceFrame, 105);
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(HoldLocksFocus)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 40.f);
		Rig.Add(2, 150.f, 0.f);
		Rig.State.AdoptAuthoritative(Rig.Id(1), 0, false);
		Rig.bHeld = true;
		const FGSTN_InteractFocusResult Held = Rig.Probe();
		TestTrue(TEXT("held: the neighbour does not reset the hold"), Held.FocusId == Rig.Id(1) && Held.Reason == EGSTN_InteractFocusReason::Locked);
		Rig.bHeld = false;
		TestTrue(TEXT("released: the better one takes over"), Rig.Probe().FocusId == Rig.Id(2));

		FRig Unlocked(Up);
		Unlocked.Rules.bHoldLocksFocus = false;
		Unlocked.Add(1, 150.f, 40.f);
		Unlocked.Add(2, 150.f, 0.f);
		Unlocked.State.AdoptAuthoritative(Unlocked.Id(1), 0, false);
		Unlocked.bHeld = true;
		TestTrue(TEXT("with the lock off a hold does not protect the focus"), Unlocked.Probe().FocusId == Unlocked.Id(2));
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(GoneIncumbentSwitchesAtOnce)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		Rig.Add(1, 150.f, 0.f);
		Rig.Add(2, 150.f, 35.f);
		Rig.Frame = 100;
		TestTrue(TEXT("acquired"), Rig.Probe().FocusId == Rig.Id(1));
		Rig.Frame = 101;
		Rig.Registry.RemoveImmediate(Rig.Id(1));
		const FGSTN_InteractFocusResult Result = Rig.Probe();
		TestTrue(TEXT("the removed incumbent is replaced the next frame, with no dwell"), Result.FocusId == Rig.Id(2) && Result.Reason == EGSTN_InteractFocusReason::Switched);
		Rig.Registry.RemoveImmediate(Rig.Id(2));
		Rig.Frame = 102;
		const FGSTN_InteractFocusResult None = Rig.Probe();
		TestTrue(TEXT("none left: no focus"), !None.HasFocus() && None.Reason == EGSTN_InteractFocusReason::Lost);
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(GhostKeptUntilKnownQualifies)
{
	FRig Rig;
	Rig.Add(2, 150.f, 0.f);
	const FGSTN_InteractCandidateId Unknown(GSTNInteractProviders::Registry, 77);
	Rig.State.AdoptAuthoritative(Unknown, 95, true);
	Rig.Frame = 100;
	FGSTN_InteractFocusResult Result = Rig.Probe();
	TestTrue(TEXT("an unknown server focus is kept as a ghost"), Result.FocusId == Unknown && Result.IsGhost());
	TestEqual(TEXT("a ghost projects no meaning"), Result.MeaningIndex, 0);
	Rig.Frame = 105;
	Result = Rig.Probe();
	TestTrue(TEXT("after the dwell a known qualifying candidate replaces it"), Result.FocusId == Rig.Id(2) && !Result.IsGhost() && Result.Reason == EGSTN_InteractFocusReason::Switched);

	FRig Arrives;
	Arrives.Add(2, 150.f, 0.f);
	Arrives.State.AdoptAuthoritative(Arrives.Id(77), 95, true);
	Arrives.Frame = 100;
	TestTrue(TEXT("still a ghost"), Arrives.Probe().IsGhost());
	Arrives.Add(77, 150.f, 5.f);
	Result = Arrives.Probe();
	TestTrue(TEXT("when the record arrives the ghost becomes a normal incumbent"), Result.FocusId == Arrives.Id(77) && !Result.IsGhost() && Result.MeaningIndex == 1);
	return true;
}

GSTN_INTERACT_CORE_TEST(FromFrameDelaysCandidate)
{
	ForEachUp([this](const FVector& Up)
	{
		FRig Rig(Up);
		FGSTN_InteractCandidate Candidate = Rig.Make(1, Rig.PlaceAt(120.f, 0.f));
		Candidate.FromFrame = 120;
		Candidate.ActionableFrom = 130;
		Rig.Registry.Add(Candidate);
		Rig.Frame = 119;
		TestFalse(TEXT("not focusable before FromFrame"), Rig.Probe().HasFocus());
		Rig.Frame = 120;
		FGSTN_InteractFocusResult Result = Rig.Probe();
		TestTrue(TEXT("focusable at FromFrame but not actionable yet"), Result.HasFocus() && Result.IsUnactionable() && Result.MeaningIndex == 0);
		Rig.Frame = 130;
		Result = Rig.Probe();
		TestTrue(TEXT("the meaning is projected from ActionableFrom"), Result.MeaningIndex == 1 && !Result.IsUnactionable());
	});
	return true;
}

GSTN_INTERACT_CORE_TEST(TombstoneRemovesAtStamp)
{
	FRig Rig;
	Rig.Add(1, 120.f, 0.f);
	TestTrue(TEXT("a removal is a stamp"), Rig.Registry.Remove(Rig.Id(1), 150));
	Rig.Frame = 149;
	TestTrue(TEXT("still live the frame before the stamp"), Rig.Probe().HasFocus());
	Rig.Frame = 150;
	TestFalse(TEXT("gone at the stamp"), Rig.Probe().HasFocus());
	TestEqual(TEXT("the tombstone is still stored"), Rig.Registry.Num(), 1);
	TestFalse(TEXT("and its id is not reusable"), Rig.Add(1, 120.f, 0.f));
	TestEqual(TEXT("not purged while inside TombstoneFrames"), Rig.Registry.Purge(150 + Rig.Rules.TombstoneFrames, Rig.Rules.TombstoneFrames), 0);
	TestEqual(TEXT("purged after"), Rig.Registry.Purge(150 + Rig.Rules.TombstoneFrames + 1, Rig.Rules.TombstoneFrames), 1);
	TestEqual(TEXT("registry empty"), Rig.Registry.Num(), 0);
	return true;
}

GSTN_INTERACT_CORE_TEST(DuplicateStableKeyRejected)
{
	FRig Rig;
	TestTrue(TEXT("first add"), Rig.Add(5, 100.f, 0.f));
	TestFalse(TEXT("same key again"), Rig.Add(5, 110.f, 0.f));
	TestFalse(TEXT("an invalid id"), Rig.Registry.Add(FGSTN_InteractCandidate()));
	TestEqual(TEXT("one record"), Rig.Registry.Num(), 1);

	FGSTN_InteractRegistry Small(600.f, 2);
	TestTrue(TEXT("room"), Small.Add(Rig.Make(1, Rig.Origin)) && Small.Add(Rig.Make(2, Rig.Origin)));
	TestFalse(TEXT("MaxRecords is enforced"), Small.Add(Rig.Make(3, Rig.Origin)));

	TestEqual(TEXT("the stable key hash is deterministic"), UGSTN_InteractMath::HashStableKey(TEXT("Level.Chest_3")), UGSTN_InteractMath::HashStableKey(TEXT("Level.Chest_3")));
	return true;
}

GSTN_INTERACT_CORE_TEST(NoFlickerAmongFiftyItems)
{
	auto Run = [this](const FVector& Up, bool bGated, bool bWalk) -> int32
	{
		FRig Rig(Up);
		if (!bGated)
		{
			Rig.Rules.MinDwellFrames = 0;
			Rig.Rules.SwitchMarginAbs = 0.f;
			Rig.Rules.SwitchMarginRatio = 0.f;
		}
		// 10 columns x 5 rows, 40 cm apart, the character centred between the two middle columns, rows ahead
		int32 Handle = 1;
		for (int32 Row = 0; Row < 5; ++Row)
		{
			for (int32 Column = 0; Column < 10; ++Column)
			{
				Rig.Registry.Add(Rig.Make(Handle++, Rig.Origin + Rig.Basis.Forward * (100.f + Row * 40.f) + Rig.Basis.Right * ((Column - 4.5f) * 40.f)));
			}
		}
		FRandomStream Noise(77);
		int32 Switches = 0;
		FGSTN_InteractCandidateId Last;
		const FVector WalkStart = Rig.Origin;
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			Rig.Frame = 1000 + Frame;
			const int32 Steps = Noise.RandRange(-2, 2);
			const FVector Direction = bWalk ? Rig.Basis.Right : Rig.Basis.Forward;
			const FVector Side = bWalk ? Rig.Basis.Forward : Rig.Basis.Right;
			Rig.Stick = Direction + Side * (Steps / 127.f);
			Rig.StickMagnitude = 1.f;
			if (bWalk && Frame < 300)
			{
				Rig.Origin = WalkStart + Rig.Basis.Right * (Frame * 1.5f);
			}
			const FGSTN_InteractFocusResult Result = Rig.Probe();
			if (Result.Reason == EGSTN_InteractFocusReason::Switched)
			{
				++Switches;
				TestTrue(TEXT("a switch keeps a focus"), Result.HasFocus() && Result.FocusId != Last);
			}
			Last = Result.FocusId;
		}
		return Switches;
	};

	for (const FVector& Up : { FVector(0, 0, 1), FVector(1, 0, 0), FVector(1, 1, 1).GetSafeNormal() })
	{
		const int32 Standing = Run(Up, true, false);
		const int32 Baseline = Run(Up, false, false);
		TestEqual(TEXT("standing with stick noise: zero switches"), Standing, 0);
		TestTrue(TEXT("the ungated baseline flips on the same noise"), Baseline > 0);

		const int32 Walking = Run(Up, true, true);
		const FGSTN_InteractRules Defaults;
		TestTrue(TEXT("walking along the row: at most one switch per dwell"), Walking <= 600 / Defaults.MinDwellFrames);
	}
	return true;
}

GSTN_INTERACT_CORE_TEST(LosBudgetAndIncumbentFirst)
{
	FRig Rig;
	FMockVisibility Visibility;
	Rig.Visibility = &Visibility;
	for (int32 Handle = 1; Handle <= 6; ++Handle)
	{
		FGSTN_InteractCandidate Candidate = Rig.Make(Handle, Rig.PlaceAt(100.f + Handle * 10.f, 0.f));
		Candidate.LosPolicy = EGSTN_InteractLosPolicy::StaticGeometry;
		Rig.Registry.Add(Candidate);
		if (Handle <= 2)
		{
			Visibility.BlockedEnds.Add(Candidate.GetPosition());
		}
	}
	FGSTN_InteractFocusResult Result = Rig.Probe();
	TestTrue(TEXT("the first two are behind a wall: the third is the focus"), Result.FocusId == Rig.Id(3));
	TestEqual(TEXT("three traces were spent"), Result.TracesUsed, 3);

	Visibility.Calls = 0;
	Rig.Frame = 101;
	Result = Rig.Probe();
	TestTrue(TEXT("the focus is stable"), Result.FocusId == Rig.Id(3));
	TestEqual(TEXT("a stable focus costs one trace"), Visibility.Calls, 1);

	FRig Budget;
	FMockVisibility AllBlocked;
	Budget.Visibility = &AllBlocked;
	for (int32 Handle = 1; Handle <= 6; ++Handle)
	{
		FGSTN_InteractCandidate Candidate = Budget.Make(Handle, Budget.PlaceAt(100.f + Handle * 10.f, 0.f));
		Candidate.LosPolicy = EGSTN_InteractLosPolicy::StaticGeometry;
		Budget.Registry.Add(Candidate);
		AllBlocked.BlockedEnds.Add(Candidate.GetPosition());
	}
	Budget.Probe();
	TestEqual(TEXT("never more traces than MaxLosTraces"), AllBlocked.Calls, Budget.Rules.MaxLosTraces);
	return true;
}

GSTN_INTERACT_CORE_TEST(RequirementsAndBlocking)
{
	FRig Rig;
	FGSTN_InteractCandidate Locked = Rig.Make(1, Rig.PlaceAt(100.f, 0.f));
	Locked.QueryIndex = 0;
	Rig.Registry.Add(Locked);
	Rig.Requirements.Add(FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNInteractTags::Root_Interact.GetTag())));
	TestFalse(TEXT("a failed requirement is not focusable"), Rig.Probe().HasFocus());

	Rig.Tags.AddTag(GSTNInteractTags::Root_Interact.GetTag());
	TestTrue(TEXT("a met one is"), Rig.Probe().HasFocus());

	FRig Soft;
	FGSTN_InteractCandidate Terminal = Soft.Make(1, Soft.PlaceAt(100.f, 0.f));
	Terminal.QueryIndex = 0;
	Terminal.Flags = FGSTN_InteractCandidate::FlagFocusWhenUnactionable;
	Soft.Registry.Add(Terminal);
	Soft.Requirements.Add(FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNInteractTags::Root_Interact.GetTag())));
	const FGSTN_InteractFocusResult Greyed = Soft.Probe();
	TestTrue(TEXT("bFocusWhenUnactionable: focused with no projected meaning (INT.6)"), Greyed.HasFocus() && Greyed.IsUnactionable() && Greyed.MeaningIndex == 0);

	FRig Blocked;
	Blocked.Add(1, 100.f, 0.f);
	Blocked.Rules.BlockedWhen = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNInteractTags::Root_Interact.GetTag()));
	TestTrue(TEXT("focus before the block"), Blocked.Probe().HasFocus());
	Blocked.Tags.AddTag(GSTNInteractTags::Root_Interact.GetTag());
	Blocked.Frame++;
	TestTrue(TEXT("BlockedWhen clears the focus"), !Blocked.Probe().HasFocus());
	return true;
}

GSTN_INTERACT_CORE_TEST(CycleSkipsIncumbent)
{
	FRig Rig;
	Rig.Rules.CycleSkipFrames = 30;
	Rig.Add(1, 100.f, 0.f);
	Rig.Add(2, 120.f, 10.f);
	TestTrue(TEXT("best first"), Rig.Probe().FocusId == Rig.Id(1));
	Rig.Frame++;
	Rig.bCycle = true;
	TestTrue(TEXT("cycle: the next candidate at once, no dwell"), Rig.Probe().FocusId == Rig.Id(2));
	Rig.bCycle = false;
	Rig.Frame += 5;
	TestTrue(TEXT("the skipped one stays out during the window"), Rig.Probe().FocusId == Rig.Id(2));

	FRig Off;
	Off.Add(1, 100.f, 0.f);
	Off.Add(2, 120.f, 10.f);
	Off.Probe();
	Off.Frame++;
	Off.bCycle = true;
	TestTrue(TEXT("with CycleSkipFrames 0 the command does nothing"), Off.Probe().FocusId == Off.Id(1));
	return true;
}

GSTN_INTERACT_CORE_TEST(IsStillInteractableUsesSlackAndStamps)
{
	FRig Rig;
	FGSTN_InteractCandidate Candidate = Rig.Make(1, Rig.PlaceAt(240.f, 0.f));
	Candidate.ActionableFrom = 90;
	Candidate.ToFrame = 200;
	const TConstArrayView<FGameplayTagQuery> NoQueries;
	TestTrue(TEXT("40 cm past the reach is inside the 60 cm slack"), UGSTN_InteractMath::IsStillInteractable(Candidate, Rig.Origin, nullptr, NoQueries, 100, Rig.Rules));
	Candidate.PositionCm = FGSTN_InteractCandidate::QuantiseCm(Rig.PlaceAt(280.f, 0.f));
	TestFalse(TEXT("80 cm past is not"), UGSTN_InteractMath::IsStillInteractable(Candidate, Rig.Origin, nullptr, NoQueries, 100, Rig.Rules));
	Candidate.PositionCm = FGSTN_InteractCandidate::QuantiseCm(Rig.PlaceAt(100.f, 0.f));
	TestFalse(TEXT("after ToFrame the record is gone"), UGSTN_InteractMath::IsStillInteractable(Candidate, Rig.Origin, nullptr, NoQueries, 200, Rig.Rules));
	TestFalse(TEXT("before ActionableFrom it cannot be acted on"), UGSTN_InteractMath::IsStillInteractable(Candidate, Rig.Origin, nullptr, NoQueries, 89, Rig.Rules));
	return true;
}

GSTN_INTERACT_CORE_TEST(MeaningTableIsOrderIndependent)
{
	FGSTN_InteractMeaning A;
	A.Tag = GSTNInteractTags::Root_Interact.GetTag();
	A.DefaultPriority = 50;
	FGSTN_InteractMeaning B;
	B.Tag = GSTNInteractTags::Root_Target.GetTag();
	B.DefaultPriority = 80;
	FGSTN_InteractMeaning Invalid;

	FGSTN_InteractMeaningTable First;
	FGSTN_InteractMeaningTable Second;
	TArray<FString> Errors;
	First.Build({ A, B, Invalid, B }, &Errors);
	Second.Build({ B, A });
	TestEqual(TEXT("an invalid tag and a duplicate are reported"), Errors.Num(), 2);
	TestEqual(TEXT("two meanings"), First.Num(), 2);
	TestEqual(TEXT("indices follow the sorted tag names, not the listing order"), First.IndexOf(A.Tag), Second.IndexOf(A.Tag));
	TestEqual(TEXT("1-based: 0 is no meaning"), First.IndexOf(A.Tag), 1);
	TestEqual(TEXT("unknown tags are 0"), First.IndexOf(FGameplayTag()), 0);
	TestEqual(TEXT("default priority"), First.DefaultPriorityOf(First.IndexOf(B.Tag)), 80);
	TestEqual(TEXT("the signature does not depend on the listing order"), First.Signature(), Second.Signature());
	B.DefaultPriority = 81;
	FGSTN_InteractMeaningTable Third;
	Third.Build({ A, B });
	TestNotEqual(TEXT("and changes with the content"), First.Signature(), Third.Signature());
	return true;
}

GSTN_INTERACT_CORE_TEST(RulesInvariantsAndTags)
{
	FGSTN_InteractRules Rules;
	FString Error;
	TestTrue(TEXT("the defaults are valid"), Rules.IsValid(&Error));
	Rules.ReachCm = 900.f;
	Rules.ArcNearDeg = 10.f;
	TestFalse(TEXT("a reach above the hard maximum is invalid"), Rules.IsValid(&Error));
	TestTrue(TEXT("Sanitise fixes it"), Rules.Sanitise());
	TestTrue(TEXT("and the result is valid"), Rules.IsValid(&Error));

	TArray<FGameplayTag> Declared;
	GSTNInteractTags::GetDeclaredTags(Declared);
	TestEqual(TEXT("three native tags"), Declared.Num(), 3);
	for (const FGameplayTag& Tag : Declared)
	{
		TestTrue(TEXT("each tag resolves"), Tag.IsValid());
	}
	TestTrue(TEXT("Interact.Target sits under Interact"), GSTNInteractTags::Root_Target.GetTag().MatchesTag(GSTNInteractTags::Root_Interact.GetTag()));

	const FGSTN_InteractCandidateId Id(7, 0xFFFFFFFFu);
	TestTrue(TEXT("the opaque key round trips"), FGSTN_InteractCandidateId::FromKey(Id.ToKey()) == Id);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
