// Kit mods, pure baking.

#include "GSTN_KitModBaking.h"
#include "Hash/CityHash.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitReachUtils.h"
#include "Timeline/GSTN_TimelineTypes.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(GSTNKitModsTags::Mod, "Mod", "Root of the kit mod identity tags (Mod.<Name>)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(GSTNKitModsTags::Mod_Slot, "Mod.Slot", "Root of the kit mod slot types");

const FName FGSTN_LoadoutModsExt::WireId(TEXT("KitMods"));

namespace
{
	constexpr int32 TimeOne = 1000;
}

bool FGSTN_CombinedMoveModifier::IsIdentity() const
{
	return FGSTN_KitModBaking::QuantizeTimeScale(TimeScale) == TimeOne && HitboxScale.Equals(FVector::OneVector, 0.0) && ReachScale == 1.f
		&& DamageScale == 1.f && KnockbackScale == 1.f && StaggerForceScale == 1.f && HitstopScale == 1.f && AddedTags.IsEmpty();
}

const FGSTN_KitModConfig* FGSTN_KitModBaking::SelectConfig(const FGSTN_LoadoutModsExt& Ext, const FGameplayTagContainer& KitTags)
{
	const FGSTN_KitModConfig* Wildcard = nullptr;
	for (const FGSTN_KitModConfig& Config : Ext.Configs)
	{
		if (!Config.Kit.IsValid())
		{
			Wildcard = Wildcard ? Wildcard : &Config;
		}
		else if (KitTags.HasTagExact(Config.Kit))
		{
			return &Config;
		}
	}
	return Wildcard;
}

FGSTN_CombinedMoveModifier FGSTN_KitModBaking::Combine(TConstArrayView<const FGSTN_MoveModifier*> Modifiers, const FGSTN_KitModLimits& Limits)
{
	FGSTN_CombinedMoveModifier Out;
	for (const FGSTN_MoveModifier* Modifier : Modifiers)
	{
		if (!Modifier)
		{
			continue;
		}
		Out.TimeScale *= Modifier->TimeScale;
		Out.HitboxScale *= Modifier->HitboxScale;
		Out.ReachScale *= Modifier->ReachScale;
		Out.DamageScale *= Modifier->DamageScale;
		Out.KnockbackScale *= Modifier->KnockbackScale;
		Out.StaggerForceScale *= Modifier->StaggerForceScale;
		Out.HitstopScale *= Modifier->HitstopScale;
		Out.AddedTags.AppendTags(Modifier->AddedTags);
	}
	Out.TimeScale = FMath::Clamp(Out.TimeScale, FMath::Max(Limits.MinTimeScale, 0.1f), FMath::Max(Limits.MaxTimeScale, Limits.MinTimeScale));
	Out.ReachScale = FMath::Clamp(Out.ReachScale, UGSTN_HitReachUtils::MinReachScale, FMath::Min(Limits.MaxReachScale, UGSTN_HitReachUtils::MaxReachScale));
	Out.HitboxScale = Out.HitboxScale.ComponentMax(FVector(0.05));
	return Out;
}

bool FGSTN_KitModBaking::MatchesFilter(const FGameplayTagQuery& Filter, const FGameplayTagContainer& MoveTags)
{
	return Filter.IsEmpty() || Filter.Matches(MoveTags);
}

int32 FGSTN_KitModBaking::QuantizeTimeScale(float TimeScale)
{
	return FMath::Max(FMath::RoundToInt32(TimeScale * static_cast<float>(TimeOne)), 1);
}

int32 FGSTN_KitModBaking::ScaleFrame(int32 Frame, int32 Quantized)
{
	if (Frame < 0)
	{
		return Frame;
	}
	return static_cast<int32>((static_cast<int64>(Frame) * Quantized + TimeOne / 2) / TimeOne);
}

void FGSTN_KitModBaking::BakeTimeline(const FGSTN_CombatTimelineData& Base, const FGSTN_CombinedMoveModifier& Modifier, FGSTN_CombatTimelineData& OutDerived)
{
	OutDerived = Base;

	// 1. Hit data and hitbox size. The multipliers of a relationship variant still apply on top at run time
	FGSTN_HitVariant Scales;
	Scales.Override.DamageMultiplier = Modifier.DamageScale;
	Scales.Override.KnockbackMultiplier = Modifier.KnockbackScale;
	Scales.Override.HitstopMultiplier = Modifier.HitstopScale;
	Scales.Override.StaggerForceMultiplier = Modifier.StaggerForceScale;
	const bool bScaleHits = Modifier.DamageScale != 1.f || Modifier.KnockbackScale != 1.f || Modifier.HitstopScale != 1.f || Modifier.StaggerForceScale != 1.f;
	const bool bScaleShapes = !Modifier.HitboxScale.Equals(FVector::OneVector, 0.0);
	for (TInstancedStruct<FGSTN_TimelineTrack>& Instance : OutDerived.Tracks)
	{
		if (FGSTN_TimelineTrack_Hitbox* Track = Instance.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>())
		{
			for (FGSTN_HitboxSpec& Spec : Track->Hitboxes)
			{
				if (bScaleHits)
				{
					Spec.Data = UGSTN_HitDataUtils::ApplyVariant(Spec.Data, Scales);
				}
				if (bScaleShapes)
				{
					Spec.Shape.Extent *= Modifier.HitboxScale;
				}
			}
		}
	}

	// 2. Reach along each hitbox's axis (L4)
	if (Modifier.ReachScale != 1.f)
	{
		UGSTN_HitReachUtils::ApplyReachScale(OutDerived, Modifier.ReachScale);
	}

	// 3. Time
	const int32 Q = QuantizeTimeScale(Modifier.TimeScale);
	if (Q != TimeOne)
	{
		OutDerived.TotalFrames = FMath::Max(ScaleFrame(Base.TotalFrames, Q), Base.TotalFrames > 0 ? 1 : 0);
		OutDerived.FirstActionableFrame = FMath::Min(ScaleFrame(Base.FirstActionableFrame, Q), OutDerived.TotalFrames);
		OutDerived.Tail.StartFrame = Base.Tail.StartFrame >= 0 ? FMath::Min(ScaleFrame(Base.Tail.StartFrame, Q), OutDerived.TotalFrames) : Base.Tail.StartFrame;
		OutDerived.TurnUntilFrame = ScaleFrame(Base.TurnUntilFrame, Q);
		OutDerived.OverrideFrames = Base.OverrideFrames > 0 ? FMath::Max(ScaleFrame(Base.OverrideFrames, Q), 1) : Base.OverrideFrames;
		OutDerived.MontagePlayRate = Base.MontagePlayRate * static_cast<float>(TimeOne) / static_cast<float>(Q);

		for (TInstancedStruct<FGSTN_TimelineTrack>& Instance : OutDerived.Tracks)
		{
			FGSTN_TimelineTrack* Track = Instance.GetMutablePtr<FGSTN_TimelineTrack>();
			if (!Track)
			{
				continue;
			}
			// A window keeps at least one frame, so a 1-frame active window never vanishes
			const int32 Start = ScaleFrame(Track->StartFrame, Q);
			Track->EndFrame = FMath::Max(ScaleFrame(Track->EndFrame, Q), Start + 1);
			Track->StartFrame = Start;

			if (FGSTN_TimelineTrack_WeaponSwitch* Switch = Instance.GetMutablePtr<FGSTN_TimelineTrack_WeaponSwitch>())
			{
				Switch->SwapFrame = ScaleFrame(Switch->SwapFrame, Q);
			}
			if (FGSTN_TimelineTrack_Hitbox* Hitbox = Instance.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>())
			{
				for (FGSTN_HitboxSpec& Spec : Hitbox->Hitboxes)
				{
					int32 Previous = INDEX_NONE;
					for (FGSTN_HitboxKey& Key : Spec.Keys)
					{
						// Keys are sorted by frame: two keys never collapse onto one frame
						Key.Frame = FMath::Max(ScaleFrame(Key.Frame, Q), Previous + 1);
						Previous = Key.Frame;
					}
				}
			}
		}

		// Baked sockets: one transform per timeline frame, resampled by nearest source frame
		for (TPair<FName, FGSTN_BakedSocketFrames>& Pair : OutDerived.BakedSockets)
		{
			const TArray<FTransform>& Source = Base.BakedSockets[Pair.Key].Frames;
			if (Source.IsEmpty())
			{
				continue;
			}
			const int32 NewCount = FMath::Max(ScaleFrame(Source.Num(), Q), 1);
			Pair.Value.Frames.SetNum(NewCount);
			for (int32 Frame = 0; Frame < NewCount; ++Frame)
			{
				const int32 SourceFrame = static_cast<int32>((static_cast<int64>(Frame) * TimeOne + Q / 2) / Q);
				Pair.Value.Frames[Frame] = Source[FMath::Clamp(SourceFrame, 0, Source.Num() - 1)];
			}
		}
	}

	// 4. Tags for the whole run
	if (!Modifier.AddedTags.IsEmpty())
	{
		FGSTN_TimelineTrack_Tags TagsTrack;
		TagsTrack.StartFrame = 0;
		TagsTrack.EndFrame = FMath::Max(OutDerived.TotalFrames, 1);
		TagsTrack.Tags = Modifier.AddedTags;
		OutDerived.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>(TagsTrack));
	}
}

FName FGSTN_KitModBaking::MakeDerivedName(const FSoftObjectPath& BasePath, TConstArrayView<FSoftObjectPath> ModIdsInSlotOrder)
{
	FString Key = BasePath.ToString();
	for (const FSoftObjectPath& Mod : ModIdsInSlotOrder)
	{
		Key += TEXT("|");
		Key += Mod.ToString();
	}
	const FTCHARToUTF8 Utf8(*Key);
	const uint64 Hash = CityHash64(Utf8.Get(), Utf8.Length());
	return FName(*FString::Printf(TEXT("GSTNModTL_%016llx"), Hash));
}
