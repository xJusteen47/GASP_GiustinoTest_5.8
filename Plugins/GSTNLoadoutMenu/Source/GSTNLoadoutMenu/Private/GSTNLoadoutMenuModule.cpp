// Pre-mission loadout screen: module, rules, the console toggles and the input dump.

#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "GSTN_LoadoutMenuRules.h"
#include "GSTN_LoadoutMenuSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

bool UGSTN_LoadoutMenuRules::ShouldAutoOpenIn(const UWorld& World) const
{
	if (bAutoOpenEverywhere)
	{
		return true;
	}
	const FString LevelPackage = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	return AutoOpenLevels.ContainsByPredicate([&LevelPackage](const TSoftObjectPtr<UWorld>& Level)
	{
		return Level.ToSoftObjectPath().GetLongPackageName() == LevelPackage;
	});
}

namespace
{
	/** Opens the screen (bDebug: the bare debug list) for every local player of the world, or closes it where it is open; a refusal says why */
	void ToggleMenus(UWorld* World, bool bDebug)
	{
		if (!World || !GEngine)
		{
			return;
		}
		for (ULocalPlayer* Player : GEngine->GetGamePlayers(World))
		{
			UGSTN_LoadoutMenuSubsystem* Menu = Player ? Player->GetSubsystem<UGSTN_LoadoutMenuSubsystem>() : nullptr;
			if (!Menu)
			{
				continue;
			}
			if (Menu->IsMenuOpen())
			{
				Menu->CloseMenu();
			}
			else if (!Menu->OpenMenu(bDebug))
			{
				const FString Message = FString::Printf(TEXT("Loadout screen not opened for %s: %s"), *Player->GetName(), *Menu->GetOpenProblem());
				UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Orange, Message);
			}
		}
	}

	/** Testing and levels that do not open it by themselves */
	FAutoConsoleCommandWithWorld ToggleCommand(
		TEXT("GSTN.LoadoutMenu"),
		TEXT("Opens the loadout screen for every local player (closes it where it is open)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { ToggleMenus(World, false); }));

	FAutoConsoleCommandWithWorld ToggleDebugCommand(
		TEXT("GSTN.LoadoutMenuDebug"),
		TEXT("Opens the bare debug loadout list for every local player (closes it where it is open), as the loadout debug pad does."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { ToggleMenus(World, true); }));

	/**
	 * Logs, for every local player of the world, the mapping contexts Enhanced Input has applied (with their priority) and every key
	 * mapping that survived the priority/consume pass: the quickest way to see which context owns a pad button in a playtest
	 */
	FAutoConsoleCommandWithWorld DumpCommand(
		TEXT("GSTN.InputDump"),
		TEXT("Logs the applied mapping contexts and the live key mappings of every local player."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!World || !GEngine)
			{
				return;
			}
			for (ULocalPlayer* Player : GEngine->GetGamePlayers(World))
			{
				const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
				const UEnhancedPlayerInput* PlayerInput = Input ? Input->GetPlayerInput() : nullptr;
				if (!PlayerInput)
				{
					continue;
				}
				const UGSTN_LoadoutMenuSubsystem* Menu = Player->GetSubsystem<UGSTN_LoadoutMenuSubsystem>();
				UE_LOG(LogTemp, Display, TEXT("GSTNInputDump %s menu=%d"), *Player->GetName(), Menu && Menu->IsMenuOpen() ? 1 : 0);
				// The applied set is not public: ask the subsystem about every context in memory (debug only)
				for (TObjectIterator<UInputMappingContext> It; It; ++It)
				{
					int32 Priority = 0;
					if (Input->HasMappingContext(*It, Priority))
					{
						UE_LOG(LogTemp, Display, TEXT("GSTNInputDump   context %s priority %d"), *It->GetPathName(), Priority);
					}
				}
				for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->GetEnhancedActionMappingsView())
				{
					UE_LOG(LogTemp, Display, TEXT("GSTNInputDump   %s -> %s%s"), *Mapping.Key.ToString(), *GetPathNameSafe(Mapping.Action),
						Mapping.Action && Mapping.Action->bConsumeInput ? TEXT(" (consumes)") : TEXT(""));
				}
			}
		}));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNLoadoutMenu)
