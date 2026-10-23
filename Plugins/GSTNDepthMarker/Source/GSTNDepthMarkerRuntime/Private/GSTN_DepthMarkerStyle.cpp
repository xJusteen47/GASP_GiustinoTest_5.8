#include "GSTN_DepthMarkerStyle.h"

#include "GSTN_DepthMarkerPresenter.h"

void UGSTN_DepthMarkerStyle::PopulateDefaultPresenters()
{
	Presenters.Reset();
	const TArray<EGSTN_DepthMarkerState> BelowStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };

	UGSTN_DepthPresenter_Ring* GroundRing = NewObject<UGSTN_DepthPresenter_Ring>(this);
	GroundRing->ActiveStates = BelowStates;
	Presenters.Add(GroundRing);

	UGSTN_DepthPresenter_Line* GroundLine = NewObject<UGSTN_DepthPresenter_Line>(this);
	GroundLine->ActiveStates = BelowStates;
	Presenters.Add(GroundLine);

	// The above layer is lighter so two markers do not compete (§8)
	UGSTN_DepthPresenter_Ring* AboveRing = NewObject<UGSTN_DepthPresenter_Ring>(this);
	AboveRing->Layer = EGSTN_DepthLayer::Above;
	AboveRing->ActiveStates = { EGSTN_DepthMarkerState::AboveReference };
	AboveRing->AlphaScale = 0.6f;
	Presenters.Add(AboveRing);

	UGSTN_DepthPresenter_Line* AboveLine = NewObject<UGSTN_DepthPresenter_Line>(this);
	AboveLine->Layer = EGSTN_DepthLayer::Above;
	AboveLine->ActiveStates = { EGSTN_DepthMarkerState::AboveReference };
	AboveLine->AlphaScale = 0.6f;
	Presenters.Add(AboveLine);
}

void UGSTN_DepthMarkerStyle::PopulateApproachPresenters()
{
	Presenters.Reset();
	DefaultTint = FLinearColor(0.35f, 0.85f, 1.f, 1.f);

	UGSTN_DepthPresenter_Ring* Ring = NewObject<UGSTN_DepthPresenter_Ring>(this);
	Ring->Layer = EGSTN_DepthLayer::Approach;
	Ring->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Presenters.Add(Ring);

	UGSTN_DepthPresenter_Line* Line = NewObject<UGSTN_DepthPresenter_Line>(this);
	Line->Layer = EGSTN_DepthLayer::Approach;
	Line->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Presenters.Add(Line);
}

bool UGSTN_DepthMarkerStyle::Validate(TArray<FText>& OutErrors) const
{
	const int32 Before = OutErrors.Num();
	for (int32 Index = 0; Index < Presenters.Num(); ++Index)
	{
		if (!Presenters[Index])
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Presenter %d is empty"), Index)));
		}
	}
	if (Fade.HideHeight > Fade.ShowHeight)
	{
		OutErrors.Add(FText::FromString(TEXT("Fade: HideHeight above ShowHeight leaves no hysteresis (the marker would flicker)")));
	}
	if (Fade.FadeEndHeight < 1.f)
	{
		OutErrors.Add(FText::FromString(TEXT("Fade: FadeEndHeight must be at least 1 cm")));
	}
	return OutErrors.Num() == Before;
}
