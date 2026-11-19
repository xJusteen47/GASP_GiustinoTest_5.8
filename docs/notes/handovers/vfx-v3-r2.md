# Handover VFX v3 r2 (2026-11-18, laptop, Opus, budget ~15 $)
Branch `claude/vfx-v3` from 6984be39 (pushed as `claude/project-thread-ltu6q4`). Captures r4 light + dark, 6 sequences each, **Fable high: FAIL**
(`docs/notes/reviews/vfx-v3/r4/REVIEW.md`, 9 labelled fixes). Nothing goes to the owner.
## Done
- Build 5.9 laptop OK (`Saved/Logs/ci/v3-r2-build.log`, "Result: Succeeded"). **The r1 crash fix holds**: 5 PIE runs, no ensure/fatal.
- Owner rule "dust only on run start and skid": the table still had `Jump.Ground -> NS_GSTN_JumpPuff` (r1 handover said "owner to confirm").
  Row removed in `setup_presentation_table.py`, table rebuilt ("12 rows", r3b light run.log.ue.log); the log now shows `Jump.Ground ... niagara=-`.
- `pie_capture_vfx.py`: the backdrop floor was put at pawn z - 92, ~4 cm UNDER the hub floor (r3: blue hub floor in every frame, "light"
  never captured). Now a down trace gives the real floor (log "backdrop floor z -0.0 (pawn z 100.0)"), plane +1 cm.
- `vfx_v3_capture.ps1 -Pre <module>` runs a Tools/GAS setup module before the capture. `vfx_v3_review.py`: no numpy (PIL only), 60 fps
  animated WebP when ffmpeg is missing (laptop), sheets from the SIDE view (game/close cameras sit behind the attacker: the victim hides the hit).
- Pack: `docs/notes/reviews/vfx-v3/r4/` 12 sheets "astro_N | 8 side frames" + side WebP (committed); game/close WebP only local in the same folder.
## Laptop traps (new)
- Run-Timed "BLOCCATO" at editor start right after "Completed SDK detection": the Zen DDC server never came up (no zenserver log that run).
  Fix: start it by hand from `%LOCALAPPDATA%\UnrealEngine\Common\Zen\Install\zenserver.runcontext` before the run.
- 6 sequences in one run time out at 14 min on the laptop: 3 per run (r4 = light, light2, dark, dark2; merged into Saved/VfxV3/r4pack).
## Fable FAIL, next round (code/asset thread on the desktop or laptop, ~1 h editor + ~5 $ review)
1. look, dust (run start, skid, landing): 1-2 grey smudges of 20-30 cm, spawned 1-1.5 m behind, invisible on the light floor. Sprite size
   x3-4, peak opacity 0.6-0.7, spawn at the foot, darker underside (surface colour x0.6-0.7) in M_GSTN_VFX_Smoke.
2. look, hit: star good (crisp, 8 rays, peak on frame 1) but small and pale vs astro_1: start scale x1.5-2, fringe/streaks more saturated, gone by frame 4.
3. dati: hit_debris looks like hit_medium (no shards, no cloud): bind debris to the strong tier. Fable also sees a "white ring" on the
   ground at bounce/landing: log shows only NS_GSTN_LandRing (v3 = puffs on a 30 cm circle) and NS_GSTN_Impact fire, so it is either
   those puffs or the contact shadow disc. **Not verified**: check one frame (dark jump v49/v121) before changing data.
4. cattura: the character leaves the side frame in jump and ground_bounce (follow the pawn); no sprint lines over the light floor (check
   the 450 cm/s gate in that run); "light" hit floor reads mid grey (backdrop colour 0.82 under the hub light: raise it or make it unlit).
## Choices applied (rule 6)
Jump cue removed (owner rule beats the r1 "3 light puffs"). Landing dust kept (owner rule names run start and skid only, so landing is
a question for the owner: listed for the coordinator). Dark dust colour 0.30,0.28,0.26 (my pick, no surface types in the hub).
Cost: ~12 $ estimated (build 7.5 min, 5 capture runs, Fable review ~100k tokens).
