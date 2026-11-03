// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

struct FGSTN_InputHistory;
class UGSTN_LinkTypeDefinition;
struct FGSTN_ButtonMap;
struct FGSTN_LinkFragment;
struct FGSTN_LinkKitExtension;
struct FGSTN_MashRule;

/**
 * The grabbed side's mash (m6 spec 8, Mash), in the link tick: the parked role that does not act counts the raw press events of the mash buttons
 * (and, with bCountFlicks, the stick flicks) that its input history holds NEWER than the fragment's MashProcessedFrame; each moves the link's
 * EndFrame ReducePerPress earlier (UGSTN_LinkMath::MashProgress, never before the current frame), and MashProcessedFrame takes the history's
 * latest frame, so a frame's press counts once however often the frame is simulated. The command matcher's Mash condition is not used: no slot
 * fires while the grabbed is linked. The escape itself is the watchdog's: the break rule reads EndFrame and breaks with reason Escape on it.
 *
 * Which rule: the grabber's kit grab rules when its kit carries a link extension, else the type's own grab rule (FGSTN_LinkThrow::FindGrabRules);
 * a type with neither has no mash (EndFrame stays 0). The initial EndFrame is decided at formation by the LinkRequest step (InitialEndFrame).
 */
struct GSTNLINKNP_API FGSTN_LinkMash
{
	/** LinkFrame + the rule's MashInitialFrames for the damage percent and the resistance (1 = unscaled) */
	static int32 InitialEndFrame(const FGSTN_MashRule& Rule, int32 LinkFrame, float DamagePercent, float Resistance = 1.f);

	/** The history bits of the rule's buttons through the entity's button map; all buttons for an empty list or without a map */
	static uint32 ButtonMaskOf(const FGSTN_MashRule& Rule, const FGSTN_ButtonMap* Buttons);

	/** Presses of Mask and flicks in History newer than After, in OutPresses / OutFlicks */
	static void CountSince(const FGSTN_InputHistory& History, uint32 Mask, int32 After, int32& OutPresses, int32& OutFlicks);

	/**
	 * One mash tick on the fragment's Body link at Frame: true when EndFrame or MashProcessedFrame changed. Does nothing for a link without an
	 * EndFrame (no mash) or once the escape frame is reached
	 */
	static bool Step(FGSTN_LinkFragment& Fragment, const FGSTN_InputHistory& History, const FGSTN_MashRule& Rule, uint32 Mask, int32 Frame);
};
