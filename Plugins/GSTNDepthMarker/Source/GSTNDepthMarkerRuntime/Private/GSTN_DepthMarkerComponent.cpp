#include "GSTN_DepthMarkerComponent.h"

#include "GSTN_DepthMarkerSubsystem.h"

UGSTN_DepthMarkerComponent::UGSTN_DepthMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Presentation only: nothing about the marker is replicated, every machine computes its own
	SetIsReplicatedByDefault(false);
}

void UGSTN_DepthMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	// Null on a dedicated server (the subsystem is never created there): the component is then inert
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterMarker(this);
	}
}

void UGSTN_DepthMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterMarker(this);
	}
	Super::EndPlay(EndPlayReason);
}
