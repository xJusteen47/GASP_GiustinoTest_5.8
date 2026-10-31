// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/GSTN_HurtboxMirror.h"

#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	bool IsAlpha(TCHAR C) { return FChar::IsAlpha(C); }
	bool IsUpper(TCHAR C) { return FChar::IsUpper(C); }
	bool IsLower(TCHAR C) { return FChar::IsLower(C); }

	/** Does a fragment found at Index (length Length) sit on a word boundary of Name? See FindCounterpart */
	bool IsOnWordBoundary(const FString& Name, int32 Index, int32 Length)
	{
		const int32 End = Index + Length;
		const bool bBeforeOk = Index == 0 || !IsAlpha(Name[Index]) || !IsAlpha(Name[Index - 1]) || (IsUpper(Name[Index]) && IsLower(Name[Index - 1]));
		const bool bAfterOk = End >= Name.Len() || !IsAlpha(Name[End - 1]) || !IsAlpha(Name[End]) || (IsUpper(Name[End]) && IsLower(Name[End - 1]));
		return bBeforeOk && bAfterOk;
	}

	/** The replacement in the case of the text it replaces: "_L" gives "_R", "Left" gives "Right", "left" gives "right" */
	FString MatchCase(const FString& Matched, const FString& Replacement)
	{
		bool bAnyLetter = false;
		bool bAllUpper = true;
		bool bAllLower = true;
		for (const TCHAR C : Matched)
		{
			if (IsAlpha(C))
			{
				bAnyLetter = true;
				bAllUpper &= IsUpper(C);
				bAllLower &= IsLower(C);
			}
		}
		if (!bAnyLetter)
		{
			return Replacement;
		}
		if (bAllUpper)
		{
			return Replacement.ToUpper();
		}
		if (bAllLower)
		{
			return Replacement.ToLower();
		}
		return Replacement;
	}

	/** The hurtbox name or bone of the other side, using the bone analysis when the bones are known */
	bool CounterpartOf(const FString& Name, const GSTNHurtboxMirror::FResolvedConfig& Config, FString& Out)
	{
		return GSTNHurtboxMirror::FindCounterpart(Name, Config.Pairs, Out);
	}
}

bool GSTNHurtboxMirror::FindCounterpart(const FString& Name, TConstArrayView<FGSTN_MirrorNamePair> Pairs, FString& OutCounterpart, ESide* OutSide)
{
	for (const FGSTN_MirrorNamePair& Pair : Pairs)
	{
		if (Pair.Left.IsEmpty() || Pair.Right.IsEmpty())
		{
			continue; // a half-edited line must pair nothing
		}
		const ESearchCase::Type Case = Pair.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
		for (int32 Direction = 0; Direction < 2; ++Direction)
		{
			const FString& From = Direction == 0 ? Pair.Left : Pair.Right;
			const FString& To = Direction == 0 ? Pair.Right : Pair.Left;
			int32 SearchFrom = 0;
			while (SearchFrom <= Name.Len() - From.Len())
			{
				const int32 Found = Name.Find(From, Case, ESearchDir::FromStart, SearchFrom);
				if (Found == INDEX_NONE)
				{
					break;
				}
				if (IsOnWordBoundary(Name, Found, From.Len()))
				{
					const FString Matched = Name.Mid(Found, From.Len());
					const FString Replacement = Pair.bCaseSensitive ? To : MatchCase(Matched, To);
					OutCounterpart = Name.Left(Found) + Replacement + Name.Mid(Found + From.Len());
					if (OutSide != nullptr)
					{
						*OutSide = Direction == 0 ? ESide::Left : ESide::Right;
					}
					return true;
				}
				SearchFrom = Found + 1;
			}
		}
	}
	if (OutSide != nullptr)
	{
		*OutSide = ESide::None;
	}
	return false;
}

GSTNHurtboxMirror::FNameAnalysis GSTNHurtboxMirror::AnalyzeNames(TConstArrayView<FName> Names, TConstArrayView<FGSTN_MirrorNamePair> Pairs)
{
	FNameAnalysis Analysis;
	// A rig and its counterpart may differ in case after the swap ("hand_R" for "hand_l"): look the counterpart up ignoring case
	TMap<FString, FName> ByLowerCase;
	for (const FName Name : Names)
	{
		ByLowerCase.Add(Name.ToString().ToLower(), Name);
	}
	for (const FName Name : Names)
	{
		FString Counterpart;
		if (!FindCounterpart(Name.ToString(), Pairs, Counterpart))
		{
			Analysis.Centre.AddUnique(Name);
			continue;
		}
		const FName* Existing = ByLowerCase.Find(Counterpart.ToLower());
		if (Existing != nullptr && *Existing != Name)
		{
			Analysis.Pairs.Add(Name, *Existing);
		}
		else
		{
			Analysis.Unpaired.AddUnique(Name);
		}
	}
	return Analysis;
}

GSTNHurtboxMirror::FResolvedConfig GSTNHurtboxMirror::ResolveConfig(EGSTN_SkeletonMirrorMode Mode, TConstArrayView<FGSTN_MirrorNamePair> SkeletonPairs,
	TConstArrayView<FGSTN_MirrorNamePair> GlobalPairs, TConstArrayView<FName> BoneNames, TConstArrayView<FName> FallbackNames)
{
	FResolvedConfig Config;
	TConstArrayView<FGSTN_MirrorNamePair> InForce = SkeletonPairs.IsEmpty() ? GlobalPairs : SkeletonPairs;
	Config.Pairs.Append(InForce.GetData(), InForce.Num());
	Config.bBonesKnown = !BoneNames.IsEmpty();
	if (Mode == EGSTN_SkeletonMirrorMode::Off)
	{
		Config.Reason = LOCTEXT("MirrorOffByRule", "Mirroring is switched off for this skeleton in the GSTN Combat Editor settings (Skeleton Mirror Rules).");
		return Config;
	}
	if (Config.Pairs.IsEmpty())
	{
		Config.Reason = LOCTEXT("MirrorNoPairs", "There are no left/right name pairs in the GSTN Combat Editor settings, so there is nothing to mirror by.");
		return Config;
	}
	Config.Bones = AnalyzeNames(Config.bBonesKnown ? BoneNames : FallbackNames, Config.Pairs);
	if (!Config.Bones.IsMirrorable())
	{
		Config.Reason = LOCTEXT("MirrorNoRecognisablePairs", "No bone of this skeleton has a left/right counterpart under the name pairs in force, so mirroring is off. Add pairs in the GSTN Combat Editor settings if the bones are named another way.");
		return Config;
	}
	Config.bEnabled = true;
	Config.Reason = FText::Format(LOCTEXT("MirrorPairsFound", "{0} left/right pairs recognised."), FText::AsNumber(Config.Bones.Pairs.Num() / 2));
	return Config;
}

GSTNHurtboxMirror::FResolvedConfig GSTNHurtboxMirror::ResolveConfig(const UGSTN_CombatEditorSettings& Settings, const USkeleton* Skeleton, TConstArrayView<FName> BoneNames, TConstArrayView<FName> FallbackNames)
{
	EGSTN_SkeletonMirrorMode Mode = EGSTN_SkeletonMirrorMode::Auto;
	TConstArrayView<FGSTN_MirrorNamePair> SkeletonPairs;
	if (Skeleton != nullptr)
	{
		const FSoftObjectPath Path(Skeleton);
		for (const FGSTN_SkeletonMirrorRule& Rule : Settings.SkeletonMirrorRules)
		{
			if (Rule.Skeleton.ToSoftObjectPath() == Path)
			{
				Mode = Rule.Mode;
				SkeletonPairs = Rule.NamePairs;
				break;
			}
		}
	}
	return ResolveConfig(Mode, SkeletonPairs, Settings.MirrorNamePairs, BoneNames, FallbackNames);
}

EAxis::Type GSTNHurtboxMirror::GetLateralAxis(EGSTN_MirrorForwardAxis Forward)
{
	return (Forward == EGSTN_MirrorForwardAxis::PositiveX || Forward == EGSTN_MirrorForwardAxis::NegativeX) ? EAxis::Y : EAxis::X;
}

FTransform GSTNHurtboxMirror::MirrorTransform(const FTransform& Transform, EGSTN_MirrorForwardAxis Forward)
{
	// M flips the lateral axis. M R M for a rotation with quaternion (v, w) is the rotation (-M v, w): the lateral component of v stays, the other two change sign
	const bool bLateralY = GetLateralAxis(Forward) == EAxis::Y;
	FVector Location = Transform.GetLocation();
	FQuat Rotation = Transform.GetRotation();
	if (bLateralY)
	{
		Location.Y = -Location.Y;
		Rotation = FQuat(-Rotation.X, Rotation.Y, -Rotation.Z, Rotation.W);
	}
	else
	{
		Location.X = -Location.X;
		Rotation = FQuat(Rotation.X, -Rotation.Y, -Rotation.Z, Rotation.W);
	}
	return FTransform(Rotation, Location, Transform.GetScale3D());
}

FGSTN_HitShape GSTNHurtboxMirror::MirrorShape(const FGSTN_HitShape& Shape, EGSTN_MirrorForwardAxis Forward)
{
	FGSTN_HitShape Mirrored = Shape;
	Mirrored.Local = MirrorTransform(Shape.Local, Forward);
	return Mirrored;
}

TArray<FName> GSTNHurtboxMirror::FPlan::GetOverwrittenNames(const TArray<FGSTN_Hurtbox>& Set) const
{
	TArray<FName> Names;
	for (const FPlanEntry& Entry : Entries)
	{
		if (Set.IsValidIndex(Entry.ExistingIndex))
		{
			Names.Add(Set[Entry.ExistingIndex].Name);
		}
	}
	return Names;
}

bool GSTNHurtboxMirror::FPlan::NeedsConfirmation() const
{
	for (const FPlanEntry& Entry : Entries)
	{
		if (Entry.ExistingIndex != INDEX_NONE)
		{
			return true;
		}
	}
	return false;
}

GSTNHurtboxMirror::FPlan GSTNHurtboxMirror::MakePlan(const TArray<FGSTN_Hurtbox>& Set, TConstArrayView<int32> Selected, const FResolvedConfig& Config, EGSTN_MirrorForwardAxis Forward)
{
	FPlan Plan;
	if (!Config.bEnabled)
	{
		return Plan;
	}

	TMap<FName, int32> IndexByName;
	for (int32 Index = 0; Index < Set.Num(); ++Index)
	{
		IndexByName.Add(Set[Index].Name, Index);
	}
	TSet<int32> SelectedSet;
	for (const int32 Index : Selected)
	{
		SelectedSet.Add(Index);
	}

	// Sources in set order, so the result does not depend on the order of the clicks
	TArray<int32> Sources;
	Sources.Append(Selected.GetData(), Selected.Num());
	Sources.Sort();
	for (const int32 Index : Sources)
	{
		if (!Set.IsValidIndex(Index))
		{
			continue;
		}
		const FGSTN_Hurtbox& Source = Set[Index];

		FString CounterpartName;
		if (!CounterpartOf(Source.Name.ToString(), Config, CounterpartName))
		{
			// No marker on the name: the middle, unless its bone is sided (then the name is what is missing)
			FString BoneCounterpart;
			if (!Source.Bone.IsNone() && CounterpartOf(Source.Bone.ToString(), Config, BoneCounterpart))
			{
				Plan.Unpaired.AddUnique(Source.Name);
			}
			else
			{
				Plan.Centre.AddUnique(Source.Name);
			}
			continue;
		}

		FGSTN_Hurtbox Mirrored = Source;
		Mirrored.Name = FName(*CounterpartName);

		// The bone: mapped through the skeleton's pairs when the bones are known, by the name rule otherwise
		bool bBoneOk = true;
		if (!Source.Bone.IsNone())
		{
			if (Config.bBonesKnown)
			{
				const FName* Other = Config.Bones.Pairs.Find(Source.Bone);
				bBoneOk = Other != nullptr;
				if (Other != nullptr)
				{
					Mirrored.Bone = *Other;
				}
			}
			else
			{
				FString BoneCounterpart;
				bBoneOk = CounterpartOf(Source.Bone.ToString(), Config, BoneCounterpart);
				if (bBoneOk)
				{
					Mirrored.Bone = FName(*BoneCounterpart);
				}
			}
		}
		if (!bBoneOk)
		{
			Plan.Unpaired.AddUnique(Source.Name);
			continue;
		}

		// Two selected hurtboxes that are each other's counterpart: the first one is the source, the second is what it overwrites
		const int32* CounterpartIndex = IndexByName.Find(Mirrored.Name);
		if (CounterpartIndex != nullptr && SelectedSet.Contains(*CounterpartIndex) && *CounterpartIndex < Index)
		{
			Plan.SkippedAsTarget.AddUnique(Source.Name);
			continue;
		}

		Mirrored.Shape = MirrorShape(Source.Shape, Forward);
		FPlanEntry Entry;
		Entry.SourceIndex = Index;
		Entry.Mirrored = Mirrored;
		Entry.ExistingIndex = CounterpartIndex != nullptr ? *CounterpartIndex : INDEX_NONE;
		Plan.Entries.Add(Entry);
	}
	return Plan;
}

TArray<FGSTN_Hurtbox> GSTNHurtboxMirror::Apply(const TArray<FGSTN_Hurtbox>& Set, const FPlan& Plan)
{
	TArray<FGSTN_Hurtbox> Result = Set;
	for (const FPlanEntry& Entry : Plan.Entries)
	{
		if (Result.IsValidIndex(Entry.ExistingIndex))
		{
			Result[Entry.ExistingIndex] = Entry.Mirrored;
		}
		else
		{
			Result.Add(Entry.Mirrored);
		}
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
