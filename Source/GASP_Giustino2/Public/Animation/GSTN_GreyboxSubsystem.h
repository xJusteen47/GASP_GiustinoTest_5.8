// Greybox capsule switch (greybox-capsula, owner 2026-11-24 2A): shows a capsule with a nose in place of the character mesh, to judge the
// movement feel without final animations.
//   gstn.Greybox 0 = the character as it is
//   gstn.Greybox 1 = every pawn's character mesh (the GASP driver mesh: an anim class of gstn.Loco.Base's set) is hidden but keeps animating,
//                    and a capsule mesh (/Game/GSTN/Greybox/SK_GSTN_GreyboxCapsule, anim Blueprint ABP_GSTN_GreyboxCapsule) attached to it
//                    copies its pose by bone names and adds squash, stretch and lean (UGSTN_GreyboxAnimInstance)
// The driver stays the invisible mannequin, so hitboxes, hurtboxes, sockets and the simulation do not change (skin decision 1A,
// 2026-11-23: one fixed invisible driver). Skin parts and weapons attached to the driver are hidden with it. Combine with gstn.Loco.Base 2
// for the simple chooser graph. Per process, like gstn.Loco.Base; presentation only.

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
	};

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
	bool bLoadFailed = false;
};
