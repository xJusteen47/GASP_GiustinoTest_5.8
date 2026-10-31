// Links, Network Prediction half.

#include "GSTN_LinkDebugDraw.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkTags.h"
#include "HAL/IConsoleManager.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace
{
	int32 GLinkDebugDraw = 0;
	FAutoConsoleVariableRef CVarLinkDebugDraw(TEXT("gstn.Combat.Debug.Links"), GLinkDebugDraw,
		TEXT("1 draws the links of every pawn on this machine: partner line, parked capsule, shown transform and hurtbox anchor, grab target."),
		ECVF_Cheat);

	void DrawAxes(const UWorld* World, const FTransform& Transform, float Size, float Thickness)
	{
		const FVector Origin = Transform.GetLocation();
		const FQuat Rotation = Transform.GetRotation();
		DrawDebugLine(World, Origin, Origin + Rotation.GetForwardVector() * Size, FColor::Red, false, -1.f, 0, Thickness);
		DrawDebugLine(World, Origin, Origin + Rotation.GetRightVector() * Size, FColor::Green, false, -1.f, 0, Thickness);
		DrawDebugLine(World, Origin, Origin + Rotation.GetUpVector() * Size, FColor::Blue, false, -1.f, 0, Thickness);
	}
}

bool FGSTN_LinkDebugDraw::IsEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GLinkDebugDraw != 0;
#endif
}

void FGSTN_LinkDebugDraw::DrawEntity(const AActor* Owner, const UGSTN_LinkPresentationComponent& Presentation)
{
#if ENABLE_DRAW_DEBUG
	if (!IsEnabled() || !Owner)
	{
		return;
	}
	const UWorld* World = Owner->GetWorld();
	const FVector Location = Owner->GetActorLocation();
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		if (ASC->HasMatchingGameplayTag(GSTNLinkTags::Interact_Target_Grab))
		{
			DrawDebugCircle(World, Location, 70.f, 24, FColor::Cyan, false, -1.f, 0, 1.5f, FVector::ForwardVector, FVector::RightVector, false);
		}
	}

	const FGSTN_LinkFragment* Fragment = UGSTN_LinkPresentationComponent::FindShownFragment(Owner);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
	if (!Fragment || !Registry || !Combat)
	{
		return;
	}
	const FGSTN_LinkState& Body = Fragment->Links.Body;
	if (Body.Phase == EGSTN_LinkPhase::None)
	{
		return;
	}
	const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Body.LinkTypeIndex);
	const FColor Color = Definition ? Definition->DebugColor : FColor::Orange;
	const AActor* PartnerActor = Body.PartnerId.IsNull() ? nullptr : Combat->FindOwnerActor(Body.PartnerId.Id);
	if (PartnerActor)
	{
		if (Body.Phase == EGSTN_LinkPhase::Pending)
		{
			DrawDebugDirectionalArrow(World, Location, PartnerActor->GetActorLocation(), 20.f, Color, false, -1.f, 0, 0.5f);
		}
		else
		{
			DrawDebugLine(World, Location, PartnerActor->GetActorLocation(), Color, false, -1.f, 0, 2.f);
		}
	}
	const FGameplayTag RoleTag = Registry->GetRoleTag(Body.LinkTypeIndex, Body.Role);
	DrawDebugString(World, Location + FVector(0.f, 0.f, 110.f), FString::Printf(TEXT("%s %s s%d p%d%s"),
		RoleTag.IsValid() ? *RoleTag.GetTagName().ToString() : TEXT("(no role)"), *Registry->GetLinkType(Body.LinkTypeIndex).GetTagName().ToString(),
		Body.Serial, static_cast<int32>(Body.Phase), Body.HasFlag(GSTNLinkFlags::Predicted) ? TEXT(" predicted") : TEXT("")), nullptr, Color, 0.f, true);

	FTransform Shown;
	if (Presentation.GetLinkedTransform(Shown))
	{
		// The capsule where the simulation parks it, the transform shown here, the anchor the hurtboxes use
		DrawDebugCapsule(World, Location, 90.f, 34.f, Owner->GetActorQuat(), FColor::Silver, false, -1.f, 0, 1.f);
		DrawAxes(World, Shown, 40.f, 2.f);
		for (const FGSTN_SnapshotEntry& Entry : Combat->GetSnapshot().Entries)
		{
			if (Combat->FindOwnerActor(Entry.EntityId) == Owner)
			{
				DrawAxes(World, Entry.Transform, 60.f, 4.f);
				break;
			}
		}
	}
#endif
}
