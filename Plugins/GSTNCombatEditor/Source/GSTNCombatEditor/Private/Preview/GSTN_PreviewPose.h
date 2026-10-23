// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UAnimMontage;
class UDebugSkelMeshComponent;

/**
 * Sets the pose of the preview mesh to a timeline frame, and nothing else (combat-m5-editor 5.4, 7.2). One function, Evaluate(Frame), is the
 * pose the preview shows and the pose the socket baker (E8) will read, so what you see is what is baked.
 *
 * The pose is set, never ticked: the montage is put at
 *
 *     Position = SectionStart + (Frame / TickRate) x PlayRate
 *
 * on a preview anim instance (the one UDebugSkelMeshComponent owns) and the animation is evaluated once with a zero delta. No clock runs, so a
 * frame gives the same pose whatever was shown before, forward or backward (spike S1, docs/notes/m5-spike-report.md). Blend-in does not
 * move the pose (the preview montage is at full weight) and the root bone's track, which is the root motion, is not in the pose: movement
 * of the character is the Movement track's business, so a hitbox in root space needs only the pose. Without a montage the reference pose is
 * shown.
 *
 * Nothing here names a bone: sockets are asked for by whatever name the caller has.
 */
class FGSTN_PreviewPose
{
public:
	/**
	 * The mesh component to pose. Weak: the scene owns it. MeshToRoot is where the mesh sits in root space (the scene's placement), kept here so
	 * GetSocketRootSpace and the baker (E8) apply the same transform
	 */
	void Bind(UDebugSkelMeshComponent* InComponent, const FTransform& InMeshToRoot = FTransform::Identity);

	/**
	 * Chooses what is played: Montage may be null (reference pose). Section (or NAME_None for the whole montage) and PlayRate are the
	 * timeline's, TickRate is the simulation's. Call again when any of them changes; then call Evaluate.
	 */
	void SetMontage(UAnimMontage* InMontage, FName InSection, float InPlayRate, float InTickRate);

	/**
	 * Poses the mesh for a timeline frame (clamped to >= 0) and refreshes its bone transforms. False when no component is bound.
	 * Repeatable: the same arguments always give the same pose (EvaluateIsRepeatable).
	 */
	bool Evaluate(int32 Frame);

	/** The montage position of a frame (seconds), clamped to the montage. 0 with no montage. Pure: the formula above */
	static double ComputePosition(const UAnimMontage* Montage, FName Section, float PlayRate, float TickRate, int32 Frame);

	/** Component-space transform of a bone or socket of the current pose; false if the mesh has none of that name */
	bool GetSocketComponentSpace(FName SocketOrBone, FTransform& OutTransform) const;

	/** The same in root space: with the mesh-to-root placement applied */
	bool GetSocketRootSpace(FName SocketOrBone, FTransform& OutTransform) const;

	bool HasMontage() const { return Montage.IsValid(); }

	/** The position the last Evaluate put the montage at */
	double GetLastPosition() const { return LastPosition; }

private:
	void ConfigurePlayer(UDebugSkelMeshComponent& Mesh);

	TWeakObjectPtr<UDebugSkelMeshComponent> Component;
	FTransform MeshToRoot = FTransform::Identity;
	TWeakObjectPtr<UAnimMontage> Montage;
	FName Section = NAME_None;
	float PlayRate = 1.f;
	float TickRate = 62.5f;
	double LastPosition = 0.0;

	/** The preview instance has been given the current montage (a new montage or a new mesh component needs it again) */
	bool bPlayerConfigured = false;
};
