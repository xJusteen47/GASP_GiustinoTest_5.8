// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Widgets/SGSTN_InputDisplay.h"

#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_StandardSlots.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatDebug"

namespace
{
	// Geometry of the strip, in slate units
	constexpr float GutterWidth = 14.f;
	constexpr float MarkerRowHeight = 8.f;
	constexpr float ButtonRowHeight = 11.f;
	constexpr float StickRowHeight = 14.f;
	constexpr float FacingRowHeight = 11.f;
	constexpr float EventLineHeight = 11.f;
	constexpr float MinColumnWidth = 3.f;

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo SmallFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", 8);
	}

	void PaintRect(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FVector2f& Size, const FLinearColor& Color)
	{
		if (Size.X > 0.f && Size.Y > 0.f)
		{
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), WhiteBrush(), ESlateDrawEffect::None, Color);
		}
	}

	void PaintText(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FString& Text, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(200.f, 12.f), FSlateLayoutTransform(Position)), Text, SmallFont(), ESlateDrawEffect::None, Color);
	}

	FLinearColor CellColor(const FGSTN_InputDisplayCell& Cell)
	{
		if (Cell.bConsumed)
		{
			// Greyed: the press was used by a command, so it can no longer start another
			return Cell.Kind == EGSTN_InputCellKind::Hold ? FLinearColor(0.2f, 0.2f, 0.2f) : FLinearColor(0.42f, 0.42f, 0.42f);
		}
		switch (Cell.Kind)
		{
		case EGSTN_InputCellKind::Press:   return FLinearColor(0.35f, 0.85f, 0.4f);
		case EGSTN_InputCellKind::Hold:    return FLinearColor(0.16f, 0.45f, 0.22f);
		case EGSTN_InputCellKind::Release: return FLinearColor(0.9f, 0.5f, 0.2f);
		case EGSTN_InputCellKind::Tap:     return FLinearColor(0.4f, 0.9f, 0.95f);
		default:                           return FLinearColor::Transparent;
		}
	}

	FLinearColor StickColor(const FGSTN_InputDisplayStickCell& Cell)
	{
		switch (Cell.Zone)
		{
		case EGSTN_StickZone::Aim:  return FLinearColor(0.22f, 0.3f, 0.5f);
		case EGSTN_StickZone::Tilt: return FLinearColor(0.2f, 0.4f, 0.85f);
		default:                    return FLinearColor(0.12f, 0.12f, 0.13f);
		}
	}
}

// ---- SGSTN_InputStrip

void SGSTN_InputStrip::Construct(const FArguments& InArgs)
{
	ButtonNames = InArgs._ButtonNames;
	NumFrames = FMath::Max(1, InArgs._NumFrames);
}

void SGSTN_InputStrip::SetLayout(const FGSTN_InputDisplayLayout& InLayout)
{
	Layout = InLayout;
	Message = FText::GetEmpty();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SGSTN_InputStrip::SetMessage(const FText& InMessage)
{
	Layout = FGSTN_InputDisplayLayout();
	Message = InMessage;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SGSTN_InputStrip::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const float Height = MarkerRowHeight + ButtonNames.Num() * ButtonRowHeight + StickRowHeight + FacingRowHeight + 2.f * EventLineHeight;
	return FVector2D(240.0, Height + 2.f);
}

float SGSTN_InputStrip::GetColumnWidth(float Width) const
{
	return FMath::Max(MinColumnWidth, (Width - GutterWidth) / NumFrames);
}

void SGSTN_InputStrip::GetVisibleRange(float Width, float ColumnWidth, int32& OutFirstColumn, float& OutOriginX) const
{
	// Newest column against the right edge; a narrow widget cuts the oldest columns off
	OutOriginX = Width - Layout.NumColumns * ColumnWidth;
	OutFirstColumn = OutOriginX >= GutterWidth ? 0 : FMath::Min(Layout.NumColumns, FMath::CeilToInt((GutterWidth - OutOriginX) / ColumnWidth));
}

int32 SGSTN_InputStrip::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const int32 NumButtons = ButtonNames.Num();
	const float ButtonsTop = MarkerRowHeight;
	const float StickTop = ButtonsTop + NumButtons * ButtonRowHeight;
	const float FacingTop = StickTop + StickRowHeight;
	const float EventsTop = FacingTop + FacingRowHeight;
	const FLinearColor TextColor(0.85f, 0.85f, 0.85f);
	const FLinearColor DimColor(0.55f, 0.55f, 0.55f);

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	PaintRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f::ZeroVector, Size, FLinearColor(0.06f, 0.06f, 0.065f));

	// Row labels
	for (int32 Button = 0; Button < NumButtons; ++Button)
	{
		PaintText(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(2.f, ButtonsTop + Button * ButtonRowHeight - 1.f), FString::Chr(FGSTN_InputDisplayLayout::GetButtonLetter(Button)), DimColor);
	}
	PaintText(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(2.f, StickTop), TEXT("S"), DimColor);

	if (Layout.NumColumns == 0)
	{
		PaintText(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(GutterWidth + 6.f, StickTop - 4.f), Message.ToString(), DimColor);
		OutDrawElements.PopClip();
		return LayerId + 2;
	}

	const float ColumnWidth = GetColumnWidth(Size.X);
	int32 FirstColumn = 0;
	float OriginX = 0.f;
	GetVisibleRange(Size.X, ColumnWidth, FirstColumn, OriginX);
	const float Pad = ColumnWidth >= 6.f ? 1.f : 0.f;

	for (int32 Column = FirstColumn; Column < Layout.NumColumns; ++Column)
	{
		const float X = OriginX + Column * ColumnWidth;

		// A faint line every 10 frames so the eye can count gaps
		if ((Layout.FirstFrame + Column) % 10 == 0)
		{
			PaintRect(OutDrawElements, LayerId, AllottedGeometry, FVector2f(X, ButtonsTop), FVector2f(1.f, EventsTop - ButtonsTop), FLinearColor(1.f, 1.f, 1.f, 0.07f));
		}

		if (Layout.ColumnMarker[Column] != INDEX_NONE)
		{
			PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(X, 1.f), FVector2f(FMath::Max(ColumnWidth - Pad, 2.f), MarkerRowHeight - 2.f), FLinearColor(1.f, 0.62f, 0.1f));
		}

		for (int32 Button = 0; Button < NumButtons; ++Button)
		{
			const FGSTN_InputDisplayCell& Cell = Layout.Buttons[Button][Column];
			if (Cell.Kind != EGSTN_InputCellKind::None)
			{
				// A release is the thin end of the bar: half height, so it reads as "the button went up here"
				const bool bThin = Cell.Kind == EGSTN_InputCellKind::Release;
				PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry,
					FVector2f(X, ButtonsTop + Button * ButtonRowHeight + (bThin ? 4.f : 1.f)),
					FVector2f(ColumnWidth - Pad, bThin ? ButtonRowHeight - 8.f : ButtonRowHeight - 2.f), CellColor(Cell));
			}
		}

		const FGSTN_InputDisplayStickCell& Stick = Layout.Stick[Column];
		PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(X, StickTop + 1.f), FVector2f(ColumnWidth - Pad, StickRowHeight - 2.f), StickColor(Stick));
		if (Stick.bFlick)
		{
			PaintRect(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2f(X, StickTop + 1.f), FVector2f(FMath::Max(ColumnWidth - Pad, 2.f), 2.f), FLinearColor(1.f, 0.9f, 0.2f));
		}
		if (Stick.bEvent)
		{
			PaintText(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2f(X, StickTop), FGSTN_InputDisplayLayout::DescribeStick(Stick), TextColor);
			if (Stick.FacingKey != Stick.StickKey)
			{
				PaintText(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2f(X, FacingTop - 1.f), FString::Chr(Stick.FacingKey), DimColor);
			}
		}
	}

	// Events with the frame gap, two lines so close events do not sit on each other
	int32 Line = 0;
	for (const FGSTN_InputDisplayEvent& Event : Layout.Events)
	{
		if (Event.Column < FirstColumn)
		{
			continue;
		}
		const float X = OriginX + Event.Column * ColumnWidth;
		PaintRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(X, EventsTop), FVector2f(1.f, 3.f + Line * EventLineHeight), FLinearColor(1.f, 1.f, 1.f, 0.25f));
		PaintText(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2f(X + 1.f, EventsTop + Line * EventLineHeight), Event.GetLabel(), DimColor);
		Line = 1 - Line;
	}

	OutDrawElements.PopClip();
	return LayerId + 4;
}

FText SGSTN_InputStrip::DescribeAt(const FVector2D& Local) const
{
	if (Layout.NumColumns == 0)
	{
		return FText::GetEmpty();
	}
	const float Width = GetCachedGeometry().GetLocalSize().X;
	const float ColumnWidth = GetColumnWidth(Width);
	int32 FirstColumn = 0;
	float OriginX = 0.f;
	GetVisibleRange(Width, ColumnWidth, FirstColumn, OriginX);
	const int32 Column = FMath::FloorToInt((Local.X - OriginX) / ColumnWidth);
	if (Column < FirstColumn || Column >= Layout.NumColumns)
	{
		return FText::GetEmpty();
	}
	const int32 Frame = Layout.FirstFrame + Column;
	const int32 NumButtons = ButtonNames.Num();
	const float ButtonsTop = MarkerRowHeight;
	const float StickTop = ButtonsTop + NumButtons * ButtonRowHeight;

	if (Local.Y < ButtonsTop)
	{
		const int32 Marker = Layout.ColumnMarker[Column];
		if (Marker != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("MarkerTip", "Frame {0}: command {1}"), Frame, FText::FromString(Layout.Markers[Marker].Command.ToString()));
		}
	}
	else if (Local.Y < StickTop)
	{
		const int32 Button = FMath::FloorToInt((Local.Y - ButtonsTop) / ButtonRowHeight);
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
		const FGSTN_InputDisplayStickCell& Stick = Layout.Stick[Column];
		return FText::Format(LOCTEXT("StickTip", "Frame {0}: stick {1}, relative to the character {2}"), Frame,
			FText::FromString(FGSTN_InputDisplayLayout::DescribeStick(Stick)), FText::FromString(FString::Chr(Stick.FacingKey)));
	}
	return FText::Format(LOCTEXT("FrameTip", "Frame {0}"), Frame);
}

// ---- SGSTN_InputDisplay

void SGSTN_InputDisplay::Construct(const FArguments& InArgs)
{
	FetchDelegate = InArgs._Fetch;
	NumFrames = FMath::Clamp(InArgs._NumFrames, 10, FGSTN_InputHistory::MaxEventAge + 1);
	ButtonTags = InArgs._ButtonTags;
	if (ButtonTags.IsEmpty())
	{
		ButtonTags = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag(),
			GSTNCombatButtonTags::Button_Crouch.GetTag(), GSTNCombatButtonTags::Button_Shield.GetTag(), GSTNCombatButtonTags::Button_Jump.GetTag() };
	}
	ScriptSlots = UGSTN_StandardSlots::Build();

	TArray<FString> Names;
	for (const FGameplayTag& ButtonTag : ButtonTags)
	{
		Names.Add(ButtonTag.ToString());
	}

	Strip = SNew(SGSTN_InputStrip).ButtonNames(Names).NumFrames(NumFrames);
	Strip->SetMessage(LOCTEXT("NoPawn", "No local combat pawn"));
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

	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Strip.ToSharedRef()
		];

	if (InArgs._bShowScriptBox)
	{
		Box->AddSlot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
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

void SGSTN_InputDisplay::SetScriptSlots(TArray<FGSTN_InputSlotSpec> InSlots)
{
	ScriptSlots = MoveTemp(InSlots);
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
	for (const FGSTN_InputSlotSpec& Slot : ScriptSlots)
	{
		Slots.Add(&Slot);
	}
	FGSTN_InputDisplayScriptResult Result = FGSTN_InputDisplayScript::Resolve(Notation, ButtonTags, Slots, *GetDefault<UGSTN_InputSettings>());
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
		: FText::Format(LOCTEXT("ScriptNothing", "Frame {0}: no command of the set matches"), ScriptResult.LastFrame));
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
	else if (!FetchDelegate.IsBound() || !FetchDelegate.Execute(Data))
	{
		if (bHasLastData || Layout.NumColumns != 0)
		{
			bHasLastData = false;
			Layout = FGSTN_InputDisplayLayout();
			Strip->SetMessage(LOCTEXT("NoPawn", "No local combat pawn"));
		}
		return;
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
	Params.NumButtons = ButtonTags.Num();
	Layout = FGSTN_InputDisplayLayout::Build(Data, Params);
	Strip->SetLayout(Layout);

	if (ResultText.IsValid() && !bScripted)
	{
		// The live strip as a notation, to copy into the box
		SetResultText(Layout.Events.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("LiveNotation", "As a script: {0}"), FText::FromString(Layout.ToNotation())));
	}
}

#undef LOCTEXT_NAMESPACE
