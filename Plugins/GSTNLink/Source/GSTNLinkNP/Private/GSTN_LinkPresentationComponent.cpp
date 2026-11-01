// Links, Network Prediction half.

#include "GSTN_LinkPresentationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkDebugDraw.h"
#include "GSTN_LinkRegistry.h"
#include "MoverComponent.h"
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
	// M7 G1: an initiator that predicts its link (bInitiatorPredicts) and is the parked role is SHOWN parked from its request on (Pending), so the
	// optimistic accept PendingFrames later does not pop; a refusal detaches it again. The simulation parks it from the accept only
	const bool bShownPending = Definition && Definition->bInitiatorPredicts && Body.Phase == EGSTN_LinkPhase::Pending
		&& Body.HasFlag(GSTNLinkFlags::Predicted) && Definition->InitiatorRole.bParked;
	const uint8 ShownRole = bShownPending ? 1 : Body.Role;
	const EGSTN_LinkPhase ShownPhase = bShownPending ? EGSTN_LinkPhase::Active : Body.Phase;
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
		Partner = nullptr;
	}
	FGSTN_LinkDebugDraw::DrawEntity(GetOwner(), *this);
}

void UGSTN_LinkPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachMesh();
	SetCollisionIgnoredPartner(nullptr);
	Super::EndPlay(EndPlayReason);
}
