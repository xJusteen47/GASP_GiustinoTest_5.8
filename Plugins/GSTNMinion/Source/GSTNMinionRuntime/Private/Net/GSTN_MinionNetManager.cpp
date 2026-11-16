// Minion runtime (replication, M8 G6).

#include "Net/GSTN_MinionNetManager.h"

#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/UnrealNetwork.h"

AGSTN_MinionNetManager::AGSTN_MinionNetManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	// Changes only when a new type registers; push model sends it then, the frequency only bounds the latency of that change
	SetNetUpdateFrequency(1.f);
	SetMinNetUpdateFrequency(1.f);
	SetCanBeDamaged(false);
}

void AGSTN_MinionNetManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AGSTN_MinionNetManager, Info, Params);
}

void AGSTN_MinionNetManager::SetRegistryInfo(const FGSTN_MinionNetRegistryInfo& InInfo)
{
	Info = InInfo;
	MARK_PROPERTY_DIRTY_FROM_NAME(AGSTN_MinionNetManager, Info, this);
}

void AGSTN_MinionNetManager::OnRep_Info()
{
	if (const UWorld* World = GetWorld())
	{
		if (UGSTN_MinionClientRegistry* Registry = World->GetSubsystem<UGSTN_MinionClientRegistry>())
		{
			Registry->ApplyRegistryInfo(Info);
		}
	}
}
