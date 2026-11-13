// Pre-mission loadout screen: module, rules and the console toggle.

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GSTN_LoadoutMenuRules.h"
#include "GSTN_LoadoutMenuSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

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
	/** Opens the screen for every local player of the world, or closes it where it is open: testing and levels that do not open it by themselves */
	FAutoConsoleCommandWithWorld ToggleCommand(
		TEXT("GSTN.LoadoutMenu"),
		TEXT("Opens the loadout screen for every local player (closes it where it is open)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (!World || !GEngine)
			{
				return;
			}
			for (ULocalPlayer* Player : GEngine->GetGamePlayers(World))
			{
				if (UGSTN_LoadoutMenuSubsystem* Menu = Player ? Player->GetSubsystem<UGSTN_LoadoutMenuSubsystem>() : nullptr)
				{
					Menu->IsMenuOpen() ? Menu->CloseMenu() : static_cast<void>(Menu->OpenMenu());
				}
			}
		}));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNLoadoutMenu)
