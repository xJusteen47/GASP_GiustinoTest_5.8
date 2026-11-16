// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_HitboxOutliner.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
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
			auto ReadData = [WeakModel]() -> const FGSTN_CombatTimelineData*
			{
				const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin();
				return Pinned.IsValid() ? Pinned->GetData() : nullptr;
			};

			if (Column != GSTNOutlineColName)
			{
				return SNew(SBox).Padding(FMargin(4.f, 2.f)).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([ReadData, Row]() { const FGSTN_CombatTimelineData* Data = ReadData(); return Data ? GSTNHitboxOutline::GetSummary(*Data, Row) : FText::GetEmpty(); })
				];
			}

			// The name renames in place like the Outliner and the Content Browser: F2, double-click, or a click on the name of the selected row
			SAssignNew(NameBlock, SInlineEditableTextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(Row.IsGroup() ? "Bold" : "Regular", 9))
				.Text_Lambda([ReadData, Row]() { const FGSTN_CombatTimelineData* Data = ReadData(); return Data ? GSTNHitboxOutline::GetName(*Data, Row) : FText::GetEmpty(); })
				.IsSelected(this, &SHitboxOutlineRow::IsSelectedExclusively)
				.OnVerifyTextChanged_Lambda([](const FText& NewText, FText& OutError) { return GSTNHitboxOutline::IsValidName(NewText, OutError); })
				.OnTextCommitted_Lambda([WeakModel, Row](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnCleared)
					{
						return;
					}
					if (const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin())
					{
						GSTNHitboxOutline::Rename(*Pinned, Row, NewText);
					}
				});

			// The expander arrow of the tree sits in the first column, then the native icon of what the row is
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(14.f)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush(Row.IsGroup() ? "Icons.FolderOpen" : "Icons.Target"))
					.ColorAndOpacity(FSlateColor::UseForeground())
					.DesiredSizeOverride(FVector2D(16.f, 16.f))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(2.f, 2.f))
				.VAlign(VAlign_Center)
				[
					NameBlock.ToSharedRef()
				];
		}

		/** F2, double-click, the context menu: opens the rename box */
		void BeginRename()
		{
			if (NameBlock.IsValid())
			{
				NameBlock->EnterEditingMode();
			}
		}

	private:
		TSharedPtr<SInlineEditableTextBlock> NameBlock;
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
			.OnContextMenuOpening(this, &SGSTN_HitboxOutliner::MakeContextMenu)
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
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(6.f, 2.f))
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
			.Visibility_Lambda([this]() { return Roots.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text(LOCTEXT("OutlineHint", "F2 or double-click to rename. Right-click to add, duplicate or delete a hitbox."))
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
	// Focus again (the playhead may have been moved away since), then rename, as a double-click on a name does in the editor's lists
	if (Item.IsValid())
	{
		const FGSTN_HitboxOutlineItem Picked = *Item;
		GSTNHitboxOutline::Focus(*Model, Picked);
		RenameSelected();
	}
}

SGSTN_HitboxOutliner::FItemPtr SGSTN_HitboxOutliner::GetSelectedItem() const
{
	const TArray<FItemPtr> Selected = Tree.IsValid() ? Tree->GetSelectedItems() : TArray<FItemPtr>();
	return Selected.IsEmpty() ? nullptr : Selected[0];
}

void SGSTN_HitboxOutliner::RenameSelected()
{
	const FItemPtr Item = GetSelectedItem();
	if (!Item.IsValid())
	{
		return;
	}
	Tree->RequestScrollIntoView(Item);
	if (const TSharedPtr<ITableRow> Row = Tree->WidgetFromItem(Item))
	{
		StaticCastSharedPtr<SHitboxOutlineRow>(Row)->BeginRename();
	}
}

TSharedPtr<SWidget> SGSTN_HitboxOutliner::MakeContextMenu()
{
	const FItemPtr Item = GetSelectedItem();
	if (!Item.IsValid())
	{
		return nullptr;
	}
	const FGSTN_HitboxOutlineItem Row = *Item;
	const TWeakPtr<FGSTN_TimelineEditorModel> WeakModel = Model;
	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("GSTNHitboxEdit", Row.IsGroup() ? LOCTEXT("OutlineMenuGroup", "Hitbox Group") : LOCTEXT("OutlineMenuHitbox", "Hitbox"));
	Menu.AddMenuEntry(FGenericCommands::Get().Rename->GetLabel(), LOCTEXT("OutlineRenameTip", "Give it a name (F2). Empty shows the number again"),
		FGenericCommands::Get().Rename->GetIcon(), FUIAction(FExecuteAction::CreateSP(this, &SGSTN_HitboxOutliner::RenameSelected)));
	if (Row.IsGroup())
	{
		Menu.AddMenuEntry(LOCTEXT("OutlineAddHitbox", "Add Hitbox"), LOCTEXT("OutlineAddHitboxTip", "Add a sphere to this group (on the group when the group has a transform)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateLambda([WeakModel, Row]() { if (const TSharedPtr<FGSTN_TimelineEditorModel> M = WeakModel.Pin()) { GSTNHitboxOutline::AddHitbox(*M, Row.Track); } })));
	}
	else
	{
		Menu.AddMenuEntry(FGenericCommands::Get().Duplicate->GetLabel(), LOCTEXT("OutlineDuplicateTip", "Copy this hitbox, keys and hit data included, into the same group (Ctrl+D)"),
			FGenericCommands::Get().Duplicate->GetIcon(),
			FUIAction(FExecuteAction::CreateLambda([WeakModel, Row]() { if (const TSharedPtr<FGSTN_TimelineEditorModel> M = WeakModel.Pin()) { GSTNHitboxOutline::DuplicateHitbox(*M, Row); } })));
		Menu.AddMenuEntry(FGenericCommands::Get().Delete->GetLabel(), LOCTEXT("OutlineDeleteTip", "Remove this hitbox (Del). The last hitbox of a group stays: delete the track for that"),
			FGenericCommands::Get().Delete->GetIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([WeakModel, Row]() { if (const TSharedPtr<FGSTN_TimelineEditorModel> M = WeakModel.Pin()) { GSTNHitboxOutline::DeleteHitbox(*M, Row); } }),
				FCanExecuteAction::CreateLambda([WeakModel, Row]()
				{
					const TSharedPtr<FGSTN_TimelineEditorModel> M = WeakModel.Pin();
					const FGSTN_CombatTimelineData* Data = M.IsValid() ? M->GetData() : nullptr;
					const FGSTN_TimelineTrack_Hitbox* Track = Data != nullptr ? Data->GetHitboxTrack(Row.Track) : nullptr;
					return Track != nullptr && Track->Hitboxes.Num() > 1;
				})));
	}
	Menu.EndSection();
	return Menu.MakeWidget();
}

FReply SGSTN_HitboxOutliner::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FItemPtr Item = GetSelectedItem();
	if (!Item.IsValid())
	{
		return FReply::Unhandled();
	}
	const FGSTN_HitboxOutlineItem Row = *Item;
	if (InKeyEvent.GetKey() == EKeys::F2)
	{
		RenameSelected();
		return FReply::Handled();
	}
	if (!Row.IsGroup() && InKeyEvent.GetKey() == EKeys::Delete)
	{
		GSTNHitboxOutline::DeleteHitbox(*Model, Row);
		return FReply::Handled();
	}
	if (!Row.IsGroup() && InKeyEvent.GetKey() == EKeys::D && InKeyEvent.IsControlDown())
	{
		GSTNHitboxOutline::DuplicateHitbox(*Model, Row);
		return FReply::Handled();
	}
	return FReply::Unhandled();
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
