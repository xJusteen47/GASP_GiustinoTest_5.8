// Links, Network Prediction half.

#include "GSTN_LinkPresentationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_LinkAnimSelection.h"
#include "GSTN_LinkDebugDraw.h"
#include "GSTN_LinkRegistry.h"
#include "MoverComponent.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "State/GSTN_LinkMath.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace GSTNLinkPresentationPrivate
{
	USkeletalMeshComponent* FindMesh(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}

	/** The loadout of a fighter: on the actor, else on its PlayerState (where combat-m3 puts it), as the combat component finds it */
	const UGSTN_LoadoutComponent* FindLoadout(const AActor* Actor)
	{
		const UGSTN_LoadoutComponent* Found = Actor ? Actor->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		if (!Found)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
			Found = PlayerState ? PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		}
		return Found;
	}
}

UGSTN_LinkPresentationComponent::UGSTN_LinkPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the simulation and Mover's presentation moved the actors of this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

const FGSTN_LinkFragment* UGSTN_LinkPresentationComponent::FindShownFragment(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	return Sync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments) : nullptr;
}

UGSTN_LinkPresentationComponent* UGSTN_LinkPresentationComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_LinkPresentationComponent* Existing = Owner->FindComponentByClass<UGSTN_LinkPresentationComponent>())
	{
		return Existing;
	}
	UGSTN_LinkPresentationComponent* Component = NewObject<UGSTN_LinkPresentationComponent>(Owner, TEXT("GSTN_LinkPresentation"));
	Component->RegisterComponent();
	return Component;
}

bool UGSTN_LinkPresentationComponent::GetLinkedTransform(FTransform& OutTransform) const
{
	if (!bParkedShown)
	{
		return false;
	}
	OutTransform = LinkedTransform;
	return true;
}

bool UGSTN_LinkPresentationComponent::UpdateLinkedTransform(FGameplayTag& OutRoleTag)
{
	AActor* Owner = GetOwner();
	const FGSTN_LinkFragment* Fragment = FindShownFragment(Owner);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Fragment || !Registry)
	{
		return false;
	}
	const FGSTN_LinkState& Body = Fragment->Links.Body;
	const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Body.LinkTypeIndex);
	// M7 G1: an initiator that predicts its link (bInitiatorPredicts) and is the parked role is SHOWN parked from its request on (Pending), so the
	// optimistic accept PendingFrames later does not pop; a refusal detaches it again. The simulation parks it from the accept only
	const bool bShownPending = Definition && Definition->bInitiatorPredicts && Body.Phase == EGSTN_LinkPhase::Pending
		&& Body.HasFlag(GSTNLinkFlags::Predicted) && Definition->InitiatorRole.bParked;
	const uint8 ShownRole = bShownPending ? 1 : Body.Role;
	const EGSTN_LinkPhase ShownPhase = bShownPending ? EGSTN_LinkPhase::Active : Body.Phase;
	if (Definition && (ShownPhase == EGSTN_LinkPhase::Active || ShownPhase == EGSTN_LinkPhase::Exiting))
	{
		// G10: the shown role of any role (the carrier's too), for its anim layer; and the type's slide time, kept after the link ends for the detach
		OutRoleTag = Definition->GetRoleDef(ShownRole).RoleTag;
		ShownTypeBlendSeconds = Definition->PresentationBlendSeconds;
	}
	if (!(FGSTN_LinkSnapshotContributor::ComputeBits(Definition, ShownRole, ShownPhase) & GSTNLinkSnapshotBits::Parked) || Body.PartnerId.IsNull())
	{
		return false;
	}
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	AActor* PartnerActor = Combat ? Combat->FindOwnerActor(Body.PartnerId.Id) : nullptr;
	if (!PartnerActor)
	{
		return false;
	}
	const FGSTN_LinkRoleDef& Role = Definition->GetRoleDef(ShownRole);
	PartnerSocket = Role.SocketOnPartner;
	// M7 G2: the parked role's own turn (LocalYawQ), as the snapshot substitution composes it, so the mesh faces where its hurtboxes do
	RoleOffset = UGSTN_LinkMath::ComposeLocalYaw(Body.ResolveParkedOffset(Role.SocketOffset), ShownRole == Body.Role ? Body.LocalYawQ : static_cast<int16>(0));

	const USkeletalMeshComponent* PartnerMesh = PartnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (bUseLiveSocket && PartnerMesh && !PartnerSocket.IsNone() && PartnerMesh->DoesSocketExist(PartnerSocket))
	{
		// The animated hand, this machine's pose: presentation only, never what the hurtboxes use
		LinkedTransform = RoleOffset * PartnerMesh->GetSocketTransform(PartnerSocket);
	}
	else
	{
		const FGSTN_LinkBodyData* PartnerBody = FGSTN_LinkAttach::Find(PartnerActor);
		const FTransform Socket = PartnerBody ? PartnerBody->GetSocket(PartnerSocket) : FTransform::Identity;
		FTransform PartnerTransform = PartnerActor->GetActorTransform();
		PartnerTransform.SetScale3D(FVector::OneVector);
		LinkedTransform = RoleOffset * Socket * PartnerTransform;
	}

	if (Partner.Get() != PartnerActor)
	{
		DetachMesh();
		Partner = PartnerActor;
	}
	return true;
}

void UGSTN_LinkPresentationComponent::AttachMesh(AActor* NewPartner)
{
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	USkeletalMeshComponent* PartnerMesh = NewPartner ? NewPartner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || !PartnerMesh)
	{
		return;
	}
	if (!bMeshAttached)
	{
		MeshOriginalParent = Mesh->GetAttachParent();
		MeshOriginalSocket = Mesh->GetAttachSocketName();
		MeshOriginalRelative = Mesh->GetRelativeTransform();
		// Mover's own offset of its visual component, not the relative transform of this frame (its smoothing may have moved the mesh)
		if (const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetPrimaryVisualComponent() == Mesh)
		{
			MeshOriginalRelative = Mover->GetBaseVisualComponentTransform();
		}
		bMeshAttached = true;
	}
	// The parked transform places the owner's ROOT on the socket; the mesh keeps its own offset from the root. Asserted on every tick, not
	// once: Mover writes its primary visual component every frame before the actors tick (FinalizeFrame resets the relative transform to its
	// base, dropping the role offset: the fixed second step of S2 finding A; FinalizeSmoothingFrame puts it back on its own capsule in world
	// space: the parked entity's mesh that never rode on its own client, S2 finding B). This tick runs after both (TG_PostPhysics)
	const FName Socket = PartnerMesh->DoesSocketExist(PartnerSocket) ? PartnerSocket : NAME_None;
	if (Mesh->GetAttachParent() != PartnerMesh || Mesh->GetAttachSocketName() != Socket)
	{
		Mesh->AttachToComponent(PartnerMesh, FAttachmentTransformRules::KeepRelativeTransform, Socket);
	}
	const FTransform Relative = MeshOriginalRelative * RoleOffset;
	if (!Mesh->GetRelativeTransform().Equals(Relative))
	{
		Mesh->SetRelativeTransform(Relative);
	}
}

void UGSTN_LinkPresentationComponent::DetachMesh()
{
	if (!bMeshAttached)
	{
		return;
	}
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (Mesh)
	{
		USceneComponent* Parent = MeshOriginalParent.IsValid() ? MeshOriginalParent.Get() : (Owner ? Owner->GetRootComponent() : nullptr);
		Mesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, MeshOriginalSocket);
		Mesh->SetRelativeTransform(MeshOriginalRelative);
	}
	bMeshAttached = false;
}

void UGSTN_LinkPresentationComponent::SetCollisionIgnoredPartner(AActor* NewPartner)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* OwnRoot = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	AActor* Previous = IgnoredPartner.Get();
	if (!OwnRoot || Previous == NewPartner)
	{
		return;
	}
	if (Previous)
	{
		OwnRoot->IgnoreActorWhenMoving(Previous, false);
	}
	if (NewPartner && NewPartner != Owner)
	{
		OwnRoot->IgnoreActorWhenMoving(NewPartner, true);
		IgnoredPartner = NewPartner;
	}
	else
	{
		IgnoredPartner = nullptr;
	}
}

void UGSTN_LinkPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FGameplayTag RoleTag;
	const bool bWasAttached = bMeshAttached;
	bParkedShown = UpdateLinkedTransform(RoleTag);
	if (bParkedShown)
	{
		if (bAttachMesh)
		{
			AttachMesh(Partner.Get());
		}
	}
	else
	{
		DetachMesh();
		Partner = nullptr;
	}
	// G10: an attach or a detach this frame slides the mesh from where it was shown at the end of the last frame
	if (bWasAttached != bMeshAttached && bHasLastShown)
	{
		StartMeshBlend(LastShownWorld);
	}
	ApplyMeshBlend(DeltaTime);
	if (const USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner()))
	{
		LastShownWorld = Mesh->GetComponentTransform();
		bHasLastShown = true;
	}
	UpdateShownRole(RoleTag);
	FGSTN_LinkDebugDraw::DrawEntity(GetOwner(), *this);
}

float UGSTN_LinkPresentationComponent::GetPresentationBlendAlpha() const
{
	return bBlending && BlendSeconds > 0.f ? FMath::Clamp(BlendElapsed / BlendSeconds, 0.f, 1.f) : 1.f;
}

void UGSTN_LinkPresentationComponent::StartMeshBlend(const FTransform& FromWorld)
{
	BlendSeconds = ShownTypeBlendSeconds;
	BlendElapsed = 0.f;
	BlendFromWorld = FromWorld;
	bBlending = BlendSeconds > 0.f;
}

void UGSTN_LinkPresentationComponent::ApplyMeshBlend(float DeltaTime)
{
	if (!bBlending)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner());
	const USceneComponent* Parent = Mesh ? Mesh->GetAttachParent() : nullptr;
	if (!Parent)
	{
		bBlending = false;
		return;
	}
	// The settled transform is recomputed from the attachment, never read back from the mesh: Mover may or may not have rewritten the mesh
	// this frame, and the last frame's blended write must not feed the next one
	const FTransform TargetRelative = bMeshAttached ? MeshOriginalRelative * RoleOffset : MeshOriginalRelative;
	BlendElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(BlendElapsed / BlendSeconds, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		Mesh->SetRelativeTransform(TargetRelative);
		bBlending = false;
		return;
	}
	const FTransform Settled = TargetRelative * Parent->GetSocketTransform(Mesh->GetAttachSocketName());
	FTransform Shown;
	Shown.Blend(BlendFromWorld, Settled, FMath::SmoothStep(0.f, 1.f, Alpha));
	Mesh->SetWorldTransform(Shown);
}

void UGSTN_LinkPresentationComponent::UpdateShownRole(const FGameplayTag& NewRoleTag)
{
	if (NewRoleTag == ShownRoleTag)
	{
		return;
	}
	const FGameplayTag OldRoleTag = ShownRoleTag;
	ShownRoleTag = NewRoleTag;
	UnlinkRoleLayer();
	if (bLinkRoleAnimLayers && NewRoleTag.IsValid())
	{
		LinkRoleLayer(NewRoleTag);
	}
	OnShownRoleChanged.Broadcast(OldRoleTag, NewRoleTag);
}

void UGSTN_LinkPresentationComponent::LinkRoleLayer(const FGameplayTag& RoleTag)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const UGSTN_LoadoutComponent* Loadout = GSTNLinkPresentationPrivate::FindLoadout(GetOwner());
	if (!Loadout)
	{
		return;
	}
	const FGSTN_ResolvedLoadout& Resolved = Loadout->GetResolvedLoadout();
	const UGSTN_WeaponDefinition* Weapon = nullptr;
	for (const UGSTN_WeaponDefinition* Candidate : Resolved.Weapons)
	{
		if (Candidate)
		{
			// The first resolved weapon: the loadout's main one
			Weapon = Candidate;
			break;
		}
	}
	WantedRoleLayer = UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Resolved.Kit, Weapon, RoleTag);
	if (WantedRoleLayer.IsNull())
	{
		return;
	}
	if (WantedRoleLayer.Get())
	{
		HandleRoleLayerLoaded(RoleTag);
		return;
	}
	// Loaded off the critical path (GatherRoleAnimLayers is what a client preloads, so this is rare); linked only if the role is still shown
	RoleLayerHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WantedRoleLayer.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UGSTN_LinkPresentationComponent::HandleRoleLayerLoaded, RoleTag));
}

void UGSTN_LinkPresentationComponent::HandleRoleLayerLoaded(FGameplayTag ForRoleTag)
{
	RoleLayerHandle.Reset();
	UClass* LayerClass = WantedRoleLayer.Get();
	USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner());
	if (ForRoleTag != ShownRoleTag || !LayerClass || !Mesh)
	{
		return;
	}
	Mesh->LinkAnimClassLayers(TSubclassOf<UAnimInstance>(LayerClass));
	LinkedRoleLayer = LayerClass;
}

void UGSTN_LinkPresentationComponent::UnlinkRoleLayer()
{
	if (RoleLayerHandle.IsValid())
	{
		RoleLayerHandle->CancelHandle();
		RoleLayerHandle.Reset();
	}
	if (LinkedRoleLayer)
	{
		if (USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner()))
		{
			Mesh->UnlinkAnimClassLayers(TSubclassOf<UAnimInstance>(LinkedRoleLayer.Get()));
		}
		LinkedRoleLayer = nullptr;
	}
	WantedRoleLayer.Reset();
}

void UGSTN_LinkPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachMesh();
	UnlinkRoleLayer();
	SetCollisionIgnoredPartner(nullptr);
	Super::EndPlay(EndPlayReason);
}
