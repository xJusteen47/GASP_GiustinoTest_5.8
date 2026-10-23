// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Details/GSTN_TimelineDataCustomization.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_InlineTimeline.h"
#include "Timeline/GSTN_PhaseBarData.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

TSharedRef<IPropertyTypeCustomization> FGSTN_TimelineDataCustomization::MakeInstance()
{
	return MakeShared<FGSTN_TimelineDataCustomization>();
}

FName FGSTN_TimelineDataCustomization::GetStructName()
{
	return FGSTN_CombatTimelineData::StaticStruct()->GetFName();
}

FGSTN_TimelineEditContext FGSTN_TimelineDataCustomization::ResolveContext(const IPropertyHandle& PropertyHandle)
{
	TArray<UObject*> Outers;
	PropertyHandle.GetOuterObjects(Outers);
	const FProperty* Property = PropertyHandle.GetProperty();
	if (Outers.Num() != 1 || Outers[0] == nullptr || Property == nullptr)
	{
		return FGSTN_TimelineEditContext();
	}

	const FName Name = Property->GetFName();
	if (Outers[0]->IsA<UGSTN_CombatTimeline>())
	{
		return Name == GET_MEMBER_NAME_CHECKED(UGSTN_CombatTimeline, Data) ? FGSTN_TimelineEditContext::FromOwnerObject(Outers[0]) : FGSTN_TimelineEditContext();
	}
	return Name == GET_MEMBER_NAME_CHECKED(UGSTN_GA_BaseCombat, Timeline) ? FGSTN_TimelineEditContext::FromOwnerObject(Outers[0]) : FGSTN_TimelineEditContext();
}

FText FGSTN_TimelineDataCustomization::MakeSummary() const
{
	if (!Handle.IsValid() || !Handle->IsValidHandle() || Handle->GetNumPerObjectValues() != 1)
	{
		return FText::GetEmpty();
	}
	FText Summary;
	Handle->EnumerateConstRawData([&Summary](const void* RawData, const int32, const int32) -> bool
	{
		if (RawData != nullptr)
		{
			const FGSTN_CombatTimelineData& Data = *static_cast<const FGSTN_CombatTimelineData*>(RawData);
			Summary = FGSTN_PhaseBarData::Compute(Data).FormatSummary(UGSTN_CombatEditorSettings::GetSimulationTickRate());
		}
		return false;
	});
	return Summary;
}

EVisibility FGSTN_TimelineDataCustomization::GetOpenVisibility() const
{
	return (Handle.IsValid() && Handle->IsValidHandle() && ResolveContext(*Handle).IsValid()) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FGSTN_TimelineDataCustomization::OnOpenClicked() const
{
	if (Handle.IsValid() && Handle->IsValidHandle())
	{
		GSTNInlineTimeline::OpenEditor(ResolveContext(*Handle));
	}
	return FReply::Handled();
}

void FGSTN_TimelineDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	Handle = PropertyHandle;

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(280.f)
	.MaxDesiredWidth(640.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(STextBlock)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.Text_Raw(this, &FGSTN_TimelineDataCustomization::MakeSummary)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Visibility_Raw(this, &FGSTN_TimelineDataCustomization::GetOpenVisibility)
			.ToolTipText(LOCTEXT("OpenTimelineEditorTip", "Opens the timeline editor on this timeline. Edits go to the same data as the properties below."))
			.OnClicked_Raw(this, &FGSTN_TimelineDataCustomization::OnOpenClicked)
			[
				SNew(STextBlock)
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				.Text(LOCTEXT("OpenTimelineEditor", "Open Timeline Editor"))
			]
		]
	];
}

void FGSTN_TimelineDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Every child as the default layout would have it: the panel stays a complete editor of the data
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandle(Index).ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
