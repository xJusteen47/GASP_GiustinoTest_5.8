#pragma once

#include "CoreMinimal.h"

/**
 * Connect-time agreement on the combat sync fragments (M6 F11 task B).
 *
 * A fragment travels as (type id, length, payload) and a machine without the struct for an id skips it
 * (FGSTN_SyncFragmentList::NetSerialize): a client with a fragment plugin disabled would join a server that has it and simply
 * never see that state, predicting without it and reconciling forever. So the set of registered fragments is part of what two
 * machines must share to play together, like the engine and project versions.
 *
 * It goes through the engine's own compatibility path rather than a new message: the module folds
 * FGSTN_SyncFragmentRegistry::GetSignature() into the local network version (FNetworkVersion::GetLocalNetworkVersionOverride),
 * which the client sends in its Hello and the server compares before anything is replicated; a mismatch is refused there with
 * the engine's "incompatible version" response. The compatibility override logs the fragments of this machine when the versions
 * differ, so the refusal says what to look at. Handshake-time code, not simulation code.
 *
 * If a project already binds either delegate, the version override keeps its value as the base (chained) and the compatibility
 * override is left alone (the plain version comparison still refuses the client, without the extra log line).
 */
struct GSTNCOMBATNP_API FGSTN_FragmentNetworkVersion
{
	/** Binds the delegates; called by the module at startup */
	static void Install();

	/** Unbinds what Install bound and restores a chained override */
	static void Uninstall();

	/**
	 * The engine/project version without the fragments (a chained project override if there was one). The engine caches whatever it computed
	 * last and returns that cache even when asked for the base, so this recomputes and drops the cache again
	 */
	static uint32 GetBaseVersion();

	/** The network version for an engine/project base and a fragment signature: the base itself when the signature is 0 (no fragments) */
	static uint32 Combine(uint32 BaseVersion, uint32 FragmentSignature);

	/** The compatibility rule: equal versions. Logs this machine's fragments when they differ (bLog) */
	static bool IsCompatible(uint32 LocalVersion, uint32 RemoteVersion, bool bLog = true);
};
