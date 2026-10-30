// Links, Network Prediction half.

#include "GSTN_LinkPresentationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkDebugDraw.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "State/GSTN_LinkMath.h"
#include "World/GSTN_CombatWorldSubsystem.h"

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

bool UGSTN_LinkPresentationComponent::UpdateLinkedTransform()
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
	if (!(FGSTN_LinkSnapshotContributor::ComputeBits(Definition, Body.Role, Body.Phase) & GSTNLinkSnapshotBits::Parked) || Body.PartnerId.IsNull())
	{
		return false;
	}
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	AActor* PartnerActor = Combat ? Combat->FindOwnerActor(Body.PartnerId.Id) : nullptr;
	if (!PartnerActor)
	{
		return false;
	}
	const FGSTN_LinkRoleDef& Role = Definition->GetRoleDef(Body.Role);
	PartnerSocket = Role.SocketOnPartner;
	RoleOffset = Role.SocketOffset;

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
		LinkedTransform = UGSTN_LinkMath::ParkedTransformOnSocket(PartnerTransform, Socket, Role);
	}

	if (Partner.Get() != PartnerActor)
	{
		DetachMesh();
		Partner = PartnerActor;
	}
	if (!Role.bCollisionWithPartner && IgnoredPartner.Get() != PartnerActor)
	{
		SetPartnerCollisionIgnored(IgnoredPartner.Get(), false);
		SetPartnerCollisionIgnored(PartnerActor, true);
	}
	return true;
}

void UGSTN_LinkPresentationComponent::AttachMesh(AActor* NewPartner)
{
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	USkeletalMeshComponent* PartnerMesh = NewPartner ? NewPartner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || !PartnerMesh || bMeshAttached)
	{
		return;
	}
	MeshOriginalParent = Mesh->GetAttachParent();
	MeshOriginalSocket = Mesh->GetAttachSocketName();
	MeshOriginalRelative = Mesh->GetRelativeTransform();
	// The parked transform places the owner's ROOT on the socket; the mesh keeps its own offset from the root
	const FName Socket = PartnerMesh->DoesSocketExist(PartnerSocket) ? PartnerSocket : NAME_None;
	Mesh->AttachToComponent(PartnerMesh, FAttachmentTransformRules::KeepRelativeTransform, Socket);
	Mesh->SetRelativeTransform(MeshOriginalRelative * RoleOffset);
	bMeshAttached = true;
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

void UGSTN_LinkPresentationComponent::SetPartnerCollisionIgnored(AActor* Other, bool bIgnore)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* OwnRoot = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	UPrimitiveComponent* OtherRoot = Other ? Cast<UPrimitiveComponent>(Other->GetRootComponent()) : nullptr;
	if (!OwnRoot || !OtherRoot)
	{
		return;
	}
	OwnRoot->IgnoreActorWhenMoving(Other, bIgnore);
	OtherRoot->IgnoreActorWhenMoving(Owner, bIgnore);
	IgnoredPartner = bIgnore ? Other : nullptr;
}

void UGSTN_LinkPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bParkedShown = UpdateLinkedTransform();
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
		SetPartnerCollisionIgnored(IgnoredPartner.Get(), false);
		Partner = nullptr;
	}
	FGSTN_LinkDebugDraw::DrawEntity(GetOwner(), *this);
}

void UGSTN_LinkPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachMesh();
	SetPartnerCollisionIgnored(IgnoredPartner.Get(), false);
	Super::EndPlay(EndPlayReason);
}
