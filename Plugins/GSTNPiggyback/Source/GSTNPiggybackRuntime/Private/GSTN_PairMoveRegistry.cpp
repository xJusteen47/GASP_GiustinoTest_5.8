// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_PairMoveRegistry.h"

#include "GSTN_LinkRegistry.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PiggybackTags.h"
#include "GSTN_Placer_Mount.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPairMove, Log, All);

namespace GSTNPairMoveRegistry
{
	TStrongObjectPtr<UGSTN_PairMoveRegistry>& Global()
	{
		static TStrongObjectPtr<UGSTN_PairMoveRegistry> Registry;
		return Registry;
	}
}

UGSTN_PairMoveRegistry* UGSTN_PairMoveRegistry::Get()
{
	return GSTNPairMoveRegistry::Global().Get();
}

void UGSTN_PairMoveRegistry::RegisterGlobal()
{
	TStrongObjectPtr<UGSTN_PairMoveRegistry>& Registry = GSTNPairMoveRegistry::Global();
	if (!Registry.IsValid())
	{
		Registry.Reset(NewObject<UGSTN_PairMoveRegistry>(GetTransientPackage(), TEXT("GSTN_PairMoveRegistry")));
	}
	if (UGSTN_LinkRegistry* LinkRegistry = UGSTN_LinkRegistry::Get())
	{
		LinkRegistry->RegisterActionProvider(GSTNPiggybackTags::Type_Mount, Registry.Get());
	}
}

void UGSTN_PairMoveRegistry::UnregisterGlobal()
{
	if (UGSTN_LinkRegistry* LinkRegistry = UGSTN_LinkRegistry::Get())
	{
		LinkRegistry->UnregisterActionProvider(GSTNPiggybackTags::Type_Mount);
	}
	GSTNPairMoveRegistry::Global().Reset();
}

int32 UGSTN_PairMoveRegistry::IndexOfId(int32 Id) const
{
	return Definitions.IndexOfByPredicate([Id](const UGSTN_PairMoveDefinition* Definition) { return Definition->PairMoveId == Id; });
}

bool UGSTN_PairMoveRegistry::AddDefinition(const UGSTN_PairMoveDefinition* Definition, FString* OutError)
{
	auto Refuse = [OutError](FString&& Message)
	{
		UE_LOG(LogGSTNPairMove, Warning, TEXT("Pair move refused: %s"), *Message);
		if (OutError)
		{
			*OutError = MoveTemp(Message);
		}
		return false;
	};
	if (!Definition)
	{
		return Refuse(TEXT("null definition"));
	}
	const FString Name = Definition->GetName();
	if (Definition->PairMoveId < 1 || Definition->PairMoveId > 32767)
	{
		return Refuse(FString::Printf(TEXT("%s: id %d outside 1..32767"), *Name, Definition->PairMoveId));
	}
	if (!Definition->InitiatorAbility || !Definition->PartnerAbility)
	{
		return Refuse(FString::Printf(TEXT("%s: missing ability"), *Name));
	}
	if (Definition->Outcome == EGSTN_LinkActionOutcome::SwapRoles)
	{
		return Refuse(FString::Printf(TEXT("%s: SwapRoles is reserved"), *Name));
	}
	const int32 Existing = IndexOfId(Definition->PairMoveId);
	if (Existing != INDEX_NONE)
	{
		if (Definitions[Existing] == Definition)
		{
			return true;
		}
		return Refuse(FString::Printf(TEXT("%s: id %d already held by %s"), *Name, Definition->PairMoveId, *Definitions[Existing]->GetName()));
	}

	UGSTN_Placer_Mount* Placer = NewObject<UGSTN_Placer_Mount>(this);
	Placer->Mode = Definition->ExitMode;
	Placer->DistanceCm = Definition->ExitDistanceCm;

	// Sorted insert by id: the listing order is the id order whatever order kits were compiled in
	int32 Insert = 0;
	while (Insert < Definitions.Num() && Definitions[Insert]->PairMoveId < Definition->PairMoveId)
	{
		++Insert;
	}
	Definitions.Insert(const_cast<UGSTN_PairMoveDefinition*>(Definition), Insert);
	Placers.Insert(Placer, Insert);
	return true;
}

int32 UGSTN_PairMoveRegistry::AddSet(const UGSTN_PairMoveSet* Set)
{
	int32 Known = 0;
	if (Set)
	{
		for (const UGSTN_PairMoveDefinition* Definition : Set->PairMoves)
		{
			Known += AddDefinition(Definition) ? 1 : 0;
		}
	}
	return Known;
}

void UGSTN_PairMoveRegistry::Reset()
{
	Definitions.Reset();
	Placers.Reset();
}

TArray<const UGSTN_PairMoveDefinition*> UGSTN_PairMoveRegistry::GetDefinitions() const
{
	TArray<const UGSTN_PairMoveDefinition*> Out;
	Out.Reserve(Definitions.Num());
	for (const UGSTN_PairMoveDefinition* Definition : Definitions)
	{
		Out.Add(Definition);
	}
	return Out;
}

const UGSTN_PairMoveDefinition* UGSTN_PairMoveRegistry::FindById(int32 Id) const
{
	const int32 Index = IndexOfId(Id);
	return Index == INDEX_NONE ? nullptr : Definitions[Index].Get();
}

const UGSTN_PairMoveDefinition* UGSTN_PairMoveRegistry::FindByTag(const FGameplayTag& PairTag) const
{
	for (const UGSTN_PairMoveDefinition* Definition : Definitions)
	{
		if (Definition->PairTag == PairTag)
		{
			return Definition;
		}
	}
	return nullptr;
}

bool UGSTN_PairMoveRegistry::ResolveLinkAction(int16 Id, FGSTN_LinkActionInfo& OutInfo) const
{
	const int32 Index = IndexOfId(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Definitions[Index]->MakeLinkActionInfo(Placers[Index], OutInfo);
	return true;
}
