// Held objects, world side.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GSTN_HeldObjectActor.generated.h"

class UGSTN_HeldObjectComponent;
class UStaticMeshComponent;

/**
 * A ready-made held object: a replicated actor with a mesh and UGSTN_HeldObjectComponent. Place it (or a Blueprint child with another
 * mesh and a definition) and it can be picked up and thrown. Without a mesh it shows the engine cube scaled to the rest half height.
 * The mesh never collides in this slice (a pickup is a distance check, landing traces the world, characters walk through it).
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API AGSTN_HeldObjectActor : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_HeldObjectActor();

	UGSTN_HeldObjectComponent* GetHeldObject() const { return HeldObject; }

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Held")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Held")
	TObjectPtr<UGSTN_HeldObjectComponent> HeldObject;
};
