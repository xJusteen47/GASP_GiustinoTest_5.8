# guard-scripts-1 (rebuild phase 1, cloud, Opus): audit gates. Branch claude/project-thread-gz06ss from k3vjuh b5843bdf; no ABP/content
Done, offline-tested (`python <test>` prints ESITO; 4 suites, 21 cases OK). Not compiled, PowerShell not run (cloud):
1. `Tools/AnimPipeline/capture_assert.py <frames.jsonl> <clip> <first>:<last>` or `--spec asserts.json <capture_dir>`: ASSERT: OK
   when the clip (names with `|`, wildcards; matching entries summed) has weight >= 0.8 on >= 90 % of the window (decision: 90 %
   tolerates a one-frame tick-record dip, never another clip owning the pose; `--coverage 1.0` for strict). Rows with no weights =
   "ASSERT: FAIL ... catena di cattura"; old montage rows only with `--montage-implicit`. Test: tests/test_capture_assert.py.
   - C++ `GSTN_GASNPPTestComponent.cpp`: combat.anim now also lists active MONTAGES with blend weight (they are not tick records,
     so attacks were invisible to the assert). Needs the desktop build.
   - `pie_capture_move.py`: sequences take `'expect'`, or env PIE_CAPTURE_ASSERT=<asserts.json>; writes <out>/assert.txt.
     `pie_capture_timed.ps1 -Assert <json>` sets it and passes `-AssertFile` to Run-Timed.
2. Conservation guard `Tools/AnimPipeline/clipcheck/conserve.py` (pure Python): per bone of arm/leg/spine chains swing + peak speed
   (one-frame flicks ignored), pelvis movement, root travel; > 15 % loss = FAIL. Wired into `fix_clips_ue.py` (steps return edits;
   a failing step writes nothing, the clip is not saved and is reloaded; with WRITE also a "total" check) and
   `clip_check.py --conserve <before.fbx> <after.fbx>`. Tests: tests/test_conserve.py, tests/test_fix_guard_mock.py (fake unreal).
   Real data: the existing `ease_snaps` filter would be REFUSED on dash r3 (ball_r toe bend 62 deg released in one frame -> 12 deg
   bump, -77 %), dash r1 and run start r1; run start r3 finger deflick passes. So the dash toe fix belongs in the clip.
3. `Tools/VFX/vfx_spec_check.py` + `Tools/VFX/specs/landing_dust.json`: layers >= 2, dust grows only, dust/solid scene-lit, alpha 0 at
   end; rules by kind (dust/solid/flash) keep the owner rules' scope. Replaces vfx_size_monotonic.py (on the VFX branch 65g9ca).
   Checked against 65g9ca's setup + flipbooks: landing r6c PASS; old self-lit SmokeReal/PuffDust FAIL (unlit; PuffDust shrinks).
   The spec's presets live only on 65g9ca: on this branch `--all` says "setup missing" until the branches meet.
4. `Run-Timed.ps1`: before any UnrealEditor* run renames <project>\Saved\Autosaves\PackageRestoreData.json to .<date>.bak
   (`-KeepRestoreData` skips); `-unattended` always; `-AssertFile` adds the ASSERT line, exit 3 = ran OK but ASSERT not OK.
   SelfTest case 5+6 added (not run: no PowerShell in the cloud).

Desktop checklist (one thread, ~20 min):
1. `powershell -File Tools\CI\Run-Timed.ps1 -SelfTest` -> 5 lines, all PASS.
2. Build (`Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log`) -> ESITO: OK (C++ montage entries).
3. `pie_capture_timed.ps1 -Kit KIT_Test_Overlord -Out Saved\GuardTest -Sequences jab1 -Assert <json {"jab1":[{"clip":"AM_*Jab1*","window":[25,45]}]}>`
   -> run.log.esito line 2 "ASSERT: OK"; frames.jsonl combat.anim shows "AM_... 1.00 ...". Then a wrong clip name -> exit 3.
4. Dry run `fix_clips_ue.py` (no CLIPFIX_WRITE) -> fix_report.json has "guard" per step; dash r3 shows guard_fail filters.
5. `python Tools/VFX/vfx_spec_check.py --all` on a checkout with the VFX branch -> VFX_SPEC: PASS. Open: dash/skid dust specs.
COSTO: 9 $ | 0 min desktop | 0 token review
