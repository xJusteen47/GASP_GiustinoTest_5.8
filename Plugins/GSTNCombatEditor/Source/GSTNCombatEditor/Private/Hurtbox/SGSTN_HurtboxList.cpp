// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/SGSTN_HurtboxList.h"

#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	const FName GSTNColIndex(TEXT("Index"));
	const FName GSTNColName(TEXT("Name"));
	const FName GSTNColBone(TEXT("Bone"));
	const FName GSTNColTags(TEXT("Tags"));
	const FName GSTNColPriority(TEXT("Priority"));

	class SHurtboxRow : public SMultiColumnTableRow<TSharedPtr<int32>>
	{
	public:
		SLATE_BEGIN_ARGS(SHurtboxRow) {}
			SLATE_ARGUMENT(TSharedPtr<FGSTN_HurtboxEditModel>, Model)
			SLATE_ARGUMENT(int32, Index)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
		{
			Model = InArgs._Model;
			Index = InArgs._Index;
			SMultiColumnTableRow<TSharedPtr<int32>>::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const TWeakPtr<FGSTN_HurtboxEditModel> WeakModel = Model;
			const int32 Row = Index;
			auto Cell = [WeakModel, Row](TFunction<FText(const FGSTN_Hurtbox&)> Read) -> TSharedRef<SWidget>
			{
				return SNew(SBox).Padding(FMargin(4.f, 2.f))
				[
					SNew(STextBlock).Text_Lambda([WeakModel, Row, Read]() -> FText
					{
						const TSharedPtr<FGSTN_HurtboxEditModel> Pinned = WeakModel.Pin();
						const TArray<FGSTN_Hurtbox>* Hurtboxes = Pinned.IsValid() ? Pinned->GetHurtboxes() : nullptr;
						return Hurtboxes != nullptr && Hurtboxes->IsValidIndex(Row) ? Read((*Hurtboxes)[Row]) : FText::GetEmpty();
					})
				];
			};
			if (Column == GSTNColIndex)
			{
				return Cell([Row](const FGSTN_Hurtbox&) { return FText::AsNumber(Row); });
			}
			if (Column == GSTNColName)
			{
				return Cell([](const FGSTN_Hurtbox& Hurtbox) { return FText::FromName(Hurtbox.Name); });
			}
			if (Column == GSTNColBone)
			{
				return Cell([](const FGSTN_Hurtbox& Hurtbox) { return FText::FromName(Hurtbox.Bone); });
			}
			if (Column == GSTNColTags)
			{
				return Cell([](const FGSTN_Hurtbox& Hurtbox)
				{
					FString Joined;
					for (const FGameplayTag& Tag : Hurtbox.Tags)
					{
						Joined += (Joined.IsEmpty() ? TEXT("") : TEXT(", ")) + Tag.ToString();
					}
					return FText::FromString(Joined);
				});
			}
			return Cell([](const FGSTN_Hurtbox& Hurtbox) { return FText::AsNumber(static_cast<int32>(Hurtbox.Priority)); });
		}

	private:
		TSharedPtr<FGSTN_HurtboxEditModel> Model;
		int32 Index = INDEX_NONE;
	};
}

void SGSTN_HurtboxList::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SAssignNew(List, SListView<FRow>)
		.ListItemsSource(&Rows)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SGSTN_HurtboxList::OnGenerateRow)
		.OnSelectionChanged(this, &SGSTN_HurtboxList::HandleSelectionChanged)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(GSTNColIndex).DefaultLabel(LOCTEXT("GSTNColIndex", "#")).FixedWidth(34.f)
			+ SHeaderRow::Column(GSTNColName).DefaultLabel(LOCTEXT("GSTNColName", "Name")).FillWidth(0.3f)
			+ SHeaderRow::Column(GSTNColBone).DefaultLabel(LOCTEXT("GSTNColBone", "Bone")).FillWidth(0.25f)
			+ SHeaderRow::Column(GSTNColTags).DefaultLabel(LOCTEXT("GSTNColTags", "Zone tags")).FillWidth(0.3f)
			+ SHeaderRow::Column(GSTNColPriority).DefaultLabel(LOCTEXT("GSTNColPriority", "Priority")).FixedWidth(60.f)
		)
	];

	ChangedHandle = Model->OnChanged.AddSP(this, &SGSTN_HurtboxList::Rebuild);
	SelectionHandle = Model->OnSelectionChanged.AddSP(this, &SGSTN_HurtboxList::SyncSelectionToList);
	Rebuild();
}

SGSTN_HurtboxList::~SGSTN_HurtboxList()
{
	if (Model.IsValid())
	{
		Model->OnChanged.Remove(ChangedHandle);
		Model->OnSelectionChanged.Remove(SelectionHandle);
	}
}

void SGSTN_HurtboxList::Rebuild()
{
	const int32 Count = Model->Num();
	// Same count: keep the row widgets (they read the model by themselves), so a drag in the details does not rebuild the list under the mouse
	if (Count != Rows.Num())
	{
		Rows.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Rows.Add(MakeShared<int32>(Index));
		}
		List->RequestListRefresh();
	}
	SyncSelectionToList();
}

void SGSTN_HurtboxList::SyncSelectionToList()
{
	bSyncing = true;
	List->ClearSelection();
	for (const int32 Index : Model->GetSelection())
	{
		if (Rows.IsValidIndex(Index))
		{
			List->SetItemSelection(Rows[Index], true, ESelectInfo::Direct);
		}
	}
	bSyncing = false;
}

void SGSTN_HurtboxList::HandleSelectionChanged(FRow Item, ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
	TArray<int32> Selected;
	for (const FRow& Row : List->GetSelectedItems())
	{
		if (Row.IsValid())
		{
			Selected.Add(*Row);
		}
	}
	Model->SetSelection(Selected);
}

TSharedRef<ITableRow> SGSTN_HurtboxList::OnGenerateRow(FRow Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SHurtboxRow, OwnerTable).Model(Model).Index(Item.IsValid() ? *Item : INDEX_NONE);
}

#undef LOCTEXT_NAMESPACE
