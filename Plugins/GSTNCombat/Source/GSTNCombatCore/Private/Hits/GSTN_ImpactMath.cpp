// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_ImpactMath.h"
#include "GSTN_SurfaceUtils.h"

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

uint16 FGSTN_ImpactProfile::Pack() const
{
	uint16 Bits = static_cast<uint16>(Force) & 0x3;
	for (const EGSTN_ImpactOutcome Outcome : Forbid)
	{
		const uint8 Index = static_cast<uint8>(Outcome);
		if (Index >= 1 && Index <= 3)
		{
			Bits |= static_cast<uint16>(1u << (1 + Index));
		}
	}
	Bits |= bForceIgnoresSurface ? (1u << 5) : 0u;
	Bits |= bNoTech ? (1u << 6) : 0u;
	const int32 Scale = FMath::Clamp(FMath::RoundToInt(SpeedScale * SpeedScaleUnitsPerOne), 0, 255);
	Bits |= static_cast<uint16>((Scale ^ SpeedScaleUnitsPerOne) << 8);
	return Bits;
}

FGSTN_ImpactProfile FGSTN_ImpactProfile::Unpack(uint16 Packed)
{
	FGSTN_ImpactProfile Profile;
	Profile.Force = static_cast<EGSTN_ImpactOutcome>(Packed & 0x3);
	for (uint8 Index = 1; Index <= 3; ++Index)
	{
		if (Packed & (1u << (1 + Index)))
		{
			Profile.Forbid.Add(static_cast<EGSTN_ImpactOutcome>(Index));
		}
	}
	Profile.bForceIgnoresSurface = (Packed & (1u << 5)) != 0;
	Profile.bNoTech = (Packed & (1u << 6)) != 0;
	Profile.SpeedScale = static_cast<float>(((Packed >> 8) & 0xFF) ^ SpeedScaleUnitsPerOne) / static_cast<float>(SpeedScaleUnitsPerOne);
	return Profile;
}

void UGSTN_SurfaceImpactTable::Compile()
{
	IndexByTag.Reset();
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index].Surface.IsValid())
		{
			// The first row of a tag wins, so a duplicate does not change an index already handed out
			IndexByTag.FindOrAdd(Rows[Index].Surface, Index);
		}
	}
	BuildSurfaceIndexLookup();
}

void UGSTN_SurfaceImpactTable::BuildSurfaceIndexLookup() const
{
	RowBySurfaceIndex.Reset();
	RowBySurfaceIndex.Init(INDEX_NONE, 256);
	// Index 0 is "no type"; the type list ends at the first index that names no tag
	for (int32 TypeIndex = 1; TypeIndex < 256; ++TypeIndex)
	{
		const FGameplayTag Type = UGSTN_SurfaceUtils::GetSurfaceTypeFromIndex(static_cast<uint8>(TypeIndex));
		if (!Type.IsValid())
		{
			break;
		}
		RowBySurfaceIndex[TypeIndex] = FindRowIndex(Type);
	}
	CompiledTypeChecksum = UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum();
	bCompiledOnce = true;
}

const FGSTN_SurfaceImpactRow* UGSTN_SurfaceImpactTable::GetRow(int32 SurfaceTypeIndex) const
{
	// The surface type table can bake after this asset loaded (and the editor rebuilds it when tags change): follow it
	if (!bCompiledOnce || CompiledTypeChecksum != UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum())
	{
		BuildSurfaceIndexLookup();
	}
	const int32 RowIndex = RowBySurfaceIndex.IsValidIndex(SurfaceTypeIndex) ? RowBySurfaceIndex[SurfaceTypeIndex] : INDEX_NONE;
	return Rows.IsValidIndex(RowIndex) ? &Rows[RowIndex] : nullptr;
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
	Result.ResidualSpeedScale = Row->ResidualSpeedScale;
	Result.BreakStopFrames = Row->BreakStopFrames;
	return Result;
}
