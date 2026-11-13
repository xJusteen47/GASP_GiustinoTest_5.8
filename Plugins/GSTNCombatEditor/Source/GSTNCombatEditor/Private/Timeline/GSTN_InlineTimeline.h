// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditContext.h"

class UGSTN_CombatTimeline;

/**
 * What inline mode (combat-m5-editor 2.1 item 3, 4.1) needs beyond the edit context: opening the timeline editor on a target, the
 * decision after a Blueprint compile, and the extraction of an inline timeline into a timeline asset. Plain functions with no Slate
 * type in their signatures, so the tests call them without a window.
 */
namespace GSTNInlineTimeline
{
	/**
	 * Opens the timeline editor window on the context (an asset or an ability Blueprint). False, with nothing opened, when the context is
	 * not valid or the target already has an editor, which is then brought to the front.
	 */
	bool OpenEditor(const FGSTN_TimelineEditContext& Context);

	/** What an open inline editor does when its Blueprint has just compiled */
	enum class EAfterCompile : uint8
	{
		/** The data is still there: the editor redraws from it (the default object is a new one, the values were carried over) */
		Refresh,
		/** The Blueprint has no inline timeline any more (reparented to something else, its class could not be built): the editor closes with a message */
		Close
	};

	EAfterCompile DecideAfterCompile(const FGSTN_TimelineEditContext& Context);

	/** True when the ability already names a Timeline Asset, so an extraction would replace what plays at run time with the new asset */
	bool ExtractWouldReplaceAsset(const FGSTN_TimelineEditContext& Context);

	/** Asked before an extraction replaces the ability's Timeline Asset: the one it names now; true = go ahead */
	using FConfirmReplaceAsset = TFunction<bool(const FSoftObjectPath& ExistingAsset)>;

	/**
	 * Moves the inline timeline into NewAsset: copies the data, points the ability's TimelineAsset at it and clears the inline property,
	 * all in one transaction on the default object, then marks the Blueprint modified. NewAsset is a freshly created, empty asset the
	 * caller owns (creating and saving it is the caller's business: a dialog in the editor, a transient object in a test). False, with
	 * nothing changed, when the context is not an inline one that resolves, or the asset is null.
	 *
	 * An ability that already names a Timeline Asset (the inline data is shadowed and does not play) keeps it unless ConfirmReplace is
	 * given and says yes: with none, or a no, nothing changes and the result is false. The callback is a parameter so the editor can
	 * show a dialog and a test can answer for it.
	 */
	bool ExtractToAsset(const FGSTN_TimelineEditContext& Context, UGSTN_CombatTimeline* NewAsset, const FConfirmReplaceAsset& ConfirmReplace = FConfirmReplaceAsset());

	/** The package path ("/Folder") new assets next to the Blueprint go to, and a name for the extracted timeline ("<Blueprint>_Timeline") */
	void GetExtractDefaults(const FGSTN_TimelineEditContext& Context, FString& OutPackagePath, FString& OutAssetName);
}
