// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Window/SGSTN_DebugOptionsPanel.h"
#include "Kit/GSTN_DebugKitPicker.h"
#include "Kit/GSTN_DebugSkinPicker.h"
#include "Widgets/Input/SComboBox.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Regex.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatDebug"

namespace
{
	const TCHAR* const VariablePrefix = TEXT("gstn.");

	/** The variable by name, looked up each time: a module that unloads takes its variables with it, a stored pointer would dangle */
	IConsoleVariable* FindVariable(const FString& Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(*Name);
	}

	/** The first local player controller of a play session (PIE or a game), or null in the editor world */
	APlayerController* FindPlayController()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
			{
				if (APlayerController* Controller = World->GetFirstPlayerController())
				{
					return Controller;
				}
			}
		}
		return nullptr;
	}

	/** The part of the name after the group, which is what a row shows (gstn.Combat.Debug.Hitboxes -> Debug.Hitboxes) */
	FString ShortName(const FString& Name)
	{
		int32 First = INDEX_NONE;
		if (!Name.FindChar(TEXT('.'), First))
		{
			return Name;
		}
		const int32 Second = Name.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, First + 1);
		return Second == INDEX_NONE ? Name : Name.Mid(Second + 1);
	}

	/** Playtest switches without "Debug" in their name that the owner wants in this window (2026-11-21): the locomotion base,
	 *  the run start clip choice, the camera and the run skid. Their cvars live in the game module (GSTN_LocoBaseSubsystem.cpp); this plugin only lists them. */
	bool IsPlaytestVariable(const FString& Name)
	{
		// skid-physics-1 (2026-11-21): the skid's switches, one control each (Motion has levels, Sim and PlayAtClipSpeed are on/off). TurnSide
		// stays out: a capture override whose help has no "2 =" would get a check box that cannot reach its value 2
		static const TCHAR* const SkidSwitches[] = { TEXT("gstn.Skid.Sim"), TEXT("gstn.Skid.Motion"), TEXT("gstn.Skid.PlayAtClipSpeed") };
		for (const TCHAR* Skid : SkidSwitches)
		{
			if (Name.Equals(Skid, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		// m10-m11-leftovers (2026-11-21): the held objects' presentation switches (hand attach, blend, arc preview, prompt, wind-up) and the
		// equipment look's (gstn.Equipment.*), one control each
		if (Name.StartsWith(TEXT("gstn.Held."), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Equipment."), ESearchCase::IgnoreCase)
			|| Name.Equals(TEXT("gstn.Combat.KitPlaceholderLook"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		// grab-link (2026-11-23): the grab hold's playtest switches (turn ergonomics, walk/jump scales, neck hold, max hold, escape hit), one control each
		if (Name.StartsWith(TEXT("gstn.Grab."), ESearchCase::IgnoreCase))
		{
			return true;
		}
		// run-stages-leggibili S1 (2026-11-21): gstn.RunStage.Readable, the readable run stage steps on/off (one check box)
		if (Name.StartsWith(TEXT("gstn.RunStage."), ESearchCase::IgnoreCase))
		{
			return true;
		}
		// surface feedback (2026-11-21): gstn.Audio.* (footwear x floor footsteps) and gstn.Haptics.* (pad rumble), cvars in GSTNPresentationCore
		if (Name.StartsWith(TEXT("gstn.Audio."), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Haptics."), ESearchCase::IgnoreCase))
		{
			return true;
		}
		// crouch-mesh-pop-1 (2026-11-21): gstn.Mesh.* (CapsuleShiftBeforeAnim, the crouch flash fix A/B); stance width layer (runstart-stancewidth): gstn.Loco.Stance*
		return Name.StartsWith(TEXT("gstn.RunStart."), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Mesh."), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Camera."), ESearchCase::IgnoreCase) || Name.Equals(TEXT("gstn.Loco.Base"), ESearchCase::IgnoreCase)
			// greybox-capsula (2026-11-24): the capsule in place of the character mesh (gstn.Greybox), game module
			|| Name.Equals(TEXT("gstn.Greybox"), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Debug.SpeedGraph"), ESearchCase::IgnoreCase)
			|| Name.StartsWith(TEXT("gstn.Loco.Stance"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("gstn.Loco.TorsoLean"), ESearchCase::IgnoreCase)
			// M12 part 1 (2026-11-21): lives/checkpoints and missions (plugins GSTNRespawn, GSTNMission; listed only when they are loaded)
			|| Name.StartsWith(TEXT("gstn.Respawn."), ESearchCase::IgnoreCase) || Name.StartsWith(TEXT("gstn.Mission."), ESearchCase::IgnoreCase)
			// getup-momentum-1 (2026-11-21): the get-up exit speeds (GSTN_DefenceAbilities.cpp), one number each
			|| Name.StartsWith(TEXT("gstn.Getup."), ESearchCase::IgnoreCase);
	}

	/** Readable name of a gstn.RunStart.Variant value (keep in step with that cvar's help text), shown next to the number */
	FString RunStartVariantLabel(int32 Value)
	{
		static const TCHAR* const Labels[] = {
			TEXT("v4r7 (approved)"), TEXT("v4r8c loop sped up"), TEXT("v4r8a high-knee pump"), TEXT("v4r8b cartoon wheel"),
			TEXT("stock relaxed"), TEXT("generic g1"), TEXT("generic g2"), TEXT("generic g3"), TEXT("sprinter s1"), TEXT("sprinter s2")};
		if (Value == 20)
		{
			return TEXT("generic w1 (sprinter weight)");	// 10-19 are the Handoff copies of 0-9: new variants start at 20
		}
		if (Value == 21)
		{
			return TEXT("generic w2 (default: forward + back/left/right turns)");
		}
		if (Value == 22)
		{
			return TEXT("w2 wide feet (w2 + gstn.Loco.StanceWidth)");
		}
		if (Value == 23)
		{
			return TEXT("w2 lively agile (w2v: sway, twist, rear elbow, lean; feet 4/2/0 cm)");
		}
		if (Value == 24)
		{
			return TEXT("22 + toe fix (w2f: foot rigid in the rev, toe no longer folds)");
		}
		if (Value == 25)
		{
			return TEXT("23 + toe fix (w2vf) + torso lean gstn.Loco.TorsoLean");
		}
		if (Value == 26)
		{
			return TEXT("24 + turnarounds 2 (w2t*f: toe fix, feet >= 11 cm apart, arm IK against elbow jitter)");
		}
		return Value >= 0 && Value < UE_ARRAY_COUNT(Labels) ? FString(Labels[Value]) : FString(TEXT("?"));
	}

	bool IsRunStartVariant(const FString& Name)
	{
		return Name.Equals(TEXT("gstn.RunStart.Variant"), ESearchCase::IgnoreCase);
	}

	/** The run start variants the drop-down offers, the standard first (10-19 are the Handoff copies: never picked directly) */
	const TArray<TSharedPtr<int32>>& RunStartVariantOptions()
	{
		static TArray<TSharedPtr<int32>> Options;
		if (Options.Num() == 0)
		{
			for (const int32 Value : {21, 22, 23, 24, 25, 26, 20, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9})
			{
				Options.Add(MakeShared<int32>(Value));
			}
		}
		return Options;
	}

	/** Owner 2026-11-21: one control per variable, no check box AND number for the same value. An int whose help names a value of 2 or
	 *  more ("2 = also log") has levels and gets a number only; any other int or bool is an on/off switch and gets a check box only. */
	bool HasLevels(const IConsoleVariable* Variable)
	{
		FRegexMatcher Matcher(FRegexPattern(TEXT("(^|[^0-9.])([2-9]|[1-9][0-9]+) *=")), Variable->GetHelp());
		return Matcher.FindNext();
	}
}

TArray<FString> SGSTN_DebugOptionsPanel::CollectVariableNames()
{
	TArray<FString> Names;
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda(
		[&Names](const TCHAR* Name, IConsoleObject* Object)
		{
			if (Object == nullptr || Object->AsVariable() == nullptr || Object->TestFlags(ECVF_Unregistered))
			{
				return;
			}
			const FString NameString(Name);
			if (NameString.Contains(TEXT("Debug"), ESearchCase::IgnoreCase) || IsPlaytestVariable(NameString))
			{
				Names.Add(NameString);
			}
		}), VariablePrefix);
	Names.Sort();
	return Names;
}

FString SGSTN_DebugOptionsPanel::GroupOf(const FString& VariableName)
{
	TArray<FString> Parts;
	VariableName.ParseIntoArray(Parts, TEXT("."));
	return Parts.Num() >= 3 ? Parts[1] : FString(TEXT("Other"));
}

void SGSTN_DebugOptionsPanel::Construct(const FArguments& InArgs)
{
	for (const FString& Name : FGSTN_DebugKitPicker::GetKitNames())
	{
		KitOptions.Add(MakeShared<FString>(Name));
		if (Name == TEXT("KIT_Test_Playtest"))
		{
			SelectedKit = KitOptions.Last();
		}
	}
	if (!SelectedKit.IsValid() && KitOptions.Num() > 0)
	{
		SelectedKit = KitOptions[0];
	}
	// "base" first: the original look, the reference every skin is compared with
	SkinOptions.Add(MakeShared<FString>(TEXT("base")));
	for (const FString& Name : FGSTN_DebugSkinPicker::GetSkinNames())
	{
		SkinOptions.Add(MakeShared<FString>(Name));
	}
	SelectedSkin = SkinOptions[0];

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 6.f, 6.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("KitLabel", "Kit"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&KitOptions)
				.InitiallySelectedItem(SelectedKit)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectedKit = Item; })
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SelectedKit.IsValid() ? *SelectedKit : FString(TEXT("(no kit)"))); })
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ApplyKit", "Apply kit"))
				.ToolTipText(LOCTEXT("ApplyKitTip", "Gives the first local player this kit (same server path as the loadout screen). Needs a play session. The hub's start kit is gstn.Debug.KitAtStart."))
				.IsEnabled(this, &SGSTN_DebugOptionsPanel::HasPlayWorld)
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnApplyKit)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda([this]() { return KitStatus; })
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f, 6.f, 6.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("SkinLabel", "Skin"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&SkinOptions)
				.InitiallySelectedItem(SelectedSkin)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { SelectedSkin = Item; })
				[
					SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SelectedSkin.IsValid() ? *SelectedSkin : FString(TEXT("(no skin)"))); })
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ForceSkin", "Force"))
				.ToolTipText(LOCTEXT("ForceSkinTip", "Shows this skin on the first local player whatever the loadout says, on this machine only (gstn.Debug.Skin). base = the original look. Hitboxes and the game do not change."))
				.IsEnabled(this, &SGSTN_DebugOptionsPanel::HasPlayWorld)
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnForceSkin, false)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ForceSkinAll", "Force on all"))
				.ToolTipText(LOCTEXT("ForceSkinAllTip", "The same on every pawn of the play world (targets and dummies), to compare body sizes side by side."))
				.IsEnabled(this, &SGSTN_DebugOptionsPanel::HasPlayWorld)
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnForceSkin, true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ReleaseSkin", "Release"))
				.ToolTipText(LOCTEXT("ReleaseSkinTip", "Every pawn goes back to its loadout's skin."))
				.IsEnabled(this, &SGSTN_DebugOptionsPanel::HasPlayWorld)
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnReleaseSkin)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda([this]() { return SkinStatus; })
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("FilterHint", "Filter (e.g. Hitbox, Minion)"))
				.OnTextChanged(this, &SGSTN_DebugOptionsPanel::OnFilterChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GameplayDebugger", "Gameplay Debugger"))
				.ToolTipText(LOCTEXT("GameplayDebuggerTip", "Opens or closes the Gameplay Debugger in the running play session (the same as its key). Needs a play session."))
				.IsEnabled(this, &SGSTN_DebugOptionsPanel::HasPlayWorld)
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnToggleGameplayDebugger)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTip", "Lists the debug variables again (a plugin loaded after the window opened adds its own)."))
				.OnClicked(this, &SGSTN_DebugOptionsPanel::OnRefresh)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(Rows, SVerticalBox)
			]
		]
	];
	Rebuild();
}

void SGSTN_DebugOptionsPanel::Rebuild()
{
	Rows->ClearChildren();

	// Group the names, keeping the alphabetical order inside each group; Combat first, then the others alphabetically
	TMap<FString, TArray<FString>> Groups;
	for (const FString& Name : CollectVariableNames())
	{
		if (Filter.IsEmpty() || Name.Contains(Filter, ESearchCase::IgnoreCase))
		{
			Groups.FindOrAdd(GroupOf(Name)).Add(Name);
		}
	}
	TArray<FString> GroupNames;
	Groups.GetKeys(GroupNames);
	GroupNames.Sort([](const FString& A, const FString& B)
	{
		const bool bA = A == TEXT("Combat");
		const bool bB = B == TEXT("Combat");
		return bA != bB ? bA : A < B;
	});

	if (GroupNames.Num() == 0)
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Empty", "No gstn.*Debug* variable matches. Clear the filter, or press Refresh after enabling a plugin."))
			.AutoWrapText(true)
		];
		return;
	}

	for (const FString& GroupName : GroupNames)
	{
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		for (const FString& Name : Groups[GroupName])
		{
			if (IConsoleVariable* Variable = FindVariable(Name))
			{
				Body->AddSlot()
				.AutoHeight()
				.Padding(4.f, 2.f)
				[
					MakeRow(Name, Variable)
				];
			}
		}

		Rows->AddSlot()
		.AutoHeight()
		.Padding(4.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(false)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(GroupName))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
			.BodyContent()
			[
				Body
			]
		];
	}
}

TSharedRef<SWidget> SGSTN_DebugOptionsPanel::MakeRow(const FString& Name, IConsoleVariable* Variable)
{
	const FText Tooltip = FText::FromString(FString::Printf(TEXT("%s\n\n%s"), *Name, Variable->GetHelp()));
	const bool bReadOnly = Variable->TestFlags(ECVF_ReadOnly);

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox).ToolTipText(Tooltip);

	const bool bVariant = IsRunStartVariant(Name);
	const bool bLevels = Variable->IsVariableInt() && !bVariant && HasLevels(Variable);
	if (Variable->IsVariableBool() || (Variable->IsVariableInt() && !bVariant && !bLevels))
	{
		// On/off switch: check box only (on = 1)
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsEnabled(!bReadOnly)
			.IsChecked_Lambda([Name]()
			{
				const IConsoleVariable* V = FindVariable(Name);
				return V && V->GetInt() != 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Name](ECheckBoxState State)
			{
				if (IConsoleVariable* V = FindVariable(Name))
				{
					V->Set(State == ECheckBoxState::Checked ? 1 : 0, ECVF_SetByConsole);
				}
			})
		];
	}

	Row->AddSlot()
	.FillWidth(1.f)
	.VAlign(VAlign_Center)
	.Padding(6.f, 0.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(ShortName(Name)))
	];

	if (bVariant)
	{
		// Run start clip: a drop-down of readable names (gstn.RunStart.Handoff adds 10 in the ABP)
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(220.f)
			[
				SNew(SComboBox<TSharedPtr<int32>>)
				.IsEnabled(!bReadOnly)
				.OptionsSource(&RunStartVariantOptions())
				.OnGenerateWidget_Lambda([](TSharedPtr<int32> Option)
				{
					return SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d  %s"), *Option, *RunStartVariantLabel(*Option))));
				})
				.OnSelectionChanged_Lambda([Name](TSharedPtr<int32> Option, ESelectInfo::Type)
				{
					IConsoleVariable* V = FindVariable(Name);
					if (Option.IsValid() && V)
					{
						V->Set(*Option, ECVF_SetByConsole);
					}
				})
				[
					SNew(STextBlock).Text_Lambda([Name]()
					{
						const IConsoleVariable* V = FindVariable(Name);
						return FText::FromString(V ? FString::Printf(TEXT("%d  %s"), V->GetInt(), *RunStartVariantLabel(V->GetInt())) : FString());
					})
				]
			]
		];
	}
	else if (bLevels)
	{
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(70.f)
			[
				SNew(SSpinBox<int32>)
				.IsEnabled(!bReadOnly)
				.MinValue(TOptional<int32>())
				.MaxValue(TOptional<int32>())
				.Delta(1)
				.Value_Lambda([Name]()
				{
					const IConsoleVariable* V = FindVariable(Name);
					return V ? V->GetInt() : 0;
				})
				.OnValueCommitted_Lambda([Name](int32 Value, ETextCommit::Type)
				{
					if (IConsoleVariable* V = FindVariable(Name))
					{
						V->Set(Value, ECVF_SetByConsole);
					}
				})
			]
		];
	}
	else if (Variable->IsVariableFloat())
	{
		Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(90.f)
			[
				SNew(SSpinBox<float>)
				.IsEnabled(!bReadOnly)
				.MinValue(TOptional<float>())
				.MaxValue(TOptional<float>())
				.Value_Lambda([Name]()
				{
					const IConsoleVariable* V = FindVariable(Name);
					return V ? V->GetFloat() : 0.f;
				})
				.OnValueCommitted_Lambda([Name](float Value, ETextCommit::Type)
				{
					if (IConsoleVariable* V = FindVariable(Name))
					{
						V->Set(Value, ECVF_SetByConsole);
					}
				})
			]
		];
	}
	else if (Variable->IsVariableString())
	{
		Row->AddSlot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SEditableTextBox)
			.IsReadOnly(bReadOnly)
			.Text_Lambda([Name]()
			{
				const IConsoleVariable* V = FindVariable(Name);
				return V ? FText::FromString(V->GetString()) : FText::GetEmpty();
			})
			.OnTextCommitted_Lambda([Name](const FText& Text, ETextCommit::Type)
			{
				if (IConsoleVariable* V = FindVariable(Name))
				{
					V->Set(*Text.ToString(), ECVF_SetByConsole);
				}
			})
		];
	}

	return Row;
}

FReply SGSTN_DebugOptionsPanel::OnRefresh()
{
	Rebuild();
	return FReply::Handled();
}

FReply SGSTN_DebugOptionsPanel::OnApplyKit()
{
	APlayerController* Controller = FGSTN_DebugKitPicker::FindPlayController();
	FString Error;
	if (!SelectedKit.IsValid() || !Controller)
	{
		KitStatus = LOCTEXT("KitNoPlay", "start Play first");
	}
	else if (FGSTN_DebugKitPicker::RequestKit(*Controller, *SelectedKit, Error))
	{
		KitStatus = FText::Format(LOCTEXT("KitSent", "asked for {0}"), FText::FromString(*SelectedKit));
	}
	else
	{
		KitStatus = FText::FromString(Error);
	}
	return FReply::Handled();
}

FReply SGSTN_DebugOptionsPanel::OnForceSkin(bool bAllPawns)
{
	APlayerController* Controller = FindPlayController();
	FString Error;
	if (!SelectedSkin.IsValid() || !Controller)
	{
		SkinStatus = LOCTEXT("SkinNoPlay", "start Play first");
	}
	else if (FGSTN_DebugSkinPicker::ForceSkin(*Controller, *SelectedSkin, bAllPawns, Error))
	{
		SkinStatus = FText::Format(LOCTEXT("SkinForced", "forced {0}"), FText::FromString(*SelectedSkin));
	}
	else
	{
		SkinStatus = FText::FromString(Error);
	}
	return FReply::Handled();
}

FReply SGSTN_DebugOptionsPanel::OnReleaseSkin()
{
	APlayerController* Controller = FindPlayController();
	FString Error;
	if (Controller && FGSTN_DebugSkinPicker::ForceSkin(*Controller, TEXT("off"), true, Error))
	{
		SkinStatus = LOCTEXT("SkinReleased", "released");
	}
	else
	{
		SkinStatus = Controller ? FText::FromString(Error) : LOCTEXT("SkinNoPlay", "start Play first");
	}
	return FReply::Handled();
}

bool SGSTN_DebugOptionsPanel::HasPlayWorld() const
{
	return FindPlayController() != nullptr;
}

FReply SGSTN_DebugOptionsPanel::OnToggleGameplayDebugger()
{
	if (APlayerController* Controller = FindPlayController())
	{
		Controller->ConsoleCommand(TEXT("EnableGDT"));
	}
	return FReply::Handled();
}

void SGSTN_DebugOptionsPanel::OnFilterChanged(const FText& Text)
{
	Filter = Text.ToString().TrimStartAndEnd();
	Rebuild();
}

#undef LOCTEXT_NAMESPACE
