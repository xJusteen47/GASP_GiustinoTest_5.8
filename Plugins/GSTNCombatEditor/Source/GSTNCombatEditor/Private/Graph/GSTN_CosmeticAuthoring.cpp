// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Graph/GSTN_CosmeticAuthoring.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Timeline/GSTN_TimelineTypes.h"

int32 UGSTN_CosmeticAuthoring::SetHitCueTag(TSubclassOf<UGSTN_GA_BaseCombat> Move, FGameplayTag CueTag)
{
	UGSTN_GA_BaseCombat* Default = *Move ? Move->GetDefaultObject<UGSTN_GA_BaseCombat>() : nullptr;
	if (!Default || !Default->TimelineAsset.IsNull())
	{
		return -1;
	}
	int32 Changed = 0;
	Default->Modify();
	for (TInstancedStruct<FGSTN_TimelineTrack>& Track : Default->Timeline.Tracks)
	{
		FGSTN_TimelineTrack_Hitbox* Hitbox = Track.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>();
		if (!Hitbox)
		{
			continue;
		}
		for (FGSTN_HitboxSpec& Spec : Hitbox->Hitboxes)
		{
			if (Spec.Data.CueTag != CueTag)
			{
				Spec.Data.CueTag = CueTag;
				++Changed;
			}
		}
	}
	if (Changed > 0)
	{
		if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Move))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
	return Changed;
}

bool UGSTN_CosmeticAuthoring::GetActiveFrames(TSubclassOf<UGSTN_GA_BaseCombat> Move, int32& OutFirst, int32& OutLast)
{
	OutFirst = OutLast = INDEX_NONE;
	const UGSTN_GA_BaseCombat* Default = *Move ? Move->GetDefaultObject<UGSTN_GA_BaseCombat>() : nullptr;
	if (!Default)
	{
		return false;
	}
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Default->GetTimelineData().Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Hitbox = Track.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (!Hitbox || Hitbox->Hitboxes.IsEmpty())
		{
			continue;
		}
		// The track is open over [StartFrame, EndFrame)
		const int32 Last = FMath::Max(Hitbox->EndFrame - 1, Hitbox->StartFrame);
		OutFirst = OutFirst == INDEX_NONE ? Hitbox->StartFrame : FMath::Min(OutFirst, Hitbox->StartFrame);
		OutLast = OutLast == INDEX_NONE ? Last : FMath::Max(OutLast, Last);
	}
	return OutFirst != INDEX_NONE;
}
