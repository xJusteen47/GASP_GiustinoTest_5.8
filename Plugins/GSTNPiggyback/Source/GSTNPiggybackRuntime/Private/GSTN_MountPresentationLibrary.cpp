// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_MountPresentationLibrary.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PiggybackTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"

const FName UGSTN_MountPresentationLibrary::DefaultHandSocketBase(TEXT("GSTN_MountHand"));
const FName UGSTN_MountPresentationLibrary::DefaultFootSocketBase(TEXT("GSTN_MountFoot"));

namespace GSTNMountPresentationPrivate
{
	const FGSTN_RiderProfile* FindRiderProfile(const AActor* Rider)
	{
		const UGSTN_LoadoutComponent* Loadout = Rider ? Rider->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			const APawn* Pawn = Cast<APawn>(Rider);
			const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
			Loadout = PlayerState ? PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		}
		const UGSTN_PrimaryKitDefinition* Kit = Loadout ? Loadout->GetResolvedLoadout().Kit : nullptr;
		const FGSTN_MountKitExtension* MountData = Kit ? Kit->Extensions.Find<FGSTN_MountKitExtension>() : nullptr;
		return MountData ? &MountData->Rider : nullptr;
	}

	/** Both <Base>L and <Base>R on the mesh, in world space */
	bool FindPair(const USkeletalMeshComponent& Mesh, const FName Base, FTransform& OutL, FTransform& OutR)
	{
		const FName Left(*(Base.ToString() + TEXT("L")));
		const FName Right(*(Base.ToString() + TEXT("R")));
		if (!Mesh.DoesSocketExist(Left) || !Mesh.DoesSocketExist(Right))
		{
			return false;
		}
		OutL = Mesh.GetSocketTransform(Left);
		OutR = Mesh.GetSocketTransform(Right);
		return true;
	}
}

bool UGSTN_MountPresentationLibrary::GetRiderIKTargets(const AActor* Rider, FGSTN_MountIKTargets& Out)
{
	Out = FGSTN_MountIKTargets();
	const UGSTN_LinkPresentationComponent* Presentation = Rider ? Rider->FindComponentByClass<UGSTN_LinkPresentationComponent>() : nullptr;
	if (!Presentation || !Presentation->IsLinkedParked() || Presentation->GetShownRoleTag() != GSTNPiggybackTags::Role_Rider)
	{
		return false;
	}
	const AActor* Carrier = Presentation->GetBodyPartner();
	const USkeletalMeshComponent* CarrierMesh = Carrier ? Carrier->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!CarrierMesh)
	{
		return false;
	}
	const FGSTN_RiderProfile* Profile = GSTNMountPresentationPrivate::FindRiderProfile(Rider);
	const FName HandBase = Profile && !Profile->RiderPoseSocketHands.IsNone() ? Profile->RiderPoseSocketHands : DefaultHandSocketBase;
	const FName FootBase = Profile && !Profile->RiderPoseSocketFeet.IsNone() ? Profile->RiderPoseSocketFeet : DefaultFootSocketBase;
	Out.bHands = GSTNMountPresentationPrivate::FindPair(*CarrierMesh, HandBase, Out.HandL, Out.HandR);
	Out.bFeet = GSTNMountPresentationPrivate::FindPair(*CarrierMesh, FootBase, Out.FootL, Out.FootR);
	Out.Alpha = Presentation->GetPresentationBlendAlpha();
	return Out.bHands || Out.bFeet;
}
