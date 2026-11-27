// Greybox capsule switch (greybox-capsula, owner 2026-11-24 2A): shows a capsule with a nose in place of the character mesh, to judge the
// movement feel without final animations.
//   gstn.Greybox 0 = the character as it is
//   gstn.Greybox 1 = every pawn's character mesh (the GASP driver mesh: an anim class of gstn.Loco.Base's set) is hidden but keeps animating,
//                    and a capsule mesh (/Game/GSTN/Greybox/SK_GSTN_GreyboxCapsule, anim Blueprint ABP_GSTN_GreyboxCapsule) attached to it
//                    copies its pose by bone names and adds squash, stretch and lean (UGSTN_GreyboxAnimInstance)
// The driver stays the invisible mannequin, so hitboxes, hurtboxes, sockets and the simulation do not change (skin decision 1A,
// 2026-11-23: one fixed invisible driver). Skin parts and weapons attached to the driver are hidden with it. Combine with gstn.Loco.Base 2
// for the simple chooser graph. Per process, like gstn.Loco.Base; presentation only. Default 1 since 2026-11-25 (owner: the capsule is the
// playtest look); harness runs pin 0 through Tools/CI/Run-Timed.ps1, so a capture shows the character unless its driver sets 1.
//
// Kit look on the capsule (owner 2026-11-24 14:43-14:45): the kit's stand-in (the coloured box of UGSTN_KitPlaceholderLookSubsystem, hidden
// with the driver) becomes a half-circle collar behind the capsule's upper back, in the same colour (ColourForKit, so it changes with the
// kit); a kit whose asset name contains one of gstn.Greybox.SwordKits (default "Overlord": owner 14:45, the Overlord kit only) also carries the greatsword on the
// back. Both are static meshes made by setup_greybox_assets.py (step accessories), modelled in the capsule's component space and riding
// its pelvis, so they tilt and squash with the rigid body.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_GreyboxSubsystem.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

UCLASS()
class GASP_GIUSTINO2_API UGSTN_GreyboxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* CapsuleMeshPath = TEXT("/Game/GSTN/Greybox/SK_GSTN_GreyboxCapsule.SK_GSTN_GreyboxCapsule");
	static constexpr const TCHAR* CapsuleAnimClassPath = TEXT("/Game/GSTN/Greybox/ABP_GSTN_GreyboxCapsule.ABP_GSTN_GreyboxCapsule_C");
	static constexpr const TCHAR* CollarMeshPath = TEXT("/Game/GSTN/Greybox/SM_GSTN_Greybox_Collar.SM_GSTN_Greybox_Collar");
	static constexpr const TCHAR* SwordMeshPath = TEXT("/Game/GSTN/Greybox/SM_GSTN_Greybox_Greatsword.SM_GSTN_Greybox_Greatsword");

	/** Pure: does a kit whose asset name is KitName carry the greatsword, for a comma-separated list of name parts (case-insensitive) */
	static bool KitCarriesSword(const FString& KitName, const FString& SwordKits);

	static bool IsEnabled();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	/** What one driver mesh looked like before the capsule took its place */
	struct FDriverState
	{
		TWeakObjectPtr<USkeletalMeshComponent> Capsule;
		uint8 TickOption = 0;
		/** Attached visuals (skin parts, weapons) hidden with the driver, and whether each was hidden before */
		TArray<TPair<TWeakObjectPtr<USceneComponent>, bool>> HiddenChildren;
		bool bDriverWasHidden = false;
		/** Kit look on the capsule (null when the mesh assets are missing) and the kit it shows */
		TWeakObjectPtr<class UStaticMeshComponent> Collar;
		TWeakObjectPtr<class UStaticMeshComponent> Sword;
		FPrimaryAssetId ShownKit;
		bool bKitShown = false;
	};

	/** Collar colour and sword visibility for the owner's applied kit (cheap: only acts when the kit changes) */
	void UpdateKitLook(USkeletalMeshComponent& Driver, FDriverState& State);
	class UStaticMeshComponent* AddAccessory(USkeletalMeshComponent& Capsule, class UStaticMesh* Mesh, FName Name);

	void HandleActorSpawned(AActor* Actor);
	bool IsDriverMesh(const USkeletalMeshComponent* Mesh) const;
	void Enable(USkeletalMeshComponent* Driver);
	void Disable(USkeletalMeshComponent* Driver, FDriverState& State);
	bool LoadAssets();

	FDelegateHandle SpawnHandle;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> Meshes;
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FDriverState> Drivers;

	TWeakObjectPtr<class USkeletalMesh> CapsuleMesh;
	TWeakObjectPtr<UClass> CapsuleAnimClass;
	TWeakObjectPtr<class UStaticMesh> CollarMesh;
	TWeakObjectPtr<class UStaticMesh> SwordMesh;
	bool bLoadFailed = false;
};
