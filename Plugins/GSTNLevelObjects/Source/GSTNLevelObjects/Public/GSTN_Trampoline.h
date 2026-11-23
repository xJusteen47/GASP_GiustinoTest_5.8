// A placeable trampoline: an adjustable box that launches whoever lands on its top face (M4 H11b).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_SurfaceProvider.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_Trampoline.generated.h"

class UArrowComponent;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** How the optional mesh follows the size of the box */
UENUM(BlueprintType)
enum class EGSTN_TrampolineMeshFit : uint8
{
	/** The mesh is scaled per axis so its bounds fill the box */
	StretchToBox,
	/** The mesh keeps the scale it has (a designer-made mesh with its own proportions) */
	Keep
};

/**
 * A trampoline: a box (depth X, width Y, height Z, in cm) whose top face launches. The launch face is the actor's local up, so tilting
 * the actor tilts the launch, and a trampoline mounted on a wall launches away from it. Launch directions are computed in the actor's own
 * frame (UGSTN_SurfaceUtils::ComputeLaunchDirection), never from world up, so it works under any gravity.
 *
 * It is an IGSTN_SurfaceProvider on the actor: it answers Surface.Type.Trampoline for every face (a tumbling victim bounces off any
 * side, the H11 row) and Surface.Behaviour.Launch only for the top face, where the combat simulation queues the launch move. Nothing in
 * the object is simulation state. The launch parameters and the size are static configuration read by every machine from the level
 * (rollback rule): a trampoline that moves or changes its parameters during a match is out of M4, and the validator warns when its
 * Mobility is not Static.
 *
 * The mesh is optional and soft: the CDO never loads it, an instance loads it in OnConstruction. The collision is the box.
 * To change mesh, size or launch without C++, make a Blueprint child and set the properties.
 */
UCLASS(Blueprintable, ClassGroup = (GSTN), meta = (DisplayName = "GSTN Trampoline"))
class GSTNLEVELOBJECTS_API AGSTN_Trampoline : public AActor, public IGSTN_SurfaceProvider
{
	GENERATED_BODY()

public:
	AGSTN_Trampoline();

	/** Full dimensions of the box in cm: depth (X), width (Y), height (Z). The edit widget in the viewport moves the far corner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline", meta = (ClampMin = "10", ForceUnits = "cm", MakeEditWidget))
	FVector Size = FVector(200.0, 200.0, 40.0);

	/** What the trampoline looks like; empty = no mesh (the box is invisible). Soft: not loaded by the CDO. Default: the engine's cube */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline|Mesh")
	TSoftObjectPtr<UStaticMesh> MeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline|Mesh")
	EGSTN_TrampolineMeshFit MeshFit = EGSTN_TrampolineMeshFit::StretchToBox;

	/** Offset of the mesh from the centre of the box, in the actor's local space */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline|Mesh", meta = (ForceUnits = "cm"))
	FVector MeshOffset = FVector::ZeroVector;

	/** How the top face launches. Static configuration: identical on every machine, never written at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline|Launch")
	FGSTN_SurfaceLaunchParams Launch;

	/** Contact normals within this angle of the actor's up count as the top face (the face that launches) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trampoline|Launch", meta = (ClampMin = "1", ClampMax = "60", ForceUnits = "Degrees", AdvancedDisplay))
	float LaunchFaceToleranceDegrees = 25.f;

	/** Half extent of the box for a size, each axis at least half the minimum size. Pure */
	static FVector SizeToBoxExtent(const FVector& InSize);

	/** Scale of the mesh for a fit mode: StretchToBox divides the box size by the mesh's bounds size, Keep returns 1. Pure */
	static FVector ComputeMeshScale(EGSTN_TrampolineMeshFit Fit, const FVector& InSize, const FVector& MeshBoundsSize);

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Static configuration only: answers from Launch and the actor's transform, the same on every machine */
	virtual FGSTN_SurfaceBehaviour GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UBoxComponent* GetBox() const { return Box; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	/** Rebuilds the box extent, the mesh and the editor preview from Size and the mesh settings */
	void Rebuild();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trampoline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trampoline", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

#if WITH_EDITORONLY_DATA
	/** Shows the launch direction of the top face; never part of a packaged game */
	UPROPERTY()
	TObjectPtr<UArrowComponent> LaunchArrow;
#endif
};
