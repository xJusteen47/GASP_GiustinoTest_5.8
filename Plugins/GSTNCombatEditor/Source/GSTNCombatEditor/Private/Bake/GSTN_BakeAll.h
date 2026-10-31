// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Bake/GSTN_SocketBaker.h"

class UGSTN_CombatTimeline;
class USkeletalMesh;

/**
 * Bake all (combat-m5-editor 7.3): bakes the sockets of every timeline asset whose bake is missing or stale, without opening an editor.
 * The mesh of each asset is the one its preview subject names (the per-user config of the timeline editor), else the mesh its last bake
 * recorded, else the settings' default preview skin. An asset with none of those is skipped and said so. The baked assets are marked dirty,
 * not saved, unless the caller asks (the console command's "save" argument), so a designer reviews and submits what changed.
 *
 * Only timeline assets are visited. A timeline inline in an ability Blueprint is baked from its own editor: loading every Blueprint of the
 * project to find the ones that need a bake is what Validate all already does, and a bake needs a mesh, which a Blueprint does not name.
 */
namespace GSTNBakeAll
{
	struct FEntry
	{
		FAssetData Asset;
		EGSTN_BakeStatus Before = EGSTN_BakeStatus::Unknown;
		bool bBaked = false;

		/** Why nothing happened, or what went wrong */
		FText Note;
	};

	struct FOptions
	{
		/** Only assets whose package path starts with this. Empty = all project and plugin content except /Engine and /Temp */
		FString PathPrefix;

		/** Save the packages that were baked */
		bool bSave = false;

		/** A slow task with a cancel button (the console command sets it; tests do not) */
		bool bShowProgress = false;
	};

	struct FResult
	{
		TArray<FEntry> Entries;
		int32 Baked = 0;
		int32 Failed = 0;
		int32 UpToDate = 0;
		int32 Skipped = 0;

		FText Summary() const;
	};

	/** The mesh Bake all reads the sockets of this timeline from, null when nothing names one (see the namespace comment) */
	USkeletalMesh* ResolveMesh(const UGSTN_CombatTimeline& Timeline);

	/** The timeline assets of the registry that this run visits (nothing is loaded) */
	void CollectAssets(const FOptions& Options, TArray<FAssetData>& OutAssets);

	FResult Run(const FOptions& Options);

	/** gstn.Combat.BakeAll [save] [/Package/Path]. Registered by the module, removed at shutdown */
	void RegisterConsoleCommand();
	void UnregisterConsoleCommand();
}
