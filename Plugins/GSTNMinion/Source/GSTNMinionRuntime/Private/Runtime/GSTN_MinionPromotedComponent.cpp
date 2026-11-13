// Minion runtime (authority batch, M8 G9a; readers G9b).

#include "Runtime/GSTN_MinionPromotedComponent.h"

#include "Net/UnrealNetwork.h"
#include "Present/GSTN_MinionPresentationSubsystem.h"

UGSTN_MinionPromotedComponent::UGSTN_MinionPromotedComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Created at run time by the promotion, so it replicates as a dynamic component of the fighter
	SetIsReplicatedByDefault(true);
}

void UGSTN_MinionPromotedComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Written once before the fighter's first replication: the initial bunch carries them, nothing changes afterwards
	DOREPLIFETIME_CONDITION(UGSTN_MinionPromotedComponent, PromotedFromMinionId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGSTN_MinionPromotedComponent, PromotedFromPackId, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGSTN_MinionPromotedComponent, SourceTypeTag, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGSTN_MinionPromotedComponent, HealthFraction, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGSTN_MinionPromotedComponent, Team, COND_InitialOnly);
}

void UGSTN_MinionPromotedComponent::BeginPlay()
{
	Super::BeginPlay();
	// A client applies the initial bunch before BeginPlay, so the ids are there. A world without minion presentation shows the fighter as is
	if (UGSTN_MinionPresentationSubsystem* Presentation = UGSTN_MinionPresentationSubsystem::GetActive(GetWorld()))
	{
		Presentation->NotePromotedFighter(this);
	}
}
