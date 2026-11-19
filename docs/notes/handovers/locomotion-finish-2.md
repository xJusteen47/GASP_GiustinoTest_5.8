# Locomotion finish 2: run stop V2, skid attempt 2, hitstop 2A (desktop _ue59, Opus, 2026-11-19) - context near limit, handed back
Branch `claude/locomotion-finish-2` from 6f8c25f1 + merge of `claude/project-thread-ii4go3` (VFX v3 hd1, no file overlap). Reviews and
captures: docs/notes/reviews/locomotion-finish-2/ (raw frames Saved/LF2/{loco1,loco2,hit_light}).

## B. Hitstop 2A + VFX open items 1-2 - Fable PASS (review_fable_hitstop.md), owner videos video/hitstop_2A_*.mp4 (1080p, 60 fps)
- C++ FGSTN_PresentationCue: HitstopTime (Matter default / Energy = full speed in hitstop), MatterNiagara (second system, always slowed),
  bSnapToFloor (trace down 400 cm before Offset). OneShots carry bEnergy. Hit row: Energy + MatterNiagara NS_GSTN_HitDebris (debris moved
  out of NS_GSTN_HitSpark); star life 0.083 s. GroundBounce: snap, offset z 5.
- Not verified: first medium hit f26-30 five identical frames (capture hitch or star age; no frames.jsonl in pie_capture_vfx). Side notes
  (hd1 item 4): strong-hit smoke washes the attacker f62-73, 4-point grain stars hang at head height.
- Headless gotcha: in -run=pythonscript EditorAssetLibrary.does_asset_exist is False until scan_paths_synchronous(["/GSTNPresentation",
  "/Game"], True); setup_presentation_vfx then tries create_asset and fails (nothing overwritten).

## 5. Run stop V2 - Fable FAIL attempt 1 (review_fable_runstop.md)
- V2 clips AS_RunStop_Ganon_{L,R}_V2 written (Tools/GAS/run_stop2_write.py RUNSTOP_VERSION=V2) + route_states; DA points at V2, Mode Off.
- Dust: new event Presentation.Event.RunStopPlant (snapshot bRunStopping from UGSTN_RunPresentationComponent::IsRunStopping; StopRelease
  fires only under 120 cm/s = end of brake). Row: NS_GSTN_SkidDust on root, min 300, max 0.5 s.
- FAIL: posa, back foot steps up at v197-205 (wide brake pose only v190-196), narrow straight legs v205-222; dust too big/long (hides feet
  v194-229) with star glints. Capture 230 frames ends at clip frame 44: rise and exit not verified.
- Attempt 2 (next thread): run_stop_gen.py move the RF_POS/RF_ROT step (and RB_BALL drag, line ~132) to STOP+2..STOP+10, pelvis -11/-12 to
  STOP; dust half size (scale on the cue, scale_size) and half life, a dust system without star grains; capture 260 frames.

## 4. Skid attempt 2 - Fable FAIL (review_fable_skid.md); 2 attempts used -> owner/coordinator decides
- Applied: Skid -> Conduit also at state time >= 0.42 s, xf 0.2; -> Skid guarded by new C++ GetAnimOwnerSkidElapsed < 0.4
  (Tools/GAS/skid_exit_fix.py; backup Saved/Backups/Skid/ABP_GSTN_Locomotion_pre_lf2.uasset). Slide 219 -> 170 cm, clean exit, no pop.
- Remaining: after the exit the pose does not step (right foot planted f151-170). Reviewer's next fix: blend into the run loop at
  matched phase; log the playing clip + time in frames.jsonl first. 7 frozen frames f117-123 at the reverse (hitch or capture stall).

## Not started: 6 L3 dash (sim ledge rule = deterministic Mover/combat work, own thread, Opus high), run start fixes, air crouch landing.
## Also: stray empty Tools/AnimPipeline/run_stop_gen.py and root IN-USO.txt: delete refused by permissions again. vfx_owner_mp4.py falls
back to ffmpeg on PATH. Lock released. Cost: ~1 h 15 desktop, 3 builds, 3 headless runs, 3 Play captures, 3 Fable reviews (~320k tokens).
