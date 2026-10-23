// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Widgets/SGSTN_InputDisplay.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Display/GSTN_InputDisplayMetrics.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Input/GSTN_InputSettings.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Sources/GSTN_LocalInputSource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNInputDebug"

namespace
{
	using FM = FGSTN_InputDisplayMetrics;

	/** Width of one digit of the small font, for the frame-gap labels */
	constexpr float GapCharWidth = 6.f;

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FSlateBrush* IconBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 3.f);
		return &Brush;
	}

	FSlateFontInfo SmallFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", 8);
	}

	FSlateFontInfo LetterFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", 9);
	}

	FVector2f MeasureText(const FString& Text, const FSlateFontInfo& Font)
	{
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			return FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
		}
		return FVector2f(Text.Len() * 6.f, 11.f);
	}

	void PaintRect(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FVector2f& Size, const FLinearColor& Color, bool bRounded = false)
	{
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), bRounded ? IconBrush() : WhiteBrush(), ESlateDrawEffect::None, Color);
		}
	}

	void PaintText(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FString& Text, const FLinearColor& Color, const FSlateFontInfo& Font)
	{
		FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(200.f, 14.f), FSlateLayoutTransform(Position)), Text, Font, ESlateDrawEffect::None, Color);
	}

	/** Text with its centre at a point */
	void PaintTextCentred(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, const FString& Text, const FLinearColor& Color, const FSlateFontInfo& Font)
	{
		const FVector2f Size = MeasureText(Text, Font);
		PaintText(Elements, Layer, Geometry, Centre - Size * 0.5f, Text, Color, Font);
	}

	void PaintLine(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** An arrow centred on a point: shaft and head, HalfLength from the centre to the tip. A dot for neutral */
	void PaintArrow(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre, EGSTN_StickDirection Direction, float HalfLength, float Thickness, const FLinearColor& Color)
	{
		const FVector2f Dir = FM::GetArrowDirection(Direction);
		if (Dir.IsNearlyZero())
		{
			const float Dot = HalfLength * 0.5f;
			PaintRect(Elements, Layer, Geometry, Centre - FVector2f(Dot, Dot) * 0.5f, FVector2f(Dot, Dot), Color, true);
			return;
		}
		const FVector2f Perp(-Dir.Y, Dir.X);
		const FVector2f Tip = Centre + Dir * HalfLength;
		const FVector2f Tail = Centre - Dir * HalfLength;
		const float HeadLength = HalfLength * 0.62f;
		const FVector2f HeadBase = Tip - Dir * HeadLength;
		PaintLine(Elements, Layer, Geometry, { Tail, Tip }, Color, Thickness);
		PaintLine(Elements, Layer, Geometry, { HeadBase + Perp * HeadLength * 0.75f, Tip, HeadBase - Perp * HeadLength * 0.75f }, Color, Thickness);
	}

	FLinearColor StickArrowColor(const FGSTN_InputDisplayStickCell& Cell)
	{
		if (Cell.bFlick)
		{
			return FLinearColor(1.f, 0.85f, 0.2f);
		}
		switch (Cell.Zone)
		{
		case EGSTN_StickZone::Tilt: return FLinearColor(0.85f, 0.92f, 1.f);
		case EGSTN_StickZone::Aim:  return FLinearColor(0.45f, 0.58f, 0.85f);
		default:                    return FLinearColor(0.32f, 0.32f, 0.36f);
		}
	}

	FLinearColor Dim(const FLinearColor& Colour, float Scale)
	{
		return FLinearColor(Colour.R * Scale, Colour.G * Scale, Colour.B * Scale, Colour.A);
	}

	/** Y of the top of each row, from the row heights fitted to the strip's height */
	struct FRowTops
	{
		FM::FRowHeights H;
		float Stick;
		float Facing;
		float Buttons;
		float Gaps;
		float Bottom;

		FRowTops(int32 NumButtons, float Height)
			: H(FM::FitRowHeights(Height, NumButtons))
		{
			Stick = H.Marker;
			Facing = Stick + H.Stick;
			Buttons = Facing + H.Facing;
			Gaps = Buttons + NumButtons * H.Button;
			Bottom = Gaps + H.Gap;
		}
	};
}

// ---- SGSTN_InputStrip

void SGSTN_InputStrip::Construct(const FArguments& InArgs)
{
	ButtonNames = InArgs._ButtonNames;
	NumFrames = FMath::Clamp(InArgs._NumFrames, 1, FGSTN_InputHistory::MaxEventAge + 1);
}

void SGSTN_InputStrip::SetLayout(const FGSTN_InputDisplayLayout& InLayout)
{
	Layout = InLayout;
	Message = FText::GetEmpty();
	Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
}

void SGSTN_InputStrip::SetMessage(const FText& InMessage)
{
	if (Layout.NumColumns == 0 && Message.EqualTo(InMessage))
	{
		return;
	}
	Layout = FGSTN_InputDisplayLayout();
	Message = InMessage;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SGSTN_InputStrip::SetButtonNames(const TArray<FString>& InNames)
{
	if (ButtonNames != InNames)
	{
		ButtonNames = InNames;
		Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	}
}

FVector2D SGSTN_InputStrip::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// Room for the gutter and twenty columns; the widget takes all the width it is given
	return FVector2D(FM::GutterWidth + 20.f * FM::MinColumnWidth, FM::GetStripHeight(ButtonNames.Num()));
}

int32 SGSTN_InputStrip::GetFirstVisibleColumn() const
{
	const float Width = GetCachedGeometry().GetLocalSize().X;
	return FM::GetFirstVisibleColumn(Layout.NumColumns, FM::GetVisibleColumns(Width, NumFrames));
}

int32 SGSTN_InputStrip::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const int32 NumButtons = ButtonNames.Num();
	const FRowTops Rows(NumButtons, Size.Y);
	const FLinearColor TextColor(0.85f, 0.85f, 0.85f);
	const FLinearColor DimColor(0.55f, 0.55f, 0.58f);
	const FLinearColor Background(0.06f, 0.06f, 0.065f);

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	PaintRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size, Background);

	// Row names in the gutter: the letter of a button in its colour, so the row and its icons read as one
	for (int32 Button = 0; Button < NumButtons; ++Button)
	{
		const float Centre = Rows.Buttons + (Button + 0.5f) * Rows.H.Button;
		PaintTextCentred(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(FM::GutterWidth * 0.5f, Centre), FString::Chr(FGSTN_InputDisplayLayout::GetButtonLetter(Button)), FM::GetButtonColor(Button), LetterFont());
	}
	PaintTextCentred(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(FM::GutterWidth * 0.5f, Rows.Stick + Rows.H.Stick * 0.5f), TEXT("STK"), DimColor, SmallFont());
	if (Rows.H.Facing > 0.f)
	{
		PaintTextCentred(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(FM::GutterWidth * 0.5f, Rows.Facing + Rows.H.Facing * 0.5f), TEXT("rel"), Dim(DimColor, 0.8f), SmallFont());
	}

	if (Layout.NumColumns == 0)
	{
		PaintText(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(FM::GutterWidth + 10.f, Rows.Stick + 4.f), Message.ToString(), DimColor, SmallFont());
		OutDrawElements.PopClip();
		return LayerId + 2;
	}

	const int32 Visible = FM::GetVisibleColumns(Size.X, NumFrames);
	const float ColumnWidth = FM::GetColumnWidth(Size.X, Visible);
	const int32 FirstColumn = FM::GetFirstVisibleColumn(Layout.NumColumns, Visible);
	const float IconSize = FMath::Min3(FM::IconSize, ColumnWidth - 2.f, Rows.H.Button - 2.f);

	// Row separators, faint, so the eye follows a row across the strip
	for (int32 Button = 0; Button <= NumButtons; ++Button)
	{
		PaintRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f(FM::GutterWidth, Rows.Buttons + Button * Rows.H.Button), FVector2f(Size.X - FM::GutterWidth, 1.f), FLinearColor(1.f, 1.f, 1.f, 0.05f));
	}

	for (int32 Column = FirstColumn; Column < Layout.NumColumns; ++Column)
	{
		const float X = FM::GetColumnX(Size.X, ColumnWidth, Layout.NumColumns, Column);
		const float CentreX = X + ColumnWidth * 0.5f;

		// A faint line at every tenth frame, so gaps can be counted
		if ((Layout.FirstFrame + Column) % 10 == 0)
		{
			PaintRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f(X, Rows.Stick), FVector2f(1.f, Rows.Gaps - Rows.Stick), FLinearColor(1.f, 1.f, 1.f, 0.08f));
		}

		// A command came out on this frame
		if (Layout.ColumnMarker[Column] != INDEX_NONE)
		{
			PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(X + 2.f, 1.f), FVector2f(ColumnWidth - 4.f, Rows.H.Marker - 3.f), FLinearColor(1.f, 0.62f, 0.1f), true);
		}

		// The stick: an arrow, a dot for neutral, the relative direction as a small arrow below when it differs
		const FGSTN_InputDisplayStickCell& Stick = Layout.Stick[Column];
		const FVector2f StickCentre(CentreX, Rows.Stick + Rows.H.Stick * 0.5f);
		const bool bTilted = Stick.Zone != EGSTN_StickZone::Neutral;
		PaintArrow(OutDrawElements, LayerId + 2, AllottedGeometry, StickCentre, bTilted ? Stick.StickDirection : EGSTN_StickDirection::Neutral, 8.5f, 2.f, StickArrowColor(Stick));
		if (bTilted && Rows.H.Facing > 0.f && Stick.FacingDirection != Stick.StickDirection)
		{
			PaintArrow(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2f(CentreX, Rows.Facing + Rows.H.Facing * 0.5f), Stick.FacingDirection, 3.8f, 1.f, Dim(StickArrowColor(Stick), 0.7f));
		}

		// The buttons
		for (int32 Button = 0; Button < NumButtons; ++Button)
		{
			const FGSTN_InputDisplayCell& Cell = Layout.Buttons[Button][Column];
			if (Cell.Kind == EGSTN_InputCellKind::None)
			{
				continue;
			}
			const float RowCentre = Rows.Buttons + (Button + 0.5f) * Rows.H.Button;
			const FLinearColor Colour = FM::GetButtonColor(Button);
			const FVector2f IconPosition(CentreX - IconSize * 0.5f, RowCentre - IconSize * 0.5f);
			const FString Letter = FString::Chr(FGSTN_InputDisplayLayout::GetButtonLetter(Button));

			switch (Cell.Kind)
			{
			case EGSTN_InputCellKind::Hold:
			{
				// A bar between the press and the release; cells touch so a long hold reads as one piece
				const FLinearColor BarColour = Cell.bConsumed ? FLinearColor(0.2f, 0.2f, 0.2f) : Dim(Colour, 0.55f);
				PaintRect(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2f(X, RowCentre - 3.f), FVector2f(ColumnWidth, 6.f), BarColour);
				break;
			}
			case EGSTN_InputCellKind::Release:
			{
				// A hollow marker: the button went up here
				const float Edge = IconSize * 0.7f;
				const FLinearColor RingColour = Cell.bConsumed ? FLinearColor(0.35f, 0.35f, 0.35f) : Colour;
				PaintRect(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2f(CentreX - Edge * 0.5f, RowCentre - Edge * 0.5f), FVector2f(Edge, Edge), RingColour, true);
				PaintRect(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2f(CentreX - Edge * 0.5f + 2.f, RowCentre - Edge * 0.5f + 2.f), FVector2f(Edge - 4.f, Edge - 4.f), Background, true);
				break;
			}
			default:
			{
				// A press or a tap: the icon of the button with its letter. A tap has a white rim; a press a command used is grey
				if (Cell.Kind == EGSTN_InputCellKind::Tap)
				{
					PaintRect(OutDrawElements, LayerId + 2, AllottedGeometry, IconPosition - FVector2f(1.5f, 1.5f), FVector2f(IconSize + 3.f, IconSize + 3.f), FLinearColor::White, true);
				}
				const FLinearColor Fill = Cell.bConsumed ? FLinearColor(0.34f, 0.34f, 0.35f) : Colour;
				PaintRect(OutDrawElements, LayerId + 3, AllottedGeometry, IconPosition, FVector2f(IconSize, IconSize), Fill, true);
				PaintTextCentred(OutDrawElements, LayerId + 4, AllottedGeometry, FVector2f(CentreX, RowCentre), Letter, Cell.bConsumed ? FLinearColor(0.7f, 0.7f, 0.72f) : FM::GetButtonTextColor(Button), LetterFont());
				break;
			}
			}
		}
	}

	// Under the strip: a tick for every event, and the gap in frames between neighbouring events more than one frame apart.
	// Labels that would touch are dropped by ChooseGapLabels, so nothing overlaps
	for (const FGSTN_InputDisplayEvent& Event : Layout.Events)
	{
		if (Event.Column >= FirstColumn)
		{
			const float CentreX = FM::GetColumnX(Size.X, ColumnWidth, Layout.NumColumns, Event.Column) + ColumnWidth * 0.5f;
			PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(CentreX - 0.5f, Rows.Gaps + 1.f), FVector2f(1.f, 3.f), FLinearColor(1.f, 1.f, 1.f, 0.35f));
		}
	}
	for (const FGSTN_InputGapLabel& Label : FM::ChooseGapLabels(Layout, FirstColumn, Size.X, ColumnWidth, GapCharWidth))
	{
		const float FromX = FM::GetColumnX(Size.X, ColumnWidth, Layout.NumColumns, Label.FromColumn) + ColumnWidth * 0.5f;
		const float ToX = FM::GetColumnX(Size.X, ColumnWidth, Layout.NumColumns, Label.ToColumn) + ColumnWidth * 0.5f;
		PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(FromX, Rows.Gaps + 2.f), FVector2f(ToX - FromX, 1.f), FLinearColor(1.f, 1.f, 1.f, 0.18f));
		PaintText(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2f(Label.X, Rows.Gaps + 3.f), Label.Text, DimColor, SmallFont());
	}

	OutDrawElements.PopClip();
	return LayerId + 5;
}

FText SGSTN_InputStrip::DescribeAt(const FVector2D& Local) const
{
	if (Layout.NumColumns == 0)
	{
		return FText::GetEmpty();
	}
	const float Width = GetCachedGeometry().GetLocalSize().X;
	const int32 NumButtons = ButtonNames.Num();
	const FRowTops Rows(NumButtons, GetCachedGeometry().GetLocalSize().Y);

	// The gutter names the row
	if (Local.X < FM::GutterWidth)
	{
		if (Local.Y >= Rows.Buttons && Local.Y < Rows.Gaps)
		{
			const int32 Button = FMath::FloorToInt((Local.Y - Rows.Buttons) / Rows.H.Button);
			return FText::FromString(ButtonNames.IsValidIndex(Button) ? ButtonNames[Button] : FString());
		}
		return Local.Y >= Rows.Stick && Local.Y < Rows.Buttons ? LOCTEXT("StickRowTip", "The stick, and under it the direction relative to the character where it differs") : FText::GetEmpty();
	}

	const int32 Visible = FM::GetVisibleColumns(Width, NumFrames);
	const float ColumnWidth = FM::GetColumnWidth(Width, Visible);
	const int32 FirstColumn = FM::GetFirstVisibleColumn(Layout.NumColumns, Visible);
	const int32 Column = FMath::FloorToInt((Local.X - FM::GetColumnX(Width, ColumnWidth, Layout.NumColumns, 0)) / ColumnWidth);
	if (Column < FirstColumn || Column >= Layout.NumColumns)
	{
		return FText::GetEmpty();
	}
	const int32 Frame = Layout.FirstFrame + Column;

	if (Local.Y < Rows.Stick)
	{
		const int32 Marker = Layout.ColumnMarker[Column];
		if (Marker != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("MarkerTip", "Frame {0}: command {1}"), Frame, FText::FromString(Layout.Markers[Marker].Command.ToString()));
		}
	}
	else if (Local.Y < Rows.Buttons)
	{
		// The only place the numpad notation is shown
		const FGSTN_InputDisplayStickCell& Stick = Layout.Stick[Column];
		const FText Facing = Stick.FacingKey == Stick.StickKey ? FText::GetEmpty()
			: FText::Format(LOCTEXT("FacingTip", ", relative to the character {0} ({1})"), FText::FromString(FString::Chr(Stick.FacingKey)), FText::FromString(FGSTN_InputDisplayLayout::GetDirectionName(Stick.FacingDirection)));
		return FText::Format(LOCTEXT("StickTip", "Frame {0}: stick {1}{2}"), Frame, FText::FromString(FGSTN_InputDisplayLayout::DescribeStickForTooltip(Stick)), Facing);
	}
	else if (Local.Y < Rows.Gaps)
	{
		const int32 Button = FMath::FloorToInt((Local.Y - Rows.Buttons) / Rows.H.Button);
		if (Layout.Buttons.IsValidIndex(Button))
		{
			const FGSTN_InputDisplayCell& Cell = Layout.Buttons[Button][Column];
			const FText Name = FText::FromString(ButtonNames.IsValidIndex(Button) ? ButtonNames[Button] : FString::Chr(FGSTN_InputDisplayLayout::GetButtonLetter(Button)));
			if (Cell.bConsumed && Cell.MarkerIndex != INDEX_NONE)
			{
				return FText::Format(LOCTEXT("ConsumedTip", "Frame {0}: {1}, used by {2} (frame {3})"), Frame, Name,
					FText::FromString(Layout.Markers[Cell.MarkerIndex].Command.ToString()), Layout.Markers[Cell.MarkerIndex].Frame);
			}
			if (Cell.bConsumed)
			{
				return FText::Format(LOCTEXT("ConsumedUnknownTip", "Frame {0}: {1}, used by a command"), Frame, Name);
			}
			return FText::Format(LOCTEXT("ButtonTip", "Frame {0}: {1}"), Frame, Name);
		}
	}
	else
	{
		// The frame gaps: what the numbers under the strip say
		for (const FGSTN_InputDisplayEvent& Event : Layout.Events)
		{
			if (Event.Column == Column)
			{
				return FText::Format(LOCTEXT("EventTip", "Frame {0}: {1}, {2} frames after the previous event"), Frame, FText::FromString(Event.GetLabel()), Event.Gap);
			}
		}
	}
	return FText::Format(LOCTEXT("FrameTip", "Frame {0}"), Frame);
}

// ---- SGSTN_InputDisplay

void SGSTN_InputDisplay::Construct(const FArguments& InArgs)
{
	FetchDelegate = InArgs._Fetch;
	LocalSource = InArgs._LocalSource;
	NumFrames = FMath::Clamp(InArgs._NumFrames, 10, FGSTN_InputHistory::MaxEventAge + 1);

	if (InArgs._Profile.IsSet())
	{
		Profile = InArgs._Profile.GetValue();
		bProfileFixed = true;
	}
	else
	{
		Profile = FGSTN_InputSourceRegistry::Get().GetDefaultProfile();
	}

	Strip = SNew(SGSTN_InputStrip).NumFrames(NumFrames);
	ApplyProfile(Profile);
	Strip->SetMessage(GetNoDataMessage());
	const TWeakPtr<SGSTN_InputStrip> WeakStrip = Strip;
	Strip->SetToolTipText(TAttribute<FText>::CreateLambda([WeakStrip]()
	{
		// The cursor position is read here, at the moment the tooltip asks, so the text follows the mouse
		const TSharedPtr<SGSTN_InputStrip> Pinned = WeakStrip.Pin();
		if (!Pinned.IsValid())
		{
			return FText::GetEmpty();
		}
		const FVector2f Cursor = FSlateApplication::Get().GetCursorPos();
		return Pinned->DescribeAt(Pinned->GetCachedGeometry().AbsoluteToLocal(Cursor));
	}));

	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	if (InArgs._bShowPlayerPicker && LocalSource.IsValid())
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock).Text(LOCTEXT("PickerLabel", "Showing"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SComboButton)
				.ToolTipText(LOCTEXT("PickerTip", "Which player's input to show. By default the play window that has the focus, or had it last; pick a player to keep showing that one."))
				.OnGetMenuContent(this, &SGSTN_InputDisplay::MakePlayerMenu)
				.ButtonContent()
				[
					SNew(STextBlock).Text(this, &SGSTN_InputDisplay::GetPickerLabel)
				]
			]
		];
	}

	Box->AddSlot().FillHeight(1.f).VAlign(VAlign_Top)
	[
		Strip.ToSharedRef()
	];

	if (InArgs._bShowScriptBox)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SAssignNew(NotationBox, SEditableTextBox)
			.HintText(LOCTEXT("NotationHint", "Scripted input, e.g. 0:face=90 0:4t 6:6t! 7:+A (Enter to show, empty for live)"))
			.OnTextCommitted(this, &SGSTN_InputDisplay::HandleNotationCommitted)
		];
		Box->AddSlot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SAssignNew(ResultText, STextBlock)
			.AutoWrapText(true)
		];
	}
	ChildSlot[Box];
}

void SGSTN_InputDisplay::ApplyProfile(const FGSTN_InputDisplayProfile& InProfile)
{
	Profile = InProfile;
	TArray<FString> Names;
	for (int32 Index = 0; Index < Profile.GetNumButtons(); ++Index)
	{
		const bool bNamed = Profile.ButtonTags.IsValidIndex(Index) && Profile.ButtonTags[Index].IsValid();
		Names.Add(bNamed ? Profile.ButtonTags[Index].ToString() : FString::Printf(TEXT("Button %c"), FGSTN_InputDisplayLayout::GetButtonLetter(Index)));
	}
	if (Strip.IsValid())
	{
		Strip->SetButtonNames(Names);
	}
	bHasLastData = false;
}

FText SGSTN_InputDisplay::GetNoDataMessage() const
{
	return LocalSource.IsValid() && !LocalSource->GetStatusText().IsEmpty() ? LocalSource->GetStatusText() : LOCTEXT("NoPawn", "No local player with an input history");
}

FText SGSTN_InputDisplay::GetPickerLabel() const
{
	if (!LocalSource.IsValid())
	{
		return FText::GetEmpty();
	}
	const FText Status = LocalSource->GetStatusText();
	return LocalSource->GetSelection().IsFollowingFocus() ? FText::Format(LOCTEXT("PickerFollowing", "Follow the focused play window: {0}"), Status) : Status;
}

TSharedRef<SWidget> SGSTN_InputDisplay::MakePlayerMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const TSharedPtr<FGSTN_LocalInputSource> Source = LocalSource;

	Menu.AddMenuEntry(LOCTEXT("FollowFocus", "Follow the focused play window"),
		LOCTEXT("FollowFocusTip", "Show the player of the play window that has the focus, or had it last"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([Source]() { Source->GetSelection().SetFollowFocus(); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Source]() { return Source->GetSelection().IsFollowingFocus(); })),
		NAME_None, EUserInterfaceActionType::RadioButton);

	TArray<FGSTN_InputTargetCandidate> Candidates;
	FGSTN_LocalInputSource::ListPlayers(Candidates);
	Menu.BeginSection("Players", LOCTEXT("PlayersSection", "Local players"));
	if (Candidates.IsEmpty())
	{
		Menu.AddWidget(SNew(STextBlock).Text(LOCTEXT("NoPlayers", "None: start a play session")), FText::GetEmpty());
	}
	for (const FGSTN_InputTargetCandidate& Candidate : Candidates)
	{
		const FString Key = Candidate.Key;
		Menu.AddMenuEntry(Candidate.Label,
			Candidate.bHasSource ? LOCTEXT("PlayerTip", "Show this player") : LOCTEXT("PlayerNoSourceTip", "Show this player (nothing registered reads its pawn yet)"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Source, Key]() { Source->GetSelection().SetFixed(Key); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Source, Key]() { return !Source->GetSelection().IsFollowingFocus() && Source->GetSelection().GetFixedKey() == Key; })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

void SGSTN_InputDisplay::SetScriptSlots(TArray<FGSTN_InputSlotSpec> InSlots)
{
	Profile.Slots = MoveTemp(InSlots);
}

void SGSTN_InputDisplay::SetResultText(const FText& Text)
{
	if (ResultText.IsValid())
	{
		ResultText->SetText(Text);
	}
}

bool SGSTN_InputDisplay::SetScriptedNotation(const FString& Notation)
{
	if (NotationBox.IsValid())
	{
		NotationBox->SetText(FText::FromString(Notation));
	}
	if (Notation.TrimStartAndEnd().IsEmpty())
	{
		bScripted = false;
		ScriptResult = FGSTN_InputDisplayScriptResult();
		bHasLastData = false;
		SetResultText(FText::GetEmpty());
		Refresh();
		return true;
	}

	TArray<const FGSTN_InputSlotSpec*> Slots;
	for (const FGSTN_InputSlotSpec& Slot : Profile.Slots)
	{
		Slots.Add(&Slot);
	}
	FGSTN_InputDisplayScriptResult Result = FGSTN_InputDisplayScript::Resolve(Notation, Profile.ButtonTags, Slots, *GetDefault<UGSTN_InputSettings>());
	if (!Result.bOk)
	{
		SetResultText(FText::Format(LOCTEXT("ScriptError", "Not a valid input: {0}"), FText::FromString(Result.Error)));
		return false;
	}

	ScriptResult = MoveTemp(Result);
	bScripted = true;
	bHasLastData = false;
	SetResultText(ScriptResult.Command.IsValid()
		? FText::Format(LOCTEXT("ScriptResolves", "Frame {0} starts {1}"), ScriptResult.LastFrame, FText::FromString(ScriptResult.Command.ToString()))
		: (Profile.Slots.IsEmpty()
			? FText::Format(LOCTEXT("ScriptNoSet", "Frame {0}: no command set is known for this game, the notation was only played"), ScriptResult.LastFrame)
			: FText::Format(LOCTEXT("ScriptNothing", "Frame {0}: no command of the set matches"), ScriptResult.LastFrame)));
	Refresh();
	return true;
}

void SGSTN_InputDisplay::HandleNotationCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		SetScriptedNotation(Text.ToString());
	}
}

void SGSTN_InputDisplay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	Refresh();
}

void SGSTN_InputDisplay::Refresh()
{
	FGSTN_InputDisplayData Data;
	if (bScripted)
	{
		Data = ScriptResult.Data;
	}
	else
	{
		bool bGotData = false;
		if (LocalSource.IsValid())
		{
			bGotData = LocalSource->Fetch(Data);

			// The buttons and the command set follow the source: a new pawn can have other buttons
			const IGSTN_InputHistorySource* Current = LocalSource->GetSource().Get();
			if (!bProfileFixed && Current != ProfileSource)
			{
				ProfileSource = Current;
				ApplyProfile(Current ? LocalSource->GetProfile() : FGSTN_InputSourceRegistry::Get().GetDefaultProfile());
			}
		}
		else if (FetchDelegate.IsBound())
		{
			bGotData = FetchDelegate.Execute(Data);
		}

		if (!bGotData)
		{
			if (bHasLastData || Layout.NumColumns != 0)
			{
				bHasLastData = false;
				Layout = FGSTN_InputDisplayLayout();
			}
			// The message names the player and says what is missing: it can change while there is still nothing to draw
			Strip->SetMessage(GetNoDataMessage());
			return;
		}
	}
	ApplyData(Data);
}

void SGSTN_InputDisplay::ApplyData(const FGSTN_InputDisplayData& Data)
{
	if (bHasLastData && LastData.History == Data.History && LastData.Markers.Num() == Data.Markers.Num())
	{
		bool bSame = true;
		for (int32 Index = 0; Index < Data.Markers.Num() && bSame; ++Index)
		{
			bSame = LastData.Markers[Index].Frame == Data.Markers[Index].Frame && LastData.Markers[Index].Command == Data.Markers[Index].Command;
		}
		if (bSame)
		{
			return;
		}
	}
	LastData = Data;
	bHasLastData = true;

	FGSTN_InputDisplayLayoutParams Params;
	Params.MaxColumns = NumFrames;
	Params.NumButtons = Profile.GetNumButtons();
	Layout = FGSTN_InputDisplayLayout::Build(Data, Params);
	Strip->SetLayout(Layout);

	if (ResultText.IsValid() && !bScripted)
	{
		// The live strip as a notation, to copy into the box
		SetResultText(Layout.Events.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("LiveNotation", "As a script: {0}"), FText::FromString(Layout.ToNotation())));
	}
}

#undef LOCTEXT_NAMESPACE
