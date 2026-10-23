// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UBlueprint;

/**
 * What a timeline editor edits, so the model and the widgets do not care whether the data is a UGSTN_CombatTimeline asset or
 * the inline timeline of an ability Blueprint (combat-m5-editor 4.1). A small value type: it holds weak pointers and resolves the
 * data on every call, never caching a pointer across edits (a Blueprint recompile replaces the object the data lives in).
 *
 * Asset mode is complete. Inline mode is declared so the model and the toolkit are written against the final shape, but a
 * Blueprint context never resolves until E13 implements the lookup on the class default object: Get() is null, IsValid() false.
 */
struct GSTNCOMBATEDITOR_API FGSTN_TimelineEditContext
{
	FGSTN_TimelineEditContext() = default;

	/** The timeline asset is edited in place */
	static FGSTN_TimelineEditContext ForAsset(UGSTN_CombatTimeline* Timeline);

	/** The inline timeline of the Blueprint's ability. Not resolvable before E13 */
	static FGSTN_TimelineEditContext ForBlueprint(UBlueprint* Blueprint);

	/** True when Get() returns data. A context whose asset was deleted or whose property is gone is not valid */
	bool IsValid() const { return Get() != nullptr; }

	bool IsInline() const { return bInline; }

	/** The data being edited, null when the context is not valid. Resolved on every call */
	FGSTN_CombatTimelineData* Get() const;

	/**
	 * The object that transactions record and that undo clients match: the timeline asset, or (inline) the class default
	 * object holding the property.
	 */
	UObject* GetOwner() const;

	/** What the editor identifies the target by and what Save saves: the asset, or the Blueprint. Two editors on one target are refused */
	UObject* GetTarget() const;

	/** Records the owner in the open transaction and marks its package dirty. Call before changing the data */
	void Modify() const;

	/** Tells the owner its data changed (PostEditChangeProperty; inline also marks the Blueprint modified). Interactive for the frames of a drag */
	void NotifyChanged(EPropertyChangeType::Type ChangeType = EPropertyChangeType::ValueSet) const;

	/** Name for window titles and messages */
	FText GetDisplayName() const;

	bool operator==(const FGSTN_TimelineEditContext& Other) const { return GetTarget() == Other.GetTarget(); }

private:
	TWeakObjectPtr<UGSTN_CombatTimeline> Timeline;
	TWeakObjectPtr<UBlueprint> Blueprint;
	bool bInline = false;
};

/**
 * Proof that an editor owns a context. Held by the toolkit for as long as it is open; released when it closes. While a lease
 * exists no second editor can be opened on the same target.
 */
class GSTNCOMBATEDITOR_API FGSTN_TimelineContextLease
{
public:
	~FGSTN_TimelineContextLease();

	/** What the holder does when someone tries to open the same target again: bring its tab to the front */
	TFunction<void()> OnFocusRequested;

	const UObject* GetTarget() const { return Target.Get(); }

private:
	friend class FGSTN_TimelineEditContextRegistry;
	FGSTN_TimelineContextLease() = default;

	TWeakObjectPtr<const UObject> Target;
};

/** Which targets have an open editor. Two toolkits on one asset would fight over undo and the dirty state, so the second is refused. */
class GSTNCOMBATEDITOR_API FGSTN_TimelineEditContextRegistry
{
public:
	/**
	 * Claims the context's target. Returns the lease, or null when the context is invalid or another editor already holds the
	 * target, in which case that editor's OnFocusRequested runs (the open one is focused, as the asset editor subsystem does).
	 */
	static TSharedPtr<FGSTN_TimelineContextLease> Acquire(const FGSTN_TimelineEditContext& Context, TFunction<void()> OnFocusRequested = nullptr);

	/** Is an editor open on this target? */
	static bool IsOpen(const FGSTN_TimelineEditContext& Context);
};
