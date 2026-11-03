// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"
#include "Preview/GSTN_PreviewPose.h"
#include "Preview/GSTN_PreviewSubject.h"

class UDebugSkelMeshComponent;
class UMeshComponent;
class USceneComponent;

/** The capsule drawn as the root of the preview: where the character's collision would be. Root space, +X forward, Z up */
struct FGSTN_PreviewCapsule
{
	FVector Center = FVector::ZeroVector;
	float Radius = 34.f;
	float HalfHeight = 88.f;
};

/**
 * The 3D scene of the timeline editor (combat-m5-editor 5.4): the main mesh of the preview subject on a preview anim instance, any number of
 * attached meshes, the floor and the lights of the advanced preview scene. It knows nothing of timelines: the controller tells it the subject,
 * the montage and the frame.
 *
 * "Root space" is the space the timeline's hitboxes live in: the actor, +X forward, Z up, origin at the middle of the capsule. The mesh is
 * placed in it automatically from the mesh alone, so no bone name or humanoid convention is needed: it is turned so the mesh's own forward
 * axis (USkeletalMesh::ForwardAxis) faces +X, and moved down so the middle of its reference-pose bounds is at the origin's height. The
 * capsule is derived from the same bounds (a game that knows its capsule will say so through the subject later). The same transform is what the socket baker (E8) applies to go from component space to root space.
 */
class FGSTN_TimelinePreviewScene : public FAdvancedPreviewScene
{
public:
	FGSTN_TimelinePreviewScene();
	virtual ~FGSTN_TimelinePreviewScene() override;

	/**
	 * Replaces what the scene shows. Rebuilds the main mesh component and one component per attachment. Returns problems that only the scene can
	 * see (a parent socket the main mesh does not have). A null mesh leaves the scene without a subject: the capsule falls back to the default
	 */
	TArray<FText> SetSubject(const FGSTN_ResolvedSubject& Subject);

	/** The component that carries the pose; null while there is no mesh */
	UDebugSkelMeshComponent* GetMeshComponent() const { return MeshComponent.Get(); }

	FGSTN_PreviewPose& GetPose() { return Pose; }
	const FGSTN_PreviewPose& GetPose() const { return Pose; }

	int32 GetNumAttachments() const { return AttachmentComponents.Num(); }
	UMeshComponent* GetAttachmentComponent(int32 Index) const { return AttachmentComponents.IsValidIndex(Index) ? AttachmentComponents[Index].Get() : nullptr; }

	/** Root-space placement of the main mesh (the transform of the placement component the mesh hangs from); identity without a mesh */
	const FTransform& GetMeshToRoot() const { return MeshToRoot; }

	/** The capsule of the root, from the main mesh's bounds, or the default capsule while there is no mesh */
	const FGSTN_PreviewCapsule& GetRootCapsule() const { return Capsule; }

	/** Bounds of everything shown (the main mesh and its attachments) in root space, for framing the camera. A default box without a subject */
	FBoxSphereBounds GetSubjectBounds() const;

	/**
	 * The placement and the capsule for a mesh, from its bounds in mesh space and its forward axis. Pure, apart so the "no assumptions" test can
	 * feed it any shape. OutMeshToRoot rotates the forward axis to +X and drops the bounds' middle to the origin's height.
	 */
	static FGSTN_PreviewCapsule ComputePlacement(const FBoxSphereBounds& MeshBounds, EAxis::Type MeshForwardAxis, FTransform& OutMeshToRoot);

private:
	void ClearSubject();

	/** Carries MeshToRoot. The debug mesh component hangs from it with an identity transform: it resets its own relative transform whenever it changes what it previews (root motion handling), which would drop the placement */
	TWeakObjectPtr<USceneComponent> PlacementComponent;
	TWeakObjectPtr<UDebugSkelMeshComponent> MeshComponent;
	TArray<TWeakObjectPtr<UMeshComponent>> AttachmentComponents;
	FGSTN_PreviewPose Pose;
	FTransform MeshToRoot = FTransform::Identity;
	FGSTN_PreviewCapsule Capsule;
};
