// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "GSTN_PreviewSubject.generated.h"

class UAnimInstance;
class USkeletalMesh;
class UGSTN_PrimaryKitDefinition;
class UGSTN_SkinDefinition;

/** What the preview shows: nothing, a player kit with one of its skins, or any skeletal mesh (an enemy, a prop, a turret base) */
UENUM()
enum class EGSTN_PreviewSubjectKind : uint8
{
	/** Nothing chosen: the scene shows the floor, a default capsule and the facing arrow */
	None,

	/** A player kit and one of its skins. Read only: the preview never writes the kit or the skin */
	KitSkin,

	/** Any skeletal mesh, humanoid or not. The timeline asset is the same as the player's; only the mesh differs */
	SkeletalMesh
};

/**
 * One extra mesh shown with the main one: a sword in a hand, a turret on a base. The judge of a weapon's reach is the weapon, so the
 * preview can carry any number of these. The relative transform is complete (location, rotation AND scale): a weapon of a different
 * length is a scale along one axis, and nothing here may flatten that to a uniform value.
 */
USTRUCT()
struct FGSTN_PreviewAttachment
{
	GENERATED_BODY()

	/** A skeletal mesh or a static mesh. Soft: nothing is loaded until the preview opens */
	UPROPERTY(EditAnywhere, Category = "Attachment", meta = (AllowedClasses = "/Script/Engine.SkeletalMesh,/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> Mesh;

	/** Bone or socket of the main mesh the attachment hangs from. Empty (or not found): the root of the main mesh. Any name: the skeleton is not assumed */
	UPROPERTY(EditAnywhere, Category = "Attachment")
	FName ParentSocket = NAME_None;

	/** Placement relative to the socket, with scale */
	UPROPERTY(EditAnywhere, Category = "Attachment")
	FTransform Relative = FTransform::Identity;

	bool operator==(const FGSTN_PreviewAttachment& Other) const
	{
		return Mesh == Other.Mesh && ParentSocket == Other.ParentSocket && Relative.Equals(Other.Relative, 1e-4f);
	}
};

/**
 * The thing a timeline's preview shows (combat-m5-editor 5.4 as amended: the preview subject is generic so enemies use the same timeline
 * assets). Everything is a soft reference. Stored per asset in editor-only per-user config (UGSTN_CombatEditorPreviewState), never in the
 * timeline asset, which is a runtime type.
 */
USTRUCT()
struct FGSTN_PreviewSubject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preview")
	EGSTN_PreviewSubjectKind Kind = EGSTN_PreviewSubjectKind::None;

	UPROPERTY(EditAnywhere, Category = "Preview|Player", meta = (EditCondition = "Kind == EGSTN_PreviewSubjectKind::KitSkin", EditConditionHides))
	TSoftObjectPtr<UGSTN_PrimaryKitDefinition> Kit;

	/** A skin of the kit (its CompatibleSkins query). The mesh and the animation class come from the skin */
	UPROPERTY(EditAnywhere, Category = "Preview|Player", meta = (EditCondition = "Kind == EGSTN_PreviewSubjectKind::KitSkin", EditConditionHides))
	TSoftObjectPtr<UGSTN_SkinDefinition> Skin;

	UPROPERTY(EditAnywhere, Category = "Preview|Mesh", meta = (EditCondition = "Kind == EGSTN_PreviewSubjectKind::SkeletalMesh", EditConditionHides))
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/**
	 * Animation class of the subject, shown for reference. The pose of a montage comes from the montage played on a preview instance
	 * (set per frame, never ticked), not from this class: a state machine cannot be set to a frame.
	 */
	UPROPERTY(EditAnywhere, Category = "Preview|Mesh", meta = (EditCondition = "Kind == EGSTN_PreviewSubjectKind::SkeletalMesh", EditConditionHides))
	TSoftClassPtr<UAnimInstance> AnimClass;

	/** Extra meshes (a weapon, a turret) hung from the main mesh. Shown for both kinds of subject */
	UPROPERTY(EditAnywhere, Category = "Preview|Attachments")
	TArray<FGSTN_PreviewAttachment> Attachments;

	bool operator==(const FGSTN_PreviewSubject& Other) const
	{
		return Kind == Other.Kind && Kit == Other.Kit && Skin == Other.Skin && Mesh == Other.Mesh && AnimClass == Other.AnimClass && Attachments == Other.Attachments;
	}
	bool operator!=(const FGSTN_PreviewSubject& Other) const { return !(*this == Other); }
};

/** One attachment with its assets loaded */
struct FGSTN_ResolvedAttachment
{
	/** A USkeletalMesh or a UStaticMesh */
	UObject* Mesh = nullptr;
	FName ParentSocket = NAME_None;
	FTransform Relative = FTransform::Identity;
};

/** A subject with its assets loaded, ready for the scene. Holds raw pointers: the scene takes them at once */
struct FGSTN_ResolvedSubject
{
	USkeletalMesh* Mesh = nullptr;
	TSubclassOf<UAnimInstance> AnimClass;
	TArray<FGSTN_ResolvedAttachment> Attachments;

	/** Short name for the toolbar: the mesh name, or "Kit / Skin" */
	FText Label;

	/** Why there is no mesh (a kit without a skin, a skin without a mesh, a mesh that does not load), empty when all is well. Attachments that failed to load are listed too */
	TArray<FText> Problems;
};

namespace GSTNPreviewSubject
{
	/** Loads what the subject names (synchronously: the editor asked for it). Never fails hard; Problems says what is missing */
	FGSTN_ResolvedSubject Resolve(const FGSTN_PreviewSubject& Subject);

	/**
	 * The rotation that turns the mesh so its forward axis (USkeletalMesh::ForwardAxis, set per asset by whoever imported it, +Y for the
	 * engine's characters) points along +X, which is where the actor faces in root space. No bone names, no humanoid convention.
	 */
	FQuat MeshToRootRotation(EAxis::Type MeshForwardAxis);
}

/** The subject of one asset in the user's config */
USTRUCT()
struct FGSTN_PreviewSubjectEntry
{
	GENERATED_BODY()

	/** Path of the timeline asset, or of the Blueprint of an inline timeline */
	UPROPERTY()
	FSoftObjectPath Asset;

	UPROPERTY()
	FGSTN_PreviewSubject Subject;
};

/**
 * Editor-only, per-user, per-project: the preview subject of each timeline asset (and each ability Blueprint with an inline timeline),
 * keyed by asset path. Lives in EditorPerProjectUserSettings.ini, so it survives a restart, is not shared with the team and is not in the
 * versioned asset (UGSTN_CombatTimeline is a runtime type and carries no editor-only preview properties).
 */
UCLASS(config = EditorPerProjectUserSettings)
class UGSTN_CombatEditorPreviewState : public UObject
{
	GENERATED_BODY()

public:
	/** The stored subject of an asset; false (and OutSubject untouched) when none was stored */
	static bool Find(const FSoftObjectPath& Asset, FGSTN_PreviewSubject& OutSubject);

	/** Stores (or replaces) the subject of an asset and writes the config. An empty subject (Kind None, no attachments) removes the entry */
	static void Store(const FSoftObjectPath& Asset, const FGSTN_PreviewSubject& Subject);

	/** Forgets the asset's entry and writes the config */
	static void Remove(const FSoftObjectPath& Asset);

	/** Re-reads the file, dropping what is in memory. A test of persistence uses it to stand in for a restart */
	static void ReloadFromDisk();

private:
	UPROPERTY(Config)
	TArray<FGSTN_PreviewSubjectEntry> Entries;
};
