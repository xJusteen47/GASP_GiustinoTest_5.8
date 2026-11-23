// Debug look: the mannequin in an Astro Bot-like satin metal (owner 2026-11-19, to check a look). Same colours, only metallic and roughness.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_SatinLook.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;

/**
 * While gstn.Debug.SatinMetal is on, every pawn mesh slot whose material has the mannequin's "Primary Metallic" parameter is wrapped in a
 * dynamic instance of itself (its colours, a skin's or a kit's, are kept) that takes the metallic and roughness values of
 * /Game/GSTN/Materials/MI_GSTN_Mannequin_Satin (tune that asset). Off puts the original material back. Pawns that spawn later are covered too.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_SatinLookSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/** The parameters copied from the satin asset */
	static const TArray<FName>& SatinParameters();

private:
	struct FSlot
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		int32 Index = INDEX_NONE;
		TWeakObjectPtr<UMaterialInterface> Original;
		TWeakObjectPtr<UMaterialInstanceDynamic> Satin;
	};

	void Apply();
	void RestoreAll();

	TArray<FSlot> Slots;
	float SinceScan = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SatinAsset;
	bool bSatinTried = false;
};
