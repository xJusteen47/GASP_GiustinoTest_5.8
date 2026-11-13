// Platform parts bridge: console commands for the PIE harness (not in Shipping).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformPartsComponent.h"
#include "HAL/IConsoleManager.h"
#include "Sim/GSTN_PartsAttach.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPlatformPartsDebug, Log, All);

namespace GSTNPlatformPartsDebug
{
	template <typename TFunc>
	void ForEachPlatform(TFunc&& Func)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (UGSTN_EnemyPlatformComponent* Platform = It->FindComponentByClass<UGSTN_EnemyPlatformComponent>())
				{
					Func(**It, *Platform);
				}
			}
		}
	}

	/**
	 * Turns every platform of every game world into a fighter with parts, the way a kit would (same set on every machine): shape ShapeId
	 * names part PartName, a one-part set is attached, a bridge component is added. For the harness's test fighter, which has no combat
	 * component: the broken mask then comes from gstn.PlatformParts.SetBrokenMask
	 */
	static FAutoConsoleCommand CmdAttachTest(
		TEXT("gstn.PlatformParts.AttachTest"),
		TEXT("gstn.PlatformParts.AttachTest [PartName=Flank] [ShapeId=0]: on every platform of every game world, shape ShapeId names a part of a one-part test set, attached with a parts bridge"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FName PartName(Args.Num() > 0 ? *Args[0] : TEXT("Flank"));
			const int32 ShapeId = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			static TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
			if (!Set.IsValid() || Set->Parts.Num() != 1 || Set->Parts[0].Name != PartName)
			{
				Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
				FGSTN_PartDefinition Part;
				Part.Name = PartName;
				Part.PartCap = 100.f;
				Set->Parts.Add(Part);
			}
			ForEachPlatform([&](AActor& Actor, UGSTN_EnemyPlatformComponent& Platform)
			{
				for (FGSTN_PlatformShape& Shape : Platform.Spec.Shapes)
				{
					if (Shape.ShapeId == ShapeId)
					{
						Shape.PartName = PartName;
					}
				}
				// The bridge replicates (the authority's mask and frame): created on the authority, the clients get it from replication
				if (Actor.HasAuthority() && !Actor.FindComponentByClass<UGSTN_PlatformPartsComponent>())
				{
					UGSTN_PlatformPartsComponent* Bridge = NewObject<UGSTN_PlatformPartsComponent>(&Actor, TEXT("PlatformParts"));
					Bridge->RegisterComponent();
				}
				const bool bAttached = FGSTN_PartsAttach::ApplyPartSet(&Actor, Set.Get(), nullptr);
				UE_LOG(LogGSTNPlatformPartsDebug, Log, TEXT("%s: shape %d -> part %s, set attached %d"), *Actor.GetPathName(), ShapeId, *PartName.ToString(), bAttached ? 1 : 0);
			});
		}));

	/** The broken mask every bridge of every game world reads, in place of the combat state; -1 goes back to the combat state */
	static FAutoConsoleCommand CmdSetBrokenMask(
		TEXT("gstn.PlatformParts.SetBrokenMask"),
		TEXT("gstn.PlatformParts.SetBrokenMask Mask: every parts bridge of every game world reads this broken mask (bit per part) instead of the combat state; -1 = back to the combat state"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				return;
			}
			const int32 Value = FCString::Atoi(*Args[0]);
			ForEachPlatform([&](AActor& Actor, UGSTN_EnemyPlatformComponent& Platform)
			{
				if (UGSTN_PlatformPartsComponent* Bridge = Actor.FindComponentByClass<UGSTN_PlatformPartsComponent>())
				{
					Bridge->SetBrokenMaskOverride(Value < 0 ? TOptional<uint16>() : TOptional<uint16>(static_cast<uint16>(Value & 0xFFFF)));
					UE_LOG(LogGSTNPlatformPartsDebug, Log, TEXT("%s: broken mask %d from frame %d"), *Actor.GetPathName(), Value, Platform.GetLatestPoseFrame() + 1);
				}
			});
		}));
}

#endif
