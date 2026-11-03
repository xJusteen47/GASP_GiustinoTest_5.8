// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditorCommands.h"

#include "Framework/Commands/InputChord.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_TimelineEditorCommands::FGSTN_TimelineEditorCommands()
	: TCommands<FGSTN_TimelineEditorCommands>(
		TEXT("GSTNTimelineEditor"),
		LOCTEXT("TimelineEditorCommandsContext", "GSTN Timeline Editor"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FGSTN_TimelineEditorCommands::RegisterCommands()
{
	UI_COMMAND(StepBack, "Step Back", "Move the playhead one frame back", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma));
	UI_COMMAND(StepForward, "Step Forward", "Move the playhead one frame forward", EUserInterfaceActionType::Button, FInputChord(EKeys::Period));
	UI_COMMAND(PreviousKey, "Previous Key", "Move the playhead to the previous key or window edge", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Comma));
	UI_COMMAND(NextKey, "Next Key", "Move the playhead to the next key or window edge", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::Period));
	UI_COMMAND(PlayPause, "Play", "Play or pause the timeline at the real tick rate", EUserInterfaceActionType::Button, FInputChord(EKeys::SpaceBar));

	UI_COMMAND(AddKeyAtPlayhead, "Add Key", "Add a key at the playhead to the selected track, with the values of the curve at that frame", EUserInterfaceActionType::Button, FInputChord(EKeys::K));
	UI_COMMAND(ToggleAutoKey, "Auto-Key", "A gizmo drag on a frame without a key creates the key first", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::K));
	UI_COMMAND(DeleteSelection, "Delete", "Delete the selected keys, or the selected tracks when no key is selected", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(CopyKeys, "Copy Keys", "Copy the selected keys of one track", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(PasteKeys, "Paste Keys", "Paste the copied keys at the playhead on the selected track", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));
	UI_COMMAND(DuplicateKeys, "Duplicate Keys", "Copy the selected keys to the playhead", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::D));

	UI_COMMAND(FrameAll, "Frame All", "Show the whole timeline", EUserInterfaceActionType::Button, FInputChord(EKeys::Home));
	UI_COMMAND(FrameSelection, "Frame Selection", "Show the selected windows and keys", EUserInterfaceActionType::Button, FInputChord(EKeys::F));

	UI_COMMAND(FitTotalFramesToMontage, "Fit Length to Montage", "Set the total frames to the length of the montage section at its play rate. Never done automatically", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(BakeSockets, "Bake Sockets", "Bake the socket transforms of this timeline", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));
	UI_COMMAND(ValidateNow, "Validate", "Validate this timeline now", EUserInterfaceActionType::Button, FInputChord(EKeys::F7));
	UI_COMMAND(ToggleShowHitboxes, "Show Hitboxes", "Draw the hitboxes in the preview viewport", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleShowHurtboxes, "Show Hurtboxes", "Draw the hurtboxes in the preview viewport", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleShowMovementPath, "Show Movement Path", "Draw the approximate path of the movement tracks in the preview viewport", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
