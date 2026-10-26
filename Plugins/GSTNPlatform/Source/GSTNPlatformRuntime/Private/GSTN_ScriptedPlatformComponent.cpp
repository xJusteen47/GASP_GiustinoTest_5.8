// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_ScriptedPlatformComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_ScriptedPlatformComponent)

UGSTN_ScriptedPlatformComponent::UGSTN_ScriptedPlatformComponent()
{
	SetIsReplicatedByDefault(true);
}

void UGSTN_ScriptedPlatformComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UGSTN_ScriptedPlatformComponent, PathOrigin, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(UGSTN_ScriptedPlatformComponent, bHasPathOrigin, COND_InitialOnly);
}

void UGSTN_ScriptedPlatformComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && !bHasPathOrigin)
	{
		SetPathOrigin(FTransform(Owner->GetActorQuat(), Owner->GetActorLocation()));
	}
}

void UGSTN_ScriptedPlatformComponent::SetPathOrigin(const FTransform& Origin)
{
	PathOrigin = FTransform(Origin.GetRotation(), Origin.GetLocation());
	bHasPathOrigin = true;
}

FTransform UGSTN_ScriptedPlatformComponent::ComputeOwnerPose_Implementation(const FGSTN_PlatformPoseContext& Context) const
{
	if (!bHasPathOrigin)
	{
		return Super::ComputeOwnerPose_Implementation(Context);
	}
	return Path.Evaluate(PathOrigin, Context.GetTimeSeconds());
}

void UGSTN_ScriptedPlatformComponent::OnPoseApplied(const FGSTN_PlatformPoseContext& Context, const FTransform& OwnerPose)
{
	Super::OnPoseApplied(Context, OwnerPose);
	AActor* Owner = GetOwner();
	if (bMoveOwner && bHasPathOrigin && Owner)
	{
		Owner->SetActorLocationAndRotation(OwnerPose.GetLocation(), OwnerPose.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}
