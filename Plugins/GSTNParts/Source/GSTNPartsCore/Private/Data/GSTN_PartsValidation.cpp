// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartsValidation.h"

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartsLimits.h"
#include "Math/GSTN_PartMath.h"

void FGSTN_PartsValidationReport::AddError(const FString& Message)
{
	FGSTN_PartsIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.bError = true;
	Issue.Message = Message;
}

void FGSTN_PartsValidationReport::AddWarning(const FString& Message)
{
	FGSTN_PartsIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.bError = false;
	Issue.Message = Message;
}

int32 FGSTN_PartsValidationReport::NumErrors() const
{
	int32 Count = 0;
	for (const FGSTN_PartsIssue& Issue : Issues)
	{
		Count += Issue.bError ? 1 : 0;
	}
	return Count;
}

bool FGSTN_PartsValidationReport::Contains(bool bError, const TCHAR* Text) const
{
	for (const FGSTN_PartsIssue& Issue : Issues)
	{
		if (Issue.bError == bError && Issue.Message.Contains(Text, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

namespace
{
	FString PartLabel(int32 Index, const FGSTN_PartDefinition& Part)
	{
		return FString::Printf(TEXT("Part %d (%s)"), Index, *Part.Name.ToString());
	}

	/** A value that is a finite number at or above zero */
	bool IsFiniteNonNegative(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.f;
	}

	void ValidateReaction(const FString& Label, const FGSTN_PartReaction& Reaction, FGSTN_PartsValidationReport& Report)
	{
		if (!FMath::IsFinite(Reaction.StaggerThreshold))
		{
			Report.AddError(FString::Printf(TEXT("%s: the stagger threshold is not a finite number."), *Label));
		}
		if (!IsFiniteNonNegative(Reaction.JuggleFactor) || !IsFiniteNonNegative(Reaction.HitstopScale)
			|| !IsFiniteNonNegative(Reaction.KnockbackScale))
		{
			Report.AddError(FString::Printf(TEXT("%s: the juggle factor, hitstop scale and knockback scale must be finite and not negative."), *Label));
		}
	}
}

void FGSTN_PartsValidation::ValidateParts(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_GrabGate& GrabGate,
	const FGSTN_PartReaction& BodyReaction, const TSet<FName>* KnownBones, int32 MaxPartsAllowed,
	FGSTN_PartsValidationReport& Report)
{
	const int32 Num = Parts.Num();
	if (Num == 0)
	{
		Report.AddWarning(TEXT("The part set has no parts, so an entity with it behaves as without parts."));
	}
	if (Num > MaxPartsAllowed)
	{
		Report.AddError(FString::Printf(TEXT("The part set has %d parts; the limit is %d (the width of the broken mask). Parts beyond it are ignored."), Num, MaxPartsAllowed));
	}

	ValidateReaction(TEXT("Body reaction"), BodyReaction, Report);

	TSet<FName> Names;
	TMap<FName, int32> HurtboxOwners;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FGSTN_PartDefinition& Part = Parts[Index];
		const FString Label = PartLabel(Index, Part);

		if (Part.Name.IsNone())
		{
			Report.AddError(FString::Printf(TEXT("%s has no name; names identify parts for designers and bridges."), *Label));
		}
		else
		{
			bool bAlreadyUsed = false;
			Names.Add(Part.Name, &bAlreadyUsed);
			if (bAlreadyUsed)
			{
				Report.AddError(FString::Printf(TEXT("%s: the name is used by another part; names must be unique."), *Label));
			}
		}

		for (const FGameplayTag& Type : Part.Types)
		{
			// Compared as text: the validator must not name a tag the parts module does not own, and it changes no behaviour
			if (!Type.ToString().StartsWith(TEXT("Part.Type.")))
			{
				Report.AddWarning(FString::Printf(TEXT("%s: type %s is not a Part.Type.* tag, so rules that look for part types will not see it."), *Label, *Type.ToString()));
			}
		}

		if (Part.Hurtboxes.IsEmpty())
		{
			Report.AddWarning(FString::Printf(TEXT("%s names no hurtbox, so no hit can land on it."), *Label));
		}
		for (const FName& Hurtbox : Part.Hurtboxes)
		{
			if (Hurtbox.IsNone())
			{
				Report.AddError(FString::Printf(TEXT("%s lists an empty hurtbox name."), *Label));
				continue;
			}
			if (const int32* Owner = HurtboxOwners.Find(Hurtbox))
			{
				Report.AddError(FString::Printf(TEXT("%s: hurtbox %s already belongs to part %d; a hurtbox belongs to at most one part."), *Label, *Hurtbox.ToString(), *Owner));
			}
			else
			{
				HurtboxOwners.Add(Hurtbox, Index);
			}
		}

		if (KnownBones)
		{
			for (const FName& Bone : Part.Bones)
			{
				if (!KnownBones->Contains(Bone))
				{
					Report.AddWarning(FString::Printf(TEXT("%s: bone %s is not in the preview mesh."), *Label, *Bone.ToString()));
				}
			}
		}

		if (Part.ParentIndex != INDEX_NONE && (Part.ParentIndex < 0 || Part.ParentIndex >= Num || Part.ParentIndex == Index))
		{
			Report.AddError(FString::Printf(TEXT("%s: the parent index %d is out of range or the part itself."), *Label, Part.ParentIndex));
		}

		if (Part.Hardness < 0 || Part.Hardness > GSTN_PartsLimits::MaxHardness)
		{
			Report.AddError(FString::Printf(TEXT("%s: hardness %d is outside 0..%d."), *Label, Part.Hardness, GSTN_PartsLimits::MaxHardness));
		}
		if (!FMath::IsFinite(Part.PartCap))
		{
			Report.AddError(FString::Printf(TEXT("%s: the part cap is not a finite number."), *Label));
		}
		if (!IsFiniteNonNegative(Part.PartDamageScale) || !IsFiniteNonNegative(Part.MainTransfer) || !FMath::IsFinite(Part.BrokenMainTransfer))
		{
			Report.AddError(FString::Printf(TEXT("%s: the damage scale and the transfers must be finite and not negative."), *Label));
		}
		if (!FMath::IsFinite(Part.LostMassFraction) || Part.LostMassFraction < 0.f || Part.LostMassFraction > 1.f)
		{
			Report.AddError(FString::Printf(TEXT("%s: the lost mass fraction must be between 0 and 1."), *Label));
		}
		if (Part.BrokenPolicy == EGSTN_BrokenHurtboxPolicy::Replace)
		{
			Report.AddWarning(FString::Printf(TEXT("%s: the Replace policy needs a hit shape, which is not declared yet; it behaves as Keep for damage."), *Label));
		}
		ValidateReaction(Label, Part.Reaction, Report);

		for (int32 EffectIndex = 0; EffectIndex < Part.OnBreak.Num(); ++EffectIndex)
		{
			const FInstancedStruct& Entry = Part.OnBreak[EffectIndex];
			if (!Entry.IsValid())
			{
				Report.AddWarning(FString::Printf(TEXT("%s: break effect %d is empty (its struct is not loaded, perhaps from a disabled plugin) and is ignored."), *Label, EffectIndex));
				continue;
			}
			if (!Entry.GetScriptStruct()->IsChildOf(FGSTN_PartBreakEffect::StaticStruct()))
			{
				Report.AddWarning(FString::Printf(TEXT("%s: break effect %d is a %s, which is not a part break effect, and is ignored."), *Label, EffectIndex, *Entry.GetScriptStruct()->GetName()));
				continue;
			}
			if (const FGSTN_PartBreakEffect_MainDamage* MainDamage = Entry.GetPtr<FGSTN_PartBreakEffect_MainDamage>())
			{
				if (!IsFiniteNonNegative(MainDamage->AddPercent))
				{
					Report.AddError(FString::Printf(TEXT("%s: the bonus main damage of break effect %d must be finite and not negative."), *Label, EffectIndex));
				}
			}
			else if (const FGSTN_PartBreakEffect_Launch* Launch = Entry.GetPtr<FGSTN_PartBreakEffect_Launch>())
			{
				// The ClampMin/ClampMax of the properties only guard the editor: data from an ini, JSON or code reaches here unclamped
				if (!IsFiniteNonNegative(Launch->Speed))
				{
					Report.AddError(FString::Printf(TEXT("%s: the launch speed of break effect %d must be finite and not negative."), *Label, EffectIndex));
				}
				if (!FMath::IsFinite(Launch->UpFraction) || Launch->UpFraction < 0.f || Launch->UpFraction > 1.f)
				{
					Report.AddError(FString::Printf(TEXT("%s: the up fraction of break effect %d must be between 0 and 1."), *Label, EffectIndex));
				}
			}
			else if (const FGSTN_PartBreakEffect_Stagger* Stagger = Entry.GetPtr<FGSTN_PartBreakEffect_Stagger>())
			{
				if (Stagger->Frames < 0)
				{
					Report.AddError(FString::Printf(TEXT("%s: the stagger frames of break effect %d must not be negative."), *Label, EffectIndex));
				}
			}
			else if (const FGSTN_PartBreakEffect_Stats* Stats = Entry.GetPtr<FGSTN_PartBreakEffect_Stats>())
			{
				if (Stats->Magnitudes.IsEmpty())
				{
					Report.AddWarning(FString::Printf(TEXT("%s: break effect %d lists no stat magnitude, so it changes nothing."), *Label, EffectIndex));
				}
				for (const FGSTN_PartStatMagnitude& Magnitude : Stats->Magnitudes)
				{
					if (!Magnitude.Stat.IsValid())
					{
						Report.AddError(FString::Printf(TEXT("%s: a magnitude of break effect %d has no stat tag."), *Label, EffectIndex));
					}
					else if (!Magnitude.Stat.ToString().StartsWith(TEXT("Combat.SetByCaller.Stat.")))
					{
						// The root is the GAS module's tag, which this module cannot name; the effect is applied by SetByCaller key
						Report.AddWarning(FString::Printf(TEXT("%s: stat %s of break effect %d is not under Combat.SetByCaller.Stat, so the applied effect will not read it."), *Label, *Magnitude.Stat.ToString(), EffectIndex));
					}
					if (!FMath::IsFinite(Magnitude.Magnitude))
					{
						Report.AddError(FString::Printf(TEXT("%s: a magnitude of break effect %d is not a finite number."), *Label, EffectIndex));
					}
				}
			}
			else if (const FGSTN_PartBreakEffect_DisableHurtbox* DisableHurtbox = Entry.GetPtr<FGSTN_PartBreakEffect_DisableHurtbox>())
			{
				if (DisableHurtbox->Hurtboxes.IsEmpty())
				{
					Report.AddWarning(FString::Printf(TEXT("%s: break effect %d disables no hurtbox."), *Label, EffectIndex));
				}
				if (DisableHurtbox->Hurtboxes.Contains(NAME_None))
				{
					Report.AddError(FString::Printf(TEXT("%s: break effect %d lists an empty hurtbox name."), *Label, EffectIndex));
				}
			}
			if (!Part.IsBreakable())
			{
				Report.AddWarning(FString::Printf(TEXT("%s is unbreakable (part cap 0 or less), so its break effect %d never fires unless a broken parent cascades onto it."), *Label, EffectIndex));
			}
		}
	}

	// Parent cycles: one report per cycle, at its lowest index. A part that merely hangs off a cycle is not part of it
	for (int32 Start = 0; Start < Num; ++Start)
	{
		int32 Cursor = Parts[Start].ParentIndex;
		int32 Lowest = Start;
		bool bInCycle = false;
		for (int32 Step = 0; Step < Num && Cursor >= 0 && Cursor < Num; ++Step)
		{
			if (Cursor == Start)
			{
				bInCycle = true;
				break;
			}
			Lowest = FMath::Min(Lowest, Cursor);
			Cursor = Parts[Cursor].ParentIndex;
		}
		if (bInCycle && Lowest == Start)
		{
			Report.AddError(FString::Printf(TEXT("%s is in a parent cycle; breaking one part would never settle."), *PartLabel(Start, Parts[Start])));
		}
	}

	// A part can break by damage only when it is breakable, or by cascade when an ancestor does: the others never appear in the
	// broken mask, so a gate that waits for them would lock the entity for good
	const uint16 Reachable = UGSTN_PartMath::ComputeCascade(Parts, UGSTN_PartMath::GetBreakableMask(Parts));
	const int32 NumReachable = FMath::CountBits(static_cast<uint32>(Reachable));

	if (GrabGate.MinBrokenParts > Num)
	{
		Report.AddError(FString::Printf(TEXT("The grab gate needs %d broken parts but the set has %d."), GrabGate.MinBrokenParts, Num));
	}
	else if (GrabGate.MinBrokenParts > NumReachable)
	{
		Report.AddError(FString::Printf(TEXT("The grab gate needs %d broken parts but only %d can ever break (a part with a cap of 0 or less breaks only with a breakable ancestor): the entity could never be grabbed."), GrabGate.MinBrokenParts, NumReachable));
	}

	bool bAnyRequiredReachable = false;
	bool bAnyRequiredInRange = false;
	for (const int32 Required : GrabGate.RequiredParts)
	{
		if (Required < 0 || Required >= Num)
		{
			Report.AddError(FString::Printf(TEXT("The grab gate requires part %d, which does not exist."), Required));
			continue;
		}
		bAnyRequiredInRange = true;
		const bool bReachable = Required < GSTN_PartsLimits::MaxParts && (Reachable & static_cast<uint16>(1u << Required)) != 0;
		bAnyRequiredReachable |= bReachable;
		if (!bReachable && !GrabGate.bAnyOf)
		{
			Report.AddError(FString::Printf(TEXT("The grab gate requires %s, which can never break (its cap is 0 or less and no ancestor is breakable): the entity could never be grabbed."), *PartLabel(Required, Parts[Required])));
		}
	}
	if (GrabGate.bAnyOf && bAnyRequiredInRange && !bAnyRequiredReachable)
	{
		Report.AddError(TEXT("The grab gate needs any of its required parts, but none of them can ever break: the entity could never be grabbed."));
	}
}

void FGSTN_PartsValidation::ValidateBands(TConstArrayView<FGSTN_PenetrationBand> Bands, FGSTN_PartsValidationReport& Report)
{
	if (Bands.IsEmpty())
	{
		Report.AddError(TEXT("The penetration table has no bands, so no hit could be resolved against a part."));
		return;
	}

	for (int32 Index = 0; Index < Bands.Num(); ++Index)
	{
		const FGSTN_PenetrationBand& Band = Bands[Index];
		const FString Label = FString::Printf(TEXT("Band %d (MinDelta %d)"), Index, Band.MinDelta);
		if (!IsFiniteNonNegative(Band.PartScale) || !IsFiniteNonNegative(Band.MainScale) || !IsFiniteNonNegative(Band.StaggerScale)
			|| !IsFiniteNonNegative(Band.HitstopScale))
		{
			Report.AddError(FString::Printf(TEXT("%s: the scales must be finite and not negative."), *Label));
		}
		if (Band.bRicochet && (Band.PartScale != 0.f || Band.MainScale != 0.f || Band.StaggerScale != 0.f || Band.HitstopScale != 0.f))
		{
			Report.AddWarning(FString::Printf(TEXT("%s ricochets, so its part, main, stagger and hitstop scales are ignored (a ricochet deals no damage and no reaction)."), *Label));
		}
		if (Band.bRicochet && Index == 0)
		{
			Report.AddWarning(FString::Printf(TEXT("%s is the top band and ricochets: a hit that ignores hardness always takes the top band, so it would ricochet off everything."), *Label));
		}
		if (Index > 0)
		{
			if (Bands[Index - 1].MinDelta == Band.MinDelta)
			{
				Report.AddError(FString::Printf(TEXT("%s has the same MinDelta as the band before it; the band to use would be ambiguous."), *Label));
			}
			else if (Bands[Index - 1].MinDelta < Band.MinDelta)
			{
				Report.AddError(FString::Printf(TEXT("%s is not sorted: bands must run from the highest MinDelta down (use Normalize)."), *Label));
			}
		}
	}

	bool bAnyDamaging = false;
	for (const FGSTN_PenetrationBand& Band : Bands)
	{
		bAnyDamaging |= !Band.bRicochet && (Band.PartScale > 0.f || Band.MainScale > 0.f);
	}
	if (!bAnyDamaging)
	{
		Report.AddWarning(TEXT("No band deals damage (every band ricochets or has part and main scales of 0), so no hit could hurt a part or the entity through it."));
	}

	if (Bands.Last().MinDelta > GSTN_PartsLimits::MinPossibleDelta)
	{
		Report.AddWarning(FString::Printf(TEXT("The lowest band starts at MinDelta %d, above the lowest possible delta %d; hits below it fall back to that band."), Bands.Last().MinDelta, GSTN_PartsLimits::MinPossibleDelta));
	}
}
