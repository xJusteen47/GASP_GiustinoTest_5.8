#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_EDITOR
#include "Input/GSTN_CombatSettings.h"
#include "Misc/DataValidation.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatRulesValidation.h"
#endif

FPrimaryAssetId UGSTN_CombatTimeline::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("GSTN_CombatTimeline")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CombatTimeline::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// The simulation tick rate is a project setting of the NP layer, which the Core cannot see: the montage check assumes
	// the default 60 Hz tick here and the details panel validation of the ability does the exact one
	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidation::MakeContext(Data, 60.f), Messages);
	for (const FGSTN_TimelineMessage& Message : Messages)
	{
		if (Message.bError)
		{
			Context.AddError(FText::FromString(Message.Text));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Message.Text));
		}
	}

	// What each hitbox does to the standard victims under the default rules, one column per hitbox (hit data matrix)
	TArray<FGSTN_ReactionMatrixHit> Columns;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* HitboxTrack = Track.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		for (int32 Index = 0; HitboxTrack != nullptr && Index < HitboxTrack->Hitboxes.Num(); ++Index)
		{
			FGSTN_ReactionMatrixHit& Column = Columns.AddDefaulted_GetRef();
			Column.Name = FString::Printf(TEXT("Hitbox %d"), Columns.Num() - 1);
			Column.Hit = HitboxTrack->Hitboxes[Index].Data;
		}
	}
	if (!Columns.IsEmpty())
	{
		if (const UGSTN_CombatRules* Rules = UGSTN_CombatSettings::Get().ResolveDefaultRules())
		{
			TArray<FGSTN_ReactionMatrixVictim> Victims;
			for (const FGSTN_ReactionMatrixHit& Standard : FGSTN_CombatRulesValidation::MakeStandardMatrixHits())
			{
				FGSTN_ReactionMatrixVictim& Victim = Victims.AddDefaulted_GetRef();
				Victim.Name = Standard.Name;
				Victim.Weight = Standard.Name == TEXT("Light") ? 60.f : (Standard.Name == TEXT("Medium") ? 100.f : 160.f);
			}
			Context.AddMessage(EMessageSeverity::Info, FText::FromString(TEXT("Hit matrix against the default rules:\n") + FGSTN_CombatRulesValidation::PrintReactionMatrix(*Rules, Victims, Columns)));
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
