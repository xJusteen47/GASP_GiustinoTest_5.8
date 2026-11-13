// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_HitboxOutliner.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	const FName GSTNOutlineColName(TEXT("Name"));
	const FName GSTNOutlineColSummary(TEXT("Summary"));

	class SHitboxOutlineRow : public SMultiColumnTableRow<TSharedPtr<FGSTN_HitboxOutlineItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SHitboxOutlineRow) {}
			SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
			SLATE_ARGUMENT(TSharedPtr<FGSTN_HitboxOutlineItem>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
		{
			Model = InArgs._Model;
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FGSTN_HitboxOutlineItem>>::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const TWeakPtr<FGSTN_TimelineEditorModel> WeakModel = Model;
			const FGSTN_HitboxOutlineItem Row = Item.IsValid() ? *Item : FGSTN_HitboxOutlineItem();
			const bool bName = Column == GSTNOutlineColName;
			TSharedRef<STextBlock> Text = SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(bName && Row.IsGroup() ? "Bold" : "Regular", 9))
				.ColorAndOpacity(bName ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
				.Text_Lambda([WeakModel, Row, bName]() -> FText
				{
					const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin();
					const FGSTN_CombatTimelineData* Data = Pinned.IsValid() ? Pinned->GetData() : nullptr;
					if (Data == nullptr)
					{
						return FText::GetEmpty();
					}
					return bName ? GSTNHitboxOutline::GetName(*Data, Row) : GSTNHitboxOutline::GetSummary(*Data, Row);
				});

			if (!bName)
			{
				return SNew(SBox).Padding(FMargin(4.f, 2.f)).VAlign(VAlign_Center)[Text];
			}
			// The expander arrow of the tree sits in the first column
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(14.f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(2.f, 2.f))
				.VAlign(VAlign_Center)
				[
					Text
				];
		}

	private:
		TSharedPtr<FGSTN_TimelineEditorModel> Model;
		TSharedPtr<FGSTN_HitboxOutlineItem> Item;
	};
}

void SGSTN_HitboxOutliner::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(Tree, STreeView<FItemPtr>)
			.TreeItemsSource(&Roots)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SGSTN_HitboxOutliner::OnGenerateRow)
			.OnGetChildren(this, &SGSTN_HitboxOutliner::OnGetChildren)
			.OnSelectionChanged(this, &SGSTN_HitboxOutliner::HandleTreeSelectionChanged)
			.OnMouseButtonDoubleClick(this, &SGSTN_HitboxOutliner::HandleDoubleClick)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(GSTNOutlineColName).DefaultLabel(LOCTEXT("OutlineColName", "Hitbox")).FillWidth(0.35f)
				+ SHeaderRow::Column(GSTNOutlineColSummary).DefaultLabel(LOCTEXT("OutlineColSummary", "What it is")).FillWidth(0.65f)
			)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(6.f, 4.f))
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
			.Visibility_Lambda([this]() { return Roots.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(LOCTEXT("OutlineEmpty", "This move has no hitbox group yet. Add a Hitbox track on the timeline."))
		]
	];

	DataHandle = Model->OnDataChanged.AddSP(this, &SGSTN_HitboxOutliner::Rebuild);
	SelectionHandle = Model->OnSelectionChanged.AddSP(this, &SGSTN_HitboxOutliner::SyncSelectionToTree);
	Rebuild();
}

SGSTN_HitboxOutliner::~SGSTN_HitboxOutliner()
{
	if (Model.IsValid())
	{
		Model->OnDataChanged.Remove(DataHandle);
		Model->OnSelectionChanged.Remove(SelectionHandle);
	}
}

void SGSTN_HitboxOutliner::Rebuild()
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	TArray<FGSTN_HitboxOutlineItem> Outline = Data != nullptr ? GSTNHitboxOutline::Build(*Data) : TArray<FGSTN_HitboxOutlineItem>();
	// Same shape: keep the rows (the cells read the data), so a gizmo drag does not rebuild the tree under the mouse
	if (Outline != Built)
	{
		Built = MoveTemp(Outline);
		Roots.Reset();
		ChildrenByTrack.Reset();
		for (const FGSTN_HitboxOutlineItem& Item : Built)
		{
			FItemPtr Row = MakeShared<FGSTN_HitboxOutlineItem>(Item);
			if (Item.IsGroup())
			{
				Roots.Add(Row);
				ChildrenByTrack.FindOrAdd(Item.Track);
			}
			else
			{
				ChildrenByTrack.FindOrAdd(Item.Track).Add(Row);
			}
		}
		Tree->RequestTreeRefresh();
		// Every group open: a move has a handful of hitboxes, and a closed group would hide the row a viewport click selects
		for (const FItemPtr& Root : Roots)
		{
			Tree->SetItemExpansion(Root, true);
		}
	}
	SyncSelectionToTree();
}

SGSTN_HitboxOutliner::FItemPtr SGSTN_HitboxOutliner::FindRow(const FGSTN_HitboxOutlineItem& Item) const
{
	if (Item.IsGroup())
	{
		const FItemPtr* Found = Roots.FindByPredicate([&Item](const FItemPtr& Row) { return Row.IsValid() && *Row == Item; });
		return Found != nullptr ? *Found : nullptr;
	}
	if (const TArray<FItemPtr>* Children = ChildrenByTrack.Find(Item.Track))
	{
		const FItemPtr* Found = Children->FindByPredicate([&Item](const FItemPtr& Row) { return Row.IsValid() && *Row == Item; });
		return Found != nullptr ? *Found : nullptr;
	}
	return nullptr;
}

void SGSTN_HitboxOutliner::SyncSelectionToTree()
{
	TGuardValue<bool> Guard(bSyncing, true);
	FGSTN_HitboxOutlineItem Selected;
	const FItemPtr Row = GSTNHitboxOutline::FindSelected(*Model, Selected) ? FindRow(Selected) : nullptr;
	if (!Row.IsValid())
	{
		Tree->ClearSelection();
		return;
	}
	if (!Tree->IsItemSelected(Row))
	{
		if (!Selected.IsGroup())
		{
			if (const FItemPtr Group = FindRow(FGSTN_HitboxOutlineItem(Selected.Track, INDEX_NONE)))
			{
				Tree->SetItemExpansion(Group, true);
			}
		}
		Tree->SetSelection(Row, ESelectInfo::Direct);
		Tree->RequestScrollIntoView(Row);
	}
}

void SGSTN_HitboxOutliner::HandleTreeSelectionChanged(FItemPtr Item, ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct || !Item.IsValid())
	{
		return;
	}
	// Copy: Focus broadcasts the selection, and the sync may rebuild nothing but still touches the tree's selection
	const FGSTN_HitboxOutlineItem Picked = *Item;
	GSTNHitboxOutline::Focus(*Model, Picked);
}

void SGSTN_HitboxOutliner::HandleDoubleClick(FItemPtr Item)
{
	// A second click on the selected row: focus again (the playhead may have been moved away since)
	if (Item.IsValid())
	{
		const FGSTN_HitboxOutlineItem Picked = *Item;
		GSTNHitboxOutline::Focus(*Model, Picked);
	}
}

TSharedRef<ITableRow> SGSTN_HitboxOutliner::OnGenerateRow(FItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SHitboxOutlineRow, OwnerTable).Model(Model).Item(Item);
}

void SGSTN_HitboxOutliner::OnGetChildren(FItemPtr Item, TArray<FItemPtr>& OutChildren)
{
	if (Item.IsValid() && Item->IsGroup())
	{
		if (const TArray<FItemPtr>* Children = ChildrenByTrack.Find(Item->Track))
		{
			OutChildren = *Children;
		}
	}
}

#undef LOCTEXT_NAMESPACE
