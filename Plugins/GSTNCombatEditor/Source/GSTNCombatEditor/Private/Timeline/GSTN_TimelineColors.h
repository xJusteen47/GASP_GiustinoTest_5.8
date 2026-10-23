// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Timeline/GSTN_PhaseBarData.h"

/**
 * The colours of the bars the timeline paints and the text on them, as plain functions with no Slate in them so a test can check
 * them. The rule they share: a bar is a dark, saturated fill with a bright outline of the track colour, and the text on it is
 * whichever of white and black contrasts more with that fill (WCAG contrast ratio on the relative luminance). Bright pastel bars with
 * a fixed text colour were unreadable in the dark editor theme (UI test 2026-10-23), and a selection must never be shown by
 * lightening the fill: it gets an outline instead.
 */
namespace GSTNTimelineColors
{
	/** Fraction of the track colour the bar fill keeps. Dark enough that white text reads on every colour the settings ship with */
	constexpr float BarFillScale = 0.4f;

	/** Relative luminance of a linear colour (Rec. 709 weights), 0 black to 1 white */
	inline float RelativeLuminance(const FLinearColor& Color)
	{
		return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
	}

	/** WCAG contrast ratio of two colours, 1 (identical) to 21 (black on white) */
	inline float ContrastRatio(const FLinearColor& A, const FLinearColor& B)
	{
		const float LA = RelativeLuminance(A);
		const float LB = RelativeLuminance(B);
		return (FMath::Max(LA, LB) + 0.05f) / (FMath::Min(LA, LB) + 0.05f);
	}

	/**
	 * White or black, whichever contrasts more with the background. Against pure black and pure white the better of the two is at
	 * least 4.5:1 for every background, which is the readable-text threshold, so no colour needs a third choice.
	 */
	inline FLinearColor ReadableTextOn(const FLinearColor& Background)
	{
		const FLinearColor White(1.f, 1.f, 1.f, 1.f);
		const FLinearColor Black(0.f, 0.f, 0.f, 1.f);
		return ContrastRatio(Background, White) >= ContrastRatio(Background, Black) ? White : Black;
	}

	/** The fill of a window bar of a track colour: the same hue, darkened, opaque */
	inline FLinearColor BarFill(const FLinearColor& TrackColor)
	{
		return FLinearColor(TrackColor.R * BarFillScale, TrackColor.G * BarFillScale, TrackColor.B * BarFillScale, 1.f);
	}

	/** The outline of a window bar: the track colour as it is set, so the bar still reads as that track */
	inline FLinearColor BarOutline(const FLinearColor& TrackColor)
	{
		return FLinearColor(TrackColor.R, TrackColor.G, TrackColor.B, 1.f);
	}

	/** Fill of a phase of the phase bar. Startup is blue, active red, recovery orange, actionable green: hues far apart, all dark */
	inline FLinearColor PhaseFill(EGSTN_PhaseKind Kind)
	{
		switch (Kind)
		{
		case EGSTN_PhaseKind::Startup:
			return FLinearColor(0.03f, 0.17f, 0.55f, 1.f);
		case EGSTN_PhaseKind::Active:
			return FLinearColor(0.55f, 0.04f, 0.04f, 1.f);
		case EGSTN_PhaseKind::Recovery:
			return FLinearColor(0.55f, 0.30f, 0.02f, 1.f);
		case EGSTN_PhaseKind::Actionable:
			return FLinearColor(0.04f, 0.33f, 0.10f, 1.f);
		default:
			return FLinearColor(0.20f, 0.20f, 0.22f, 1.f);
		}
	}

	/** The name of a phase, for the label of a segment that is wide enough to carry it */
	inline const TCHAR* PhaseName(EGSTN_PhaseKind Kind)
	{
		switch (Kind)
		{
		case EGSTN_PhaseKind::Startup: return TEXT("Startup");
		case EGSTN_PhaseKind::Active: return TEXT("Active");
		case EGSTN_PhaseKind::Recovery: return TEXT("Recovery");
		case EGSTN_PhaseKind::Actionable: return TEXT("Actionable");
		default: return TEXT("Busy");
		}
	}

	/** The name cell of a selected track row: a dark blue, so its text is the white ReadableTextOn picks and a selected row is not a pale one */
	inline FLinearColor SelectedHeaderFill()
	{
		return FLinearColor(0.02f, 0.13f, 0.42f, 1.f);
	}
}
