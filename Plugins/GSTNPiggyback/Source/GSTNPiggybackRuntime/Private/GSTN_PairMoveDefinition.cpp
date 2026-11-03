// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_PairMoveDefinition.h"

#include "Data/GSTN_LinkOutcomeTrack.h"
#include "GSTN_PiggybackTags.h"
#include "Misc/DataValidation.h"
#include "Timeline/GSTN_TimelineTypes.h"

#define LOCTEXT_NAMESPACE "GSTNPairMove"

const FPrimaryAssetType UGSTN_PairMoveDefinition::PrimaryAssetType(TEXT("GSTN_PairMove"));

FGameplayTag UGSTN_PairMoveDefinition::GetInitiatorRoleTag() const
{
	return InitiatorRole == EGSTN_PairRole::Carrier ? GSTNPiggybackTags::Role_Carrier.GetTag() : GSTNPiggybackTags::Role_Rider.GetTag();
}

FGameplayTag UGSTN_PairMoveDefinition::GetPartnerRoleTag() const
{
	return InitiatorRole == EGSTN_PairRole::Carrier ? GSTNPiggybackTags::Role_Rider.GetTag() : GSTNPiggybackTags::Role_Carrier.GetTag();
}

int32 UGSTN_PairMoveDefinition::FindPartnerMoveKeyFrame(const FGSTN_CombatTimelineData& Timeline, int32 Id)
{
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Timeline.Tracks)
	{
		if (const FGSTN_LinkOutcomeTrack* Outcomes = Track.GetPtr<FGSTN_LinkOutcomeTrack>())
		{
			for (const FGSTN_LinkOutcomeKey& Key : Outcomes->Keys)
			{
				if (Key.Outcome == EGSTN_LinkOutcome::PartnerMove && Key.PartnerMoveId == Id)
				{
					return Outcomes->StartFrame + Key.Frame;
				}
			}
		}
	}
	return INDEX_NONE;
}

void UGSTN_PairMoveDefinition::Validate(TArray<FString>& OutErrors) const
{
	const FGSTN_CombatTimelineData* InitiatorTimeline = InitiatorAbility ? &InitiatorAbility->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData() : nullptr;
	const FGSTN_CombatTimelineData* PartnerTimeline = PartnerAbility ? &PartnerAbility->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData() : nullptr;
	ValidateWithTimelines(InitiatorTimeline, PartnerTimeline, OutErrors);

	// The initiator's base names its definition: it must be this one
	if (const UGSTN_GA_PairMove* PairCDO = InitiatorAbility ? Cast<UGSTN_GA_PairMove>(InitiatorAbility->GetDefaultObject()) : nullptr)
	{
		if (PairCDO->PairMove && PairCDO->PairMove != this)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the initiator ability %s names another pair move (%s)"), *GetName(), *InitiatorAbility->GetName(), *PairCDO->PairMove->GetName()));
		}
	}
}

void UGSTN_PairMoveDefinition::ValidateWithTimelines(const FGSTN_CombatTimelineData* InitiatorTimeline, const FGSTN_CombatTimelineData* PartnerTimeline, TArray<FString>& OutErrors) const
{
	const FString Name = GetName();
	if (PairMoveId < 1 || PairMoveId > 32767)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: PairMoveId %d outside 1..32767"), *Name, PairMoveId));
	}
	if (!PairTag.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: no pair tag"), *Name));
	}
	if (Outcome == EGSTN_LinkActionOutcome::SwapRoles)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: SwapRoles is reserved (not built)"), *Name));
	}
	if (Outcome == EGSTN_LinkActionOutcome::Break && OutcomeFrame == INDEX_NONE)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: a Break outcome needs an OutcomeFrame"), *Name));
	}

	// Roles: each side's requirements must fit the role it plays
	const FGameplayTag InitiatorRoleTag = GetInitiatorRoleTag();
	const FGameplayTag PartnerRoleTag = GetPartnerRoleTag();
	if (InitiatorRequirements.RequireTags.HasTagExact(PartnerRoleTag) || InitiatorRequirements.IgnoreTags.HasTagExact(InitiatorRoleTag))
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the initiator's requirements exclude its own role (%s)"), *Name, *InitiatorRoleTag.ToString()));
	}
	if (PartnerRequirements.RequireTags.HasTagExact(InitiatorRoleTag) || PartnerRequirements.IgnoreTags.HasTagExact(PartnerRoleTag))
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the partner's requirements exclude its own role (%s)"), *Name, *PartnerRoleTag.ToString()));
	}

	if (!PartnerTimeline)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: no partner ability"), *Name));
	}
	else if (PartnerTimeline->TotalFrames <= 0)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the partner ability's timeline is empty"), *Name));
	}

	if (!InitiatorTimeline)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: no initiator ability"), *Name));
		return;
	}
	const int32 Total = InitiatorTimeline->TotalFrames;
	const int32 KeyFrame = FindPartnerMoveKeyFrame(*InitiatorTimeline, PairMoveId);
	if (KeyFrame == INDEX_NONE)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the initiator timeline has no PartnerMove key naming id %d"), *Name, PairMoveId));
	}
	else
	{
		const int32 PartnerStart = KeyFrame + FMath::Max(PartnerStartOffsetFrames, 1);
		if (PartnerStart >= Total)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the partner starts at initiator frame %d, past the initiator's %d frames"), *Name, PartnerStart, Total));
		}
	}
	if (OutcomeFrame != INDEX_NONE)
	{
		if (OutcomeFrame < 0 || OutcomeFrame >= Total)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: OutcomeFrame %d outside the initiator timeline (0..%d)"), *Name, OutcomeFrame, Total - 1));
		}
		else if (KeyFrame != INDEX_NONE && OutcomeFrame < KeyFrame)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: OutcomeFrame %d before the PartnerMove key (%d)"), *Name, OutcomeFrame, KeyFrame));
		}
	}
}

void UGSTN_PairMoveDefinition::MakeLinkActionInfo(const UGSTN_LinkExitPlacer* Placer, FGSTN_LinkActionInfo& OutInfo) const
{
	OutInfo = FGSTN_LinkActionInfo();
	OutInfo.PartnerAbility = PartnerAbility.Get();
	OutInfo.PartnerRequirements = PartnerRequirements;
	OutInfo.PartnerStartOffsetFrames = FMath::Max(PartnerStartOffsetFrames, 1);
	OutInfo.OutcomeFrame = OutcomeFrame;
	OutInfo.Outcome = Outcome;
	OutInfo.BreakReason = GSTNPiggybackTags::BreakReason_Outcome.GetTag();
	OutInfo.ExitPlacer = Placer;
	// An interruptible partner's part is an ordinary forced timeline: its commands stay free and a hit replaces it
	OutInfo.bPartnerLocksCommands = PartnerMode == EGSTN_PairPartnerMode::Cooperative && bPartnerLocksCommands;
	OutInfo.MaxDurationFrames = MaxDurationFrames;
}

FPrimaryAssetId UGSTN_PairMoveDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_PairMoveDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FString> Errors;
	Validate(Errors);
	for (const FString& Error : Errors)
	{
		Context.AddError(FText::FromString(Error));
	}
	return Errors.Num() > 0 ? EDataValidationResult::Invalid : CombineDataValidationResults(Result, EDataValidationResult::Valid);
}

EDataValidationResult UGSTN_PairMoveSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bErrors = false;
	TMap<int32, const UGSTN_PairMoveDefinition*> ById;
	for (const UGSTN_PairMoveDefinition* Definition : PairMoves)
	{
		if (!Definition)
		{
			Context.AddError(FText::Format(LOCTEXT("NullPairMove", "{0}: an empty pair-move entry"), FText::FromString(GetName())));
			bErrors = true;
			continue;
		}
		if (const UGSTN_PairMoveDefinition* const* Other = ById.Find(Definition->PairMoveId); Other && *Other != Definition)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s: %s and %s share id %d"), *GetName(), *(*Other)->GetName(), *Definition->GetName(), Definition->PairMoveId)));
			bErrors = true;
		}
		ById.Add(Definition->PairMoveId, Definition);
		TArray<FString> Errors;
		Definition->Validate(Errors);
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		bErrors |= Errors.Num() > 0;
	}
	return bErrors ? EDataValidationResult::Invalid : CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif

#undef LOCTEXT_NAMESPACE
