// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/**
 * Console-only debug draw of the defence (m6 spec 11, plan F13; the Gameplay Debugger rows are not built, X19):
 *   gstn.Combat.Debug.Shield 1   the bubble sphere while it exists, the shield value as a bar over the pawn, and the parry windows coloured by
 *                                source (the shield's perfect shield in cyan, a move's parry window in magenta, white once a hit was parried)
 *   gstn.Combat.Debug.Tech 1     the tech lockout and the tech / knockdown windows as bars over the pawn, with their remaining frames
 *
 * It reads what this machine shows: the latest combat state of each registered pawn (simulated or interpolated) at that state's own frame,
 * and the content the pawn is attached with (FGSTN_DefenceAttach). It never writes state, so it is the same with or without rollback. A
 * pawn with no defence attach draws nothing. Development builds only (compiled out with ENABLE_DRAW_DEBUG off and in Shipping).
 */
class FGSTN_DefenceDebugDraw
{
public:
	/** Hooks the draw to every world's post actor tick (module startup); a no-op in Shipping */
	static void Register();
	static void Unregister();

	static bool IsShieldEnabled();
	static bool IsTechEnabled();

	/** Draws one pawn's shield and tech state (whatever the console variables enable) */
	static void DrawEntity(const AActor* Owner);

private:
	static void DrawWorld(UWorld* World);
};
