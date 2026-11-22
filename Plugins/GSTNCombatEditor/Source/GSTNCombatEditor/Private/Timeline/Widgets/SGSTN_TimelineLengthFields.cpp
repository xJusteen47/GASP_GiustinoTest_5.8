// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineLengthFields.h"

#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/**
	 * A spin box over one int of the data. Typing is one transaction; dragging the spin box opens the model's drag at the first move and closes
	 * it at release, so the whole drag is one undo step. If another drag already runs, the box does not close what it did not open.
	 */
	TSharedRef<SWidget> MakeFrameSpinBox(const TSharedRef<FGSTN_TimelineEditorModel>& Model, TFunction<int32()> Get, TFunction<void(int32)> Set, const FText& DragDescription, const FText& Tip)
	{
		const TSharedRef<bool> bOwnsDrag = MakeShared<bool>(false);
		return SNew(SBox)
			.WidthOverride(72.f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.ToolTipText(Tip)
				.Value_Lambda([Get]() { return Get(); })
				.OnBeginSliderMovement_Lambda([Model, bOwnsDrag, DragDescription]() { *bOwnsDrag = Model->BeginDrag(DragDescription); })
				.OnValueChanged_Lambda([Set](int32 NewValue) { Set(NewValue); })
				.OnValueCommitted_Lambda([Set](int32 NewValue, ETextCommit::Type) { Set(NewValue); })
				.OnEndSliderMovement_Lambda([Model, bOwnsDrag](int32)
				{
					if (*bOwnsDrag)
					{
						Model->EndDrag();
						*bOwnsDrag = false;
					}
				})
			];
	}
}

void SGSTN_TimelineLengthFields::Construct(const FArguments& InArgs)
{
	check(InArgs._Model.IsValid());
	const TSharedRef<FGSTN_TimelineEditorModel> Model = InArgs._Model.ToSharedRef();

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LengthLabel", "Length"))
			.ToolTipText(LOCTEXT("LengthLabelTip", "TotalFrames: how many frames the timeline lasts. It has to be at least 1 for the timeline to validate."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeFrameSpinBox(Model,
				[Model]() { return Model->GetTotalFrames(); },
				[Model](int32 NewValue) { Model->SetTotalFrames(NewValue); },
				LOCTEXT("DragLength", "Set timeline length"),
				LOCTEXT("LengthTip", "TotalFrames, in frames of the simulation tick. Type a number or drag. Undoable."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(LOCTEXT("LengthUnit", "frames"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 4.f, 0.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FafLabel", "FAF"))
			.ToolTipText(LOCTEXT("FafLabelTip", "First actionable frame: from here on the character may act again."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeFrameSpinBox(Model,
				[Model]() { const FGSTN_CombatTimelineData* Data = Model->GetData(); return Data ? Data->FirstActionableFrame : 0; },
				[Model](int32 NewValue) { Model->SetFirstActionableFrame(NewValue); },
				LOCTEXT("DragFaf", "Move first actionable frame"),
				LOCTEXT("FafTip", "FirstActionableFrame, clamped to the timeline length once it has one. Type a number or drag. Undoable."))
		]
	];
}

#undef LOCTEXT_NAMESPACE
