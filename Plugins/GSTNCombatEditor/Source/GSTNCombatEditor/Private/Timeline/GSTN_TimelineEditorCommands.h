// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * The commands of the timeline editor (combat-m5-editor 6). A TCommands set, so every chord is rebindable in Editor Preferences >
 * Keyboard Shortcuts under "GSTN Timeline Editor". The panel (E4) binds the ones it implements; the commands that belong to
 * later checkpoints are declared now so their chords are reserved and the keyboard settings list them from the first day:
 * Bake sockets (E8), Validate (E10), the view toggles (E6) and the gizmo modes (E7, in the viewport's own set).
 *
 * Registered by the module on startup; Get() is valid while the module is loaded.
 */
class FGSTN_TimelineEditorCommands : public TCommands<FGSTN_TimelineEditorCommands>
{
public:
	FGSTN_TimelineEditorCommands();

	virtual void RegisterCommands() override;

	// Playhead
	TSharedPtr<FUICommandInfo> StepBack;
	TSharedPtr<FUICommandInfo> StepForward;
	TSharedPtr<FUICommandInfo> PreviousKey;
	TSharedPtr<FUICommandInfo> NextKey;
	TSharedPtr<FUICommandInfo> PlayPause;

	// Keys and tracks
	TSharedPtr<FUICommandInfo> AddKeyAtPlayhead;
	TSharedPtr<FUICommandInfo> ToggleAutoKey;
	TSharedPtr<FUICommandInfo> DeleteSelection;
	TSharedPtr<FUICommandInfo> CopyKeys;
	TSharedPtr<FUICommandInfo> PasteKeys;
	TSharedPtr<FUICommandInfo> DuplicateKeys;

	// View
	TSharedPtr<FUICommandInfo> FrameAll;
	TSharedPtr<FUICommandInfo> FrameSelection;

	// Timeline
	TSharedPtr<FUICommandInfo> FitTotalFramesToMontage;

	// Reserved for later checkpoints: declared, bound by the toolkit that implements them
	TSharedPtr<FUICommandInfo> BakeSockets;
	TSharedPtr<FUICommandInfo> ValidateNow;
	TSharedPtr<FUICommandInfo> ToggleShowHitboxes;
	TSharedPtr<FUICommandInfo> ToggleShowHurtboxes;
	TSharedPtr<FUICommandInfo> ToggleShowMovementPath;
};
