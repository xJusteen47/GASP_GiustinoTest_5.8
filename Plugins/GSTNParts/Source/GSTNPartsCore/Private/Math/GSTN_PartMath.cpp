// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_PartMath.h"

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "GSTN_PartsSettings.h"

namespace
{
	constexpr int32 DamageAtCap = GSTN_PartsLimits::DamageAtCap;

	/** An authored multiplier made safe: finite and not negative, else 0, so no NaN or infinity can enter the state or the result */
	float NonNegative(float Value)
	{
		return (Value >= 0.f && FMath::IsFinite(Value)) ? Value : 0.f;
	}

	/** The part index if it addresses a part of the view, else INDEX_NONE */
	int32 ValidPartIndex(int32 PartIndex, int32 NumParts)
	{
		return (PartIndex >= 0 && PartIndex < NumParts) ? PartIndex : INDEX_NONE;
	}

	int32 AddressableParts(TConstArrayView<FGSTN_PartDefinition> Parts)
	{
		return FMath::Min(Parts.Num(), GSTN_PartsLimits::MaxParts);
	}

	uint16 BitOf(int32 Index)
	{
		return static_cast<uint16>(1u << Index);
	}
}

FGSTN_PartHitInput FGSTN_PartHitInput::Make(float InDamage, int32 InPartIndex, const FGSTN_PartHitData* Data)
{
	FGSTN_PartHitInput Input;
	Input.Damage = InDamage;
	Input.PartIndex = InPartIndex;
	if (Data)
	{
		Input.Penetration = Data->Penetration;
		Input.PartDamageScale = Data->PartDamageScale;
		Input.bIgnoreHardness = Data->HasFlag(EGSTN_PartHitFlags::IgnoreHardness);
		Input.bMainOnly = Data->HasFlag(EGSTN_PartHitFlags::MainOnly);
	}
	return Input;
}

uint8 UGSTN_PartMath::AddDamage(uint8 Current, float Delta, float Cap)
{
	if (!(Delta > 0.f) || !(Cap > 0.f) || !FMath::IsFinite(Cap))
	{
		return Current;
	}
	const float Units = (Delta / Cap) * static_cast<float>(DamageAtCap);
	if (!(Units < static_cast<float>(DamageAtCap)))
	{
		return static_cast<uint8>(DamageAtCap); // also infinity and NaN-free by the check above
	}
	int32 Next = FMath::CeilToInt(static_cast<float>(Current) + Units);
	Next = FMath::Max(Next, static_cast<int32>(Current) + 1); // a nonzero hit always moves the byte
	return static_cast<uint8>(FMath::Min(Next, DamageAtCap));
}

uint8 UGSTN_PartMath::Quantise(float Damage, float Cap)
{
	return AddDamage(0, Damage, Cap);
}

float UGSTN_PartMath::Dequantise(uint8 Quantised, float Cap)
{
	return (static_cast<float>(Quantised) / static_cast<float>(DamageAtCap)) * Cap;
}

int32 UGSTN_PartMath::LookupBand(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Penetration, int32 Hardness, bool bIgnoreHardness)
{
	return UGSTN_PenetrationTable::LookupBand(SortedBands, Penetration, Hardness, bIgnoreHardness);
}

uint16 UGSTN_PartMath::ComputeCascade(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 BrokenMask)
{
	const int32 Num = AddressableParts(Parts);
	uint16 Closed = BrokenMask;
	// Each pass can only add parts and a chain is at most Num long, so Num passes always settle it, cycles included
	for (int32 Pass = 0; Pass < Num; ++Pass)
	{
		bool bChanged = false;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const int32 Parent = Parts[Index].ParentIndex;
			if ((Closed & BitOf(Index)) == 0 && Parent >= 0 && Parent < Num && (Closed & BitOf(Parent)) != 0)
			{
				Closed |= BitOf(Index);
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
	return Closed;
}

uint16 UGSTN_PartMath::GetBreakableMask(TConstArrayView<FGSTN_PartDefinition> Parts)
{
	uint16 Mask = 0;
	const int32 Num = AddressableParts(Parts);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if (Parts[Index].IsBreakable())
		{
			Mask |= BitOf(Index);
		}
	}
	return Mask;
}

bool UGSTN_PartMath::IsAllBroken(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 BrokenMask)
{
	const uint16 Breakable = GetBreakableMask(Parts);
	return Breakable != 0 && (BrokenMask & Breakable) == Breakable;
}

FGSTN_PartHitResult UGSTN_PartMath::ApplyHit(TConstArrayView<FGSTN_PartDefinition> Parts, TConstArrayView<FGSTN_PenetrationBand> SortedBands,
	FGSTN_PartsState& State, const FGSTN_PartHitInput& Hit)
{
	FGSTN_PartHitResult Result;
	const int32 Num = AddressableParts(Parts);
	const float Damage = (Hit.Damage > 0.f && FMath::IsFinite(Hit.Damage)) ? Hit.Damage : 0.f;

	int32 PartIndex = ValidPartIndex(Hit.PartIndex, Num);
	const bool bWasBroken = PartIndex != INDEX_NONE && State.IsBroken(PartIndex);
	if (bWasBroken && Parts[PartIndex].BrokenPolicy == EGSTN_BrokenHurtboxPolicy::Disable)
	{
		// The stump is hidden from the snapshot, so the hit is the body's
		PartIndex = INDEX_NONE;
		Result.bPassedToBody = true;
	}

	if (PartIndex == INDEX_NONE)
	{
		// The body has no armour and no state: the full hit goes to the main pool and the bands are not consulted
		Result.HitMainDelta = Damage;
		Result.MainDelta = Damage;
		return Result;
	}

	const FGSTN_PartDefinition& Part = Parts[PartIndex];
	Result.PartIndex = PartIndex;
	Result.bHitBrokenPart = bWasBroken;

	const int32 BandIndex = LookupBand(SortedBands, Hit.Penetration, Part.Hardness, Hit.bIgnoreHardness);
	const FGSTN_PenetrationBand* Band = (BandIndex != INDEX_NONE) ? &SortedBands[BandIndex] : nullptr;
	Result.BandIndex = BandIndex;

	if (Band && Band->bRicochet)
	{
		// The weapon bounces: no damage, no stagger, no hitstop, no state change, whatever the band's scales say
		Result.bRicochet = true;
		Result.StaggerScale = 0.f;
		Result.HitstopScale = 0.f;
		Result.NewPartDamage = State.PartDamage[PartIndex];
		return Result;
	}

	const float PartScale = Band ? NonNegative(Band->PartScale) : 1.f;
	const float MainScale = Band ? NonNegative(Band->MainScale) : 1.f;
	Result.StaggerScale = Band ? NonNegative(Band->StaggerScale) : 1.f;
	Result.HitstopScale = Band ? NonNegative(Band->HitstopScale) : 1.f;

	// Both products in the order of the spec, so the float rounding is the same on every machine
	const float Transfer = NonNegative(bWasBroken ? Part.GetBrokenMainTransfer() : Part.MainTransfer);
	Result.HitMainDelta = (Damage * MainScale) * Transfer;

	uint16 NewlyBroken = 0;
	if (!bWasBroken && Part.IsBreakable() && !Hit.bMainOnly)
	{
		const float PartDelta = ((Damage * PartScale) * NonNegative(Hit.PartDamageScale)) * NonNegative(Part.PartDamageScale);
		const uint8 Before = State.PartDamage[PartIndex];
		const uint8 After = AddDamage(Before, PartDelta, Part.PartCap);
		if (After != Before)
		{
			State.PartDamage[PartIndex] = After;
			Result.PartDelta = PartDelta;
			Result.bPartDamaged = true;
			if (After >= DamageAtCap)
			{
				// Overflow beyond the cap is not carried anywhere; the bonus is an explicit MainDamage effect
				NewlyBroken = BitOf(PartIndex);
			}
		}
	}
	Result.NewPartDamage = State.PartDamage[PartIndex];

	if (NewlyBroken != 0)
	{
		const bool bWasAllBroken = IsAllBroken(Parts, State.BrokenMask);

		// Children go with their parent in the same hit; the order below is part index order, whatever order they were found in.
		// The cascade starts from the part that just broke, not from the whole mask: a state is never expected to hold a child
		// intact under a broken parent (RestoreParts forbids it), but if one does, an unrelated break must not break it again
		// and pay its bonus a second time
		NewlyBroken = ComputeCascade(Parts, NewlyBroken) & static_cast<uint16>(~State.BrokenMask);
		State.BrokenMask |= NewlyBroken;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if ((NewlyBroken & BitOf(Index)) == 0)
			{
				continue;
			}
			State.PartDamage[Index] = static_cast<uint8>(DamageAtCap);
			for (const FInstancedStruct& Entry : Parts[Index].OnBreak)
			{
				if (const FGSTN_PartBreakEffect_MainDamage* Bonus = Entry.GetPtr<FGSTN_PartBreakEffect_MainDamage>())
				{
					Result.BonusMainDamage += NonNegative(Bonus->AddPercent);
					Result.bKill |= Bonus->bKill;
				}
			}
		}
		State.AdvanceSerial();
		Result.NewPartDamage = State.PartDamage[PartIndex];
		Result.bAllBrokenReached = !bWasAllBroken && IsAllBroken(Parts, State.BrokenMask);
	}

	Result.NewlyBrokenMask = NewlyBroken;
	Result.MainDelta = Result.HitMainDelta + Result.BonusMainDamage;
	Result.bWeakPointHit = Part.bWeakPoint;
	return Result;
}

float UGSTN_PartMath::EffectiveWeightScale(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsBakedCurve& Curve, float Progress,
	uint16 BrokenMask, float MinWeightScale)
{
	float Scale = Curve.Sample(Progress);
	const int32 Num = AddressableParts(Parts);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if ((BrokenMask & BitOf(Index)) != 0)
		{
			Scale *= 1.f - FMath::Clamp(NonNegative(Parts[Index].LostMassFraction), 0.f, 1.f);
		}
	}
	return FMath::Max(Scale, FMath::Clamp(NonNegative(MinWeightScale), 0.f, 1.f));
}

float UGSTN_PartMath::EffectiveWeight(float BaseWeight, TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsBakedCurve& Curve,
	float Progress, uint16 BrokenMask, float MinWeightScale)
{
	return BaseWeight * EffectiveWeightScale(Parts, Curve, Progress, BrokenMask, MinWeightScale);
}

bool UGSTN_PartMath::IsGrabLocked(const FGSTN_GrabGate& Gate, uint16 BrokenMask)
{
	if (Gate.IsEmpty())
	{
		return false;
	}

	bool bMet = true;
	if (Gate.MinBrokenParts > 0)
	{
		bMet = FMath::CountBits(static_cast<uint32>(BrokenMask)) >= Gate.MinBrokenParts;
	}
	if (bMet && !Gate.RequiredParts.IsEmpty())
	{
		bool bAny = false;
		bool bAll = true;
		for (const int32 Required : Gate.RequiredParts)
		{
			const bool bBroken = Required >= 0 && Required < GSTN_PartsLimits::MaxParts && (BrokenMask & BitOf(Required)) != 0;
			bAny |= bBroken;
			bAll &= bBroken;
		}
		bMet = Gate.bAnyOf ? bAny : bAll;
	}
	return !bMet;
}

void UGSTN_PartMath::ProjectDerived(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsState& State, const FGSTN_GrabGate& Gate,
	FGSTN_PartsProjection& Out)
{
	Out.Reset();
	const int32 Num = AddressableParts(Parts);
	const uint16 Mask = State.BrokenMask & static_cast<uint16>(Num >= GSTN_PartsLimits::MaxParts ? 0xFFFFu : (1u << Num) - 1u);

	Out.bAnyBroken = Mask != 0;
	Out.bAllBroken = IsAllBroken(Parts, Mask);
	Out.bGrabLocked = IsGrabLocked(Gate, Mask);

	for (int32 Index = 0; Index < Num; ++Index)
	{
		if ((Mask & BitOf(Index)) == 0)
		{
			continue;
		}
		const FGSTN_PartDefinition& Part = Parts[Index];
		if (Part.BrokenPolicy == EGSTN_BrokenHurtboxPolicy::Disable)
		{
			for (const FName& Hurtbox : Part.Hurtboxes)
			{
				Out.DisabledHurtboxes.AddUnique(Hurtbox);
			}
		}
		for (const FInstancedStruct& Entry : Part.OnBreak)
		{
			if (const FGSTN_PartBreakEffect* Effect = Entry.GetPtr<FGSTN_PartBreakEffect>())
			{
				Effect->Project(State, Index, Out);
			}
		}
	}
}

uint16 UGSTN_PartMath::GetBrokenAncestorsMask(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 BrokenMask, uint16 Mask)
{
	const int32 Num = AddressableParts(Parts);
	uint16 Ancestors = 0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if ((Mask & BitOf(Index)) == 0)
		{
			continue;
		}
		// At most Num steps up, so a parent cycle ends the walk
		int32 Parent = Parts[Index].ParentIndex;
		for (int32 Step = 0; Step < Num && Parent >= 0 && Parent < Num; ++Step)
		{
			if ((BrokenMask & BitOf(Parent)) != 0)
			{
				Ancestors |= BitOf(Parent);
			}
			Parent = Parts[Parent].ParentIndex;
		}
	}
	return Ancestors;
}

uint16 UGSTN_PartMath::RestoreParts(TConstArrayView<FGSTN_PartDefinition> Parts, FGSTN_PartsState& State, uint16 Mask, float DamageFraction)
{
	uint16 Target = (Mask == 0) ? static_cast<uint16>(0xFFFFu) : Mask;
	// A child cannot be intact under a broken parent (children break with it, combat-parts.md §3.1): restoring a part restores the
	// broken ancestors it hangs off too, or the next unrelated break would cascade onto it again
	Target |= GetBrokenAncestorsMask(Parts, State.BrokenMask, Target);
	// A restored part is left just under the cap at most: a part restored "to the cap" would be broken again at once
	const uint8 Left = (DamageFraction > 0.f && FMath::IsFinite(DamageFraction))
		? static_cast<uint8>(FMath::Min(FMath::CeilToInt(FMath::Min(DamageFraction, 1.f) * static_cast<float>(DamageAtCap)), DamageAtCap - 1))
		: static_cast<uint8>(0);

	const uint16 Restored = State.BrokenMask & Target;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		if ((Target & BitOf(Index)) == 0)
		{
			continue;
		}
		if ((Restored & BitOf(Index)) != 0)
		{
			State.PartDamage[Index] = Left; // it was at the cap, so any fraction is an improvement
		}
		else
		{
			State.PartDamage[Index] = FMath::Min(State.PartDamage[Index], Left); // a repair never makes a part worse
		}
	}
	State.BrokenMask &= static_cast<uint16>(~Restored);
	if (Restored != 0)
	{
		State.AdvanceSerial();
	}
	return Restored;
}

int32 UGSTN_PartMath::GetPenetrationBandIndex(const UGSTN_PenetrationTable* Table, int32 Penetration, int32 Hardness, bool bIgnoreHardness)
{
	if (Table)
	{
		return LookupBand(Table->Bands, Penetration, Hardness, bIgnoreHardness);
	}
	return LookupBand(UGSTN_PenetrationTable::MakeDefaultBands(), Penetration, Hardness, bIgnoreHardness);
}

int32 UGSTN_PartMath::QuantisePartDamage(float Damage, float Cap)
{
	return Quantise(Damage, Cap);
}

float UGSTN_PartMath::DequantisePartDamage(int32 Quantised, float Cap)
{
	return Dequantise(static_cast<uint8>(FMath::Clamp(Quantised, 0, DamageAtCap)), Cap);
}

bool UGSTN_PartMath::IsPartBroken(int32 BrokenMask, int32 PartIndex)
{
	return PartIndex >= 0 && PartIndex < GSTN_PartsLimits::MaxParts && (static_cast<uint32>(BrokenMask) & (1u << PartIndex)) != 0;
}

int32 UGSTN_PartMath::CountBrokenParts(int32 BrokenMask)
{
	return FMath::CountBits(static_cast<uint32>(static_cast<uint16>(BrokenMask)));
}

float UGSTN_PartMath::GetEffectiveWeight(const UGSTN_PartSetDefinition* Set, float BaseWeight, float Progress, int32 BrokenMask)
{
	if (!Set)
	{
		return BaseWeight; // no part set: the weight is the base weight
	}
	return EffectiveWeight(BaseWeight, Set->GetParts(), Set->GetWeightCurve(), Progress, static_cast<uint16>(BrokenMask),
		UGSTN_PartsSettings::Get().MinWeightScale);
}
