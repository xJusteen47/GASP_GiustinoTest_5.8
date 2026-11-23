// Editor tooling of kit mods (M11 L8). Editor only.

#include "ContentBrowserMenuContexts.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModsEditorRules.h"
#include "GSTN_KitModsEditorUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "GSTNKitModsEditor"

namespace
{
	/** Preview on kit > <kit> > <move>: the kits the selected mods fit, the moves they change there. Kits are loaded when the menu opens */
	void FillKitMenu(UToolMenu* Menu, TArray<TWeakObjectPtr<UGSTN_KitModDefinition>> Mods)
	{
		TArray<UGSTN_KitModDefinition*> Loaded;
		for (const TWeakObjectPtr<UGSTN_KitModDefinition>& Mod : Mods)
		{
			if (Mod.IsValid())
			{
				Loaded.Add(Mod.Get());
			}
		}
		TArray<const UGSTN_PrimaryKitDefinition*> Kits;
		FGSTN_KitModsEditorRules::GatherKits(Kits);
		FToolMenuSection& Section = Menu->AddSection("Kits", LOCTEXT("Kits", "Kits these mods fit"));
		for (const UGSTN_PrimaryKitDefinition* Kit : Kits)
		{
			const bool bFitsAll = Kit && !Loaded.IsEmpty() && !Loaded.ContainsByPredicate([Kit](const UGSTN_KitModDefinition* Mod) { return !FGSTN_KitModsEditorRules::ModFitsKit(*Mod, *Kit); });
			if (!bFitsAll)
			{
				continue;
			}
			UGSTN_PrimaryKitDefinition* MutableKit = const_cast<UGSTN_PrimaryKitDefinition*>(Kit);
			const TArray<FGameplayTag> Commands = UGSTN_KitModsEditorUtils::GetModdedCommands(MutableKit, Loaded);
			Section.AddSubMenu(FName(*Kit->GetPathName()), FText::FromString(Kit->GetName()), LOCTEXT("KitTip", "The moves these mods change on this kit"),
				FNewToolMenuDelegate::CreateLambda([WeakKit = TWeakObjectPtr<UGSTN_PrimaryKitDefinition>(MutableKit), Commands, Loaded](UToolMenu* MoveMenu)
				{
					FToolMenuSection& Moves = MoveMenu->AddSection("Moves", LOCTEXT("Moves", "Modded moves"));
					if (Commands.IsEmpty())
					{
						Moves.AddMenuEntry("None", LOCTEXT("NoMoves", "No move changes"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([] { return false; })));
					}
					for (const FGameplayTag& Command : Commands)
					{
						Moves.AddMenuEntry(Command.GetTagName(), FText::FromName(Command.GetTagName()), LOCTEXT("MoveTip", "Open the modded timeline of this move in the timeline editor"),
							FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([WeakKit, Loaded, Command]
							{
								FString Message;
								UGSTN_KitModsEditorUtils::OpenPreview(WeakKit.Get(), Loaded, Command, Message);
								UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
							})));
					}
				}));
		}
	}
}

class FGSTNKitModsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_KitModsEditorRules::Register();
		PreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GSTN.KitMods.Preview"),
			TEXT("GSTN.KitMods.Preview KitPath ModPath[+ModPath...] CommandTag: opens the modded timeline of that move in the timeline editor"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&UGSTN_KitModsEditorUtils::RunConsoleCommand),
			ECVF_Default);
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGSTNKitModsEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (PreviewCommand)
		{
			IConsoleManager::Get().UnregisterConsoleObject(PreviewCommand);
			PreviewCommand = nullptr;
		}
		FGSTN_KitModsEditorRules::Unregister();
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.GSTN_KitModDefinition");
		if (!Menu)
		{
			return;
		}
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("GSTNKitModsPreview", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context)
			{
				return;
			}
			TArray<TWeakObjectPtr<UGSTN_KitModDefinition>> Mods;
			for (UGSTN_KitModDefinition* Mod : Context->LoadSelectedObjects<UGSTN_KitModDefinition>())
			{
				Mods.Add(Mod);
			}
			InSection.AddSubMenu("GSTNKitModsPreviewOnKit", LOCTEXT("PreviewOnKit", "Preview on kit"),
				LOCTEXT("PreviewOnKitTip", "Open a move of a kit, modded by the selected mods, in the timeline editor (a copy: edits are not saved)"),
				FNewToolMenuDelegate::CreateLambda([Mods](UToolMenu* SubMenu) { FillKitMenu(SubMenu, Mods); }));
		}));
	}

	IConsoleObject* PreviewCommand = nullptr;
};

IMPLEMENT_MODULE(FGSTNKitModsEditorModule, GSTNKitModsEditor)

#undef LOCTEXT_NAMESPACE
