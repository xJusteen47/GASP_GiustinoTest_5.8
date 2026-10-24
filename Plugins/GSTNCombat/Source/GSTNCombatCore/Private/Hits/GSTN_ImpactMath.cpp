// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_ImpactMath.h"

const FGSTN_ImpactResponseEntry& FGSTN_ImpactResponse::Find(const FGameplayTag& Surface) const
{
	if (Surface.IsValid())
	{
		for (const FGSTN_ImpactResponseEntry& Entry : PerSurface)
		{
			if (Entry.Surface.IsValid() && Surface.MatchesTag(Entry.Surface))
			{
				return Entry;
			}
		}
	}
	return Default;
}

void UGSTN_SurfaceImpactTable::Compile()
{
	IndexByTag.Reset();
	for (int32 Index = 0; Index < Rows.Num() && Index < 255; ++Index)
	{
		if (Rows[Index].Surface.IsValid())
		{
			// The first row of a tag wins, so a duplicate does not change an index already handed out
			IndexByTag.FindOrAdd(Rows[Index].Surface, Index);
		}
	}
}

int32 UGSTN_SurfaceImpactTable::FindRowIndex(const FGameplayTag& Surface) const
{
	for (FGameplayTag Tag = Surface; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const int32* Found = IndexByTag.Find(Tag))
		{
			return *Found;
		}
	}
	return INDEX_NONE;
}

void UGSTN_SurfaceImpactTable::PostLoad()
{
	Super::PostLoad();
	Compile();
}

#if WITH_EDITOR
void UGSTN_SurfaceImpactTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Compile();
}
#endif

FGSTN_ImpactResult UGSTN_ImpactMath::Resolve(const FGSTN_ImpactInput& Input, const UGSTN_SurfaceImpactTable* Table, const FGSTN_ImpactResponse& Response, const FGSTN_ImpactProfile& Profile)
{
	FGSTN_ImpactResult Result;
	const FGSTN_SurfaceImpactRow* Row = Table ? Table->GetRow(Input.SurfaceTypeIndex) : nullptr;
	if (!Row)
	{
		return Result;
	}
	const FGSTN_ImpactResponseEntry& Victim = Response.Find(Row->Surface);
	const auto Forbidden = [&Profile](EGSTN_ImpactOutcome Outcome) { return Profile.Forbid.Contains(Outcome); };

	EGSTN_ImpactOutcome Chosen = EGSTN_ImpactOutcome::None;
	if (Row->bForceDefault)
	{
		Chosen = Row->DefaultOutcome;
	}
	else if (Profile.Force != EGSTN_ImpactOutcome::None && !Forbidden(Profile.Force) && (Row->Allowed.AllowsAnything() || Profile.bForceIgnoresSurface))
	{
		Chosen = Profile.Force;
	}
	else if (Row->Allowed.Allows(Row->DefaultOutcome) && Victim.Allowed.Allows(Row->DefaultOutcome) && !Forbidden(Row->DefaultOutcome))
	{
		Chosen = Row->DefaultOutcome;
	}

	Result.JudgedSpeed = static_cast<float>(static_cast<double>(Input.ImpactSpeedQ) * Profile.SpeedScale);
	const double Threshold = FMath::Max(static_cast<double>(Row->MinImpactSpeed), Victim.MinImpactSpeed >= 0.f ? static_cast<double>(Victim.MinImpactSpeed) : 0.0);
	if (Chosen == EGSTN_ImpactOutcome::None || !(static_cast<double>(Input.ImpactSpeedQ) * Profile.SpeedScale > Threshold))
	{
		return Result;
	}

	Result.Outcome = Chosen;
	Result.Restitution = Victim.Restitution >= 0.f ? Victim.Restitution : Row->Restitution;
	Result.SplatFrames = Victim.SplatFrames >= 0 ? Victim.SplatFrames : Row->SplatFrames;
	Result.ImpactDamageScale = Row->ImpactDamageScale * Victim.ImpactDamageScale;
	return Result;
}
