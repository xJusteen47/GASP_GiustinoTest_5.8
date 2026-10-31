// Links, Network Prediction half.

#include "Sim/GSTN_LinkAttach.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Movement/GSTN_LinkedModeTransition.h"
#include "MoverComponent.h"

namespace
{
	struct FLinkAttachState
	{
		/** Entry 0 is the "none" entry, so a zero index reads as nothing attached */
		TArray<TUniquePtr<FGSTN_LinkBodyData>> Table;
		TMap<TWeakObjectPtr<const AActor>, uint16> Attached;

		FLinkAttachState()
		{
			Table.Add(MakeUnique<FGSTN_LinkBodyData>());
		}
	};

	FLinkAttachState& State()
	{
		static FLinkAttachState Instance;
		return Instance;
	}

	uint16 AddEntry(TUniquePtr<FGSTN_LinkBodyData> Data)
	{
		FLinkAttachState& S = State();
		if (S.Table.Num() >= MAX_uint16)
		{
			return 0;
		}
		Data->Index = static_cast<uint16>(S.Table.Num());
		S.Table.Add(MoveTemp(Data));
		return S.Table.Last()->Index;
	}

	/** Every socket a registered role parks its partner on: what the bake must cover */
	void GatherRoleSockets(TArray<FName>& Out)
	{
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		if (!Registry)
		{
			return;
		}
		for (int32 Index = 1; Index <= Registry->Num(); ++Index)
		{
			if (const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(static_cast<uint8>(Index)))
			{
				for (const FGSTN_LinkRoleDef* Role : { &Definition->InitiatorRole, &Definition->TargetRole })
				{
					if (!Role->SocketOnPartner.IsNone())
					{
						Out.AddUnique(Role->SocketOnPartner);
					}
				}
			}
		}
	}
}

FTransform FGSTN_LinkBodyData::GetSocket(FName Socket) const
{
	const FTransform* Found = Socket.IsNone() ? nullptr : BakedSockets.Find(Socket);
	return Found ? *Found : FTransform::Identity;
}

const FGSTN_LinkBodyData* FGSTN_LinkAttach::Find(const AActor* Owner)
{
	const uint16* Index = Owner ? State().Attached.Find(Owner) : nullptr;
	return Index ? At(*Index) : nullptr;
}

const FGSTN_LinkBodyData* FGSTN_LinkAttach::At(uint16 Index)
{
	const FLinkAttachState& S = State();
	return Index != 0 && S.Table.IsValidIndex(Index) ? S.Table[Index].Get() : nullptr;
}

void FGSTN_LinkAttach::BakeSockets(const USkeletalMesh* Mesh, const FTransform& MeshRelative, TConstArrayView<FName> Sockets, TMap<FName, FTransform>& Out)
{
	if (!Mesh)
	{
		return;
	}
	const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
	for (const FName& Name : Sockets)
	{
		// A socket of the mesh first, else a bone of that name; the reference pose only, never an animated pose
		FTransform Local = FTransform::Identity;
		FName Bone = Name;
		if (const USkeletalMeshSocket* Socket = Mesh->FindSocket(Name))
		{
			Local = Socket->GetSocketLocalTransform();
			Bone = Socket->BoneName;
		}
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}
		const FTransform BoneComponentSpace = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
		Out.Add(Name, Local * BoneComponentSpace * MeshRelative);
	}
}

uint16 FGSTN_LinkAttach::Apply(AActor* Owner, const UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Owner)
	{
		return 0;
	}
	TUniquePtr<FGSTN_LinkBodyData> Data = MakeUnique<FGSTN_LinkBodyData>();
	Data->Kit = Kit;

	TArray<FName> Sockets;
	GatherRoleSockets(Sockets);
	if (const USkeletalMeshComponent* MeshComponent = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		// The snapshot transform is the actor's (the capsule's): the mesh's offset from the root is part of the baked socket
		const FTransform MeshRelative = MeshComponent->GetComponentTransform().GetRelativeTransform(Owner->GetActorTransform());
		BakeSockets(MeshComponent->GetSkeletalMeshAsset(), MeshRelative, Sockets, Data->BakedSockets);
	}

	const uint16 Index = AddEntry(MoveTemp(Data));
	if (Index != 0)
	{
		State().Attached.Add(Owner, Index);
	}
	// The parked mode and its transitions on the pawn's Mover, and the presentation of a link: content setup, the same on every machine
	UGSTN_LinkedModeTransition::EnsureMoverSetup(Owner->FindComponentByClass<UMoverComponent>());
	UGSTN_LinkPresentationComponent::FindOrAdd(Owner);
	return Index;
}

void FGSTN_LinkAttach::Clear(const AActor* Owner)
{
	State().Attached.Remove(Owner);
}

void FGSTN_LinkAttach::AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UActorComponent* Component = Cast<UActorComponent>(Context.WorldContext);
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		// A tool or a test compiles without a pawn: nothing to attach to
		return;
	}
	Apply(Owner, Context.Resolved ? Context.Resolved->Kit : nullptr);
}

FName FGSTN_LinkAttach::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNLinkNP"));
	return Id;
}

int32 FGSTN_LinkAttach::Num()
{
	return State().Attached.Num();
}

void FGSTN_LinkAttach::Reset()
{
	State() = FLinkAttachState();
}

uint16 FGSTN_LinkAttach::AddForTest(const TMap<FName, FTransform>& Sockets, const FGSTN_LinkKitExtension* KitExtension)
{
	TUniquePtr<FGSTN_LinkBodyData> Data = MakeUnique<FGSTN_LinkBodyData>();
	Data->BakedSockets = Sockets;
	Data->TestKitExtension = KitExtension;
	return AddEntry(MoveTemp(Data));
}

const FGSTN_LinkKitExtension* FGSTN_LinkBodyData::GetKitExtension() const
{
	if (const UGSTN_PrimaryKitDefinition* Attached = Kit.Get())
	{
		return UGSTN_LinkRequirementLibrary::FindKitExtension(Attached);
	}
	return TestKitExtension;
}
