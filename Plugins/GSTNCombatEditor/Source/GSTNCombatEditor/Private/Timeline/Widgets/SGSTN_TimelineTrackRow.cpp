// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineTrackRow.h"

#include "Timeline/GSTN_TimelineColors.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Styling/AppStyle.h"
#include "Timeline/Widgets/SGSTN_TimelineLane.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_TimelineTrackRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
{
	using FRowBase = STableRow<TSharedPtr<FGSTN_TimelineRowData>>;

	const TSharedRef<FGSTN_TimelineEditorModel> Model = InArgs._Model.ToSharedRef();
	const int32 TrackIndex = InArgs._TrackIndex;

	// The name, read from the data each time so a row never shows a stale name after undo
	auto GetTrackStruct = [Model, TrackIndex]() -> const UScriptStruct*
	{
		const FGSTN_CombatTimelineData* Data = Model->GetData();
		return (Data != nullptr && Data->Tracks.IsValidIndex(TrackIndex)) ? Data->Tracks[TrackIndex].GetScriptStruct() : nullptr;
	};

	// The fill of the name cell: the lane colour, or a dark blue when the row is selected. The text colour is derived from it (never a fixed
	// white or black), so no fill can hide its own text
	auto HeaderFill = [Model, TrackIndex]() -> FLinearColor
	{
		const bool bSelected = Model->GetSelection().ContainsByPredicate([TrackIndex](const FGSTN_Selection& Item) { return Item.TrackIndex == TrackIndex; });
		return bSelected ? GSTNTimelineColors::SelectedHeaderFill() : GSTNTimelineWidgets::LaneBackground((TrackIndex & 1) != 0);
	};

	const TWeakPtr<FGSTN_TimelineEditorModel> WeakModel = Model;

	FRowBase::Construct(
		FRowBase::FArguments()
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.NoHoverTableRow"))
		.ShowSelection(false)
		.Padding(FMargin(0.f))
		.Content()
		[
			SNew(SBox)
			.HeightOverride(static_cast<float>(FGSTN_TimelineLayout::RowHeight))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(GSTNTimelineWidgets::NameColumnWidth)
					[
						SNew(SBorder)
						.Padding(FMargin(0.f))
						.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor_Lambda([HeaderFill]() -> FSlateColor
						{
							return HeaderFill();
						})
						.OnMouseButtonDown_Lambda([WeakModel, TrackIndex](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
						{
							if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
							{
								return FReply::Unhandled();
							}
							if (const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin())
							{
								Pinned->SetSelection({ FGSTN_Selection::Window(TrackIndex) });
							}
							return FReply::Handled();
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBox)
								.WidthOverride(5.f)
								[
									SNew(SBorder)
									.Padding(FMargin(0.f))
									.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
									.BorderBackgroundColor_Lambda([GetTrackStruct]() -> FSlateColor
									{
										const UScriptStruct* Struct = GetTrackStruct();
										return Struct ? FGSTN_TrackEditorRegistry::Get().FindOrGeneric(Struct)->GetColor() : FLinearColor::Gray;
									})
								]
							]
							// The icon the track editor names (nothing when it names none)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
							[
								SNew(SImage)
								.Image_Lambda([GetTrackStruct]() -> const FSlateBrush*
								{
									const UScriptStruct* Struct = GetTrackStruct();
									const FName IconName = Struct ? FGSTN_TrackEditorRegistry::Get().FindOrGeneric(Struct)->GetIconBrushName() : NAME_None;
									return IconName.IsNone() ? nullptr : FAppStyle::Get().GetBrush(IconName);
								})
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.VAlign(VAlign_Center)
							.Padding(FMargin(6.f, 0.f, 4.f, 0.f))
							[
								SNew(STextBlock)
								.ColorAndOpacity_Lambda([HeaderFill]() -> FSlateColor { return FSlateColor(GSTNTimelineColors::ReadableTextOn(HeaderFill())); })
								.Text_Lambda([GetTrackStruct]() -> FText
								{
									const UScriptStruct* Struct = GetTrackStruct();
									return Struct ? FGSTN_TrackEditorRegistry::Get().FindOrGeneric(Struct)->GetDisplayName() : FText::GetEmpty();
								})
								// What the bar says, for a row whose name column is too narrow for it
								.ToolTipText_Lambda([Model, TrackIndex]() -> FText
								{
									const FGSTN_CombatTimelineData* Data = Model->GetData();
									if (Data == nullptr || !Data->Tracks.IsValidIndex(TrackIndex))
									{
										return FText::GetEmpty();
									}
									const FGSTN_TimelineTrack* Track = Data->Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>();
									return Track ? FGSTN_TrackEditorRegistry::Get().FindOrGeneric(Data->Tracks[TrackIndex].GetScriptStruct())->GetRowLabel(*Track) : FText::GetEmpty();
								})
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "SimpleButton")
								.ToolTipText(LOCTEXT("AddKeyToTrackTip", "Add a key at the playhead"))
								.ContentPadding(FMargin(6.f, 0.f))
								.Visibility_Lambda([Model, TrackIndex]() -> EVisibility
								{
									const FGSTN_CombatTimelineData* Data = Model->GetData();
									return (Data != nullptr && FGSTN_TimelineLayout::IsKeyedTrack(*Data, TrackIndex)) ? EVisibility::Visible : EVisibility::Collapsed;
								})
								.OnClicked_Lambda([WeakModel, TrackIndex]() -> FReply
								{
									if (const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin())
									{
										FGSTN_TimelineActions::AddAtFrame(*Pinned, TrackIndex, Pinned->GetPlayhead());
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(LOCTEXT("AddKeyButton", "+"))
								]
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SGSTN_TimelineLane)
					.Model(InArgs._Model)
					.Gesture(InArgs._Gesture)
					.PaintStats(InArgs._PaintStats)
					.FocusTarget(InArgs._FocusTarget)
					.TrackIndex(TrackIndex)
				]
			]
		],
		OwnerTable);
}

#undef LOCTEXT_NAMESPACE
