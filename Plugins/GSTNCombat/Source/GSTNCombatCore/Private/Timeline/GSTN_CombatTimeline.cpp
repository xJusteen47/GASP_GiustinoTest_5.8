#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineValidation.h"
#include "Timeline/GSTN_Landing.h"
#include "Timeline/GSTN_Ledge.h"

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
		const FGSTN_TimelineTrack_Hitbox* HitboxTrack = GSTNTimelineTracks::AsAttackHitbox(Track);
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

int32 FGSTN_CombatTimelineData::GetActiveEndFrame() const
{
	int32 End = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Tracks)
	{
		if (const FGSTN_TimelineTrack_Hitbox* HitboxTrack = GSTNTimelineTracks::AsAttackHitbox(Track); HitboxTrack && !HitboxTrack->Hitboxes.IsEmpty())
		{
			End = FMath::Max(End, HitboxTrack->EndFrame);
		}
	}
	return End;
}

const FGSTN_RicochetResponse* FGSTN_CombatTimelineData::GetRicochetResponse(int32 TrackIndex) const
{
	if (Tracks.IsValidIndex(TrackIndex))
	{
		if (const FGSTN_TimelineTrack_Landing* Landing = Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Landing>())
		{
			return Landing->Action == EGSTN_LandingAction::LandingLag && Landing->Lag.IsAuthored() ? &Landing->Lag : nullptr;
		}
		if (const FGSTN_TimelineTrack_LedgeCheck* Ledge = Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_LedgeCheck>())
		{
			return Ledge->Action == EGSTN_LedgeAction::Recoil && Ledge->Response.IsAuthored() ? &Ledge->Response : nullptr;
		}
	}
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(TrackIndex);
	if (Track && Track->bHasRicochetResponse && Track->RicochetResponse.IsAuthored())
	{
		return &Track->RicochetResponse;
	}
	return GetRicochetResponse();
}

FGSTN_StrikeSettings FGSTN_CombatTimelineData::ResolveStrike(int32 TrackIndex) const
{
	FGSTN_StrikeSettings Settings;
	const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(TrackIndex);
	if (Track && Track->bOverrideStrike)
	{
		Settings.Kind = Track->StrikeKind;
		Settings.Behaviour = FGSTN_StrikeSettings::ResolveBehaviour(Track->StrikeKind, Track->HardSurfaceBehaviour);
		Settings.Deflection = &Track->SweepDeflection;
		return Settings;
	}
	Settings.Kind = StrikeKind;
	Settings.Behaviour = ResolveHardSurfaceBehaviour();
	Settings.Deflection = &SweepDeflection;
	return Settings;
}
