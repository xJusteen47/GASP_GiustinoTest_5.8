// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "UObject/StrongObjectPtr.h"

struct FPropertyChangedEvent;
class FGSTN_TimelineEditorModel;
class UAnimMontage;
class USkeletalMesh;

/**
 * Ties the preview scene to one open timeline editor: it owns the scene, holds the subject being previewed, keeps the montage and the pose in step
 * with the model (data edits, the playhead) and stores the subject per asset in the user's config. The toolkit creates one; the viewport tab and
 * the toolbar's subject picker both talk to it, so the two tabs never need to know about each other.
 *
 * The montage is loaded here, synchronously, when the preview needs it: the preview cannot show a pose without it, which the model's
 * "never load on your own" rule does not cover (that rule is about the length checks).
 */
class FGSTN_PreviewController : public TSharedFromThis<FGSTN_PreviewController>
{
public:
	/**
	 * Creates the controller on a model. StorageKey is where the subject is stored in the user's config; the default is the model's target
	 * (the timeline asset, or the Blueprint of an inline timeline). A null path stores nothing (a transient timeline).
	 */
	static TSharedRef<FGSTN_PreviewController> Create(const TSharedRef<FGSTN_TimelineEditorModel>& Model, const FSoftObjectPath& StorageKey = FSoftObjectPath());
	/**
	 * A controller with no timeline behind it: the hurtbox set editor shows a mesh, not a montage, so the pose is the reference pose at frame 0. The subject
	 * is stored under StorageKey like a timeline's (the key is the hurtbox set asset). A null key stores nothing
	 */
	static TSharedRef<FGSTN_PreviewController> CreateWithoutTimeline(const FSoftObjectPath& StorageKey = FSoftObjectPath());
	/** A controller that stores nothing: the subject lives and dies with it. For tests and previews of timelines that are not assets */
	static TSharedRef<FGSTN_PreviewController> CreateUnstored(const TSharedRef<FGSTN_TimelineEditorModel>& Model);
	~FGSTN_PreviewController();

	const TSharedRef<FGSTN_TimelinePreviewScene>& GetScene() const { return Scene; }
	/** The timeline model, null for a controller made without a timeline */
	const TSharedPtr<FGSTN_TimelineEditorModel>& GetModel() const { return Model; }

	const FGSTN_PreviewSubject& GetSubject() const { return Subject; }

	/** Mutable access for the details view of the subject picker. Call SubjectEdited after changing it */
	FGSTN_PreviewSubject& EditSubject() { return Subject; }
	void SubjectEdited();

	/** The same, as the details view of the subject picker calls it */
	void SubjectEditedFromDetails(const FPropertyChangedEvent&) { SubjectEdited(); }

	/** Replaces the subject, applies it and stores it for the asset */
	void SetSubject(const FGSTN_PreviewSubject& NewSubject);

	/** Makes any skeletal mesh the main subject, keeping the attachments the user set. A null mesh changes nothing */
	void SetMainMesh(const TSoftObjectPtr<USkeletalMesh>& Mesh);

	/** Short name of what is shown, for the toolbar */
	FText GetSubjectLabel() const { return Label; }

	/** What is wrong with the subject, in words: missing assets, a socket the mesh lacks. Empty when all is well */
	const TArray<FText>& GetProblems() const { return Problems; }

	/** The pose follows the playhead by itself; this sets it to the playhead now (after the scene or the montage changed) */
	void Refresh();

	/** The key the subject is stored under (null: nothing is stored) */
	const FSoftObjectPath& GetStorageKey() const { return StorageKey; }

	/** The montage the pose plays, loaded (null: the timeline has none, or it did not load). The socket baker needs it as the pose has it */
	UAnimMontage* GetMontage() const { return Montage.Get(); }

	/** The skeletal mesh of the main subject, null while there is none. What the baker reads sockets from */
	USkeletalMesh* GetMainMesh() const;

	/** The scene changed (a new subject, a new pose): the viewport repaints */
	FSimpleMulticastDelegate OnPreviewChanged;

	/** The subject was applied (picked, edited, or set): the mesh may be another one, which the bake status depends on. Not fired by a scrub */
	FSimpleMulticastDelegate OnSubjectChanged;

private:
	FGSTN_PreviewController(const TSharedPtr<FGSTN_TimelineEditorModel>& InModel, const FSoftObjectPath& InStorageKey);
	void Initialize();
	void ApplySubject();
	void HandlePlayheadChanged(int32 Frame);
	void HandleDataChanged();

	/** Loads the timeline's montage if it changed and hands montage, section, rate and tick rate to the pose */
	void UpdateMontage();

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedRef<FGSTN_TimelinePreviewScene> Scene;
	FSoftObjectPath StorageKey;
	FGSTN_PreviewSubject Subject;
	FText Label;
	TArray<FText> Problems;
	TArray<FText> SceneProblems;

	TStrongObjectPtr<UAnimMontage> Montage;
	FSoftObjectPath MontagePath;
	FDelegateHandle PlayheadHandle;
	FDelegateHandle DataHandle;
};
