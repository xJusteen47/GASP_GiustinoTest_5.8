#include "GSTN_PartBalanceReport.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "GSTNPartsEditorModule.h"
#include "Math/GSTN_PartMath.h"

namespace
{
	TArray<FGSTN_PenetrationBand> SortedBandsOf(const UGSTN_PenetrationTable* Table)
	{
		TArray<FGSTN_PenetrationBand> Bands = Table ? Table->Bands : UGSTN_PenetrationTable::MakeDefaultBands();
		// A table loaded from disk is normalised already; a freshly authored one may not be, and the lookup needs descending order
		Bands.StableSort([](const FGSTN_PenetrationBand& A, const FGSTN_PenetrationBand& B) { return A.MinDelta > B.MinDelta; });
		return Bands;
	}

	FString CountText(int32 Count)
	{
		return Count == INDEX_NONE ? TEXT("never") : FString::FromInt(Count);
	}
}

TArray<FGSTN_PartBalanceRow> UGSTN_PartBalanceReport::ComputeFromParts(TConstArrayView<FGSTN_PartDefinition> Parts,
	TConstArrayView<FGSTN_PenetrationBand> Bands, const FGSTN_PartHitData& HitData, float Damage, float MainPool,
	float StaggerForce, float BodyStaggerThreshold)
{
	if (MainPool < 0.f)
	{
		MainPool = 0.f;
		for (const FGSTN_PartDefinition& Part : Parts)
		{
			if (Part.IsBreakable())
			{
				MainPool += Part.PartCap;
			}
		}
	}
	if (StaggerForce < 0.f)
	{
		StaggerForce = Damage;
	}

	TArray<FGSTN_PartBalanceRow> Rows;
	const int32 NumParts = FMath::Min(Parts.Num(), GSTN_PartsLimits::MaxParts);
	for (int32 PartIndex = 0; PartIndex < NumParts; ++PartIndex)
	{
		const FGSTN_PartDefinition& Part = Parts[PartIndex];
		FGSTN_PartBalanceRow& Row = Rows.AddDefaulted_GetRef();
		Row.PartIndex = PartIndex;
		Row.Name = Part.Name;
		Row.bUnbreakable = !Part.IsBreakable();

		// A fresh state per part: the numbers describe this part alone, not a fight order
		FGSTN_PartsState State;
		float MainTaken = 0.f;
		const FGSTN_PartHitInput Hit = FGSTN_PartHitInput::Make(Damage, PartIndex, &HitData);
		for (int32 HitIndex = 1; HitIndex <= MaxHits; ++HitIndex)
		{
			const FGSTN_PartHitResult Result = UGSTN_PartMath::ApplyHit(Parts, Bands, State, Hit);
			if (HitIndex == 1)
			{
				const float Threshold = Part.Reaction.StaggerThreshold >= 0.f ? Part.Reaction.StaggerThreshold : BodyStaggerThreshold;
				if (!Result.bRicochet && Threshold >= 0.f && StaggerForce * Result.StaggerScale >= Threshold)
				{
					Row.HitsToStagger = 1;
				}
			}
			MainTaken += Result.MainDelta;
			if (Row.HitsToBreak == INDEX_NONE && State.IsBroken(PartIndex))
			{
				Row.HitsToBreak = HitIndex;
			}
			if (Row.HitsToKill == INDEX_NONE && (MainTaken >= MainPool || Result.bKill))
			{
				Row.HitsToKill = HitIndex;
			}
			if ((Row.HitsToBreak != INDEX_NONE || Row.bUnbreakable) && Row.HitsToKill != INDEX_NONE)
			{
				break;
			}
		}
	}
	return Rows;
}

TArray<FGSTN_PartBalanceRow> UGSTN_PartBalanceReport::Compute(const UGSTN_PartSetDefinition* PartSet,
	const UGSTN_PenetrationTable* Table, const FGSTN_PartHitData& HitData, float Damage, float MainPool, float StaggerForce)
{
	if (!PartSet)
	{
		return {};
	}
	if (!Table)
	{
		Table = PartSet->Table.LoadSynchronous();
	}
	return ComputeFromParts(PartSet->GetParts(), SortedBandsOf(Table), HitData, Damage, MainPool, StaggerForce,
		PartSet->BodyReaction.StaggerThreshold);
}

FString UGSTN_PartBalanceReport::FormatReport(const TArray<FGSTN_PartBalanceRow>& Rows)
{
	FString Text = FString::Printf(TEXT("%-3s %-24s %8s %8s %8s\n"), TEXT("#"), TEXT("Part"), TEXT("Break"), TEXT("Kill"), TEXT("Stagger"));
	for (const FGSTN_PartBalanceRow& Row : Rows)
	{
		const FString Name = Row.bUnbreakable ? Row.Name.ToString() + TEXT(" (unbreakable)") : Row.Name.ToString();
		Text += FString::Printf(TEXT("%-3d %-24s %8s %8s %8s\n"), Row.PartIndex, *Name, *CountText(Row.HitsToBreak),
			*CountText(Row.HitsToKill), *CountText(Row.HitsToStagger));
	}
	return Text;
}

void UGSTN_PartBalanceReport::RunConsoleCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 4)
	{
		UE_LOG(LogGSTNPartsEditor, Warning, TEXT("Usage: GSTN.Parts.BalanceReport <PartSetPath> <TablePath|None> <Damage> <Penetration> [StaggerForce]"));
		return;
	}
	const UGSTN_PartSetDefinition* PartSet = LoadObject<UGSTN_PartSetDefinition>(nullptr, *Args[0]);
	if (!PartSet)
	{
		UE_LOG(LogGSTNPartsEditor, Warning, TEXT("BalanceReport: no part set at %s"), *Args[0]);
		return;
	}
	const UGSTN_PenetrationTable* Table = nullptr;
	if (!Args[1].Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		Table = LoadObject<UGSTN_PenetrationTable>(nullptr, *Args[1]);
		if (!Table)
		{
			UE_LOG(LogGSTNPartsEditor, Warning, TEXT("BalanceReport: no penetration table at %s"), *Args[1]);
			return;
		}
	}
	const float Damage = FCString::Atof(*Args[2]);
	FGSTN_PartHitData HitData;
	HitData.Penetration = static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Args[3]), 0, GSTN_PartsLimits::MaxHardness));
	const float StaggerForce = Args.IsValidIndex(4) ? FCString::Atof(*Args[4]) : -1.f;

	const TArray<FGSTN_PartBalanceRow> Rows = Compute(PartSet, Table, HitData, Damage, -1.f, StaggerForce);
	UE_LOG(LogGSTNPartsEditor, Log, TEXT("Balance report of %s: damage %.2f, penetration %d\n%s"), *PartSet->GetPathName(), Damage,
		HitData.Penetration, *FormatReport(Rows));
}
