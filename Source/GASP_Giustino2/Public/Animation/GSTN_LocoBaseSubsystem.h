// Locomotion base switch (loco-base-1): chooses which main anim graph the GASP Mover characters run.
//   gstn.Loco.Base 0 = current base: whatever the pawn or its skin sets (SandboxCharacter_Mover_ABP, or ABP_GSTN_Locomotion
//                      from the kit skin), both carrying our GSTN nodes and layers
//   gstn.Loco.Base 1 = clean base: /Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover (untouched GASP sample copy with its
//                      own chooser copies)
// Only pawn meshes running one of the current base classes are swapped, so ragdolls, NPCs and other ABPs keep theirs.
// The skin component sets its AnimClass after spawn (and again on kit/skin changes), so the swap is re-checked every frame
// instead of once at spawn: a class change by anyone else is caught on the next tick. Setting 0 again restores the class each
// mesh had. Layers linked by the anim set component are relinked by it on the new instance; a montage playing during a swap
// is cut. The cvar is per process: in a networked session every process must set the same value (ini or console).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LocoBaseSubsystem.generated.h"

class USkeletalMeshComponent;

UCLASS()
class GASP_GIUSTINO2_API UGSTN_LocoBaseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* StockBaseClassPath = TEXT("/Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover.ABP_GSTN_StockMover_C");

	/** 0 = current base, 1 = clean stock base. */
	static int32 GetSelectedBase();

	/** True for the anim classes that count as "current base" (swapped out when the clean base is selected). */
	static bool IsCurrentBaseClass(const UClass* AnimClass);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void HandleActorSpawned(AActor* Actor);
	void ApplyToMesh(USkeletalMeshComponent* Mesh);
	UClass* GetStockClass();

	FDelegateHandle SpawnHandle;

	/** Pawn meshes seen since BeginPlay. */
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> Meshes;

	/** The class each swapped mesh ran before, restored when the cvar goes back to 0. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UClass>> Originals;

	TWeakObjectPtr<UClass> StockClass;
	bool bStockLoadFailed = false;
};
