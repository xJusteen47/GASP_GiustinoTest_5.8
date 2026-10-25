// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

// The rules of combat-m5-editor 9.2 that need the hit data of M4: hitboxes, hit data, relationship variants, hurt states, baked sockets and the
// hurtbox set. Same shape as the M3 rules in GSTN_CombatValidators.cpp: pure functions of the data that fill a report.

#include "Validation/GSTN_CombatValidators.h"

#include "GameplayTagsManager.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Rules/GSTN_CombatRulesValidation.h"
#include "Timeline/GSTN_TimelineTypes.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	bool RangesOverlap(const FGSTN_TimelineTrack& A, const FGSTN_TimelineTrack& B)
	{
		return A.EndFrame > A.StartFrame && B.EndFrame > B.StartFrame && A.StartFrame < B.EndFrame && B.StartFrame < A.EndFrame;
	}

	bool IsUnregisteredTag(const FGameplayTag& Tag)
	{
		return Tag.IsValid() && !UGameplayTagsManager::Get().RequestGameplayTag(Tag.GetTagName(), false).IsValid();
	}

	const FGSTN_TimelineTrack_Hitbox* GetHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 Index)
	{
		return Data.Tracks.IsValidIndex(Index) ? Data.Tracks[Index].GetPtr<FGSTN_TimelineTrack_Hitbox>() : nullptr;
	}

	/** The same shape in the same place: type, extent, placement, space and the path of the keys, within a tenth of a millimetre */
	bool SameShape(const FGSTN_HitboxSpec& A, const FGSTN_HitboxSpec& B)
	{
		constexpr double Tolerance = 0.01;
		if (A.Shape.Type != B.Shape.Type || A.Space != B.Space || A.Socket != B.Socket || A.Keys.Num() != B.Keys.Num())
		{
			return false;
		}
		if (!A.Shape.Extent.Equals(B.Shape.Extent, Tolerance) || !A.Shape.Local.Equals(B.Shape.Local, Tolerance))
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Keys.Num(); ++Index)
		{
			const FGSTN_HitboxKey& KeyA = A.Keys[Index];
			const FGSTN_HitboxKey& KeyB = B.Keys[Index];
			if (KeyA.Frame != KeyB.Frame || !KeyA.Offset.Equals(KeyB.Offset, Tolerance) || !KeyA.Rotation.Equals(KeyB.Rotation, Tolerance) || !KeyA.Scale.Equals(KeyB.Scale, Tolerance))
			{
				return false;
			}
		}
		return true;
	}

	bool SameHitData(const FGSTN_HitData& A, const FGSTN_HitData& B)
	{
		return FGSTN_HitData::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None);
	}

	struct FHitboxRef
	{
		int32 TrackIndex = INDEX_NONE;
		int32 SpecIndex = INDEX_NONE;
		const FGSTN_TimelineTrack_Hitbox* Track = nullptr;
		const FGSTN_HitboxSpec* Spec = nullptr;
	};

	void GatherHitboxes(const FGSTN_CombatTimelineData& Data, TArray<FHitboxRef>& Out)
	{
		for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
		{
			if (const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(Data, TrackIndex))
			{
				for (int32 SpecIndex = 0; SpecIndex < Track->Hitboxes.Num(); ++SpecIndex)
				{
					Out.Add({ TrackIndex, SpecIndex, Track, &Track->Hitboxes[SpecIndex] });
				}
			}
		}
	}

	FGSTN_ValidationFinding& AddFinding(FGSTN_ValidationReport& Report, const FGSTN_TimelineRuleContext& Context, EGSTN_Severity Severity, FName Code, FText Message, const FGSTN_FindingLocation& Where)
	{
		return Report.Add(Severity, Code, MoveTemp(Message), Context.Asset, Where);
	}

	FText Num(int32 Value)
	{
		return FText::AsNumber(Value);
	}
}

void FGSTN_CombatValidators::CheckHitboxKeys(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	// Keys count from the first frame of the track, so a key at or past the window length never plays
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(Data, TrackIndex);
		if (Track == nullptr)
		{
			continue;
		}
		const int32 Length = Track->EndFrame - Track->StartFrame;
		for (int32 SpecIndex = 0; SpecIndex < Track->Hitboxes.Num(); ++SpecIndex)
		{
			const TArray<FGSTN_HitboxKey>& Keys = Track->Hitboxes[SpecIndex].Keys;
			for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
			{
				const int32 KeyFrame = Keys[KeyIndex].Frame;
				if (KeyFrame < 0 || (Length > 0 && KeyFrame >= Length))
				{
					AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HitboxKeys(),
						FText::Format(LOCTEXT("HitboxKeyOutside", "Track {0}, hitbox {1}: key {2} is at frame {3}, outside the window of {4} frames. Keys count from the first frame of the track."),
							Num(TrackIndex), Num(SpecIndex), Num(KeyIndex), Num(KeyFrame), Num(Length)),
						FGSTN_FindingLocation::OnTrack(TrackIndex, Track->StartFrame + FMath::Clamp(KeyFrame, 0, FMath::Max(Length - 1, 0))));
				}
			}
		}
	}

	// Hitboxes that share a HitGroup and are open together: the victim is hit by one of them, the one with the highest Priority, and a tie goes to the
	// lower index. Different Priorities are a sweetspot and a sourspot (M4 3), meant. Equal Priority with different hits is a tie nobody chose.
	TArray<FHitboxRef> Hitboxes;
	GatherHitboxes(Data, Hitboxes);
	for (int32 First = 0; First < Hitboxes.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Hitboxes.Num(); ++Second)
		{
			const FHitboxRef& A = Hitboxes[First];
			const FHitboxRef& B = Hitboxes[Second];
			const bool bOpenTogether = A.TrackIndex == B.TrackIndex ? A.Track->EndFrame > A.Track->StartFrame : RangesOverlap(*A.Track, *B.Track);
			if (!bOpenTogether || A.Spec->HitGroup != B.Spec->HitGroup || SameShape(*A.Spec, *B.Spec))
			{
				continue;
			}
			if (A.Spec->Data.Priority == B.Spec->Data.Priority && !SameHitData(A.Spec->Data, B.Spec->Data))
			{
				AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::HitboxKeys(),
					FText::Format(LOCTEXT("HitGroupTie", "Hitboxes {0}.{1} and {2}.{3} are in hit group {4}, are open together and have the same Priority ({5}) with different hit data: when both touch a victim the lower one wins by index. Give the stronger one a higher Priority, or another group."),
						Num(A.TrackIndex), Num(A.SpecIndex), Num(B.TrackIndex), Num(B.SpecIndex), Num(A.Spec->HitGroup), Num(A.Spec->Data.Priority)),
					FGSTN_FindingLocation::OnTrack(B.TrackIndex, FMath::Max(A.Track->StartFrame, B.Track->StartFrame)));
			}
		}
	}
}

void FGSTN_CombatValidators::CheckHitboxOverlap(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	TArray<FHitboxRef> Hitboxes;
	GatherHitboxes(Data, Hitboxes);
	for (int32 First = 0; First < Hitboxes.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Hitboxes.Num(); ++Second)
		{
			const FHitboxRef& A = Hitboxes[First];
			const FHitboxRef& B = Hitboxes[Second];
			const bool bOpenTogether = A.TrackIndex == B.TrackIndex ? A.Track->EndFrame > A.Track->StartFrame : RangesOverlap(*A.Track, *B.Track);
			if (bOpenTogether && A.Spec->HitGroup == B.Spec->HitGroup && SameShape(*A.Spec, *B.Spec))
			{
				AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::OverlapSameKind(),
					FText::Format(LOCTEXT("HitboxSameShape", "Hitboxes {0}.{1} and {2}.{3} are in hit group {4}, open together and have the same shape in the same place: the second can never hit anything the first does not."),
						Num(A.TrackIndex), Num(A.SpecIndex), Num(B.TrackIndex), Num(B.SpecIndex), Num(A.Spec->HitGroup)),
					FGSTN_FindingLocation::OnTrack(B.TrackIndex, FMath::Max(A.Track->StartFrame, B.Track->StartFrame)));
			}
		}
	}
}

namespace
{
	/** The checks of one FGSTN_HitData, base or the values of a variant. Where reads "Track 1, hitbox 0" or "Track 1, hitbox 0, variant Combat.Relationship.Ally" */
	void CheckOneHit(const FGSTN_HitData& Hit, const FText& Label, const FGSTN_FindingLocation& Where, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
	{
		const auto Negative = [&](const TCHAR* Field, double Value)
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HitData(),
				FText::Format(LOCTEXT("HitFieldNegative", "{0}: {1} is {2}, which is negative."), Label, FText::FromString(Field), FText::AsNumber(Value)), Where);
		};
		if (Hit.Damage < 0.f) { Negative(TEXT("Damage"), Hit.Damage); }
		if (Hit.BaseKnockback < 0.f) { Negative(TEXT("BaseKnockback"), Hit.BaseKnockback); }
		if (Hit.KnockbackGrowth < 0.f) { Negative(TEXT("KnockbackGrowth"), Hit.KnockbackGrowth); }
		if (Hit.ShieldDamage < 0.f) { Negative(TEXT("ShieldDamage"), Hit.ShieldDamage); }
		if (Hit.HitstunMultiplier < 0.f) { Negative(TEXT("HitstunMultiplier"), Hit.HitstunMultiplier); }
		if (Hit.StaggerForceScale < 0.f) { Negative(TEXT("StaggerForceScale"), Hit.StaggerForceScale); }
		if (Hit.HitstopMode == EGSTN_HitstopMode::Auto)
		{
			if (Hit.HitstopMultiplier < 0.f) { Negative(TEXT("HitstopMultiplier"), Hit.HitstopMultiplier); }
			if (Hit.AttackerHitstopMultiplier < 0.f) { Negative(TEXT("AttackerHitstopMultiplier"), Hit.AttackerHitstopMultiplier); }
		}
		else if (Hit.HitstopFramesOverride < 0)
		{
			Negative(TEXT("HitstopFramesOverride"), Hit.HitstopFramesOverride);
		}
		if (Hit.StaggerForceMode == EGSTN_StaggerForceMode::Override && Hit.StaggerForce < 0.f)
		{
			Negative(TEXT("StaggerForce"), Hit.StaggerForce);
		}

		// A fixed knockback replaces the base + growth bracket, so a growth next to it is dead data
		if (Hit.FixedKnockback >= 0.f && Hit.KnockbackGrowth != 0.f)
		{
			AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::HitData(),
				FText::Format(LOCTEXT("FixedWithGrowth", "{0}: FixedKnockback is set ({1}) and replaces base knockback plus growth, so KnockbackGrowth ({2}) does nothing."),
					Label, FText::AsNumber(Hit.FixedKnockback), FText::AsNumber(Hit.KnockbackGrowth)), Where);
		}

		if (IsUnregisteredTag(Hit.HitType))
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HitData(),
				FText::Format(LOCTEXT("HitTypeUnknown", "{0}: the hit type {1} is not a registered tag."), Label, FText::FromName(Hit.HitType.GetTagName())), Where);
		}
		if (IsUnregisteredTag(Hit.CueTag))
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HitData(),
				FText::Format(LOCTEXT("HitCueUnknown", "{0}: the cue {1} is not a registered tag."), Label, FText::FromName(Hit.CueTag.GetTagName())), Where);
		}
	}
}

void FGSTN_CombatValidators::CheckHitData(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	TArray<FHitboxRef> Hitboxes;
	GatherHitboxes(Data, Hitboxes);
	for (const FHitboxRef& Ref : Hitboxes)
	{
		const FGSTN_FindingLocation Where = FGSTN_FindingLocation::OnTrack(Ref.TrackIndex, Ref.Track->StartFrame);
		const FText Label = FText::Format(LOCTEXT("HitLabel", "Track {0}, hitbox {1}"), Num(Ref.TrackIndex), Num(Ref.SpecIndex));
		CheckOneHit(Ref.Spec->Data, Label, Where, Context, Report);

		// A variant's Values only matter for the fields its mask names, and its defaults are valid, so a variant with no fields has nothing to say
		for (const TPair<FGameplayTag, FGSTN_HitVariant>& Pair : Ref.Spec->RelationshipVariants)
		{
			if (Pair.Value.Override.Fields != 0)
			{
				CheckOneHit(Pair.Value.Override.Values,
					FText::Format(LOCTEXT("HitVariantLabel", "Track {0}, hitbox {1}, variant {2}"), Num(Ref.TrackIndex), Num(Ref.SpecIndex), FText::FromName(Pair.Key.GetTagName())), Where, Context, Report);
			}
		}
	}
}

void FGSTN_CombatValidators::CheckVariants(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	static const FString RelationshipRoot(TEXT("Combat.Relationship."));

	TArray<FHitboxRef> Hitboxes;
	GatherHitboxes(Data, Hitboxes);
	for (const FHitboxRef& Ref : Hitboxes)
	{
		const FGSTN_FindingLocation Where = FGSTN_FindingLocation::OnTrack(Ref.TrackIndex, Ref.Track->StartFrame);
		for (const TPair<FGameplayTag, FGSTN_HitVariant>& Pair : Ref.Spec->RelationshipVariants)
		{
			const FGSTN_HitVariant& Variant = Pair.Value;
			const FText Label = FText::Format(LOCTEXT("VariantLabel", "Track {0}, hitbox {1}, variant {2}"), Num(Ref.TrackIndex), Num(Ref.SpecIndex), FText::FromName(Pair.Key.GetTagName()));

			if (!Pair.Key.IsValid() || !Pair.Key.ToString().StartsWith(RelationshipRoot))
			{
				AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::Variants(),
					FText::Format(LOCTEXT("VariantKeyRoot", "{0}: the key is not under Combat.Relationship, so no victim ever has it and the variant never applies."), Label), Where);
			}
			if (!Variant.bIgnore && Variant.ShapeScale <= 0.f)
			{
				AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::Variants(),
					FText::Format(LOCTEXT("VariantScale", "{0}: ShapeScale is {1} and the variant does not ignore the victim. A hitbox scaled to nothing hits nobody: tick Ignore instead."), Label, FText::AsNumber(Variant.ShapeScale)), Where);
			}
			const FGSTN_HitDataOverride& Override = Variant.Override;
			if (Override.DamageMultiplier < 0.f || Override.KnockbackMultiplier < 0.f || Override.HitstunMultiplier < 0.f || Override.HitstopMultiplier < 0.f
				|| Override.ShieldDamageMultiplier < 0.f || Override.StaggerForceMultiplier < 0.f)
			{
				AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::Variants(),
					FText::Format(LOCTEXT("VariantMultiplier", "{0}: a multiplier is negative."), Label), Where);
			}
		}
	}
}

void FGSTN_CombatValidators::CheckHurtState(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack_HurtboxModifier* Track = Data.Tracks[Index].GetPtr<FGSTN_TimelineTrack_HurtboxModifier>();
		if (Track == nullptr)
		{
			continue;
		}
		const FGSTN_FindingLocation Where = FGSTN_FindingLocation::OnTrack(Index, Track->StartFrame);

		// Armor lets a hit through unstaggered only below a force: with a threshold of 0 or less no hit is ever below it, and an unset one staggers on every hit
		if (Track->Kind == EGSTN_HurtboxModifierKind::Armor && !Track->ArmorThreshold.bInfinite && Track->ArmorThreshold.Value <= 0.f)
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HurtState(),
				FText::Format(LOCTEXT("ArmorThreshold", "Hurtbox modifier {0} is Armor with a threshold of {1}: no hit is ever weaker than that, so it does nothing. Set a positive stagger force, or tick Infinite for super armor."),
					Num(Index), FText::AsNumber(Track->ArmorThreshold.Value)), Where);
		}

		if (Context.KnownHurtboxNames)
		{
			TArray<FName, TInlineAllocator<4>> Named;
			if (!Track->Part.PartName.IsNone())
			{
				Named.Add(Track->Part.PartName);
			}
			Named.Append(Track->AffectedHurtboxes);
			for (const FName Name : Named)
			{
				if (!Name.IsNone() && !Context.KnownHurtboxNames->Contains(Name))
				{
					AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::HurtState(),
						FText::Format(LOCTEXT("HurtboxNotInSet", "Hurtbox modifier {0} names the hurtbox {1}, which the hurtbox set does not have."), Num(Index), FText::FromName(Name)), Where);
				}
			}
		}
	}
}

void FGSTN_CombatValidators::CheckBakedSockets(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	TSet<FName> UsedSockets;
	TArray<FHitboxRef> Hitboxes;
	GatherHitboxes(Data, Hitboxes);
	for (const FHitboxRef& Ref : Hitboxes)
	{
		if (Ref.Spec->Space != EGSTN_HitboxSpace::BakedSocket || Ref.Spec->Socket.IsNone())
		{
			continue; // a BakedSocket hitbox with no socket name is Core's error
		}
		UsedSockets.Add(Ref.Spec->Socket);
		const FGSTN_FindingLocation Where = FGSTN_FindingLocation::OnTrack(Ref.TrackIndex, Ref.Track->StartFrame);
		const FGSTN_BakedSocketFrames* Baked = Data.BakedSockets.Find(Ref.Spec->Socket);
		if (Baked == nullptr || Baked->Frames.IsEmpty())
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::BakedSocketMissing(),
				FText::Format(LOCTEXT("SocketNotBaked", "Track {0}, hitbox {1} reads the socket '{2}', which has no baked frames. Bake the timeline's sockets."),
					Num(Ref.TrackIndex), Num(Ref.SpecIndex), FText::FromName(Ref.Spec->Socket)), Where);
		}
		else if (Baked->Frames.Num() != Data.TotalFrames)
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::BakedLength(),
				FText::Format(LOCTEXT("SocketBakedLength", "Track {0}, hitbox {1}: the socket '{2}' is baked for {3} frames and the timeline has {4}. The bake is out of date: bake again."),
					Num(Ref.TrackIndex), Num(Ref.SpecIndex), FText::FromName(Ref.Spec->Socket), Num(Baked->Frames.Num()), Num(Data.TotalFrames)), Where);
		}
	}

	for (const TPair<FName, FGSTN_BakedSocketFrames>& Pair : Data.BakedSockets)
	{
		if (!UsedSockets.Contains(Pair.Key))
		{
			AddFinding(Report, Context, EGSTN_Severity::Info, GSTNValidationCodes::BakedStale(),
				FText::Format(LOCTEXT("SocketNotUsed", "The socket '{0}' is baked but no hitbox uses it."), FText::FromName(Pair.Key)), FGSTN_FindingLocation());
		}
	}
}

void FGSTN_CombatValidators::CheckBakeStatus(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	if (!Context.BakeStatus.IsSet())
	{
		return;
	}
	const FGSTN_BakeStatusResult& Status = Context.BakeStatus.GetValue();
	if (Status.Status == EGSTN_BakeStatus::Stale)
	{
		// A frame-count mismatch is already BakedLength, with the hitbox named
		if (!Report.HasCode(GSTNValidationCodes::BakedLength()))
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::BakedStale(), Status.Reason, FGSTN_FindingLocation());
		}
	}
	else if (Status.Status == EGSTN_BakeStatus::BakedElsewhere)
	{
		AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::BakedStale(), Status.Reason, FGSTN_FindingLocation());
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Hurtbox set

void FGSTN_CombatValidators::CheckHurtboxSet(const UGSTN_HurtboxSetDefinition& Set, UObject* Asset, FGSTN_ValidationReport& Report)
{
	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::ValidateHurtboxSet(Set, Messages);
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		// The texts are Core's; GSTNCombat.Editor.Validate.HurtboxMessagesClassified pins every one of them
		FName Code = GSTNValidationCodes::HurtboxData();
		if (Message.Text.StartsWith(TEXT("The set has no hurtbox")))
		{
			Code = GSTNValidationCodes::HurtboxEmpty();
		}
		else if (Message.Text.Contains(TEXT("is used twice")) || Message.Text.Contains(TEXT("has no name")))
		{
			Code = GSTNValidationCodes::HurtboxDuplicateName();
		}
		else if (Message.Text.Contains(TEXT("no volume")))
		{
			Code = GSTNValidationCodes::HurtboxExtent();
		}
		else if (Message.Text.Contains(TEXT("no Combat.Hurtbox tag")))
		{
			Code = GSTNValidationCodes::HurtboxTag();
		}
		Report.Add(Message.bError ? EGSTN_Severity::Error : EGSTN_Severity::Warning, Code, FText::FromString(Message.Text), Asset);
	}

	// A tag that classifies a hurtbox but sits under another root is not addressable as a hurtbox zone: no modifier or reaction rule would find it
	const FGameplayTag Root = GSTNCombatHitTags::Root_Hurtbox.GetTag();
	for (int32 Index = 0; Index < Set.Hurtboxes.Num(); ++Index)
	{
		for (const FGameplayTag& Tag : Set.Hurtboxes[Index].Tags)
		{
			if (!Tag.MatchesTag(Root))
			{
				Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::HurtboxTag(),
					FText::Format(LOCTEXT("HurtboxTagRoot", "Hurtbox {0} ({1}) carries the tag {2}, which is not under Combat.Hurtbox."), Num(Index), FText::FromName(Set.Hurtboxes[Index].Name), FText::FromName(Tag.GetTagName())), Asset);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
