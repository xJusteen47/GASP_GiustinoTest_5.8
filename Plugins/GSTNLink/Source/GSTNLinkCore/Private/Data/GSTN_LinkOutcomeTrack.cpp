// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_LinkOutcomeTrack.h"

#include "GSTN_LinkTags.h"

const FGSTN_LinkOutcomeKey* FGSTN_LinkOutcomeTrack::FindKeyAt(int32 FrameFromStart) const
{
	for (const FGSTN_LinkOutcomeKey& Key : Keys)
	{
		if (Key.Frame == FrameFromStart)
		{
			return &Key;
		}
	}
	return nullptr;
}

void FGSTN_LinkOutcomeTrack::Validate(const FString& Label, TArray<FString>& OutErrors) const
{
	if (EndFrame <= StartFrame)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the track [%d, %d) is empty or inverted."), *Label, StartFrame, EndFrame));
	}
	if (Keys.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the track has no outcome key, so the move can never release or throw."), *Label));
	}

	const int32 Length = EndFrame - StartFrame;
	int32 PreviousFrame = INDEX_NONE;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const FGSTN_LinkOutcomeKey& Key = Keys[KeyIndex];
		if (Key.Frame < 0 || Key.Frame >= Length)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: key %d is at frame %d, outside the track's %d frames."), *Label, KeyIndex, Key.Frame, Length));
		}
		if (Key.Frame <= PreviousFrame)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: key %d at frame %d is not after the previous key (keys are sorted, one per frame)."), *Label, KeyIndex, Key.Frame));
		}
		PreviousFrame = Key.Frame;

		if (Key.Outcome == EGSTN_LinkOutcome::Throw && Key.ThrowHit.HitType != GSTNLinkTags::HitType_Throw.GetTag())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: key %d throws with a hit whose type is not Combat.HitType.Throw, so the throw would not skip parry and shield."), *Label, KeyIndex));
		}
	}
}
