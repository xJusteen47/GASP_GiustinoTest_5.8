// Locomotion base switch (loco-base-1): chooses which main anim graph the GASP Mover character runs.
//   gstn.Loco.Base 0 = current base  /Game/Blueprints/SandboxCharacter_Mover_ABP (GASP + our GSTN nodes and layers)
//   gstn.Loco.Base 1 = clean base    /Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover (untouched GASP sample copy)
// Only meshes whose anim class is one of the two are touched, so ragdolls, NPCs and other ABPs keep theirs.
// Set it before Play (DefaultEngine.ini [ConsoleVariables] or the console); changing it during Play swaps live meshes too,
// but layers the pawn linked at BeginPlay are lost on the swapped instance, so restart Play to compare fairly.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LocoBaseSubsystem.generated.h"

class USkeletalMeshComponent;

UCLASS()
class GASP_GIUSTINO2_API UGSTN_LocoBaseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* CurrentBaseClassPath = TEXT("/Game/Blueprints/SandboxCharacter_Mover_ABP.SandboxCharacter_Mover_ABP_C");
	static constexpr const TCHAR* StockBaseClassPath = TEXT("/Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover.ABP_GSTN_StockMover_C");

	/** 0 = current base, 1 = clean stock base. */
	static int32 GetSelectedBase();

	/** Swaps Mesh to the selected base if it runs one of the two base classes. Returns true when it changed the class. */
	static bool ApplyToMesh(USkeletalMeshComponent* Mesh);

	/** Applies the selected base to every matching mesh of this world. */
	void ApplyToWorld();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void HandleActorSpawned(AActor* Actor);

	FDelegateHandle SpawnHandle;
};
