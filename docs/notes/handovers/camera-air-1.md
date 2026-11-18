# Camera air 1: jitter on dair / fair (fisso, _ue59, 2026-11-18) - CAUSE FOUND, FIX PARTIAL, NOT VERIFIED
Branch `claude/camera-air-1` from claude/desktop-jab-fk d871e7aa. Owner bug 19:29Z: camera jitters when dair / fair start (orange kit).

## Measured (scripted PIE, t.OverrideFPS 60, inputs scripted, not the pad)
Script `Tools/GASNPPTest/diag_cam_air.py` (Run-Timed -Preset Capture; env DIAG_KITS, DIAG_OUT, DIAG_CVARS, DIAG_ABS). Rows per frame: actor,
mesh, GameplayCameraComponent (gcc), final POV. Metric `python Tools/GASNPPTest/camair_metric.py <dirs>` = mean/max |second difference of POV z|, f15-60.
- Before (Saved/CamAir/diag): plain jump smooth on both kits (mean 0.43, max 1.5). KIT_Test_Overlord dair: 10 capsule resizes (hh 86 -> 46.7
  -> 35 -> 31.3 ... ), each moves the actor 6-20 cm more than the mesh; POV steps up to 27.7 cm, while gcc (the anchor) stays smooth.
- Cause 1 (engine order): UGameplayCameraComponentBase reads its own location as the start pose and runs the camera in its tick
  (TG_PostPhysics, GameplayCameraComponentBase.cpp:622-669), before our anchor (TG_PostUpdateWork) and before the combat component's body
  capsule presentation (also TG_PostUpdateWork, random order with the anchor). Pinning the gcc to absolute location (DIAG_ABS) did not help.

## Changed (built OK, Saved/Logs/ci/camair-build1.log `Result: Succeeded`)
`FGSTN_CameraAnchorSettings::bEvaluateCameraAfterAnchor` (on): combat sim component -> anchor -> camera component, all TG_PostUpdateWork
(`UGSTN_CameraAnchorComponent::OrderTicks`). Result (Saved/CamAir/diag_fix): GASP's camera now follows the anchor smoothly, but the final POV still
steps (Overlord dair mean 3.93 max 21.0, before 4.51 max 27.7, camair_metric.py diag diag_fix). So a later stage still sees the capsule: suspects, in order: our UGSTN_PlaytestCameraModifier
push-back (400 cm) + its sweep, or a GASP rig switch at move start (POV x shifts 5-12 cm at dair start: crouch/stance rig blend).
One run with push-back off (diag_nopush, DIAG_CVARS gstn.Camera.PlaytestExtraDistance 0 / ExtraHeight 0) looked smoother but had a different
trajectory (runs differed: the script now waits for the ground between items, not yet run).

## Blocked (rule 5, 2 tries): the editor no longer starts
cap5 ESITO BLOCCATO, cap7 ESITO TIMEOUT (Saved/Logs/ci/camair-cap5/7.log.ue.log): stops after "Completed SDK detection", before Zen. The
zenserver (killed with a Run-Timed job at 22:31) was restarted by hand (pid 3740), still stuck: probably a modal dialog on another desktop.
Needs the owner to look at the screen (screen access was refused), close the dialog, then: run diag_fix2 + diag_nopush2 and compare with metric.py.

## Next
1. Owner/desktop: unblock editor start. 2. Compare push-back on/off on the same trajectory. 3. If push-back: sweep from the anchor point,
or damp the push-back result; if rig switch: keep GASP's camera mode/stance fixed during combat moves (data). Then retest both kits.
Cost estimate ~10 $ (Opus, 1 build, 6 captures).
