// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_CombatCoreTypes.h"
#include "Input/GSTN_DirectionClassifier.h"

class UGSTN_CombatInputSettings;
struct FGSTN_InputHistory;

/**
 * A scripted input session: a list of timed button and stick events that is played into an FGSTN_InputHistory frame by
 * frame, through the real direction classifier. Used by the automation tests, by the slot validator (canonical inputs of
 * a condition) and by debug output, so all of them describe inputs the same way.
 *
 * Notation (space-separated tokens, each "<frame>:<item>"):
 *  - Buttons: "+A" press, "-A" release, "*A" tap (press and release on that frame). A button is a letter (A = bit 0, B =
 *    bit 1...), "#<n>", or the last part of its tag name ("+Attack", "+Crouch"); names resolve through the button tags
 *    given at construction (bit order), by default the standard buttons Attack, Strong, Special, Crouch, Shield, Jump.
 *  - Stick, always the physical stick (Stick space), numpad layout: 8 up, 9 up-right, 6 right, 3 down-right, 2 down,
 *    1 down-left, 4 left, 7 up-left, 5 neutral. Suffix "a" = light push (Aim zone), "t" = full push (Tilt, the default).
 *    "@<deg>[a|t]" puts the stick at an exact angle (0 = up, clockwise). The stick keeps its state until the next stick
 *    token. A trailing "!" makes the move a flick: the stick is released on the previous frame so the classifier sees a
 *    quick leave of Neutral into Tilt (a "!" at frame 0 is an error).
 *  - "cam=<deg>" and "face=<deg>" set the camera and character yaw from that frame on (default 0 and 0: the character
 *    faces where the camera looks).
 *  - Example: "0:face=90 0:4t 6:6t! 7:+A" = stick left, then a flick to the right and the attack button, with the
 *    character turned 90 degrees from the camera.
 *
 * The script is a value: copying it copies the play position, which is how tests fork a session mid-way (a stand-in
 * for rollback). Frames are integers and nothing reads wall-clock time; apart from the classifier's float maths the
 * result is deterministic.
 */
class GSTNCOMBATCORE_API FGSTN_InputScript
{
public:
	/** Script with the standard buttons (GSTNInputTags::Button_*, in that bit order) */
	FGSTN_InputScript();

	/** Script whose button bits follow the given tags */
	explicit FGSTN_InputScript(TArray<FGameplayTag> InButtonTags);

	/**
	 * Parses a notation and plays it into History from frame 0 up to the last token's frame.
	 * @return false (and logs an error, sets OutError) on a syntax error or an impossible sequence.
	 */
	static bool Build(const FString& Notation, const UGSTN_CombatInputSettings& Settings, FGSTN_InputHistory& OutHistory,
		int32& OutLastFrame, FString* OutError = nullptr);

	/** Adds the events of a notation; false and OutError on a syntax error (nothing is added then) */
	bool Parse(const FString& Notation, FString& OutError);

	// ---- Programmatic building (same events the notation produces)

	void AddPress(int32 Frame, uint32 ButtonMask);
	void AddRelease(int32 Frame, uint32 ButtonMask);

	/** Press and release on the same frame */
	void AddTap(int32 Frame, uint32 ButtonMask);

	/** Stick at an angle (0 = up, clockwise) from Frame on; Zone Neutral releases it */
	void SetStick(int32 Frame, float AngleDeg, EGSTN_StickZone Zone);
	void SetStickNeutral(int32 Frame) { SetStick(Frame, 0.f, EGSTN_StickZone::Neutral); }
	void SetCameraYaw(int32 Frame, float YawDeg);
	void SetFacingYaw(int32 Frame, float YawDeg);

	/** Frame of the last event, INDEX_NONE for an empty script */
	int32 GetLastFrame() const;

	/** Bit of a button name (see class comment), INDEX_NONE if unknown */
	int32 FindButton(const FString& Name) const;

	/**
	 * Plays the script into History up to and including UntilFrame, one sample per frame, continuing from where the
	 * previous call stopped.
	 * @return false (logs an error and sets OutError) if the events are impossible: pressing a held button, releasing a
	 * released one, an event in the past.
	 */
	bool Advance(FGSTN_InputHistory& History, const UGSTN_CombatInputSettings& Settings, int32 UntilFrame, FString* OutError = nullptr);

	/** Next frame Advance will play */
	int32 GetNextFrame() const { return NextFrame; }

	/**
	 * The most recent events of a history in this notation, oldest first, e.g. "12:+A 14:6t! 15:-A". Stick directions
	 * are the physical stick; a frame that is too old to know is printed as "?". For logs and the input display.
	 */
	static FString Describe(const FGSTN_InputHistory& History, int32 MaxEvents = 16);

private:
	enum class EEventType : uint8
	{
		Press,
		Release,
		Stick,
		CameraYaw,
		FacingYaw,
	};

	struct FEvent
	{
		int32 Frame = 0;
		EEventType Type = EEventType::Press;
		uint32 ButtonMask = 0;
		float Value = 0.f;
		EGSTN_StickZone Zone = EGSTN_StickZone::Neutral;
	};

	bool ParseToken(const FString& Token, TArray<FEvent>& OutEvents, FString& OutError) const;
	void SortEvents();

	/** Button bit order, for names */
	TArray<FGameplayTag> ButtonTags;

	TArray<FEvent> Events;
	bool bSorted = true;

	// Play position
	int32 NextFrame = 0;
	int32 EventCursor = 0;
	uint32 Held = 0;
	FGSTN_ClassifierState ClassifierState;
	float StickAngle = 0.f;
	EGSTN_StickZone StickZone = EGSTN_StickZone::Neutral;
	float CameraYaw = 0.f;
	float FacingYaw = 0.f;
};
