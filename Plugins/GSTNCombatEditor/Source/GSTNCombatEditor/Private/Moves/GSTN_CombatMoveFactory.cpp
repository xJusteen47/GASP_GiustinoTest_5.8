// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Moves/GSTN_CombatMoveFactory.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AssetTypeCategories.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameplayAbilityBlueprint.h"
#include "GSTNCombatEditorModule.h"
#include "IStructureDetailsView.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatMoveFactory"

namespace GSTNCombatMoveDialog
{
	/**
	 * A modal window that edits the struct at Data in a details panel, with OK and Cancel. Returns whether OK was pressed; the struct is edited in
	 * place either way, so the caller passes a copy it can drop. Never shown in an unattended run (a commandlet, a test, a headless script)
	 */
	bool Show(UScriptStruct* Struct, uint8* Data, const FText& Title, const FText& Hint, const FText& OkLabel)
	{
		if (FApp::IsUnattended() || !GEditor || !FSlateApplication::IsInitialized())
		{
			return true;
		}
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FDetailsViewArgs ViewArgs;
		ViewArgs.bAllowSearch = false;
		ViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		const TSharedRef<FStructOnScope> Scope = MakeShared<FStructOnScope>(Struct, Data);
		const TSharedRef<IStructureDetailsView> Details = PropertyEditor.CreateStructureDetailView(ViewArgs, FStructureDetailsViewArgs(), Scope);

		bool bOk = false;
		const TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(560.f, 680.f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		const TWeakPtr<SWindow> WeakWindow = Window;
		Window->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
			[
				SNew(STextBlock).Text(Hint).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				Details->GetWidget().ToSharedRef()
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(OkLabel)
					.OnClicked_Lambda([&bOk, WeakWindow]()
					{
						bOk = true;
						if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked_Lambda([WeakWindow]()
					{
						if (const TSharedPtr<SWindow> Pinned = WeakWindow.Pin())
						{
							Pinned->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]);
		GEditor->EditorAddModalWindow(Window);
		return bOk;
	}

	/** A toast with the report (also in the log), so the designer sees what was bound or why not */
	void Notify(const FString& Report, bool bSuccess)
	{
		if (Report.IsEmpty())
		{
			return;
		}
		UE_LOG(LogGSTNCombatEditor, Display, TEXT("Combat Move: %s"), *Report.Replace(TEXT("\n"), TEXT(" | ")));
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}
		FNotificationInfo Info(FText::FromString(Report));
		Info.ExpireDuration = bSuccess ? 6.f : 12.f;
		Info.bUseSuccessFailIcons = true;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}

UGSTN_CombatMoveFactory::UGSTN_CombatMoveFactory()
{
	SupportedClass = UGameplayAbilityBlueprint::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UGSTN_CombatMoveFactory::ConfigureProperties()
{
	FGSTN_NewCombatMoveOptions Edited = Options;
	if (!GSTNCombatMoveDialog::Show(FGSTN_NewCombatMoveOptions::StaticStruct(), reinterpret_cast<uint8*>(&Edited), LOCTEXT("NewMoveTitle", "New Combat Move"),
		LOCTEXT("NewMoveHint", "Every value is a starting point: the frames, the hitbox and the binding are plain data you change later in the timeline "
			"editor and in the kit. Kit and Command are optional; with them the move is playable at once."),
		LOCTEXT("NewMoveOk", "Create")))
	{
		return false;
	}
	Options = Edited;
	return true;
}

UObject* UGSTN_CombatMoveFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FString Report;
	FString Error;
	UBlueprint* Move = UGSTN_CombatMoveAuthoring::CreateCombatMoveIn(InParent, InName, Flags, Options, Report, Error);
	if (!Error.IsEmpty())
	{
		GSTNCombatMoveDialog::Notify(Error, false);
	}
	else
	{
		// The toast is a failure when a binding was asked for and the report says it did not happen
		GSTNCombatMoveDialog::Notify(Report, !(Options.Hookup.WantsBinding() && Report.Contains(TEXT("nothing bound"))));
	}
	return Move;
}

FText UGSTN_CombatMoveFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Combat Move");
}

FText UGSTN_CombatMoveFactory::GetToolTip() const
{
	return LOCTEXT("ToolTip", "A new combat move: an ability Blueprint with a starting timeline (a hit, an action key or nothing), optionally bound to a kit or a weapon");
}

uint32 UGSTN_CombatMoveFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Gameplay;
}

const TArray<FText>& UGSTN_CombatMoveFactory::GetMenuCategorySubMenus() const
{
	static const TArray<FText> SubMenus = { LOCTEXT("CombatSubMenu", "Combat") };
	return SubMenus;
}

FString UGSTN_CombatMoveFactory::GetDefaultNewAssetName() const
{
	return TEXT("GA_NewMove");
}

namespace GSTNCombatMoveMenu
{
	static const FName OwnerName(TEXT("GSTNCombatMoveMenu"));
	static FDelegateHandle StartupHandle;

	void AddSelectedMovesToKit(TArray<TWeakObjectPtr<UBlueprint>> Moves)
	{
		FGSTN_MoveKitHookup Hookup;
		if (!GSTNCombatMoveDialog::Show(FGSTN_MoveKitHookup::StaticStruct(), reinterpret_cast<uint8*>(&Hookup), LOCTEXT("AddToKitTitle", "Add to Kit or Weapon"),
			LOCTEXT("AddToKitHint", "Binds the selected moves to a command of the kit and/or the weapon. A binding with the same command and the same "
				"requirements is replaced; the others stay. Save the kit afterwards (Ctrl+Z undoes it)."),
			LOCTEXT("AddToKitOk", "Bind")))
		{
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("AddToKitTransaction", "Add Combat Move to Kit"));
		TArray<FString> Lines;
		bool bAll = true;
		for (const TWeakObjectPtr<UBlueprint>& Weak : Moves)
		{
			if (UBlueprint* Move = Weak.Get(); Move && Move->GeneratedClass)
			{
				FString Report;
				bAll &= UGSTN_CombatMoveAuthoring::AddMoveToKit(Move->GeneratedClass.Get(), Hookup, Report);
				Lines.Add(Report);
			}
		}
		GSTNCombatMoveDialog::Notify(FString::Join(Lines, TEXT("\n")), bAll);
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(OwnerName);
		// Move Blueprints are Blueprint assets (a Gameplay Ability Blueprint, or a plain one): extend the Blueprint menu and filter by parent class
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UBlueprint::StaticClass());
		if (!Menu)
		{
			return;
		}
		Menu->FindOrAddSection("GetAssetActions").AddDynamicEntry("GSTNCombatMoveAddToKit", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context)
			{
				return;
			}
			// Filter on the asset tags first (no load), then load only the moves
			bool bAnyMove = false;
			for (const FAssetData& Asset : Context->GetSelectedAssetsOfType(UBlueprint::StaticClass()))
			{
				const UClass* Parent = UBlueprint::GetBlueprintParentClassFromAssetTags(Asset);
				bAnyMove |= Parent && Parent->IsChildOf(UGSTN_GA_BaseCombat::StaticClass());
			}
			if (!bAnyMove)
			{
				return;
			}
			FToolUIAction Action;
			Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
			{
				const UContentBrowserAssetContextMenuContext* MenuContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
				if (!MenuContext)
				{
					return;
				}
				TArray<TWeakObjectPtr<UBlueprint>> Moves;
				for (UBlueprint* Blueprint : MenuContext->LoadSelectedObjects<UBlueprint>())
				{
					if (UGSTN_CombatMoveAuthoring::IsCombatMoveBlueprint(Blueprint))
					{
						Moves.Add(Blueprint);
					}
				}
				AddSelectedMovesToKit(MoveTemp(Moves));
			});
			InSection.AddMenuEntry("GSTNCombatMoveAddToKit", LOCTEXT("AddToKit", "Add to Kit or Weapon..."),
				LOCTEXT("AddToKitTip", "Bind the selected combat moves to a command of a kit or a weapon"), FSlateIcon(), Action);
		}));
	}

	void Register()
	{
		StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
	}

	void Unregister()
	{
		if (UToolMenus::TryGet())
		{
			UToolMenus::UnRegisterStartupCallback(StartupHandle);
			UToolMenus::UnregisterOwner(OwnerName);
		}
	}
}

#undef LOCTEXT_NAMESPACE
