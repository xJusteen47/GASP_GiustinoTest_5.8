// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Details/GSTN_HitDataCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "SGameplayTagCombo.h"
#include "Timeline/Details/GSTN_HitDataMath.h"
#include "Timeline/Details/SGSTN_AngleDial.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

TSharedRef<IPropertyTypeCustomization> FGSTN_HitDataCustomization::MakeInstance()
{
	return MakeShared<FGSTN_HitDataCustomization>();
}

FName FGSTN_HitDataCustomization::GetStructName()
{
	return FGSTN_HitData::StaticStruct()->GetFName();
}

bool FGSTN_HitDataCustomization::GetHit(const TSharedPtr<IPropertyHandle>& SourceHandle, FGSTN_HitData& OutHit)
{
	if (!SourceHandle.IsValid() || !SourceHandle->IsValidHandle() || SourceHandle->GetNumPerObjectValues() != 1)
	{
		return false;
	}
	bool bFound = false;
	SourceHandle->EnumerateConstRawData([&](const void* RawData, const int32, const int32) -> bool
	{
		if (RawData != nullptr)
		{
			OutHit = *static_cast<const FGSTN_HitData*>(RawData);
			bFound = true;
		}
		return false;
	});
	return bFound;
}

TArray<FName> FGSTN_HitDataCustomization::GetLaidOutProperties()
{
	return {
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Damage), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ShieldDamage),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, BaseKnockback), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, KnockbackGrowth), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, FixedKnockback),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AngleDeg), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, SpecialAngle), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, YawSource),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstunMultiplier),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopMode), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopMultiplier), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AttackerHitstopMultiplier),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopFramesOverride), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AttackerHitstopFramesOverride),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForceMode), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForce), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForceScale),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Priority), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitType), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtraEffects),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, CueTag), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, CustomData),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Impact), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtensionData) };
}

TSharedPtr<IPropertyHandle> FGSTN_HitDataCustomization::Child(FName Name) const
{
	return Handle->GetChildHandle(Name);
}

uint8 FGSTN_HitDataCustomization::GetEnumValue(const TSharedPtr<IPropertyHandle>& EnumHandle)
{
	uint8 Value = 0;
	if (EnumHandle.IsValid() && EnumHandle->IsValidHandle())
	{
		EnumHandle->GetValue(Value);
	}
	return Value;
}

void FGSTN_HitDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	Handle = PropertyHandle;

	// Open on first sight: the groups are the editor of the hit, not detail behind a fold
	HeaderRow
	.ShouldAutoExpand(true)
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(240.f)
	.MaxDesiredWidth(640.f)
	[
		SNew(STextBlock)
		.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		.Text_Lambda([PropertyHandle]()
		{
			FGSTN_HitData Hit;
			return GetHit(PropertyHandle, Hit) ? GSTNHitDataText::Summarize(Hit) : LOCTEXT("HitSeveralValues", "Multiple values");
		})
	];
}

TSharedRef<SWidget> FGSTN_HitDataCustomization::MakeAngleWidget() const
{
	const TSharedPtr<IPropertyHandle> AngleHandle = Child(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AngleDeg));
	const TSharedPtr<IPropertyHandle> SpecialHandle = Child(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, SpecialAngle));

	const auto ReadAngle = [AngleHandle]()
	{
		float Value = 0.f;
		if (AngleHandle.IsValid() && AngleHandle->IsValidHandle())
		{
			AngleHandle->GetValue(Value);
		}
		return Value;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0.f, 2.f, 8.f, 2.f))
		[
			SNew(SBox)
			.WidthOverride(SGSTN_AngleDial::GetDialSize().X)
			.HeightOverride(SGSTN_AngleDial::GetDialSize().Y)
			.ToolTipText(LOCTEXT("DialTip", "Drag to set the launch elevation. Shift snaps to 5 degrees, Ctrl to 15, Esc cancels the drag. The line is the movement plane; above it launches up."))
			[
				SNew(SGSTN_AngleDial)
				.Angle_Lambda(ReadAngle)
				// The Sakurai angle replaces the authored one, so a drag would change nothing the hit uses
				.bReadOnly_Lambda([SpecialHandle]() { return GetEnumValue(SpecialHandle) == static_cast<uint8>(EGSTN_SpecialAngle::Sakurai3D); })
				.OnDragBegin_Lambda([AngleHandle]()
				{
					// One undo step for the whole drag: the interactive writes below join the transaction opened here
					if (GEditor != nullptr)
					{
						GEditor->BeginTransaction(TEXT("GSTNCombat"), LOCTEXT("DragAngle", "Change launch angle"), nullptr);
					}
				})
				.OnAngleChanged_Lambda([AngleHandle](float NewAngle)
				{
					if (AngleHandle.IsValid() && AngleHandle->IsValidHandle())
					{
						AngleHandle->SetValue(NewAngle, EPropertyValueSetFlags::InteractiveChange);
					}
				})
				.OnDragEnd_Lambda([AngleHandle](float FinalAngle)
				{
					if (AngleHandle.IsValid() && AngleHandle->IsValidHandle())
					{
						AngleHandle->SetValue(FinalAngle);
					}
					if (GEditor != nullptr)
					{
						GEditor->EndTransaction();
					}
				})
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.Text_Lambda([ReadAngle]() { return GSTNHitDataText::DescribeAngle(ReadAngle()); })
		];
}

TSharedRef<SWidget> FGSTN_HitDataCustomization::MakePreviewWidget() const
{
	FGSTN_KnockbackPreviewVictim& Victim = GSTNHitDataPreview::GetSessionVictim();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text(LOCTEXT("PreviewPercent", "Percent"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(SSpinBox<float>)
				.MinValue(0.f).MaxValue(999.f).MinDesiredWidth(56.f)
				.ToolTipText(LOCTEXT("PreviewPercentTip", "The damage percent the test victim has taken before this hit. Read by the percent models, ignored by the weight driven one."))
				.Value_Lambda([&Victim]() { return Victim.DamagePercent; })
				.OnValueChanged_Lambda([&Victim](float NewValue) { Victim.DamagePercent = NewValue; })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text(LOCTEXT("PreviewWeight", "Weight"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(SSpinBox<float>)
				.MinValue(0.1f).MaxValue(9999.f).MinDesiredWidth(56.f)
				.ToolTipText(LOCTEXT("PreviewWeightTip", "The test victim's weight, in the units of the rules' WeightReference (the default rules use 100 for an average combatant)."))
				.Value_Lambda([&Victim]() { return Victim.Weight; })
				.OnValueChanged_Lambda([&Victim](float NewValue) { Victim.Weight = NewValue; })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("PreviewGroundedTip", "The test victim stands on the ground. Only the Sakurai special angle reads it."))
				.IsChecked_Lambda([&Victim]() { return Victim.bGrounded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([&Victim](ECheckBoxState State) { Victim.bGrounded = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text(LOCTEXT("PreviewGrounded", "Grounded"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.f, 2.f))
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.Text_Lambda([Source = Handle]()
			{
				FGSTN_HitData Hit;
				if (!GetHit(Source, Hit))
				{
					return LOCTEXT("PreviewOneObject", "Select one hit to preview its knockback.");
				}
				return GSTNHitDataPreview::Compute(Hit, GSTNHitDataPreview::GetActiveRules(), GSTNHitDataPreview::GetSessionVictim()).Line;
			})
		];
}

void FGSTN_HitDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	Handle = PropertyHandle;

	const auto AddRows = [this](IDetailGroup& Group, std::initializer_list<FName> Names)
	{
		for (const FName Name : Names)
		{
			if (const TSharedPtr<IPropertyHandle> Property = Child(Name))
			{
				Group.AddPropertyRow(Property.ToSharedRef());
			}
		}
	};

	IDetailGroup& Damage = ChildBuilder.AddGroup(TEXT("Damage"), LOCTEXT("GroupDamage", "Damage"), true);
	AddRows(Damage, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Damage), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ShieldDamage) });

	// Knockback: the three ways to say how hard, then the direction: the dial and the elevation next to it, the special angle and the yaw source
	// each with a sentence under it that follows its value
	IDetailGroup& Knockback = ChildBuilder.AddGroup(TEXT("Knockback"), LOCTEXT("GroupKnockback", "Knockback"), true);
	AddRows(Knockback, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, BaseKnockback), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, KnockbackGrowth), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, FixedKnockback) });
	Knockback.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text(LOCTEXT("DialLabel", "Launch angle"))
		]
		.ValueContent()
		.MinDesiredWidth(300.f)
		[
			MakeAngleWidget()
		];
	AddRows(Knockback, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AngleDeg), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, SpecialAngle) });
	const TSharedPtr<IPropertyHandle> SpecialHandle = Child(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, SpecialAngle));
	Knockback.AddWidgetRow()
		.ValueContent()
		.MaxDesiredWidth(480.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([SpecialHandle]() { return GSTNHitDataText::DescribeSpecialAngle(static_cast<EGSTN_SpecialAngle>(GetEnumValue(SpecialHandle))); })
		];
	AddRows(Knockback, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, YawSource) });
	const TSharedPtr<IPropertyHandle> YawHandle = Child(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, YawSource));
	Knockback.AddWidgetRow()
		.ValueContent()
		.MaxDesiredWidth(480.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([YawHandle]() { return GSTNHitDataText::DescribeYawSource(static_cast<EGSTN_YawSource>(GetEnumValue(YawHandle))); })
		];

	IDetailGroup& Hitstun = ChildBuilder.AddGroup(TEXT("Hitstun"), LOCTEXT("GroupHitstun", "Hitstun"));
	AddRows(Hitstun, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstunMultiplier) });

	IDetailGroup& Hitstop = ChildBuilder.AddGroup(TEXT("Hitstop"), LOCTEXT("GroupHitstop", "Hitstop"));
	AddRows(Hitstop, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopMode), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopMultiplier), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AttackerHitstopMultiplier),
		GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitstopFramesOverride), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, AttackerHitstopFramesOverride) });

	IDetailGroup& Stagger = ChildBuilder.AddGroup(TEXT("Stagger"), LOCTEXT("GroupStagger", "Stagger"));
	AddRows(Stagger, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForceMode), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForce), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, StaggerForceScale) });

	// What kind of hit it is and what comes with it. The hit type is a tag combo limited to the Combat.HitType tags (the property's Categories say the same)
	IDetailGroup& Kind = ChildBuilder.AddGroup(TEXT("Hit"), LOCTEXT("GroupHit", "Hit"));
	AddRows(Kind, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Priority) });
	if (const TSharedPtr<IPropertyHandle> HitTypeHandle = Child(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, HitType)))
	{
		Kind.AddWidgetRow()
			.NameContent()
			[
				HitTypeHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(200.f)
			[
				SNew(SGameplayTagCombo)
				.PropertyHandle(HitTypeHandle)
				.Filter(TEXT("Combat.HitType"))
			];
	}
	AddRows(Kind, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtraEffects), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, CueTag), GET_MEMBER_NAME_CHECKED(FGSTN_HitData, CustomData) });

	IDetailGroup& Impact = ChildBuilder.AddGroup(TEXT("Impact"), LOCTEXT("GroupImpact", "Impact"));
	AddRows(Impact, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, Impact) });

	// The entries are structs of other plugins (the parts' hit data...): the standard editor of the array lets each customization of its own show inside it
	IDetailGroup& Extensions = ChildBuilder.AddGroup(TEXT("Extensions"), LOCTEXT("GroupExtensions", "Extensions"));
	AddRows(Extensions, { GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtensionData) });

	IDetailGroup& Preview = ChildBuilder.AddGroup(TEXT("PreviewKnockback"), LOCTEXT("GroupPreview", "Preview knockback"), true);
	Preview.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock).Font(IPropertyTypeCustomizationUtils::GetRegularFont()).Text(LOCTEXT("PreviewLabel", "Test victim"))
		]
		.ValueContent()
		.MinDesiredWidth(360.f)
		.MaxDesiredWidth(640.f)
		[
			MakePreviewWidget()
		];
}

#undef LOCTEXT_NAMESPACE
