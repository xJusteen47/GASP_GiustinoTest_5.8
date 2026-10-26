// Automation tests of the penetration table and the baked curve (plan P2).

#include "Curves/RichCurve.h"
#include "Data/GSTN_PartsBakedCurve.h"
#include "Data/GSTN_PartsValidation.h"
#include "Data/GSTN_PenetrationTable.h"
#include "GSTN_PartsTestUtils.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBandLookupTest, "GSTNParts.Core.Penetration.BandLookupMinus8ToPlus8", GSTNPartsTest::Flags)
bool FGSTN_PartsBandLookupTest::RunTest(const FString& Parameters)
{
	// Hand computed rows of the default table: [+1 and up] -> 0 above, [0] -> 1 equal, [-15 and down] -> 2 below
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	for (int32 Delta = -8; Delta <= 8; ++Delta)
	{
		const int32 Expected = Delta >= 1 ? 0 : (Delta == 0 ? 1 : 2);
		TestEqual(FString::Printf(TEXT("Delta %d"), Delta), UGSTN_PenetrationTable::LookupBandIndex(Bands, Delta), Expected);
	}

	// The same through penetration and hardness: the delta is their difference and nothing else
	TestEqual(TEXT("Pen 5 against hardness 4"), UGSTN_PenetrationTable::LookupBand(Bands, 5, 4, false), 0);
	TestEqual(TEXT("Pen 4 against hardness 4"), UGSTN_PenetrationTable::LookupBand(Bands, 4, 4, false), 1);
	TestEqual(TEXT("Pen 3 against hardness 4"), UGSTN_PenetrationTable::LookupBand(Bands, 3, 4, false), 2);
	TestEqual(TEXT("Pen 0 against the hardest part"), UGSTN_PenetrationTable::LookupBand(Bands, 0, 15, false), 2);
	TestEqual(TEXT("Pen 15 against the softest part"), UGSTN_PenetrationTable::LookupBand(Bands, 15, 0, false), 0);

	// A table with more bands: a reduced band between equal and ricochet, as the spec suggests for a softer curve
	TArray<FGSTN_PenetrationBand> Soft = Bands;
	FGSTN_PenetrationBand Reduced;
	Reduced.MinDelta = -2;
	Reduced.PartScale = 0.3f;
	Reduced.MainScale = 0.3f;
	Soft.Insert(Reduced, 2);
	for (int32 Delta = -8; Delta <= 8; ++Delta)
	{
		const int32 Expected = Delta >= 1 ? 0 : (Delta == 0 ? 1 : (Delta >= -2 ? 2 : 3));
		TestEqual(FString::Printf(TEXT("Soft table, delta %d"), Delta), UGSTN_PenetrationTable::LookupBandIndex(Soft, Delta), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsIgnoreHardnessTest, "GSTNParts.Core.Penetration.IgnoreHardness", GSTNPartsTest::Flags)
bool FGSTN_PartsIgnoreHardnessTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	// A hit that would ricochet takes the top band instead, whatever the hardness
	TestEqual(TEXT("Ricochets without the flag"), UGSTN_PenetrationTable::LookupBand(Bands, 0, 15, false), 2);
	TestEqual(TEXT("Top band with the flag"), UGSTN_PenetrationTable::LookupBand(Bands, 0, 15, true), 0);
	TestFalse(TEXT("The top band does not ricochet"), Bands[UGSTN_PenetrationTable::LookupBand(Bands, 0, 15, true)].bRicochet);
	TestEqual(TEXT("The flag on an empty table finds nothing"), UGSTN_PenetrationTable::LookupBand(TConstArrayView<FGSTN_PenetrationBand>(), 0, 15, true), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOneBandTableTest, "GSTNParts.Core.Penetration.OneBandTable", GSTNPartsTest::Flags)
bool FGSTN_PartsOneBandTableTest::RunTest(const FString& Parameters)
{
	// A table with one band reaching the lowest possible delta applies to every hit: armour that does not matter
	FGSTN_PenetrationBand Only;
	Only.MinDelta = GSTN_PartsLimits::MinPossibleDelta;
	const TArray<FGSTN_PenetrationBand> Bands = { Only };
	for (int32 Pen = 0; Pen <= 15; ++Pen)
	{
		for (int32 Hardness = 0; Hardness <= 15; ++Hardness)
		{
			if (UGSTN_PenetrationTable::LookupBand(Bands, Pen, Hardness, false) != 0)
			{
				AddError(FString::Printf(TEXT("Pen %d against hardness %d missed the only band"), Pen, Hardness));
				return false;
			}
		}
	}

	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateBands(Bands, Report);
	TestEqual(TEXT("A one band table is valid and complete"), Report.Issues.Num(), 0);

	// A band that stops short of the lowest delta is valid but warned about, and still answers (the last band)
	FGSTN_PenetrationBand Short;
	Short.MinDelta = 0;
	const TArray<FGSTN_PenetrationBand> Gap = { Short };
	TestEqual(TEXT("Below the only band falls back on it"), UGSTN_PenetrationTable::LookupBandIndex(Gap, -9), 0);
	Report = FGSTN_PartsValidationReport();
	FGSTN_PartsValidation::ValidateBands(Gap, Report);
	TestEqual(TEXT("A gap is not an error"), Report.NumErrors(), 0);
	TestTrue(TEXT("A gap is a warning"), Report.Contains(false, TEXT("lowest band starts")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsEmptyTableTest, "GSTNParts.Core.Penetration.EmptyTableRejected", GSTNPartsTest::Flags)
bool FGSTN_PartsEmptyTableTest::RunTest(const FString& Parameters)
{
	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateBands(TConstArrayView<FGSTN_PenetrationBand>(), Report);
	TestTrue(TEXT("An empty band set is an error"), Report.Contains(true, TEXT("no bands")));
	TestEqual(TEXT("Lookup in nothing finds nothing"), UGSTN_PenetrationTable::LookupBandIndex(TConstArrayView<FGSTN_PenetrationBand>(), 3), static_cast<int32>(INDEX_NONE));

	UGSTN_PenetrationTable* Table = NewObject<UGSTN_PenetrationTable>(GetTransientPackage());
	FDataValidationContext Valid;
	TestEqual(TEXT("The default table validates"), static_cast<int32>(Table->IsDataValid(Valid)), static_cast<int32>(EDataValidationResult::Valid));
	Table->Bands.Reset();
	FDataValidationContext Invalid;
	TestEqual(TEXT("An emptied table is invalid"), static_cast<int32>(Table->IsDataValid(Invalid)), static_cast<int32>(EDataValidationResult::Invalid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBandsSortedTest, "GSTNParts.Core.Penetration.BandsSortedDescending", GSTNPartsTest::Flags)
bool FGSTN_PartsBandsSortedTest::RunTest(const FString& Parameters)
{
	UGSTN_PenetrationTable* Table = NewObject<UGSTN_PenetrationTable>(GetTransientPackage());
	TestTrue(TEXT("A new table is sorted"), Table->Bands[0].MinDelta > Table->Bands[1].MinDelta && Table->Bands[1].MinDelta > Table->Bands[2].MinDelta);

	// Authored out of order, the way a designer adds rows
	Table->Bands.Reset();
	for (const int32 MinDelta : { -2, 3, -15, 0, 1 })
	{
		FGSTN_PenetrationBand Band;
		Band.MinDelta = MinDelta;
		Band.PartScale = static_cast<float>(MinDelta + 20); // marks the band so a stable order can be checked
		Table->Bands.Add(Band);
	}
	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateBands(Table->Bands, Report);
	TestTrue(TEXT("Unsorted bands are reported before Normalize"), Report.Contains(true, TEXT("not sorted")));

	Table->Normalize();
	const int32 Expected[] = { 3, 1, 0, -2, -15 };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Band %d after Normalize"), Index), Table->Bands[Index].MinDelta, Expected[Index]);
		TestEqual(FString::Printf(TEXT("Band %d kept its data"), Index), Table->Bands[Index].PartScale, static_cast<float>(Expected[Index] + 20));
	}
	Report = FGSTN_PartsValidationReport();
	FGSTN_PartsValidation::ValidateBands(Table->Bands, Report);
	TestEqual(TEXT("A normalised table is valid"), Report.NumErrors(), 0);
	TestEqual(TEXT("Lookup works on the normalised table"), UGSTN_PenetrationTable::LookupBandIndex(Table->Bands, 2), 1);

	// Two bands with one MinDelta cannot be told apart
	const FGSTN_PenetrationBand Copy = Table->Bands[1];
	Table->Bands.Add(Copy);
	Table->Normalize();
	Report = FGSTN_PartsValidationReport();
	FGSTN_PartsValidation::ValidateBands(Table->Bands, Report);
	TestTrue(TEXT("A duplicate MinDelta is an error"), Report.Contains(true, TEXT("same MinDelta")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsDefaultTableTest, "GSTNParts.Core.Data.DefaultTableThreeBands", GSTNPartsTest::Flags)
bool FGSTN_PartsDefaultTableTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	if (!TestEqual(TEXT("Three bands"), Bands.Num(), 3))
	{
		return false;
	}

	// Above: full damage
	TestEqual(TEXT("Above part scale"), Bands[0].PartScale, 1.f);
	TestEqual(TEXT("Above main scale"), Bands[0].MainScale, 1.f);
	TestFalse(TEXT("Above does not ricochet"), Bands[0].bRicochet);
	// Equal: the Helldivers 65 %
	TestEqual(TEXT("Equal part scale"), Bands[1].PartScale, 0.65f);
	TestEqual(TEXT("Equal main scale"), Bands[1].MainScale, 0.65f);
	TestFalse(TEXT("Equal does not ricochet"), Bands[1].bRicochet);
	// Below: ricochet, nothing gets through
	TestTrue(TEXT("Below ricochets"), Bands[2].bRicochet);
	TestEqual(TEXT("Below part scale"), Bands[2].PartScale, 0.f);
	TestEqual(TEXT("Below main scale"), Bands[2].MainScale, 0.f);
	TestEqual(TEXT("Below stagger scale"), Bands[2].StaggerScale, 0.f);
	TestEqual(TEXT("Below hitstop scale"), Bands[2].HitstopScale, 0.f);
	// Hardness acts on damage only (owner 2.h): the damaging bands leave stagger and hitstop alone
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Band %d stagger scale is 1"), Index), Bands[Index].StaggerScale, 1.f);
		TestEqual(FString::Printf(TEXT("Band %d hitstop scale is 1"), Index), Bands[Index].HitstopScale, 1.f);
	}
	// The cue tags are designer vocabulary: code does not name a leaf
	for (const FGSTN_PenetrationBand& Band : Bands)
	{
		TestFalse(TEXT("The built-in bands carry no cue tag"), Band.CueTag.IsValid());
	}

	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateBands(Bands, Report);
	TestEqual(TEXT("The default is valid, complete and without warnings"), Report.Issues.Num(), 0);
	TestEqual(TEXT("A new table asset starts as the default"), NewObject<UGSTN_PenetrationTable>(GetTransientPackage())->Bands.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBakedCurveHandValuesTest, "GSTNParts.Core.BakedCurve.MatchesHandValues", GSTNPartsTest::Flags)
bool FGSTN_PartsBakedCurveHandValuesTest::RunTest(const FString& Parameters)
{
	// The default weight curve, 1.0 falling to 0.6: entries are 65535 + round((39321 - 65535) * i / 63), worked by hand
	const FGSTN_PartsBakedCurve Curve = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);
	TestEqual(TEXT("Entry 0"), static_cast<int32>(Curve.Table[0]), 65535);
	TestEqual(TEXT("Entry 21"), static_cast<int32>(Curve.Table[21]), 56797);
	TestEqual(TEXT("Entry 31"), static_cast<int32>(Curve.Table[31]), 52636);
	TestEqual(TEXT("Entry 32"), static_cast<int32>(Curve.Table[32]), 52220);
	TestEqual(TEXT("Entry 42"), static_cast<int32>(Curve.Table[42]), 48059);
	TestEqual(TEXT("Entry 63"), static_cast<int32>(Curve.Table[63]), 39321);

	// Reads, worked by hand: progress 0, one half (between entries 31 and 32, 32799/65535 of the way), one, one sixth
	TestEqual(TEXT("Sample at 0"), static_cast<int32>(Curve.SampleQuantised(0)), 65535);
	TestEqual(TEXT("Sample at 32768"), static_cast<int32>(Curve.SampleQuantised(32768)), 52428);
	TestEqual(TEXT("Sample at 1"), static_cast<int32>(Curve.SampleQuantised(FGSTN_PartsBakedCurve::One)), 39321);
	TestEqual(TEXT("Sample at 10000"), static_cast<int32>(Curve.SampleQuantised(10000)), 61535);
	TestEqual(TEXT("Beyond one clamps"), static_cast<int32>(Curve.SampleQuantised(99999)), 39321);

	// The float entry points quantise first
	TestEqual(TEXT("Quantise 0.6"), static_cast<int32>(FGSTN_PartsBakedCurve::QuantiseUnit(0.6f)), 39321);
	TestEqual(TEXT("Quantise below zero"), static_cast<int32>(FGSTN_PartsBakedCurve::QuantiseUnit(-3.f)), 0);
	TestEqual(TEXT("Quantise above one"), static_cast<int32>(FGSTN_PartsBakedCurve::QuantiseUnit(7.f)), 65535);
	TestEqual(TEXT("Quantise NaN"), static_cast<int32>(FGSTN_PartsBakedCurve::QuantiseUnit(std::numeric_limits<float>::quiet_NaN())), 0);
	TestEqual(TEXT("Sample(0) is exactly 1"), Curve.Sample(0.f), 1.f);
	TestEqual(TEXT("Sample(0.5) from the integer read"), Curve.Sample(0.5f), FGSTN_PartsBakedCurve::DequantiseUnit(Curve.SampleQuantised(FGSTN_PartsBakedCurve::QuantiseUnit(0.5f))));

	// A float curve bakes to the same shape (the one place floats are evaluated, at load)
	FRichCurve Rich;
	Rich.AddKey(0.f, 1.f);
	Rich.AddKey(1.f, 0.6f);
	const FGSTN_PartsBakedCurve FromRich = FGSTN_PartsBakedCurve::FromRichCurve(Rich);
	for (int32 Index = 0; Index < GSTN_PartsLimits::CurveEntries; ++Index)
	{
		TestTrue(FString::Printf(TEXT("Baked entry %d follows the float curve"), Index), FMath::Abs(static_cast<int32>(FromRich.Table[Index]) - static_cast<int32>(Curve.Table[Index])) <= 1);
	}

	TestTrue(TEXT("A flat curve is flat"), FGSTN_PartsBakedCurve::MakeFlat(0.8f).SampleQuantised(12345) == FGSTN_PartsBakedCurve::QuantiseUnit(0.8f));
	TestTrue(TEXT("The default constructed curve is flat at one"), FGSTN_PartsBakedCurve().SampleQuantised(54321) == FGSTN_PartsBakedCurve::One);
	TestTrue(TEXT("Equal curves compare equal"), FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f) == Curve && FGSTN_PartsBakedCurve::MakeFlat(0.6f) != Curve);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBakedCurveMonotonicTest, "GSTNParts.Core.BakedCurve.Monotonic", GSTNPartsTest::Flags)
bool FGSTN_PartsBakedCurveMonotonicTest::RunTest(const FString& Parameters)
{
	const FGSTN_PartsBakedCurve Falling = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);
	const FGSTN_PartsBakedCurve Rising = FGSTN_PartsBakedCurve::MakeLinear(0.f, 1.f);
	uint32 PreviousFalling = FGSTN_PartsBakedCurve::One;
	uint32 PreviousRising = 0;
	for (uint32 Progress = 0; Progress <= FGSTN_PartsBakedCurve::One; ++Progress)
	{
		const uint32 F = Falling.SampleQuantised(Progress);
		const uint32 R = Rising.SampleQuantised(Progress);
		if (F > PreviousFalling || R < PreviousRising)
		{
			AddError(FString::Printf(TEXT("Not monotonic at progress %u (falling %u after %u, rising %u after %u)"), Progress, F, PreviousFalling, R, PreviousRising));
			return false;
		}
		PreviousFalling = F;
		PreviousRising = R;
	}
	TestEqual(TEXT("The falling curve ends at its last entry"), PreviousFalling, static_cast<uint32>(Falling.Table[63]));
	TestEqual(TEXT("The rising curve ends at one"), PreviousRising, FGSTN_PartsBakedCurve::One);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBakedCurveIntegerOnlyTest, "GSTNParts.Core.BakedCurve.IntegerOnly", GSTNPartsTest::Flags)
bool FGSTN_PartsBakedCurveIntegerOnlyTest::RunTest(const FString& Parameters)
{
	// The read is integer arithmetic: it stays between the two entries it interpolates, for every input, even on the steepest
	// possible curve (0 to one over one step), where a 32 bit product would overflow
	TArray<float> Samples;
	for (int32 Index = 0; Index < GSTN_PartsLimits::CurveEntries; ++Index)
	{
		Samples.Add(Index % 2 == 0 ? 0.f : 1.f);
	}
	const FGSTN_PartsBakedCurve Zigzag = FGSTN_PartsBakedCurve::FromSamples(Samples);
	for (uint32 Progress = 0; Progress <= FGSTN_PartsBakedCurve::One; Progress += 7)
	{
		const uint32 Position = Progress * 63;
		const uint32 Index = FMath::Min<uint32>(Position / FGSTN_PartsBakedCurve::One, 62);
		const uint32 Low = FMath::Min<uint32>(Zigzag.Table[Index], Zigzag.Table[Index + 1]);
		const uint32 High = FMath::Max<uint32>(Zigzag.Table[Index], Zigzag.Table[Index + 1]);
		const uint32 Value = Zigzag.SampleQuantised(Progress);
		if (Value < Low || Value > High)
		{
			AddError(FString::Printf(TEXT("Progress %u read %u outside [%u, %u]"), Progress, Value, Low, High));
			return false;
		}
	}

	// Exact at both ends of the table and halfway through a step of a symmetric pair
	TestEqual(TEXT("Start"), Zigzag.SampleQuantised(0), 0u);
	TestEqual(TEXT("A sample padded after the end repeats the last"), static_cast<int32>(FGSTN_PartsBakedCurve::FromSamples({ 0.f, 0.5f }).Table[63]), static_cast<int32>(FGSTN_PartsBakedCurve::QuantiseUnit(0.5f)));
	TestEqual(TEXT("No samples is flat at one"), static_cast<int32>(FGSTN_PartsBakedCurve::FromSamples(TConstArrayView<float>()).Table[10]), 65535);

	// The same input always gives the same bits
	const FGSTN_PartsBakedCurve Again = FGSTN_PartsBakedCurve::FromSamples(Samples);
	TestTrue(TEXT("Baking twice gives identical tables"), Again == Zigzag);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
