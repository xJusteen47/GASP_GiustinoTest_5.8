// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Math/Transform.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FGSTN_TimelineEditorModel;
class FPrimitiveDrawInterface;
class UScriptStruct;

// The extension point of the timeline editor (combat-m5-editor 12). A track type that exists in a runtime module becomes a row of the
// timeline, an entry of Add Track and a section of the details panel by registering one IGSTN_TrackEditor, with no other change to the
// editor. This header is Public on purpose: a plugin that adds a track (a shield window, a projectile spawn) implements the interface in
// its own editor module and registers it from its StartupModule. It includes no Slate and no widget type.

/**
 * What a track editor is given to draw its track into the viewport at the playhead frame (DrawPreview). The preview scene arrives with
 * E6; the context is declared now so a track editor written today needs no change then. Every pointer is valid for the call only.
 */
struct GSTNCOMBATEDITOR_API FGSTN_TrackPreviewContext
{
	/** The track being drawn, never null in a call */
	const FGSTN_TimelineTrack* Track = nullptr;

	/** The timeline the track belongs to */
	const FGSTN_CombatTimelineData* Data = nullptr;

	/** Position of the track in Data->Tracks */
	int32 TrackIndex = INDEX_NONE;

	/** The playhead: the frame the viewport shows */
	int32 Frame = 0;

	/** The track (or one of its keys) is part of the editor's selection: draw it thicker */
	bool bSelected = false;

	/** Root space of the previewed character in world space: the origin that root-space shapes are drawn from */
	FTransform RootTransform = FTransform::Identity;

	/** Where to draw. Null when the call is not from a viewport (tests, a headless refresh): draw nothing then */
	FPrimitiveDrawInterface* PDI = nullptr;
};

/**
 * Optional second face of a track editor: a track whose keys have a pose (the hitbox track of M4) lets the viewport show a gizmo on
 * them (combat-m5-editor 5.5, implemented in E7). Declared with the registry so a track editor can already say it is one.
 *
 * ApplyWidgetDelta writes through the model, never to the data. The model header is private to this module for now: a plugin outside it
 * can implement the query half today, and the write half once E7 publishes the model's mutation surface.
 */
class GSTNCOMBATEDITOR_API IGSTN_GizmoProvider
{
public:
	virtual ~IGSTN_GizmoProvider() = default;

	/** World location of the gizmo for one key at the context's frame. False when the key has no pose there */
	virtual bool GetWidgetLocation(const FGSTN_TrackPreviewContext& Context, int32 KeyIndex, FVector& OutLocation) const = 0;

	/** Rotation of the gizmo's local space (the key's space) in world space. False: use the world axes */
	virtual bool GetWidgetRotation(const FGSTN_TrackPreviewContext& Context, int32 KeyIndex, FQuat& OutRotation) const { return false; }

	/** Applies a gizmo drag in world space to the key, through the model (one transaction, clamping). False when nothing changed */
	virtual bool ApplyWidgetDelta(FGSTN_TimelineEditorModel& Model, const FGSTN_TrackPreviewContext& Context, int32 KeyIndex, const FVector& DeltaTranslation, const FRotator& DeltaRotation, const FVector& DeltaScale) const = 0;
};

/**
 * How the editor shows and creates one kind of track. One implementation per track struct, registered in the
 * FGSTN_TrackEditorRegistry. Stateless: the editor calls it from several timelines at once, so it keeps no timeline in a member.
 *
 * Only GetTrackStruct and GetDisplayName are required. A track editor that overrides nothing else is a window bar in the settings
 * colour, named by the struct, with its fields in the details panel: that is also exactly the row the editor makes for a track
 * struct nobody registered (the generic row), so a new runtime track never breaks the editor and a track editor is a refinement.
 */
class GSTNCOMBATEDITOR_API IGSTN_TrackEditor
{
public:
	virtual ~IGSTN_TrackEditor() = default;

	/** Frames of the window a double-click or Add Track creates when the track occupies a range. Point tracks (event, weapon switch) ignore it: they take one frame */
	static constexpr int32 FallbackDefaultLength = 6;

	/** The struct this editor handles, a child of FGSTN_TimelineTrack */
	virtual const UScriptStruct* GetTrackStruct() const = 0;

	/** Name in Add Track, the name column of the row and the details header */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Colour of the bar and the swatch of the name column. Default: the colour the settings list for the track struct
	 * (UGSTN_CombatEditorSettings::TrackColors), so a designer re-colours a track without code.
	 */
	virtual FLinearColor GetColor() const;

	/**
	 * Name of the Slate brush in the editor style (FAppStyle) drawn in the name column, NAME_None for none. A name, not a brush,
	 * so this header needs no Slate and a brush that does not exist in a later engine version only loses its icon.
	 */
	virtual FName GetIconBrushName() const { return NAME_None; }

	/** Text on the bar and in the tooltip of the name column. Default: the display name. Called on every paint: keep it cheap */
	virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const;

	/**
	 * Does the track have keys (diamonds on the lane, the key gestures)? Default: an IGSTN_KeyedTrackAccess is registered for the
	 * struct. The keyed access registry stays the single source of truth; override only to say the same thing differently.
	 */
	virtual bool HasKeys() const;

	/** Length in frames of the window of a new track (double-click on an empty lane, Add Track). Ignored by point tracks */
	virtual int32 GetDefaultLength() const { return FallbackDefaultLength; }

	/**
	 * Fills the defaults of a track the model has just created, inside the same transaction. The model has already set StartFrame
	 * and EndFrame; StartFrame is the frame the track was added at. A field that follows the start (the weapon switch's SwapFrame)
	 * is set here.
	 */
	virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 AddedAtFrame, const FGSTN_CombatTimelineData& Data) const {}

	/** Draws the track into the viewport for the playhead frame (E6). Default: nothing */
	virtual void DrawPreview(const FGSTN_TrackPreviewContext& Context) const {}

	/** Non-null when the track has keys the viewport can move with a gizmo (E7). The returned object is the editor itself or one it owns */
	virtual const IGSTN_GizmoProvider* AsGizmoProvider() const { return nullptr; }
};

/** Broadcast when an editor is added or removed, so an open Add Track menu or a row list can rebuild */
DECLARE_MULTICAST_DELEGATE(FGSTN_OnTrackEditorsChanged);

/**
 * Which track editor serves which track struct. The module registers the built-ins of the M3 track kinds at startup; a plugin that
 * adds a track registers its editor from its own startup and unregisters it on shutdown:
 *
 *   FGSTN_TrackEditorRegistry::Get().Register(MakeShared<FMyShieldTrackEditor>());
 *
 * A struct with no editor gets the generic row (FindOrGeneric): nothing ever fails to display.
 */
class GSTNCOMBATEDITOR_API FGSTN_TrackEditorRegistry
{
public:
	static FGSTN_TrackEditorRegistry& Get();

	/** Adds the editor for its GetTrackStruct, replacing an earlier one for the same struct. Ignores an editor whose struct is not a track */
	void Register(TSharedRef<const IGSTN_TrackEditor> Editor);

	/** Removes the editor of the struct. False when there was none */
	bool Unregister(const UScriptStruct* TrackStruct);

	/**
	 * The editor registered for the struct, or for its nearest registered base struct (a game that derives from a built-in track keeps
	 * its row until it registers its own). Null when none: use FindOrGeneric to draw anything.
	 */
	TSharedPtr<const IGSTN_TrackEditor> Find(const UScriptStruct* TrackStruct) const;

	/** Find, or the generic editor of the struct (window bar, name from the struct, the details panel for everything else). Never fails */
	TSharedRef<const IGSTN_TrackEditor> FindOrGeneric(const UScriptStruct* TrackStruct) const;

	/** Is this exact struct, or a base of it, registered? */
	bool IsRegistered(const UScriptStruct* TrackStruct) const { return Find(TrackStruct).IsValid(); }

	/** The registered editors, by display name: the content of Add Track */
	void GetAll(TArray<TSharedRef<const IGSTN_TrackEditor>>& OutEditors) const;

	/**
	 * Track structs that exist (concrete children of FGSTN_TimelineTrack that are not hidden) and have no editor: Add Track offers
	 * them as generic rows. Sorted by display name.
	 */
	void GetUnregisteredTrackStructs(TArray<const UScriptStruct*>& OutStructs) const;

	int32 Num() const { return Editors.Num(); }

	FGSTN_OnTrackEditorsChanged OnChanged;

private:
	TMap<const UScriptStruct*, TSharedRef<const IGSTN_TrackEditor>> Editors;

	/** Generic editors, made on demand and kept so a repaint does not allocate */
	mutable TMap<TWeakObjectPtr<const UScriptStruct>, TSharedRef<const IGSTN_TrackEditor>> GenericEditors;
};
