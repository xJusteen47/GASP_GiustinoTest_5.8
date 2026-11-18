// Ability-system layer of the combat framework: the presentation of a hitstop (Sakurai's eight hitstop techniques, owner 2026-11-18).

#pragma once

#include "CoreMinimal.h"
#include "AlphaBlend.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "GSTN_HitstopFeel.generated.h"

class AActor;
class UObject;

/** How long the hitstop shake takes to fade to nothing */
UENUM(BlueprintType)
enum class EGSTN_HitstopShakeDecay : uint8
{
	/** Over the hitstop this hit really gives (Sakurai #4: the total length is known up front and the amplitude converges over it) */
	OverHitstop,
	/** Over FGSTN_HitstopShake::DecaySeconds, whatever the hitstop length (the behaviour before 2026-11-18) */
	FixedTime,
};

/** Which attacks push their animation forward during the attacker's hitstop (Sakurai #7) */
UENUM(BlueprintType)
enum class EGSTN_HitstopAdvanceMode : uint8
{
	Off,
	/** Only a move whose attacker carries the advance tag (a Tags track on the move's timeline): an ON/OFF per attack, as in Smash */
	TaggedMoves,
	/** Every move (for a quick test: Sakurai keeps it off on attacks where the push reads wrong) */
	AllMoves,
};

/**
 * Sakurai #6: the victim is not frozen in the pose it was hit in, nor snapped to the damage pose: the reaction blends into its hit pose over a
 * few frames while the hitstop holds, then holds there until the hitstop ends. Applies to the reaction rule that starts while the victim is
 * in hitstop (the damage poses themselves are the reaction set's clips). Presentation only.
 *
 * Owner rule 2026-11-14 (BlendInFrames 0-1, "the first frame of a new state must read as a clear change") still holds for every other start;
 * here the default curve (CircularOut) puts about two thirds of the new pose on the first frame, so the change still reads at once.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_HitstopPoseBlend
{
	GENERATED_BODY()

	/** Off: a reaction started in hitstop uses the rule's own BlendInFrames and plays on through the hitstop (before 2026-11-18) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Pose Blend")
	bool bEnabled = true;

	/** The blend from the pose at the hit into the reaction (60 fps frames). Sakurai: about 4 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Pose Blend", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendFrames = 4;

	/** The blend's curve. CircularOut: fast at first (the hit reads on frame 1), soft into the pose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Pose Blend")
	EAlphaBlendOption BlendOption = EAlphaBlendOption::CircularOut;

	/** The reaction clip plays up to this frame (60 fps, from its start) and holds there while the hitstop lasts: the extreme hit pose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Pose Blend", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 HoldAtFrame = 4;
};

/**
 * Sakurai #7: during its hitstop the attacker's animation still creeps forward by less than one frame in total (the blade pushing through,
 * "zuzuzu"), then resumes as if nothing happened. Done on the presentation of the synced montage only (a lead on the time drawn, see
 * UNetMontageSimulator::SetPresentationLead): the simulation, its timeline frame and the hitboxes never move. After the hitstop the drawn
 * pose waits for the simulation to catch up with it (less than a frame), so nothing jumps back.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_HitstopAdvance
{
	GENERATED_BODY()

	FGSTN_HitstopAdvance();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Advance")
	EGSTN_HitstopAdvanceMode Mode = EGSTN_HitstopAdvanceMode::TaggedMoves;

	/** TaggedMoves: the attacker must carry this tag (default Combat.Feel.HitstopAdvance, granted by the move's Tags track) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Advance")
	FGameplayTag MoveTag;

	/** How far the animation creeps over the whole hitstop, in 60 fps animation frames. Sakurai: less than one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Advance", meta = (ClampMin = "0.0", ClampMax = "1.0", ForceUnits = "Frames"))
	float AdvanceFrames = 0.6f;

	/** True when the attacker carrying OwnerTags gets the advance */
	bool AppliesTo(const FGameplayTagContainer& OwnerTags) const;
};

/** Pure math of the hitstop presentation, for UGSTN_ReactionAnimComponent and the tests */
struct GSTNCOMBATGAS_API FGSTN_HitstopFeelMath
{
	/**
	 * Sakurai #3, in the air: the axis (in the screen plane: X = screen right, Y = screen up) the shake runs along ElapsedSeconds into the
	 * hitstop. A new axis every full shake cycle, turned by the golden angle, so over a hitstop the shake covers every direction; the first
	 * cycle is horizontal. Deterministic (no random): the same hit shakes the same way on every machine
	 */
	static FVector2D AirShakeAxis(float FrequencyHz, float ElapsedSeconds);

	/** Sakurai #3, on the ground: the screen right with the part along the owner's up removed (the feet never go into the floor) */
	static FVector GroundShakeDirection(const FVector& ScreenRight, const FVector& OwnerUp);

	/**
	 * Sakurai #7: the lead (montage seconds) Alpha (0-1) through the hitstop, for a total of MaxLeadSeconds. Eases out: fastest at the
	 * impact, slowing as the resistance builds
	 */
	static float AdvanceLead(float MaxLeadSeconds, float Alpha);

	/** After the hitstop: the lead still needed so the drawn time does not go back from PeakTime while the simulation's ShownTime catches up */
	static float AbsorbLead(float PeakTime, float ShownTime);
};

/**
 * What the hitstop presentation needs from the combat simulation and the synced montage, which live in modules this one cannot see
 * (GSTNCombatNP, the GAS-NPP fork). GSTNCombatNP fills them at startup; unset = the feature falls back (the shake decays over DecaySeconds,
 * no attacker advance). Presentation only: none of them writes the simulation.
 */
struct GSTNCOMBATGAS_API FGSTN_HitstopPresentationHooks
{
	/** Seconds of hitstop left on Actor as its combat simulation shows it now; < 0 = unknown */
	static TFunction<float(const AActor*)> GetHitstopSecondsLeft;

	/** The synced montage the actor shows this frame and its time without any lead (montage seconds); false = none */
	static TFunction<bool(const AActor*, const UObject*& /*OutMontage*/, float& /*OutTime*/)> GetPresentedMontageTime;

	/** Sets the presentation lead (montage seconds) of the actor's synced montage */
	static TFunction<void(const AActor*, float)> SetMontagePresentationLead;
};
