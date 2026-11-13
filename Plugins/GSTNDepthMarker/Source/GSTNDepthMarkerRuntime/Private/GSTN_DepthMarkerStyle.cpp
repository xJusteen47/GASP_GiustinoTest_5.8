#include "GSTN_DepthMarkerStyle.h"

#include "GSTN_DepthMarkerPresenter.h"
#include "GSTN_DepthMarkerToy.h"

void UGSTN_DepthMarkerStyle::PopulateDefaultPresenters()
{
	Presenters.Reset();
	// The default look: a ring in the colour of the player on the target and a dashed line whose dashes grow with the drop
	bUsePlayerPalette = true;
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

	// Optional grid quads: an empty PlaneMaterial keeps them off until a designer fills it
	UGSTN_DepthPresenter_Plane* GroundPlane = NewObject<UGSTN_DepthPresenter_Plane>(this);
	GroundPlane->ActiveStates = BelowStates;
	Presenters.Add(GroundPlane);

	UGSTN_DepthPresenter_Plane* AbovePlane = NewObject<UGSTN_DepthPresenter_Plane>(this);
	AbovePlane->Layer = EGSTN_DepthLayer::Above;
	AbovePlane->ActiveStates = { EGSTN_DepthMarkerState::AboveReference };
	AbovePlane->AlphaScale = 0.6f;
	Presenters.Add(AbovePlane);
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

	UGSTN_DepthPresenter_Plane* Plane = NewObject<UGSTN_DepthPresenter_Plane>(this);
	Plane->Layer = EGSTN_DepthLayer::Approach;
	Plane->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Presenters.Add(Plane);
}

void UGSTN_DepthMarkerStyle::PopulateToyPresenters()
{
	Presenters.Reset();
	bUsePlayerPalette = true;
	Toy.bEnabled = true;
	const TArray<EGSTN_DepthMarkerState> BelowStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };

	// The landing spot first: it is the marker, the beam only points at it
	UGSTN_DepthPresenter_ToyDecal* Decal = NewObject<UGSTN_DepthPresenter_ToyDecal>(this);
	Presenters.Add(Decal);

	UGSTN_DepthPresenter_Cone* Beam = NewObject<UGSTN_DepthPresenter_Cone>(this);
	Beam->ActiveStates = BelowStates;
	Presenters.Add(Beam);

	UGSTN_DepthPresenter_FogPuff* Puff = NewObject<UGSTN_DepthPresenter_FogPuff>(this);
	Puff->ActiveStates = { EGSTN_DepthMarkerState::NoGround };
	Presenters.Add(Puff);

	UGSTN_DepthPresenter_Cone* AboveBeam = NewObject<UGSTN_DepthPresenter_Cone>(this);
	AboveBeam->Layer = EGSTN_DepthLayer::Above;
	AboveBeam->ActiveStates = { EGSTN_DepthMarkerState::AboveReference };
	AboveBeam->AlphaScale = 0.6f;
	Presenters.Add(AboveBeam);

	UGSTN_DepthPresenter_FogPuff* AbovePuff = NewObject<UGSTN_DepthPresenter_FogPuff>(this);
	AbovePuff->Layer = EGSTN_DepthLayer::Above;
	AbovePuff->ActiveStates = { EGSTN_DepthMarkerState::AboveReference };
	AbovePuff->AlphaScale = 0.6f;
	Presenters.Add(AbovePuff);
}

void UGSTN_DepthMarkerStyle::PopulateToyApproachPresenters()
{
	Presenters.Reset();
	bUsePlayerPalette = true;
	Toy.bEnabled = true;

	UGSTN_DepthPresenter_Cone* Beam = NewObject<UGSTN_DepthPresenter_Cone>(this);
	Beam->Layer = EGSTN_DepthLayer::Approach;
	Beam->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Presenters.Add(Beam);

	UGSTN_DepthPresenter_FogPuff* Puff = NewObject<UGSTN_DepthPresenter_FogPuff>(this);
	Puff->Layer = EGSTN_DepthLayer::Approach;
	Puff->ActiveStates = { EGSTN_DepthMarkerState::Approach };
	Presenters.Add(Puff);
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
