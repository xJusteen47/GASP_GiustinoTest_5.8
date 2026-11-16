// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Kit/GSTN_DebugKitPicker.h"

#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutSubsystem.h"
#include "UObject/ObjectKey.h"

namespace
{
	TAutoConsoleVariable<FString> CVarKitAtStart(TEXT("gstn.Debug.KitAtStart"), TEXT(""),
		TEXT("Kit (asset name) every local player gets once when its pawn first appears in a play session. Empty = off. Debug tooling."), ECVF_Default);

	FAutoConsoleCommand KitCommand(TEXT("gstn.Debug.Kit"),
		TEXT("gstn.Debug.Kit <KitName>: gives the first local player of the play session that kit (same server path as the loadout screen)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			APlayerController* Controller = FGSTN_DebugKitPicker::FindPlayController();
			FString Error;
			if (Args.Num() < 1 || !Controller || !FGSTN_DebugKitPicker::RequestKit(*Controller, Args[0], Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("gstn.Debug.Kit: %s"), Args.Num() < 1 ? TEXT("give a kit name") : !Controller ? TEXT("no play session") : *Error);
			}
		}));

	FTSTicker::FDelegateHandle TickHandle;
	/** Controllers that already got the start kit (weak keys: a new play session has new controllers) */
	TSet<FObjectKey> Served;

	UGSTN_LoadoutComponent* FindLoadout(const APlayerController& Controller)
	{
		const APawn* Pawn = Controller.GetPawn();
		if (const APlayerState* State = Controller.PlayerState)
		{
			if (UGSTN_LoadoutComponent* Found = State->FindComponentByClass<UGSTN_LoadoutComponent>())
			{
				return Found;
			}
		}
		return Pawn ? Pawn->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
	}

	bool TickStartKit(float)
	{
		const FString Kit = CVarKitAtStart.GetValueOnGameThread().TrimStartAndEnd();
		if (Kit.IsEmpty() || !GEngine)
		{
			return true;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game) || !World->HasBegunPlay())
			{
				continue;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Controller = It->Get();
				if (!Controller || !Controller->IsLocalController() || !Controller->GetPawn() || Served.Contains(FObjectKey(Controller)))
				{
					continue;
				}
				FString Error;
				if (FGSTN_DebugKitPicker::RequestKit(*Controller, Kit, Error))
				{
					Served.Add(FObjectKey(Controller));
				}
			}
		}
		return true;
	}
}

void FGSTN_DebugKitPicker::Startup()
{
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickStartKit), 0.5f);
	}
}

void FGSTN_DebugKitPicker::Shutdown()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Served.Reset();
}

TArray<FString> FGSTN_DebugKitPicker::GetKitNames()
{
	TArray<FString> Names;
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> Ids;
		Manager->GetPrimaryAssetIdList(GSTNCombatAssetTypes::PrimaryKit(), Ids);
		for (const FPrimaryAssetId& Id : Ids)
		{
			Names.Add(Id.PrimaryAssetName.ToString());
		}
	}
	Names.Sort();
	return Names;
}

bool FGSTN_DebugKitPicker::RequestKit(APlayerController& Controller, const FString& KitName, FString& Error)
{
	UGSTN_LoadoutComponent* Loadout = FindLoadout(Controller);
	if (!Loadout)
	{
		Error = TEXT("no loadout component on the PlayerState or the pawn");
		return false;
	}
	const FPrimaryAssetId Id(GSTNCombatAssetTypes::PrimaryKit(), FName(*KitName));
	if (!GetKitNames().Contains(KitName))
	{
		Error = FString::Printf(TEXT("unknown kit %s"), *KitName);
		return false;
	}
	FGSTN_LoadoutData Data;
	Data.PrimaryKit = Id;
	Loadout->ServerRequestLoadout(Data);
	if (const ULocalPlayer* Player = Controller.GetLocalPlayer())
	{
		if (UGSTN_LoadoutSubsystem* Saved = Player->GetSubsystem<UGSTN_LoadoutSubsystem>())
		{
			Saved->SaveLoadout(TEXT("Last"), Data);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("gstn.Debug.Kit: %s asked for %s"), *Controller.GetName(), *KitName);
	return true;
}

APlayerController* FGSTN_DebugKitPicker::FindPlayController()
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
