// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

class AGameModeBase;
class APlayerController;

/** The outcome of comparing the type table checksum a client announced with the server's */
enum class EGSTN_SurfaceJoinVerdict : uint8
{
	/** The client announced the server's checksum */
	Match,

	/** The client announced nothing: it does not call AppendJoinOption / GetJoinOption, so nothing can be compared */
	MissingOption,

	/** The client announced another checksum (or a malformed value): its Surface.Type indices differ from the server's */
	Mismatch
};

/**
 * Connect-time check of the Surface.Type table (review 2026-10-23 B.2). The type indices in the sync state come from the baked
 * table, so a client whose Surface.Type tags differ from its server's reads the wrong surface types with no other sign. The
 * client announces the checksum of its table as the URL option "GSTNSurfaceTypes=<8 hex digits>"; the server (this module, on
 * the PostLogin event, which is the earliest game mode event that sees the connection) compares it with its own and kicks a
 * client that differs.
 *
 * Why PostLogin and not PreLogin: FGameModeEvents::GameModePreLoginEvent does not carry the URL options, and the plugin cannot
 * override the project's game mode or game session. The kick is the same one an admin kick does, right after the player joins.
 *
 * Why the client does not announce itself: the engine has no global hook on the client's connect URL, only the virtual
 * ULocalPlayer::GetGameLoginOptions and the travel URL string, both owned by the game. So the game calls GetJoinOption from its
 * ULocalPlayer::GetGameLoginOptions (or AppendJoinOption on the URL it passes to ClientTravel / OpenLevel). A client that does
 * not is logged as "missing option" and accepted unless UGSTN_SurfaceSettings::bRequireJoinChecksum is set.
 *
 * Pure functions apart from CheckLogin, the server hook: no state.
 */
class GSTNSURFACECORE_API FGSTN_SurfaceJoinCheck
{
public:
	/** The URL option name */
	static const TCHAR* GetOptionName();

	/** "GSTNSurfaceTypes=0000ABCD" for a checksum: what GetGameLoginOptions returns, without a leading '?' */
	static FString MakeOption(uint32 Checksum);

	/** MakeOption of this machine's table */
	static FString GetJoinOption();

	/** Adds "?GSTNSurfaceTypes=<this machine's checksum>" to a travel URL or option string, replacing an option already there */
	static void AppendJoinOption(FString& Options);

	/** Compares the option in a request URL / option string (as the connection stores it) with the server's checksum */
	static EGSTN_SurfaceJoinVerdict Evaluate(const FString& RequestOptions, uint32 ServerChecksum, uint32* OutClientChecksum = nullptr);

	/** The server side of the check for a player who just logged in: kicks a mismatch (and a missing option, when required) */
	static void CheckLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
};
