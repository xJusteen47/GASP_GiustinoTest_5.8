// Minion runtime (replication, M8 G6).

#include "Net/GSTN_MinionClientRegistry.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTNMinionCoreModule.h"
#include "Net/GSTN_MinionNetManager.h"
#include "Net/GSTN_MinionNetRelevancy.h"
#include "Net/GSTN_MinionPack.h"

bool UGSTN_MinionClientRegistry::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_MinionClientRegistry::OnItemReceived(const AGSTN_MinionPack& Pack, const FGSTN_MinionNetWireItem& Wire)
{
	const int32 PackId = Pack.GetPackId();
	if (PackId == INDEX_NONE)
	{
		return;
	}
	FGSTN_MinionClientItem& Item = Items.FindOrAdd(MakeGlobalId(PackId, Wire.Id));
	Item.PackId = PackId;
	Item.ItemId = Wire.Id;
	Item.Wire = Wire;
	Item.Pack = &Pack;
	++Item.Updates;
	++ItemUpdatesReceived;
}

void UGSTN_MinionClientRegistry::OnItemRemoved(const AGSTN_MinionPack& Pack, uint8 ItemId)
{
	const int32 GlobalId = MakeGlobalId(Pack.GetPackId(), ItemId);
	const FGSTN_MinionClientItem* Item = Items.Find(GlobalId);
	// Only the pack that wrote the entry removes it (a reused pack id may already have written a new one)
	if (Item && Item->Pack.Get() == &Pack)
	{
		Items.Remove(GlobalId);
	}
}

void UGSTN_MinionClientRegistry::OnPackGone(const AGSTN_MinionPack& Pack)
{
	for (auto It = Items.CreateIterator(); It; ++It)
	{
		const AGSTN_MinionPack* Owner = It.Value().Pack.Get();
		if (Owner == &Pack || (Owner == nullptr && It.Value().PackId == Pack.GetPackId()))
		{
			It.RemoveCurrent();
		}
	}
}

void UGSTN_MinionClientRegistry::ApplyRegistryInfo(const FGSTN_MinionNetRegistryInfo& Info)
{
	WorldOrigin = FVector3f(Info.WorldOrigin);
	const uint32 ServerHash = static_cast<uint32>(Info.Hash);
	if (ServerHash == LastServerHash && Info.TypePaths.Num() == LastTypeCount)
	{
		return;
	}
	LastServerHash = ServerHash;
	LastTypeCount = Info.TypePaths.Num();

	// Resolve each type the way this client has it. A synchronous load, but only when the server registers a new type (a wave preloads
	// its types on the server before it spawns them; G9 can preload here the same way)
	TArray<FString> Resolved;
	Resolved.Reserve(Info.TypePaths.Num());
	Definitions.SetNum(Info.TypePaths.Num());
	for (int32 Index = 0; Index < Info.TypePaths.Num(); ++Index)
	{
		const FSoftObjectPath Path(Info.TypePaths[Index]);
		UGSTN_MinionDefinition* Definition = Path.IsValid() ? Cast<UGSTN_MinionDefinition>(Path.TryLoad()) : nullptr;
		Definitions[Index] = Definition;
		Resolved.Add(Definition ? Definition->GetPathName() : FString());
	}

	const uint32 LocalHash = FGSTN_MinionRegistryHash::Compute(Resolved);
	++RegistryChecks;
	bRegistryMatching = LocalHash == ServerHash;
	if (bRegistryMatching)
	{
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet registry check ok: %d types, hash %08x"), Info.TypePaths.Num(), ServerHash);
	}
	else
	{
		++RegistryMismatches;
		FString Missing;
		for (int32 Index = 0; Index < Resolved.Num(); ++Index)
		{
			if (Resolved[Index].IsEmpty())
			{
				Missing += FString::Printf(TEXT(" [%d]%s"), Index, *Info.TypePaths[Index]);
			}
		}
		UE_LOG(LogGSTNMinion, Error, TEXT("MinionNet registry MISMATCH: server hash %08x, client %08x, %d types; unresolved on this client:%s"),
			ServerHash, LocalHash, Info.TypePaths.Num(), Missing.IsEmpty() ? TEXT(" none (different path or order)") : *Missing);
	}
}

const UGSTN_MinionDefinition* UGSTN_MinionClientRegistry::GetDefinition(int32 TypeIndex) const
{
	return Definitions.IsValidIndex(TypeIndex) ? Definitions[TypeIndex].Get() : nullptr;
}

void UGSTN_MinionClientRegistry::DumpToLog() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const int32 PlayerId = Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
	const FString WorldName = GetNameSafe(World);

	TArray<int32> Keys;
	Items.GetKeys(Keys);
	Keys.Sort();
	TSet<int32> PackIds;
	for (const int32 Key : Keys)
	{
		const FGSTN_MinionClientItem& Item = Items[Key];
		PackIds.Add(Item.PackId);
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet client player=%d world=%s pack=%d id=%d type=%d pos=%u,%u,%u yaw=%u state=%d updates=%d"),
			PlayerId, *WorldName, Item.PackId, Item.ItemId, Item.Wire.TypeIndex, Item.Wire.PositionQ[0], Item.Wire.PositionQ[1], Item.Wire.PositionQ[2],
			Item.Wire.YawQ, Item.Wire.ActionState, Item.Updates);
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionNet client summary player=%d world=%s items=%d packs=%d updates=%lld registryChecks=%d registryOk=%d mismatches=%d"),
		PlayerId, *WorldName, Items.Num(), PackIds.Num(), ItemUpdatesReceived, RegistryChecks, bRegistryMatching ? 1 : 0, RegistryMismatches);
}
