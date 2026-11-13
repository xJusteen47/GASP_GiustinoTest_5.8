// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_CombatRulesComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Input/GSTN_CombatSettings.h"
#include "Net/UnrealNetwork.h"
#include "Rules/GSTN_CombatRules.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatRules, Log, All);

UGSTN_CombatRulesComponent::UGSTN_CombatRulesComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSTN_CombatRulesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_CombatRulesComponent, RulesAsset);
}

void UGSTN_CombatRulesComponent::BeginPlay()
{
	Super::BeginPlay();
	// Every machine, before the first simulation tick: the replicated reference of a client is in place by now
	ApplyRules();
}

void UGSTN_CombatRulesComponent::SetRulesAsset(TSoftObjectPtr<UGSTN_CombatRules> NewRules)
{
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogGSTNCombatRules, Warning, TEXT("SetRulesAsset ignored: only the authority picks the rules"));
		return;
	}

	RulesAsset = NewRules;
	// Not left to OnRep_RulesAsset: the authority (a listen host included) never receives the OnRep of its own write
	ApplyRules();
}

void UGSTN_CombatRulesComponent::OnRep_RulesAsset()
{
	ApplyRules();
}

void UGSTN_CombatRulesComponent::ApplyRules()
{
	UGSTN_CombatRules* Rules = RulesAsset.IsNull() ? nullptr : RulesAsset.LoadSynchronous();
	if (Rules != nullptr)
	{
		Rules->ResolveHitPipeline();
	}
	else
	{
		if (!RulesAsset.IsNull())
		{
			UE_LOG(LogGSTNCombatRules, Warning, TEXT("Rules asset %s did not load: playing the rules of the combat settings"), *RulesAsset.ToString());
		}
		// The settings resolve their own pipeline; the object is theirs, kept alive by them
		Rules = const_cast<UGSTN_CombatRules*>(UGSTN_CombatSettings::Get().ResolveDefaultRules());
	}

	Resolved = Rules;
	++ApplyCount;
}

const UGSTN_CombatRules* UGSTN_CombatRulesComponent::GetRules()
{
	if (Resolved == nullptr)
	{
		ApplyRules();
	}
	return Resolved;
}

UGSTN_CombatRulesComponent* UGSTN_CombatRulesComponent::Find(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->FindComponentByClass<UGSTN_CombatRulesComponent>() : nullptr;
}
