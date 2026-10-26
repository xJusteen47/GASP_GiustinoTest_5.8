// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Display/GSTN_InputDisplayMetrics.h"

float FGSTN_InputDisplayMetrics::GetStripHeight(int32 NumButtons)
{
	return MarkerRowHeight + StickRowHeight + FacingRowHeight + FMath::Max(NumButtons, 0) * ButtonRowHeight + GapRowHeight;
}

FGSTN_InputDisplayMetrics::FRowHeights FGSTN_InputDisplayMetrics::FitRowHeights(float Height, int32 NumButtons)
{
	FRowHeights Full;
	if (Height >= Full.GetTotal(NumButtons))
	{
		return Full;
	}
	FRowHeights Min;
	Min.Marker = MinMarkerRowHeight;
	Min.Stick = MinStickRowHeight;
	Min.Facing = MinFacingRowHeight;
	Min.Button = MinButtonRowHeight;
	Min.Gap = MinGapRowHeight;
	const float FullTotal = Full.GetTotal(NumButtons);
	const float MinTotal = Min.GetTotal(NumButtons);
	if (Height < MinTotal)
	{
		Min.Facing = 0.f;
		return Min;
	}
	const float T = (Height - MinTotal) / FMath::Max(FullTotal - MinTotal, 1.f);
	FRowHeights Fit;
	Fit.Marker = FMath::Lerp(Min.Marker, Full.Marker, T);
	Fit.Stick = FMath::Lerp(Min.Stick, Full.Stick, T);
	Fit.Facing = FMath::Lerp(Min.Facing, Full.Facing, T);
	Fit.Button = FMath::Lerp(Min.Button, Full.Button, T);
	Fit.Gap = FMath::Lerp(Min.Gap, Full.Gap, T);
	return Fit;
}

int32 FGSTN_InputDisplayMetrics::GetVisibleColumns(float Width, int32 MaxColumns)
{
	const int32 Fit = FMath::FloorToInt32((Width - GutterWidth - RightMargin) / MinColumnWidth);
	return FMath::Clamp(Fit, 1, FMath::Max(MaxColumns, 1));
}

float FGSTN_InputDisplayMetrics::GetColumnWidth(float Width, int32 VisibleColumns)
{
	return FMath::Max((Width - GutterWidth - RightMargin) / FMath::Max(VisibleColumns, 1), MinColumnWidth);
}

float FGSTN_InputDisplayMetrics::GetColumnX(float Width, float ColumnWidth, int32 NumColumns, int32 Column)
{
	return Width - RightMargin - (NumColumns - Column) * ColumnWidth;
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
	return ChooseGapLabels(Layout, FirstColumn, Width, [&](int32 Column) { return GetColumnX(Width, ColumnWidth, Layout.NumColumns, Column) + ColumnWidth * 0.5f; }, CharWidth);
}

TArray<FGSTN_InputGapLabel> FGSTN_InputDisplayMetrics::ChooseGapLabels(const FGSTN_InputDisplayLayout& Layout, int32 FirstColumn, float Width, TFunctionRef<float(int32)> CentreOf, float CharWidth)
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
		const float FromCentre = CentreOf(From.Column);
		const float ToCentre = CentreOf(To.Column);
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

bool FGSTN_InputDisplayMetrics::IsIdleColumn(const FGSTN_InputDisplayLayout& Layout, int32 Column)
{
	if (!Layout.Stick.IsValidIndex(Column) || Layout.Stick[Column].bEvent || (Layout.ColumnMarker.IsValidIndex(Column) && Layout.ColumnMarker[Column] != INDEX_NONE))
	{
		return false;
	}
	for (const TArray<FGSTN_InputDisplayCell>& Row : Layout.Buttons)
	{
		if (Row.IsValidIndex(Column) && Row[Column].Kind != EGSTN_InputCellKind::None && Row[Column].Kind != EGSTN_InputCellKind::Hold)
		{
			return false;
		}
	}
	return true;
}

TArray<FGSTN_InputDisplaySlot> FGSTN_InputDisplayMetrics::ComputeSlots(const FGSTN_InputDisplayLayout& Layout, float Width, int32 MaxFrames, bool bCompress)
{
	TArray<FGSTN_InputDisplaySlot> Slots;
	if (Layout.NumColumns <= 0)
	{
		return Slots;
	}
	if (!bCompress)
	{
		const int32 Visible = GetVisibleColumns(Width, MaxFrames);
		const float ColumnWidth = GetColumnWidth(Width, Visible);
		for (int32 Column = GetFirstVisibleColumn(Layout.NumColumns, Visible); Column < Layout.NumColumns; ++Column)
		{
			FGSTN_InputDisplaySlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.FirstColumn = Column;
			Slot.X = GetColumnX(Width, ColumnWidth, Layout.NumColumns, Column);
			Slot.Width = ColumnWidth;
		}
		return Slots;
	}

	// Every column to a slot, idle runs merged
	TArray<FGSTN_InputDisplaySlot> All;
	for (int32 Column = 0; Column < Layout.NumColumns;)
	{
		int32 End = Column;
		while (End < Layout.NumColumns && IsIdleColumn(Layout, End))
		{
			++End;
		}
		FGSTN_InputDisplaySlot& Slot = All.AddDefaulted_GetRef();
		Slot.FirstColumn = Column;
		if (End - Column >= MinCompressedRun)
		{
			Slot.NumColumns = End - Column;
			Column = End;
		}
		else
		{
			Column += 1;
		}
	}

	// The newest slots that fit, then the width left over shared out
	const float Available = Width - GutterWidth - RightMargin;
	int32 Frames = 0;
	float Total = 0.f;
	int32 First = All.Num();
	bool bCut = false;
	while (First > 0)
	{
		const FGSTN_InputDisplaySlot& Candidate = All[First - 1];
		const float Needed = Candidate.IsCompressed() ? CompressedColumnWidth : MinColumnWidth;
		if (First < All.Num() && (Total + Needed > Available || Frames + Candidate.NumColumns > FMath::Max(MaxFrames, 1)))
		{
			bCut = true;
			break;
		}
		Total += Needed;
		Frames += Candidate.NumColumns;
		--First;
	}
	const float Scale = bCut ? FMath::Clamp(Available / FMath::Max(Total, 1.f), 1.f, 2.f) : 1.f;
	float Right = Width - RightMargin;
	for (int32 Index = All.Num() - 1; Index >= First; --Index)
	{
		FGSTN_InputDisplaySlot Slot = All[Index];
		Slot.Width = (Slot.IsCompressed() ? CompressedColumnWidth : MinColumnWidth) * Scale;
		Slot.X = Right - Slot.Width;
		Right = Slot.X;
		Slots.Insert(Slot, 0);
	}
	return Slots;
}

FGSTN_InputPressIconStyle FGSTN_InputDisplayMetrics::GetPressIconStyle(int32 Button, bool bConsumed)
{
	const FLinearColor Colour = GetButtonColor(Button);
	FGSTN_InputPressIconStyle Style;
	if (!bConsumed)
	{
		Style.Fill = Colour;
		Style.Outline = FLinearColor(0.f, 0.f, 0.f, 0.f);
		Style.Letter = GetButtonTextColor(Button);
		return Style;
	}
	Style.Fill = FLinearColor(Colour.R * 0.3f, Colour.G * 0.3f, Colour.B * 0.3f, 1.f);
	Style.Outline = FLinearColor(Colour.R, Colour.G, Colour.B, 1.f);
	// A brightened copy of the colour: reads on the dark fill for every hue of the palette
	Style.Letter = FLinearColor(FMath::Min(Colour.R * 0.3f + 0.7f, 1.f), FMath::Min(Colour.G * 0.3f + 0.7f, 1.f), FMath::Min(Colour.B * 0.3f + 0.7f, 1.f), 1.f);
	return Style;
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
