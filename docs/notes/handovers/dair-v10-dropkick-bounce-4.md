# dair V10 R4 (LightTuck) + shared defence rules, thread dair-dropkick-anim (desktop, _ue59, 2026-11-17), branch claude/project-thread-98dml5
**LightTuckR4**: AS/AM_OverlordDAirLandLightTuckR4 (card overlord_dair_land_lighttuck_r4, Antigravity plan + curve sheet; feet heights fixed by me):
f0 = the tucked air pose at contact (pelvis -47 = 43 cm, knees up, arms crossed), f1-f2 feet slide out to 48 cm, pelvis 40-42 cm, rise f4-f8 head first,
10 frames, montage blends 1/4 f, plays to its end. Authored for 0 f latency (f0 = the dair pose at contact). Kit KIT_Test_OverlordDAirV10R4 /
TL_Overlord_DAirV10R4: windows internal 0-2 LightTuckR4, 3-13 HeavyTuckR3, 14-30 HeavyR2, 31-42 LightR3 (R3 unchanged). Script: setup_overlord_dair_v10.py DAIR_V10_ROUND=R4.
Result (dair_land_early, Saved/DairV10/variant_r4b/.../frames.jsonl v84-94): no pop, arms stay crossed, standing by f8; review package docs/notes/reviews/overlord-dair-v10/r4/.
**OWNER STANDING RULE (2026-11-17 21:41, scope: shared systems)**: everything built for shared systems (locomotion/run/skid/start anims, dodge, knockback,
VFX, bounces, DEFENCE RULES) goes on ALL test kits; move-specific variants (dair V10R*, dropkick TB bounce ability) stay in their own kit until the owner
approves them. Applied here: setup_playtest_dropkick_knockdown.run_all_kits() gave /GSTNDefence/Test/DA_DefenceRules_Test to every kit with a defence
extension and no rules (shield/get-up/tech sets kept), 14 kits: KIT_Test_Heavy, KIT_Test_Hits, KIT_Test_Light, KIT_Test_Medium, KIT_Test_OverlordDAirV10,
V10R2, V10R3, V10R4, KIT_Test_OverlordDropkick, KIT_Test_Ranger, KIT_Example, KIT_Example_DoubleShotA, KIT_Example_DoubleShotB, KIT_Example_Impacts
(log Saved/PlaytestFixes/setup_playtest_dropkick_knockdown_all_kits.txt). Already had rules: the KIT_Test_Defence* kits, KIT_Test_Playtest(+TB*).
Kits WITHOUT a defence extension (e.g. KIT_Test_OverlordDAirV7/V8/V9, jab kits) were not touched: the next desktop thread (F4 build + orange kit) applies the
rule to the rest (adding an extension is a design step: check with the coordinator).
Tests `Run-Timed.ps1 -Preset Test -Tests GSTN`: 1744 Success / 0 Fail (Saved/Logs/ci/test_dair_r4.log.ue.log). No C++ this round, build current.
**F4 build list (not built here)**: (1) claude/project-thread-z9mi0s 615bd9a5: landing fixes (capsule sink, touchdown latency -> 0 f, land-crouch replay
after landing-lag montage, bounce events), steps in project files handoffs/archivio/2026-11/landing-capsule-bounce-fixes.md; once built, the dair tucked heavy window goes
back to designer f4-12 (from f4-14) and the R4 contact-frame dip should go. (2) claude/project-thread-27htqa: ground-contact fix, steps in
docs/notes/handovers/ground-contact-fix-1.md on that branch: build, run Tools/GASNPPTest/ground_contact_setup.py (it edits the shared Mover ABP, like the
z9mi0s covered-landing script: run them ONE AFTER THE OTHER, never in parallel), capture with the fix on and off, then a Fable review.
**Gamepad (owner moved it 21:40-21:48)**: checked every capture of this round (taps, held buttons, drift): input unaffected. But Play started with 1 player
(review stage "parked []"), so a hub pawn stands next to ours in the R4 captures; r4 redone once (r4b, used). The capture script was NOT changed to ignore
physical gamepads (no verified cheap switch): next thread, if needed, restore PIE to 3 players / disconnect the pad before captures.
Uncommitted owner edits left as they were: DA_ReactionAnims, AM_OverlordDAirV9, pie_wall_bounce.py. Attempts: R4 capture 2/2 (both without parking).
