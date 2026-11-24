// Pure combat logic: the jump from designer numbers (owner 2026-11-18, docs/notes/handovers/air-momentum-1.md, "Jump designer").
//
// The designer sets the height of a FULL jump (button held through the hold window) and the time it takes to reach the apex; the take-off
// speed and the gravities are derived. The solve is the closed form of the two-phase jump the state machine plays (FGSTN_JumpLogic):
//   phase 1, 0 <= t < T (T = MaxHoldFrames): held, gravity gR = k g;
//   phase 2, T <= t <= apex: gravity g2 = m g (m = 1 with the HoldWindow model, the fall gravity; m = c with CutUntilApex, the cut gravity).
// With a = tApex - T > 0:  v0 = g (m a + k T),  H = g (m a T + k T^2 / 2 + m a^2 / 2)  =>  g = H / (m a T + k T^2 / 2 + m a^2 / 2).
// With tApex <= T the apex is inside the hold: gR = 2 H / tApex^2, v0 = 2 H / tApex, g = gR / k.
// The game steps at a fixed rate (semi-implicit Euler, the cut ramp, whole frames), which loses a few percent of height, so the take-off
// speed is then corrected by a secant search on FGSTN_JumpLogic::SimulateApex (the real state machine): the full height is exact at the
// sim rate, the apex time within a frame. The short hop (released as early as MinHoldFrames allows) is measured the same way.

#pragma once

#include "CoreMinimal.h"
#include "Jump/GSTN_JumpTypes.h"
#include "GSTN_JumpDesigner.generated.h"

/** Designer jump inputs and the values derived from them. Off = the jump stats (attributes) as before */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_JumpDesignerTuning
{
	GENERATED_BODY()

	/** On: the ground jump's speed, the rise / cut / fall gravity scales and the hold frames come from here instead of the jump stats */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (DisplayName = "Jump Designer Enabled"))
	bool bEnabled = false;

	/** Height of the full jump (button held through the hold window), from the take-off point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "10", ForceUnits = "cm"))
	float FullJumpHeight = 350.f;

	/** Time from the take-off to the apex of the full jump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "0.1", ClampMax = "3", ForceUnits = "s"))
	float TimeToApex = 0.5f;

	/** Frames the button can extend the jump (the hold window, MaxJumpHoldFrames) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 MaxHoldFrames = 30;

	/** Frames the jump is held at least, however short the press (MinJumpHoldFrames): sets the short hop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "0", ClampMax = "60", ForceUnits = "Frames"))
	int32 MinHoldFrames = 0;

	/** Held gravity over the fall gravity (the float of the hold). 2.6 / 9 = the qaz1xr default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "0.05", ClampMax = "1"))
	float HoldToFallGravityRatio = 0.29f;

	/** Gravity after an early release over the fall gravity (1 = the qaz1xr default, cut = fall) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "0.2", ClampMax = "4"))
	float CutToFallGravityRatio = 1.f;

	/** Frames the gravity takes to grow after the hold ends (replaces the jump ability's CutRampFrames while the designer is on) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "0", ClampMax = "60", ForceUnits = "Frames"))
	int32 CutRampFrames = 3;

	/** What MaxHoldFrames ends (replaces the jump ability's GravityModel while the designer is on) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled"))
	EGSTN_JumpGravityModel GravityModel = EGSTN_JumpGravityModel::HoldWindow;

	/** The world gravity the scales multiply (Mover's, UE default 980) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (EditCondition = "bEnabled", ClampMin = "1", ForceUnits = "cm/s^2"))
	float WorldGravity = 980.f;

	/** Derived, read only: the ground jump's take-off speed (replaces the JumpVelocity stat) */
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)", meta = (ForceUnits = "cm/s"))
	float DerivedJumpVelocity = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)")
	float DerivedRiseGravityScale = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)")
	float DerivedCutGravityScale = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)")
	float DerivedFallGravityScale = 0.f;

	/** Measured at the sim rate (62.5 Hz) with the real state machine: full jump height and frames to its apex */
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)", meta = (ForceUnits = "cm"))
	float ResultFullHeight = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)", meta = (ForceUnits = "Frames"))
	int32 ResultFullApexFrames = 0;

	/** The short hop: released as early as MinHoldFrames allows */
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)", meta = (ForceUnits = "cm"))
	float ResultMinHeight = 0.f;

	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = "Jump|Derived (read only)", meta = (ForceUnits = "Frames"))
	int32 ResultMinApexFrames = 0;

	/** True once the derived values match the inputs (Refresh ran) */
	bool bDerivedValid = false;
};

struct GSTNCOMBATCORE_API FGSTN_JumpDesigner
{
	/** The continuous closed form only (no step correction): take-off speed and gravity magnitudes in cm/s, cm/s2 */
	static void SolveClosedForm(const FGSTN_JumpDesignerTuning& Tuning, EGSTN_JumpGravityModel Model, double StepSeconds,
		double& OutJumpVelocity, double& OutRiseGravity, double& OutCutGravity, double& OutFallGravity);

	/**
	 * Fills the derived fields of Tuning: closed form, then the take-off speed corrected at StepSeconds with the real state machine (with the
	 * tuning's cut ramp and gravity model), then the full and short hop heights measured. Pure; cheap enough for load time and the editor
	 */
	static void Refresh(FGSTN_JumpDesignerTuning& Tuning, double StepSeconds = 0.016);

	/** Writes the derived values into jump params (scales, hold frames, cut ramp, gravity model) when the designer is on */
	static void ApplyToParams(const FGSTN_JumpDesignerTuning& Tuning, FGSTN_JumpParams& InOutParams);
};
