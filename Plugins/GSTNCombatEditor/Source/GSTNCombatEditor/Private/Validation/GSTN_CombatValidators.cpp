// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_CombatValidators.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/AssetManager.h"
#include "GameplayTagsManager.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_InputTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** Half-open ranges that share a frame. An empty range overlaps nothing */
	bool RangesOverlap(const FGSTN_TimelineTrack& A, const FGSTN_TimelineTrack& B)
	{
		return A.EndFrame > A.StartFrame && B.EndFrame > B.StartFrame && A.StartFrame < B.EndFrame && B.StartFrame < A.EndFrame;
	}

	/** Two selectors that can address the same hurtbox: the same part, the same group, or tags one of which contains the other */
	bool SelectorsOverlap(const FGSTN_PartSelector& A, const FGSTN_PartSelector& B)
	{
		if (!A.PartName.IsNone() && A.PartName == B.PartName)
		{
			return true;
		}
		if (!A.GroupName.IsNone() && A.GroupName == B.GroupName)
		{
			return true;
		}
		return A.PartTag.IsValid() && B.PartTag.IsValid() && (A.PartTag.MatchesTag(B.PartTag) || B.PartTag.MatchesTag(A.PartTag));
	}

	/** A tag that names something: unset is fine (a field left empty), a name the tag table does not know is not */
	bool IsUnregistered(const FGameplayTag& Tag)
	{
		return Tag.IsValid() && !UGameplayTagsManager::Get().RequestGameplayTag(Tag.GetTagName(), false).IsValid();
	}

	FText TrackName(int32 Index)
	{
		return FText::AsNumber(Index);
	}

	FGSTN_ValidationFinding& AddFinding(FGSTN_ValidationReport& Report, const FGSTN_TimelineRuleContext& Context, EGSTN_Severity Severity, FName Code, FText Message, const FGSTN_FindingLocation& Where = FGSTN_FindingLocation())
	{
		return Report.Add(Severity, Code, MoveTemp(Message), Context.Asset, Where);
	}

	const FGSTN_TimelineTrack* GetTrack(const FGSTN_CombatTimelineData& Data, int32 Index)
	{
		return Data.Tracks.IsValidIndex(Index) ? Data.Tracks[Index].GetPtr<FGSTN_TimelineTrack>() : nullptr;
	}

	/** Which rule a message of Core's timeline validation belongs to. The texts are Core's; GSTNCombat.Editor.Validate.CoreMessagesClassified pins every one of them */
	FName ClassifyCoreMessage(const FString& Text)
	{
		// The hitbox messages (M4, E11). The baked ones are replaced by the rules of GSTN_HitRules.cpp (see IsSupersededByHitRules)
		if (Text.Contains(TEXT("has no baked frames yet")) || Text.Contains(TEXT("a BakedSocket hitbox names no socket")))
		{
			return GSTNValidationCodes::BakedSocketMissing();
		}
		if (Text.Contains(TEXT("frames, the timeline has")))
		{
			return GSTNValidationCodes::BakedLength();
		}
		if (Text.Contains(TEXT(": the shape has no volume")) || Text.Contains(TEXT("the keys are not sorted")))
		{
			return GSTNValidationCodes::HitboxKeys();
		}
		if (Text.Contains(TEXT("is data only in M4")) || Text.Contains(TEXT("A hitstop override above")))
		{
			return GSTNValidationCodes::HitData();
		}
		if (Text.Contains(TEXT("an Armor window with no ArmorThreshold")))
		{
			return GSTNValidationCodes::HurtState();
		}
		if (Text.Contains(TEXT("ricochet response is enabled")) || Text.StartsWith(TEXT("The recoil movement")))
		{
			return GSTNValidationCodes::Ricochet();
		}
		if (Text.StartsWith(TEXT("The montage lasts")))
		{
			return GSTNValidationCodes::MontageLength();
		}
		if (Text.Contains(TEXT("addresses no part")) || Text.Contains(TEXT("unknown part")) || Text.Contains(TEXT("unknown group")))
		{
			return GSTNValidationCodes::PartSelector();
		}
		if (Text.StartsWith(TEXT("The override window")) || Text.Contains(TEXT("deferred commit")))
		{
			return GSTNValidationCodes::DeferredCommit();
		}
		if (Text.StartsWith(TEXT("Track ")) && Text.EndsWith(TEXT(" is empty.")))
		{
			return GSTNValidationCodes::EmptyTrack();
		}
		if (Text.Contains(TEXT("TotalFrames")) || Text.Contains(TEXT("outside the timeline")) || Text.Contains(TEXT("empty range")) || Text.Contains(TEXT("FirstActionableFrame"))
			|| Text.StartsWith(TEXT("Tail")) || Text.StartsWith(TEXT("The tail")))
		{
			return GSTNValidationCodes::FrameRange();
		}
		return GSTNValidationCodes::CoreOther();
	}

	/**
	 * Core words as a warning what the spec makes an error with a clearer sentence: a socket with no baked frames, frames of the wrong length and
	 * an Armor window with no threshold. The rules of GSTN_HitRules.cpp report these, so Core's version is left out instead of listed twice.
	 */
	bool IsSupersededByHitRules(FName Code, const FString& Text)
	{
		return (Code == GSTNValidationCodes::BakedLength()) || (Code == GSTNValidationCodes::HurtState())
			|| (Code == GSTNValidationCodes::BakedSocketMissing() && Text.Contains(TEXT("has no baked frames yet")));
	}

	/** "Track 3 ..." -> 3; INDEX_NONE for a message that is not about a track */
	int32 ParseTrackIndex(const FString& Text)
	{
		static const FString Prefix(TEXT("Track "));
		if (!Text.StartsWith(Prefix) || Text.Len() <= Prefix.Len() || !FChar::IsDigit(Text[Prefix.Len()]))
		{
			return INDEX_NONE;
		}
		return FCString::Atoi(*Text + Prefix.Len());
	}

	FString CommandName(const FGSTN_InputSlotSpec& Spec)
	{
		return Spec.Command.IsValid() ? Spec.Command.ToString() : FString(TEXT("(no command)"));
	}

	/** The message of the table report without the context it was written in ("Kit: ", "With weapon 1: "), so two tables can be compared */
	FString StripTableContext(const FString& Text)
	{
		if (Text.StartsWith(TEXT("Kit: ")))
		{
			return Text.Mid(5);
		}
		if (Text.StartsWith(TEXT("With weapon ")))
		{
			int32 Colon = INDEX_NONE;
			if (Text.FindChar(TEXT(':'), Colon) && Colon + 2 <= Text.Len())
			{
				return Text.Mid(Colon + 2);
			}
		}
		return Text;
	}

	FString FindingKey(const FGSTN_ValidationFinding& Finding)
	{
		return Finding.Code.ToString() + TEXT("|") + StripTableContext(Finding.Message.ToString());
	}

	FName CodeOfConflict(EGSTN_SlotConflictKind Kind)
	{
		switch (Kind)
		{
		case EGSTN_SlotConflictKind::Ambiguous: return GSTNValidationCodes::SlotAmbiguous();
		case EGSTN_SlotConflictKind::Shadowed: return GSTNValidationCodes::SlotShadowed();
		case EGSTN_SlotConflictKind::DuplicateCommand: return GSTNValidationCodes::SlotDuplicateCommand();
		default: return GSTNValidationCodes::SlotInvalid();
		}
	}

	FText DescribeOrigin(const FGSTN_CompiledEntry& Entry, const FGSTN_ResolvedLoadout& Resolved)
	{
		switch (Entry.Source)
		{
		case EGSTN_EntrySource::Weapon:
		{
			const UGSTN_WeaponDefinition* Weapon = Resolved.Weapons.IsValidIndex(Entry.SourceIndex) ? Resolved.Weapons[Entry.SourceIndex] : nullptr;
			return FText::Format(LOCTEXT("OriginWeapon", "weapon {0} ({1})"), FText::AsNumber(Entry.SourceIndex), FText::FromString(Weapon ? Weapon->GetName() : FString(TEXT("not found"))));
		}
		case EGSTN_EntrySource::Super:
		{
			const UGSTN_SuperDefinition* Super = Resolved.Supers.IsValidIndex(Entry.SourceIndex) ? Resolved.Supers[Entry.SourceIndex] : nullptr;
			return FText::Format(LOCTEXT("OriginSuper", "super slot {0} ({1})"), FText::AsNumber(Entry.SourceIndex + 1), FText::FromString(Super ? Super->GetName() : FString(TEXT("empty"))));
		}
		default:
			return FText::GetEmpty();
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Timeline

FGSTN_TimelineRuleContext FGSTN_CombatValidators::MakeTimelineContext(const FGSTN_CombatTimelineData& Data, UObject* Asset, float TickRate, bool bLoadMontage)
{
	// Core never loads a montage, so the load happens here, before its context looks at the soft pointer
	if (bLoadMontage && !Data.Montage.IsNull())
	{
		Data.Montage.LoadSynchronous();
	}

	FGSTN_TimelineRuleContext Context;
	Context.Core = FGSTN_TimelineValidation::MakeContext(Data, TickRate, 0);
	Context.bMontageHasRootMotion = Data.Montage.Get() && Data.Montage.Get()->HasRootMotion();
	Context.Asset = Asset;

	// The bake is checked against the mesh its own record names: the asset alone knows no preview. The open editor replaces this with the status of
	// the mesh it shows
	if (bLoadMontage)
	{
		FGSTN_BakeInputs Inputs;
		Inputs.Montage = Data.Montage.Get();
		Inputs.TickRateHz = TickRate;
		if (Data.BakeInfo.IsSet())
		{
			Inputs.Mesh = Cast<USkeletalMesh>(Data.BakeInfo.Mesh.TryLoad());
		}
		Context.BakeStatus = GSTNBake::GetStatus(Data, Inputs);
	}
	return Context;
}

void FGSTN_CombatValidators::ValidateTimeline(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	CheckCoreRules(Data, Context, Report);
	CheckFrameRange(Data, Context, Report);
	CheckOverlapSameKind(Data, Context, Report);
	CheckRootMotionMix(Data, Context, Report);
	CheckCancelWindowTarget(Data, Context, Report);
	CheckTagsTrack(Data, Context, Report);
	CheckAimTurn(Data, Context, Report);
	CheckUnknownTracks(Data, Context, Report);
	CheckHitboxKeys(Data, Context, Report);
	CheckHitboxOverlap(Data, Context, Report);
	CheckHitData(Data, Context, Report);
	CheckVariants(Data, Context, Report);
	CheckHurtState(Data, Context, Report);
	CheckBakedSockets(Data, Context, Report);
	CheckBakeStatus(Data, Context, Report);
	CheckTrackEditors(Data, Context, Report);
}

bool FGSTN_CombatValidators::HasInlineContent(const FGSTN_CombatTimelineData& Data)
{
	return Data.TotalFrames > 0 || !Data.Tracks.IsEmpty() || !Data.Montage.IsNull();
}

bool FGSTN_CombatValidators::IsTimelineAssetResolvable(const TSoftObjectPtr<UGSTN_CombatTimeline>& Reference, bool bLoad)
{
	if (Reference.IsNull())
	{
		return false;
	}
	if (Reference.Get())
	{
		return true; // in memory, of the right class (the pointer is typed)
	}

	// Not loaded: the registry knows whether the asset exists without loading it
	const FAssetData Data = IAssetRegistry::GetChecked().GetAssetByObjectPath(Reference.ToSoftObjectPath());
	if (!Data.IsValid())
	{
		return false;
	}
	const UClass* AssetClass = Data.GetClass(EResolveClass::Yes);
	if (AssetClass == nullptr || !AssetClass->IsChildOf(UGSTN_CombatTimeline::StaticClass()))
	{
		return false;
	}
	return !bLoad || Reference.LoadSynchronous() != nullptr;
}

void FGSTN_CombatValidators::ValidateAbilityTimeline(const UGSTN_GA_BaseCombat& Ability, UObject* Asset, float TickRate, bool bLoadMontage, FGSTN_ValidationReport& Report)
{
	if (!Ability.TimelineAsset.IsNull())
	{
		if (!IsTimelineAssetResolvable(Ability.TimelineAsset, bLoadMontage))
		{
			Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::AbilityTimelineAssetUnresolved(),
				FText::Format(LOCTEXT("AbilityTimelineAssetUnresolved", "The ability names the Timeline Asset {0}, which does not exist or is not a combat timeline. The reference wins over the inline timeline, so this ability would play no timeline."),
					FText::FromString(Ability.TimelineAsset.ToSoftObjectPath().ToString())),
				Asset);
		}
		if (HasInlineContent(Ability.Timeline))
		{
			Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::AbilityInlineIgnored(),
				FText::Format(LOCTEXT("AbilityInlineIgnored", "The ability names the Timeline Asset {0}, which plays instead of the inline timeline; the inline timeline is not used."),
					FText::FromString(Ability.TimelineAsset.GetAssetName())),
				Asset);
		}
		return;
	}

	const FGSTN_TimelineRuleContext Context = MakeTimelineContext(Ability.Timeline, Asset, TickRate, bLoadMontage);
	ValidateTimeline(Ability.Timeline, Context, Report);
}

void FGSTN_CombatValidators::CheckCoreRules(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, Context.Core, Messages);
	for (const FGSTN_TimelineMessage& Message : Messages)
	{
		const FName Code = ClassifyCoreMessage(Message.Text);
		if (IsSupersededByHitRules(Code, Message.Text))
		{
			continue;
		}
		FGSTN_FindingLocation Where;
		Where.TrackIndex = ParseTrackIndex(Message.Text);
		if (const FGSTN_TimelineTrack* Track = GetTrack(Data, Where.TrackIndex))
		{
			Where.Frame = Track->StartFrame;
		}
		else
		{
			Where.TrackIndex = INDEX_NONE;
		}
		AddFinding(Report, Context, Message.bError ? EGSTN_Severity::Error : EGSTN_Severity::Warning, Code, FText::FromString(Message.Text), Where);
	}
}

void FGSTN_CombatValidators::CheckFrameRange(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	if (Data.TotalFrames < 1)
	{
		AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::FrameRange(),
			FText::Format(LOCTEXT("TotalFramesTooShort", "TotalFrames is {0}: a timeline lasts at least one frame."), FText::AsNumber(Data.TotalFrames)));
	}
	if (Data.FirstActionableFrame < 0)
	{
		AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::FrameRange(),
			FText::Format(LOCTEXT("FirstActionableNegative", "FirstActionableFrame is {0}: frames start at 0."), FText::AsNumber(Data.FirstActionableFrame)));
	}
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		if (Track && Track->StartFrame < 0)
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::FrameRange(),
				FText::Format(LOCTEXT("TrackStartNegative", "Track {0} starts at frame {1}: frames start at 0."), TrackName(Index), FText::AsNumber(Track->StartFrame)),
				FGSTN_FindingLocation::OnTrack(Index, Track->StartFrame));
		}
	}
}

void FGSTN_CombatValidators::CheckOverlapSameKind(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	const FGameplayTag GlobalLock = GSTNInputTags::Lock_Command.GetTag();

	for (int32 First = 0; First < Data.Tracks.Num(); ++First)
	{
		const FGSTN_TimelineTrack* A = GetTrack(Data, First);
		if (!A)
		{
			continue;
		}
		for (int32 Second = First + 1; Second < Data.Tracks.Num(); ++Second)
		{
			const FGSTN_TimelineTrack* B = GetTrack(Data, Second);
			if (!B || B->GetKind() != A->GetKind() || !RangesOverlap(*A, *B))
			{
				continue;
			}
			const int32 From = FMath::Max(A->StartFrame, B->StartFrame);
			const int32 To = FMath::Min(A->EndFrame, B->EndFrame);

			switch (A->GetKind())
			{
			case EGSTN_TimelineTrackKind::Movement:
				AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::OverlapSameKind(),
					FText::Format(LOCTEXT("MovementOverlap", "Movement tracks {0} and {1} overlap on frames {2} to {3}: the second would replace the layered move. Join them or move one."),
						TrackName(First), TrackName(Second), FText::AsNumber(From), FText::AsNumber(To - 1)),
					FGSTN_FindingLocation::OnTrack(Second, From));
				break;

			case EGSTN_TimelineTrackKind::HurtboxModifier:
			{
				const FGSTN_TimelineTrack_HurtboxModifier& ModA = *static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(A);
				const FGSTN_TimelineTrack_HurtboxModifier& ModB = *static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(B);
				if (ModA.Kind != ModB.Kind && SelectorsOverlap(ModA.Part, ModB.Part))
				{
					AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::OverlapSameKind(),
						FText::Format(LOCTEXT("HurtboxModifierOverlap", "Hurtbox modifier tracks {0} and {1} address the same hurtbox on frames {2} to {3} with different states: it cannot be invincible and intangible at once."),
							TrackName(First), TrackName(Second), FText::AsNumber(From), FText::AsNumber(To - 1)),
						FGSTN_FindingLocation::OnTrack(Second, From));
				}
				break;
			}

			case EGSTN_TimelineTrackKind::Tags:
			{
				// A window that owns the global lock already locks every command, so a narrower lock in an overlapping window adds nothing:
				// the designer most likely wanted one of them to be the only lock of that stretch. There is no unlock tag, so this is the
				// one way two lock sets can disagree about what the designer meant
				const FGSTN_TimelineTrack_Tags& TagsA = *static_cast<const FGSTN_TimelineTrack_Tags*>(A);
				const FGSTN_TimelineTrack_Tags& TagsB = *static_cast<const FGSTN_TimelineTrack_Tags*>(B);
				for (int32 Order = 0; Order < 2; ++Order)
				{
					const FGSTN_TimelineTrack_Tags& Global = Order == 0 ? TagsA : TagsB;
					const FGSTN_TimelineTrack_Tags& Narrow = Order == 0 ? TagsB : TagsA;
					if (!Global.Tags.HasTagExact(GlobalLock))
					{
						continue;
					}
					for (const FGameplayTag& Tag : Narrow.Tags)
					{
						if (Tag != GlobalLock && Tag.MatchesTag(GlobalLock))
						{
							AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::OverlapSameKind(),
								FText::Format(LOCTEXT("TagsLockOverlap", "Tags windows {0} and {1} overlap on frames {2} to {3}: window {0} owns the global lock, which already includes {4} of window {1}."),
									TrackName(Order == 0 ? First : Second), TrackName(Order == 0 ? Second : First), FText::AsNumber(From), FText::AsNumber(To - 1), FText::FromName(Tag.GetTagName())),
								FGSTN_FindingLocation::OnTrack(Second, From));
							break;
						}
					}
				}
				break;
			}

			default:
				// Hitboxes may overlap (multi-hit); the same-group, same-shape part of the rule is CheckHitboxOverlap
				break;
			}
		}
	}
}

void FGSTN_CombatValidators::CheckRootMotionMix(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	if (!Context.bMontageHasRootMotion)
	{
		return;
	}
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		if (Track && Track->GetKind() == EGSTN_TimelineTrackKind::Movement && static_cast<const FGSTN_TimelineTrack_Movement*>(Track)->MixMode == EGSTN_MoveMixMode::Override)
		{
			AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::RootMotionMix(),
				FText::Format(LOCTEXT("RootMotionMix", "The montage plays root motion and movement track {0} overrides the velocity: they fight for the same frames. Set the mix mode on purpose (Additive keeps both)."), TrackName(Index)),
				FGSTN_FindingLocation::OnTrack(Index, Track->StartFrame));
		}
	}
}

void FGSTN_CombatValidators::CheckCancelWindowTarget(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	bool bHasHitbox = false;
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		bHasHitbox |= Track && Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox;
	}

	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		if (!Track || Track->GetKind() != EGSTN_TimelineTrackKind::CancelWindow)
		{
			continue;
		}
		const FGSTN_TimelineTrack_CancelWindow& Window = *static_cast<const FGSTN_TimelineTrack_CancelWindow*>(Track);
		const FGSTN_FindingLocation Where = FGSTN_FindingLocation::OnTrack(Index, Track->StartFrame);

		if (Window.IntoCommands.IsEmpty())
		{
			AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::CancelWindowTarget(),
				FText::Format(LOCTEXT("CancelIntoNothing", "Cancel window {0} has an empty IntoCommands query: it cancels into nothing."), TrackName(Index)), Where);
		}
		else if (Context.KnownCommands)
		{
			bool bMatchesAny = false;
			for (const FGameplayTag& Command : *Context.KnownCommands)
			{
				bMatchesAny |= Window.IntoCommands.Matches(FGameplayTagContainer(Command));
			}
			if (!bMatchesAny)
			{
				AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::CancelWindowTarget(),
					FText::Format(LOCTEXT("CancelIntoNoCommand", "Cancel window {0} matches no command of the kits that use this ability."), TrackName(Index)), Where);
			}
		}

		if (!bHasHitbox && (Window.Condition == EGSTN_CancelCondition::OnHit || Window.Condition == EGSTN_CancelCondition::OnBlock))
		{
			AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::CancelWindowTarget(),
				FText::Format(LOCTEXT("CancelOnHitNoHitbox", "Cancel window {0} waits for a hit or a block, and the timeline has no hitbox that could cause one."), TrackName(Index)), Where);
		}
	}
}

void FGSTN_CombatValidators::CheckTagsTrack(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	const auto ReportTag = [&](const FGameplayTag& Tag, const FText& Where, int32 TrackIndex, int32 Frame)
	{
		AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::TagsTrack(),
			FText::Format(LOCTEXT("UnregisteredTag", "{0} names the tag {1}, which is not registered. Add it to the tag list or pick another."), Where, FText::FromName(Tag.GetTagName())),
			TrackIndex == INDEX_NONE ? FGSTN_FindingLocation() : FGSTN_FindingLocation::OnTrack(TrackIndex, Frame));
	};

	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		if (!Track)
		{
			continue;
		}
		if (Track->GetKind() == EGSTN_TimelineTrackKind::Tags)
		{
			for (const FGameplayTag& Tag : static_cast<const FGSTN_TimelineTrack_Tags*>(Track)->Tags)
			{
				if (IsUnregistered(Tag))
				{
					ReportTag(Tag, FText::Format(LOCTEXT("TagsWindowWhere", "Tags window {0}"), TrackName(Index)), Index, Track->StartFrame);
				}
			}
		}
		else if (Track->GetKind() == EGSTN_TimelineTrackKind::Event)
		{
			const FGSTN_TimelineTrack_Event& Event = *static_cast<const FGSTN_TimelineTrack_Event*>(Track);
			if (IsUnregistered(Event.CueTag))
			{
				ReportTag(Event.CueTag, FText::Format(LOCTEXT("EventCueWhere", "The cue of event {0}"), TrackName(Index)), Index, Track->StartFrame);
			}
			if (IsUnregistered(Event.GameplayEventTag))
			{
				ReportTag(Event.GameplayEventTag, FText::Format(LOCTEXT("EventTagWhere", "The gameplay event of event {0}"), TrackName(Index)), Index, Track->StartFrame);
			}
		}
	}
	if (Data.bHasRicochetResponse && IsUnregistered(Data.RicochetResponse.CueTag))
	{
		ReportTag(Data.RicochetResponse.CueTag, LOCTEXT("RicochetCueWhere", "The ricochet cue"), INDEX_NONE, INDEX_NONE);
	}
}

void FGSTN_CombatValidators::CheckAimTurn(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	if (Data.AimPolicy == EGSTN_AimPolicy::Camera && Data.TurnUntilFrame > Data.TotalFrames)
	{
		AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::AimTurn(),
			FText::Format(LOCTEXT("TurnUntilPastEnd", "TurnUntilFrame ({0}) is after the end of the timeline ({1} frames)."), FText::AsNumber(Data.TurnUntilFrame), FText::AsNumber(Data.TotalFrames)));
	}
	if (Data.AimPolicy != EGSTN_AimPolicy::KeepFacing && Data.MaxTurnDegrees <= 0.f)
	{
		AddFinding(Report, Context, EGSTN_Severity::Error, GSTNValidationCodes::AimTurn(),
			LOCTEXT("MaxTurnNotPositive", "The aim policy turns the character but MaxTurnDegrees is 0 or less, so it can never turn. Use KeepFacing for a move that does not turn."));
	}
}

void FGSTN_CombatValidators::CheckUnknownTracks(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const UScriptStruct* Struct = Data.Tracks[Index].GetScriptStruct();
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		if (Struct && Track && !Registry.Find(Struct).IsValid())
		{
			AddFinding(Report, Context, EGSTN_Severity::Warning, GSTNValidationCodes::UnknownTrack(),
				FText::Format(LOCTEXT("UnknownTrack", "Track {0} is a {1}, which has no track editor: it is shown as a generic row and its fields are edited in the details panel."),
					TrackName(Index), Struct->GetDisplayNameText()),
				FGSTN_FindingLocation::OnTrack(Index, Track->StartFrame));
		}
	}
}

void FGSTN_CombatValidators::CheckTrackEditors(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context, FGSTN_ValidationReport& Report)
{
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const UScriptStruct* Struct = Data.Tracks[Index].GetScriptStruct();
		const FGSTN_TimelineTrack* Track = GetTrack(Data, Index);
		const TSharedPtr<const IGSTN_TrackEditor> Editor = Struct ? Registry.Find(Struct) : nullptr;
		if (!Editor.IsValid() || !Track)
		{
			continue;
		}

		const int32 Before = Report.Num();
		Editor->Validate(*Track, Data, Report);
		for (int32 Added = Before; Added < Report.Num(); ++Added)
		{
			FGSTN_ValidationFinding& Finding = Report.Findings[Added];
			if (Finding.Where.TrackIndex == INDEX_NONE)
			{
				Finding.Where.TrackIndex = Index;
				if (Finding.Where.Frame == INDEX_NONE)
				{
					Finding.Where.Frame = Track->StartFrame;
				}
			}
			if (!Finding.Asset.IsValid())
			{
				Finding.Asset = Context.Asset;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Kit, weapon, loadout

void FGSTN_CombatValidators::CheckKitData(const UGSTN_PrimaryKitDefinition& Kit, FGSTN_ValidationReport& Report)
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	Kit.Validate(Errors, Warnings);
	UObject* Asset = const_cast<UGSTN_PrimaryKitDefinition*>(&Kit);
	for (const FText& Error : Errors)
	{
		Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::KitData(), Error, Asset);
	}
	for (const FText& Warning : Warnings)
	{
		Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::KitData(), Warning, Asset);
	}
}

void FGSTN_CombatValidators::CheckWeaponData(const UGSTN_WeaponDefinition& Weapon, FGSTN_ValidationReport& Report)
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	Weapon.Validate(Errors, Warnings);
	UObject* Asset = const_cast<UGSTN_WeaponDefinition*>(&Weapon);
	for (const FText& Error : Errors)
	{
		Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::WeaponData(), Error, Asset);
	}
	for (const FText& Warning : Warnings)
	{
		Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::WeaponData(), Warning, Asset);
	}
}

void FGSTN_CombatValidators::CheckKit(const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_InputSettings& Settings, FGSTN_ValidationReport& Report)
{
	CheckKitData(Kit, Report);

	// The kit alone: its command set and its own bindings
	FGSTN_ResolvedLoadout Resolved;
	Resolved.Kit = &Kit;
	Resolved.Loadout.PrimaryKit = Kit.GetPrimaryAssetId();

	FCommandTableOptions Options;
	Options.Asset = const_cast<UGSTN_PrimaryKitDefinition*>(&Kit);
	CheckCommandTable(Resolved, Settings, Options, Report);
}

bool FGSTN_CombatValidators::KitCanCarryWeapon(const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_WeaponDefinition& Weapon)
{
	if (Kit.PrimaryWeapon.ToSoftObjectPath() == FSoftObjectPath(&Weapon))
	{
		return true;
	}
	// An empty query allows none
	return !Kit.AllowedAdditionalWeapons.IsEmpty() && Kit.AllowedAdditionalWeapons.Matches(Weapon.WeaponTags);
}

void FGSTN_CombatValidators::CheckWeaponOnKit(const UGSTN_WeaponDefinition& Weapon, const UGSTN_PrimaryKitDefinition& Kit, const UGSTN_InputSettings& Settings, FGSTN_ValidationReport& Report)
{
	FGSTN_ResolvedLoadout KitAlone;
	KitAlone.Kit = &Kit;
	KitAlone.Loadout.PrimaryKit = Kit.GetPrimaryAssetId();

	FGSTN_ResolvedLoadout WithWeapon = KitAlone;
	WithWeapon.Weapons.Add(&Weapon);

	FCommandTableOptions Options;
	Options.Asset = const_cast<UGSTN_WeaponDefinition*>(&Weapon);
	Options.Prefix = FText::Format(LOCTEXT("OnKitPrefix", "On kit {0}: "), FText::FromString(Kit.GetName()));
	Options.Baseline = &KitAlone;
	CheckCommandTable(WithWeapon, Settings, Options, Report);
}

void FGSTN_CombatValidators::GatherButtonTags(TConstArrayView<const FGSTN_InputSlotSpec*> Specs, TArray<FGameplayTag>& OutButtonTags)
{
	FGameplayTagContainer Referenced;
	for (const FGSTN_InputSlotSpec* Spec : Specs)
	{
		if (!Spec)
		{
			continue;
		}
		for (const FGSTN_InputStep& Step : Spec->Pattern.Steps)
		{
			for (const TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				if (const FGSTN_InputCondition* Pointer = Condition.GetPtr())
				{
					Pointer->GetReferencedButtons(Referenced);
				}
			}
		}
	}
	OutButtonTags = Referenced.GetGameplayTagArray();
	OutButtonTags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.GetTagName().Compare(B.GetTagName()) < 0; });
	if (OutButtonTags.Num() > 32)
	{
		OutButtonTags.SetNum(32);
	}
}

FString FGSTN_CombatValidators::DescribeSlotInput(const FGSTN_InputSlotSpec& Spec, TConstArrayView<FGameplayTag> ButtonTags, const UGSTN_InputSettings& Settings)
{
	if (!Spec.Pattern.IsStructurallyValid())
	{
		return FString();
	}

	// The same canonical input the slot validator plays: the shortest input that triggers the pattern, played through the real classifier
	constexpr int32 StartFrame = 20;
	const FGSTN_ButtonMap Buttons(ButtonTags);
	FGSTN_InputScript Script{ TArray<FGameplayTag>(ButtonTags) };
	int32 Frame = StartFrame;
	Spec.Pattern.AppendCanonicalInput(Script, Buttons, Frame);

	FGSTN_InputHistory History;
	if (!Script.Advance(History, Settings, Frame))
	{
		return FString();
	}

	// Frames count from the first event, so the text reads the same wherever the pattern was played
	TArray<FString> Tokens;
	FGSTN_InputScript::Describe(History, 32).ParseIntoArray(Tokens, TEXT(" "));
	int32 FirstFrame = INDEX_NONE;
	FString Result;
	for (const FString& Token : Tokens)
	{
		int32 Colon = INDEX_NONE;
		if (!Token.FindChar(TEXT(':'), Colon) || Colon == 0)
		{
			continue;
		}
		const FString FrameText = Token.Left(Colon);
		const bool bKnownFrame = FrameText.IsNumeric();
		if (bKnownFrame && FirstFrame == INDEX_NONE)
		{
			FirstFrame = FCString::Atoi(*FrameText);
		}
		Result += Result.IsEmpty() ? TEXT("") : TEXT(" ");
		Result += bKnownFrame ? FString::FromInt(FCString::Atoi(*FrameText) - FirstFrame) + Token.Mid(Colon) : Token;
	}
	return Result;
}

void FGSTN_CombatValidators::CheckCommandTable(const FGSTN_ResolvedLoadout& Resolved, const UGSTN_InputSettings& Settings, const FCommandTableOptions& Options, FGSTN_ValidationReport& Report)
{
	// What the baseline already reports is not this loadout's finding
	TSet<FString> BaselineKeys;
	if (Options.Baseline)
	{
		FGSTN_ValidationReport BaselineReport;
		CheckCommandTable(*Options.Baseline, Settings, FCommandTableOptions(), BaselineReport);
		for (const FGSTN_ValidationFinding& Finding : BaselineReport.Findings)
		{
			BaselineKeys.Add(FindingKey(Finding));
		}
	}

	// The compile and the conflict check the game itself runs
	FGSTN_CompileInput Input;
	FGSTN_LoadoutAssembly::BuildCompileInput(Resolved, 0, Input);
	FGSTN_CompileReport CompileReport;
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, Settings, &CompileReport);
	FGSTN_BindingTableCompiler::FindConflicts(Table, Settings, CompileReport);

	FGSTN_ValidationReport Local;
	TSet<FString> SlotMessages;
	TSet<FString> SeenSlotFindings;

	// The slot findings again, per weapon context like the compiler's check, to know WHICH slots they are about: the engine's message
	// names the commands, and the pattern and the source of each slot are what a designer needs to fix it
	const int32 Contexts = FMath::Max(Table.WeaponCount, 1);
	for (int32 Weapon = 0; Weapon < Contexts; ++Weapon)
	{
		TArray<FGSTN_InputSlotSpec> Effective;
		TArray<const FGSTN_CompiledEntry*> Origin;
		TSet<int32> Pooled;
		for (const FGSTN_CompiledEntry& Entry : Table.Entries)
		{
			if ((Entry.Source != EGSTN_EntrySource::Weapon || Entry.SourceIndex == Weapon) && !Pooled.Contains(Entry.SlotIndex))
			{
				Pooled.Add(Entry.SlotIndex);
				Effective.Add(Table.MakeEffectiveSlot(Entry));
				Origin.Add(&Entry);
			}
		}
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Effective)
		{
			Pointers.Add(&Spec);
		}

		for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(Pointers, Settings))
		{
			SlotMessages.Add(Conflict.Message);

			const bool bHasA = Pointers.IsValidIndex(Conflict.SlotA);
			const bool bHasB = Pointers.IsValidIndex(Conflict.SlotB);
			const FString NameA = bHasA ? CommandName(*Pointers[Conflict.SlotA]) : FString();
			const FString NameB = bHasB ? CommandName(*Pointers[Conflict.SlotB]) : FString();
			if (SeenSlotFindings.Contains(FString::Printf(TEXT("%d|%s|%s"), static_cast<int32>(Conflict.Kind), *NameA, *NameB)))
			{
				continue;
			}
			SeenSlotFindings.Add(FString::Printf(TEXT("%d|%s|%s"), static_cast<int32>(Conflict.Kind), *NameA, *NameB));

			FText Message = FText::FromString(Conflict.Message);
			FText Detail;
			if (bHasA || bHasB)
			{
				TArray<const FGSTN_InputSlotSpec*> Pair;
				if (bHasA) { Pair.Add(Pointers[Conflict.SlotA]); }
				if (bHasB) { Pair.Add(Pointers[Conflict.SlotB]); }
				TArray<FGameplayTag> ButtonTags;
				GatherButtonTags(Pair, ButtonTags);

				FString Legend;
				for (int32 Button = 0; Button < ButtonTags.Num(); ++Button)
				{
					Legend += FString::Printf(TEXT("%s%c=%s"), Legend.IsEmpty() ? TEXT("") : TEXT(", "), static_cast<TCHAR>(TEXT('A') + Button), *ButtonTags[Button].ToString());
				}
				FString Lines;
				if (bHasA)
				{
					Lines += FString::Printf(TEXT("\n  %s: %s"), *NameA, *DescribeSlotInput(*Pointers[Conflict.SlotA], ButtonTags, Settings));
				}
				if (bHasB)
				{
					Lines += FString::Printf(TEXT("\n  %s: %s"), *NameB, *DescribeSlotInput(*Pointers[Conflict.SlotB], ButtonTags, Settings));
				}
				Detail = FText::Format(LOCTEXT("SlotPatternsDetail", "The input that triggers each slot, in the notation of M1 section 4 (frames counted from the first event; buttons: {0}):{1}"),
					FText::FromString(Legend), FText::FromString(Lines));

				// Which slot comes from a weapon or a super: that is what a preset has to change
				FText Sources;
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 SlotIndex = Side == 0 ? Conflict.SlotA : Conflict.SlotB;
					if (!Origin.IsValidIndex(SlotIndex))
					{
						continue;
					}
					const FText From = DescribeOrigin(*Origin[SlotIndex], Resolved);
					if (!From.IsEmpty())
					{
						const FText Note = FText::Format(LOCTEXT("SlotComesFrom", "{0} comes from {1}"), FText::FromString(Side == 0 ? NameA : NameB), From);
						Sources = Sources.IsEmpty() ? Note : FText::Format(LOCTEXT("SlotSourcesJoined", "{0}; {1}"), Sources, Note);
					}
				}
				if (!Sources.IsEmpty())
				{
					Message = FText::Format(LOCTEXT("SlotMessageWithSources", "{0} ({1})"), Message, Sources);
				}
			}

			FGSTN_ValidationFinding& Finding = Local.Add(Conflict.IsError() ? EGSTN_Severity::Error : EGSTN_Severity::Warning, CodeOfConflict(Conflict.Kind), Message, Options.Asset);
			Finding.Detail = Detail;
		}
	}

	// Everything else the compile and the check found (a binding to a removed command, two bindings that apply together...) as Core words it
	const auto PassOn = [&](const TArray<FText>& Texts, EGSTN_Severity Severity)
	{
		for (const FText& Text : Texts)
		{
			const FString String = Text.ToString();
			if (Options.OutHandledTexts)
			{
				Options.OutHandledTexts->Add(String);
			}
			if (!SlotMessages.Contains(String))
			{
				Local.Add(Severity, GSTNValidationCodes::LoadoutTable(), Text, Options.Asset);
			}
		}
	};
	PassOn(CompileReport.Errors, EGSTN_Severity::Error);
	PassOn(CompileReport.Warnings, EGSTN_Severity::Warning);

	for (FGSTN_ValidationFinding& Finding : Local.Findings)
	{
		if (BaselineKeys.Contains(FindingKey(Finding)))
		{
			continue;
		}
		if (!Options.Prefix.IsEmpty())
		{
			Finding.Message = FText::Format(LOCTEXT("PrefixedMessage", "{0}{1}"), Options.Prefix, Finding.Message);
		}
		Report.Findings.Add(MoveTemp(Finding));
	}
}

void FGSTN_CombatValidators::CheckLoadout(const FGSTN_ResolvedLoadout& Resolved, const UGSTN_InputSettings& Settings, const UGSTN_LoadoutRules& Rules, UObject* Asset, FGSTN_ValidationReport& Report)
{
	TSet<FString> Handled;
	FCommandTableOptions Options;
	Options.Asset = Asset;
	Options.OutHandledTexts = &Handled;
	CheckCommandTable(Resolved, Settings, Options, Report);

	// The rules include the table's problems as their last rule; the ones reported above are not reported twice
	FGSTN_CompiledBindingTable Table;
	FGSTN_LoadoutRuleResult Result;
	FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, 0, Settings, Rules, Table, Result);
	for (const FText& Error : Result.Errors)
	{
		if (!Handled.Contains(Error.ToString()))
		{
			Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::LoadoutRule(), Error, Asset);
		}
	}
	for (const FText& Warning : Result.Warnings)
	{
		if (!Handled.Contains(Warning.ToString()))
		{
			Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::LoadoutRule(), Warning, Asset);
		}
	}
}

void FGSTN_CombatValidators::ResolveLoadoutData(const FGSTN_LoadoutData& Data, FGSTN_ResolvedLoadout& OutResolved, TArray<TStrongObjectPtr<UObject>>& OutHold)
{
	// The same resolution the loadout component does at run time (its method is private to the component)
	OutResolved = FGSTN_ResolvedLoadout();
	OutResolved.Loadout = Data;

	const auto Resolve = [&OutHold](const FPrimaryAssetId& Id) -> UObject*
	{
		if (!Id.IsValid() || !UAssetManager::IsInitialized())
		{
			return nullptr;
		}
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		UObject* Object = Path.IsValid() ? Path.TryLoad() : nullptr;
		if (Object)
		{
			OutHold.Emplace(Object);
		}
		return Object;
	};
	const auto Hold = [&OutHold](UObject* Object) -> UObject*
	{
		if (Object)
		{
			OutHold.Emplace(Object);
		}
		return Object;
	};

	const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(Resolve(Data.PrimaryKit));
	OutResolved.Kit = Kit;
	OutResolved.Weapons.Add(Kit ? Cast<UGSTN_WeaponDefinition>(Hold(Kit->PrimaryWeapon.LoadSynchronous())) : nullptr);
	for (const FPrimaryAssetId& WeaponId : Data.AdditionalWeapons)
	{
		OutResolved.Weapons.Add(Cast<UGSTN_WeaponDefinition>(Resolve(WeaponId)));
	}
	for (const FPrimaryAssetId& SuperId : Data.Supers)
	{
		OutResolved.Supers.Add(Cast<UGSTN_SuperDefinition>(Resolve(SuperId)));
	}
	OutResolved.SuperScheme = Kit ? Cast<UGSTN_SuperInputScheme>(Hold(Kit->SuperInputScheme.LoadSynchronous())) : nullptr;
	OutResolved.Skin = Cast<UGSTN_SkinDefinition>(Resolve(Data.Skin));
}

#undef LOCTEXT_NAMESPACE
