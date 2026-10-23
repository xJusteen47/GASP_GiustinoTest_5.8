// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Display/GSTN_InputDisplayMetrics.h"

float FGSTN_InputDisplayMetrics::GetStripHeight(int32 NumButtons)
{
	return MarkerRowHeight + StickRowHeight + FacingRowHeight + FMath::Max(NumButtons, 0) * ButtonRowHeight + GapRowHeight;
}

int32 FGSTN_InputDisplayMetrics::GetVisibleColumns(float Width, int32 MaxColumns)
{
	const int32 Fit = FMath::FloorToInt32((Width - GutterWidth) / MinColumnWidth);
	return FMath::Clamp(Fit, 1, FMath::Max(MaxColumns, 1));
}

float FGSTN_InputDisplayMetrics::GetColumnWidth(float Width, int32 VisibleColumns)
{
	return FMath::Max((Width - GutterWidth) / FMath::Max(VisibleColumns, 1), MinColumnWidth);
}

float FGSTN_InputDisplayMetrics::GetColumnX(float Width, float ColumnWidth, int32 NumColumns, int32 Column)
{
	return Width - (NumColumns - Column) * ColumnWidth;
}

int32 FGSTN_InputDisplayMetrics::GetFirstVisibleColumn(int32 NumColumns, int32 VisibleColumns)
{
	return FMath::Max(NumColumns - FMath::Max(VisibleColumns, 1), 0);
}

FVector2f FGSTN_InputDisplayMetrics::GetArrowDirection(EGSTN_StickDirection Direction)
{
	constexpr float D = 0.70710678f;
	switch (Direction)
	{
	case EGSTN_StickDirection::Up:        return FVector2f(0.f, -1.f);
	case EGSTN_StickDirection::UpRight:   return FVector2f(D, -D);
	case EGSTN_StickDirection::Right:     return FVector2f(1.f, 0.f);
	case EGSTN_StickDirection::DownRight: return FVector2f(D, D);
	case EGSTN_StickDirection::Down:      return FVector2f(0.f, 1.f);
	case EGSTN_StickDirection::DownLeft:  return FVector2f(-D, D);
	case EGSTN_StickDirection::Left:      return FVector2f(-1.f, 0.f);
	case EGSTN_StickDirection::UpLeft:    return FVector2f(-D, -D);
	default:                              return FVector2f(0.f, 0.f);
	}
}

TArray<FGSTN_InputGapLabel> FGSTN_InputDisplayMetrics::ChooseGapLabels(const FGSTN_InputDisplayLayout& Layout, int32 FirstColumn, float Width, float ColumnWidth, float CharWidth)
{
	TArray<FGSTN_InputGapLabel> Labels;
	float PreviousRight = -FLT_MAX;
	for (int32 Index = 1; Index < Layout.Events.Num(); ++Index)
	{
		const FGSTN_InputDisplayEvent& From = Layout.Events[Index - 1];
		const FGSTN_InputDisplayEvent& To = Layout.Events[Index];
		const int32 Frames = To.Frame - From.Frame;
		if (Frames <= 1 || From.Column < FirstColumn || To.Column < FirstColumn)
		{
			continue;
		}

		FGSTN_InputGapLabel Label;
		Label.FromColumn = From.Column;
		Label.ToColumn = To.Column;
		Label.Frames = Frames;
		Label.Text = FString::FromInt(Frames);
		Label.Width = Label.Text.Len() * CharWidth;

		// Centred between the two event columns, kept inside the columns of the strip
		const float FromCentre = GetColumnX(Width, ColumnWidth, Layout.NumColumns, From.Column) + ColumnWidth * 0.5f;
		const float ToCentre = GetColumnX(Width, ColumnWidth, Layout.NumColumns, To.Column) + ColumnWidth * 0.5f;
		const float Left = (FromCentre + ToCentre) * 0.5f - Label.Width * 0.5f;
		Label.X = FMath::Clamp(Left, GutterWidth, FMath::Max(Width - Label.Width, GutterWidth));

		if (Label.X < PreviousRight + GapLabelSpacing)
		{
			continue;
		}
		PreviousRight = Label.X + Label.Width;
		Labels.Add(MoveTemp(Label));
	}
	return Labels;
}

FLinearColor FGSTN_InputDisplayMetrics::GetButtonColor(int32 Button)
{
	static const FLinearColor Palette[6] =
	{
		FLinearColor(0.80f, 0.10f, 0.10f), // A red
		FLinearColor(0.10f, 0.32f, 0.90f), // B blue
		FLinearColor(0.12f, 0.62f, 0.18f), // C green
		FLinearColor(0.90f, 0.68f, 0.05f), // D yellow
		FLinearColor(0.55f, 0.18f, 0.85f), // E purple
		FLinearColor(0.90f, 0.36f, 0.06f), // F orange
	};
	const int32 Index = FMath::Max(Button, 0);
	const FLinearColor Base = Palette[Index % 6];
	return Index < 6 ? Base : Base * 0.6f + FLinearColor(0.f, 0.f, 0.f, 0.4f);
}

FLinearColor FGSTN_InputDisplayMetrics::GetButtonTextColor(int32 Button)
{
	const FLinearColor Colour = GetButtonColor(Button);
	const float Luminance = 0.2126f * Colour.R + 0.7152f * Colour.G + 0.0722f * Colour.B;
	return Luminance > 0.25f ? FLinearColor::Black : FLinearColor::White;
}
