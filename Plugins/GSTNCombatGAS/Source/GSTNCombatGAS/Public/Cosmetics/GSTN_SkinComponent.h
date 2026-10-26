// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GSTN_SkinComponent.generated.h"

class UAnimInstance;
class UGSTN_LoadoutComponent;
class UGSTN_SkinDefinition;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Puts the skin of the loadout on a pawn (combat-m3 10). Presentation only: it changes a mesh, material overrides and an
 * animation class, and nothing in the simulation reads it, so it runs on every machine that renders the pawn and not at all on a
 * dedicated server.
 *
 * Put it on the pawn. It follows the loadout component of the pawn's PlayerState: when the pawn is spawned, and whenever the
 * applied loadout changes, it applies the loadout's skin (or the pawn's own look when the skin is empty). A client's pawn can
 * exist before its PlayerState has replicated, so it retries for a short while; a game that wires the PlayerState earlier calls
 * BindToLoadout itself. The pawn's original mesh, materials and animation class are remembered the first time a skin is applied
 * and put back by RestoreOriginal, so changing skins never stacks.
 *
 * The skin asset's content is soft; it is loaded here, once, when the skin is applied.
 */
UCLASS(ClassGroup = "GSTN Combat", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_SkinComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_SkinComponent();

	/** Follow the PlayerState's loadout component by itself when the component begins play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin")
	bool bAutoBind = true;

	/** How many times a not-yet-replicated PlayerState is waited for before giving up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin", meta = (ClampMin = "0", EditCondition = "bAutoBind"))
	int32 MaxBindAttempts = 40;

	/** Seconds between two attempts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin", meta = (ClampMin = "0.01", ForceUnits = "s", EditCondition = "bAutoBind"))
	float BindRetryInterval = 0.25f;

	/** Follows Loadout from now on and applies its current skin. Replaces a previous binding */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void BindToLoadout(UGSTN_LoadoutComponent* Loadout);

	/** Looks the PlayerState's loadout component up and binds to it. False if there is none yet */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	bool TryBindToPlayerState();

	/**
	 * Applies Skin to the pawn, starting from its original look. Null or a skin that changes nothing restores the original.
	 * Does nothing on a dedicated server or without a skeletal mesh. Returns true if the pawn now shows the requested look.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	bool ApplySkin(UGSTN_SkinDefinition* Skin);

	/** Puts back the mesh, material overrides and animation class the pawn had before any skin */
	UFUNCTION(BlueprintCallable, Category = "Combat|Skin")
	void RestoreOriginal();

	/** The skin shown now, null for the original look */
	UFUNCTION(BlueprintPure, Category = "Combat|Skin")
	UGSTN_SkinDefinition* GetAppliedSkin() const { return AppliedSkin.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The mesh the skin goes on: the Character's mesh, else the owner's first skeletal mesh component */
	virtual USkeletalMeshComponent* GetTargetMesh() const;

private:
	UFUNCTION()
	void HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision);

	void RetryBind();
	void CaptureOriginal(const USkeletalMeshComponent& Mesh);
	void RestoreLook(USkeletalMeshComponent& Mesh);

	TWeakObjectPtr<UGSTN_LoadoutComponent> BoundLoadout;
	TWeakObjectPtr<UGSTN_SkinDefinition> AppliedSkin;
	FTimerHandle RetryHandle;
	int32 AttemptsLeft = 0;

	/** The look before the first skin */
	bool bCaptured = false;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> OriginalMesh;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> OriginalAnimClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalOverrides;
};
