// Piggyback, editor part (editor only; the runtime never depends on it).

#include "GSTN_PiggybackLinkDebug.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_LinkRegistry.h"
#include "GSTNPiggybackEditorModule.h"
#include "HAL/IConsoleManager.h"
#include "Sim/GSTN_LinkFragment.h"
#include "State/GSTN_LinkState.h"
#include "UObject/UObjectIterator.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

namespace
{
	int32 GDebugAnchor = 0;
	FAutoConsoleVariableRef CVarDebugAnchor(TEXT("gstn.Combat.Debug.Anchor"), GDebugAnchor,
		TEXT("1 (PIE) draws, in each client world, the hurtbox anchor of every parked link role as that client's snapshot has it (green) and as the server's has it (red), with the distance."),
		ECVF_Cheat);

	int32 GDebugLinkLog = 0;
	FAutoConsoleVariableRef CVarDebugLinkLog(TEXT("gstn.Combat.Debug.LinkLog"), GDebugLinkLog,
		TEXT("1 (PIE) logs one line per change of a pawn's shown Body link (type, role, phase, serial) in every world."),
		ECVF_Cheat);

	/** What the log compares: a change of any of these is one line */
	struct FShownLink
	{
		uint8 LinkTypeIndex = 0;
		uint8 Role = 0;
		uint8 Phase = 0;
		uint8 Serial = 0;

		bool operator==(const FShownLink& Other) const
		{
			return LinkTypeIndex == Other.LinkTypeIndex && Role == Other.Role && Phase == Other.Phase && Serial == Other.Serial;
		}
	};

	TMap<TWeakObjectPtr<const AActor>, FShownLink> GLastShown;

	const FGSTN_SnapshotEntry* FindEntryOf(const UGSTN_CombatWorldSubsystem& Combat, const AActor* Owner)
	{
		for (const FGSTN_SnapshotEntry& Entry : Combat.GetSnapshot().Entries)
		{
			if (Combat.FindOwnerActor(Entry.EntityId) == Owner)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	const FGSTN_SnapshotEntry* FindEntryById(const UGSTN_CombatWorldSubsystem& Combat, int32 EntityId)
	{
		for (const FGSTN_SnapshotEntry& Entry : Combat.GetSnapshot().Entries)
		{
			if (Entry.EntityId == EntityId)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	FString LinkText(const FShownLink& Link)
	{
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		if (!Registry || (Link.LinkTypeIndex == 0 && Link.Role == 0))
		{
			return FString::Printf(TEXT("idle (phase %d)"), Link.Phase);
		}
		const FGameplayTag Type = Registry->GetLinkType(Link.LinkTypeIndex);
		const FGameplayTag Role = Registry->GetRoleTag(Link.LinkTypeIndex, Link.Role);
		return FString::Printf(TEXT("%s %s phase %d serial %d"), Type.IsValid() ? *Type.GetTagName().ToString() : TEXT("(type?)"),
			Role.IsValid() ? *Role.GetTagName().ToString() : TEXT("(role?)"), Link.Phase, Link.Serial);
	}
}

void FGSTN_PiggybackLinkDebug::Reset()
{
	GLastShown.Reset();
}

bool FGSTN_PiggybackLinkDebug::Tick(float DeltaTime)
{
	if ((GDebugAnchor == 0 && GDebugLinkLog == 0) || !GEngine)
	{
		if (GLastShown.Num() > 0)
		{
			Reset();
		}
		return true;
	}

	// The game worlds of this process and a label for each; the server's world is the one the client anchors compare with
	TMap<const UWorld*, FString> Labels;
	const UWorld* ServerWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		const UWorld* World = Context.World();
		if (!World || !World->IsGameWorld())
		{
			continue;
		}
		const ENetMode NetMode = World->GetNetMode();
		if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
		{
			ServerWorld = World;
		}
		const TCHAR* Mode = NetMode == NM_Client ? TEXT("Client") : NetMode == NM_Standalone ? TEXT("Standalone") : TEXT("Server");
		Labels.Add(World, FString::Printf(TEXT("%s %d"), Mode, Context.PIEInstance));
	}
	const UGSTN_CombatWorldSubsystem* ServerCombat = ServerWorld ? UGSTN_CombatWorldSubsystem::Get(ServerWorld) : nullptr;

	for (TObjectIterator<UGSTN_LinkPresentationComponent> It; It; ++It)
	{
		const UGSTN_LinkPresentationComponent* Presentation = *It;
		const AActor* Owner = Presentation ? Presentation->GetOwner() : nullptr;
		const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		const FString* Label = World ? Labels.Find(World) : nullptr;
		if (!Label)
		{
			continue;
		}

		if (GDebugLinkLog != 0)
		{
			FShownLink Shown;
			if (const FGSTN_LinkFragment* Fragment = UGSTN_LinkPresentationComponent::FindShownFragment(Owner))
			{
				const FGSTN_LinkState& Body = Fragment->Links.Body;
				Shown = { Body.LinkTypeIndex, Body.Role, static_cast<uint8>(Body.Phase), Body.Serial };
			}
			const FShownLink* Last = GLastShown.Find(Owner);
			if (!Last || !(*Last == Shown))
			{
				// The first sight of an idle pawn is not a transition
				if (Last || Shown.Role != 0 || Shown.Phase != 0)
				{
					UE_LOG(LogGSTNPiggybackEditor, Log, TEXT("[link] %s %s: %s -> %s"), **Label, *Owner->GetName(),
						Last ? *LinkText(*Last) : TEXT("(first seen)"), *LinkText(Shown));
				}
				GLastShown.Add(Owner, Shown);
			}
		}

#if ENABLE_DRAW_DEBUG
		FTransform LinkedTransform;
		if (GDebugAnchor != 0 && World != ServerWorld && ServerCombat && Presentation->GetLinkedTransform(LinkedTransform))
		{
			const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
			const FGSTN_SnapshotEntry* Local = Combat ? FindEntryOf(*Combat, Owner) : nullptr;
			const FGSTN_SnapshotEntry* Server = Local ? FindEntryById(*ServerCombat, Local->EntityId) : nullptr;
			if (Local)
			{
				const FVector LocalAnchor = Local->Transform.GetLocation();
				DrawDebugSphere(World, LocalAnchor, 8.f, 8, FColor::Green, false, -1.f, 0, 1.5f);
				if (Server)
				{
					const FVector ServerAnchor = Server->Transform.GetLocation();
					DrawDebugSphere(World, ServerAnchor, 8.f, 8, FColor::Red, false, -1.f, 0, 1.5f);
					DrawDebugLine(World, LocalAnchor, ServerAnchor, FColor::Yellow, false, -1.f, 0, 1.f);
					DrawDebugString(World, LocalAnchor + FVector(0.f, 0.f, 30.f), FString::Printf(TEXT("anchor %.1f cm"), FVector::Dist(LocalAnchor, ServerAnchor)),
						nullptr, FColor::Yellow, 0.f, true);
				}
			}
		}
#endif
	}

	// Pawns that went away
	for (auto It = GLastShown.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	return true;
}
