// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UScriptStruct;

/**
 * The track editors of the M3 track kinds: cancel window, movement, tags, event and weapon switch (combat-m5-editor 5.2). The Hitbox
 * and HurtboxModifier rows arrive with E5b, when M4's hitbox and hurtbox data exist; until then those two shells, and every track
 * struct a game adds without an editor, get the generic row. There is no charge track in M3's Core, so there is no charge editor.
 *
 * The module registers them at startup and removes them on shutdown. They are plain IGSTN_TrackEditor implementations in
 * GSTN_TrackEditors.cpp: the same file shows a plugin author what an editor looks like.
 */
namespace GSTNTrackEditors
{
	void RegisterBuiltIns();
	void UnregisterBuiltIns();

	/** The tags of a container or query as "Leaf, Leaf" (the last segment of each tag, in the order stored). Empty text for none. Exposed for the tests of the wording */
	FText JoinTagLeaves(const TArray<FGameplayTag>& Tags);
}
