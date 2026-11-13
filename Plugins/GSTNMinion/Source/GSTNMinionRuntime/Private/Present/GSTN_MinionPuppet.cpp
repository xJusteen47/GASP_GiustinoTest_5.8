// Minion runtime (client presentation, M8 G7).

#include "Present/GSTN_MinionPuppet.h"

#include "Animation/AnimInstance.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/SkeletalMesh.h"

AGSTN_MinionPuppet::AGSTN_MinionPuppet()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->bReceivesDecals = false;
	Mesh->SetMobility(EComponentMobility::Movable);
	// The animation cost stack of 8.4: fewer evaluations far away, no pose work while not rendered
	Mesh->bEnableUpdateRateOptimizations = true;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	// Pooled puppets start hidden and do nothing until acquired
	Mesh->SetVisibility(false);
	Mesh->SetComponentTickEnabled(false);
	SetActorHiddenInGame(true);
}

void AGSTN_MinionPuppet::ActivatePuppet(const UGSTN_MinionDefinition* InDefinition, int32 InPresentKey, const FTransform& Transform)
{
	PresentKey = InPresentKey;
	bInUse = true;
	if (Definition.Get() != InDefinition || !Definition.IsValid())
	{
		Definition = InDefinition;
		// Soft references: loaded the first time a puppet of the type is shown. Null is fine (headless, -nullrhi, tests)
		USkeletalMesh* SkeletalMesh = InDefinition ? InDefinition->PuppetMesh.LoadSynchronous() : nullptr;
		Mesh->SetSkeletalMesh(SkeletalMesh);
		UClass* AnimClass = InDefinition ? InDefinition->PuppetAnimClass.LoadSynchronous() : nullptr;
		if (SkeletalMesh && AnimClass)
		{
			Mesh->SetAnimInstanceClass(AnimClass);
		}
	}
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	SetFlash(0.f);
	SetPaused(false);
	SetShown(true);
}

void AGSTN_MinionPuppet::DeactivatePuppet()
{
	bInUse = false;
	PresentKey = INDEX_NONE;
	SetFlash(0.f);
	SetPaused(false);
	SetShown(false);
}

void AGSTN_MinionPuppet::SetPose(const FVector& Location, float YawDegrees)
{
	SetActorLocationAndRotation(Location, FRotator(0.f, YawDegrees, 0.f), false, nullptr, ETeleportType::None);
}

void AGSTN_MinionPuppet::SetFlash(float Value)
{
	if (Value == Flash)
	{
		return;
	}
	Flash = Value;
	// Custom primitive data: one float per primitive, read by the shared material, no material instance per puppet
	Mesh->SetCustomPrimitiveDataFloat(FlashDataIndex, Value);
}

void AGSTN_MinionPuppet::SetPaused(bool bInPaused)
{
	if (bInPaused == bPaused)
	{
		return;
	}
	bPaused = bInPaused;
	Mesh->GlobalAnimRateScale = bInPaused ? 0.f : 1.f;
}

void AGSTN_MinionPuppet::SetShown(bool bShown)
{
	SetActorHiddenInGame(!bShown);
	Mesh->SetVisibility(bShown);
	// No mesh means no animation to tick at all
	Mesh->SetComponentTickEnabled(bShown && Mesh->GetSkeletalMeshAsset() != nullptr);
}

AGSTN_MinionShatter::AGSTN_MinionShatter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
}

void AGSTN_MinionShatter::Play(const FVector& Location, double InEndSeconds)
{
	SetActorLocation(Location);
	EndSeconds = InEndSeconds;
	bPlaying = true;
}

void AGSTN_MinionShatter::Stop()
{
	bPlaying = false;
}
