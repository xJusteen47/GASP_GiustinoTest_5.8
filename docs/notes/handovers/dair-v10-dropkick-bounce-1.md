# dair V10 + dropkick tech bounce, thread dair-dropkick-anim (desktop, _ue59, 2026-11-17), branch claude/project-thread-98dml5 from 9a702cc9
Reviews: docs/notes/reviews/overlord-dair-v10/ and dropkick-tech-bounce-v1/ (frames.md, 8 grids each base-top/variant-bottom, mp4). Not reviewed by the owner.
**Dair V10** (variant, V9 untouched): card cards/overlord/dair/overlord_dair_v10.json = V9 with the pelvis of charge keys 0/3/8/10 lowered 36/15/16/13 cm and the
0->3 spacing charge_long -> hold_recoil. Cause (BASE 5.9): the fall pose has the pelvis ~62 cm over the root, V9's charge ~96 -> the body popped up.
Head above root, anim 0-11: V9 124.6-149.7 cm, V10 111.6-114.7 cm (fall pose 110) (`Saved/DairV10/head_curve.json`, `python Saved/AnimR2/run_in_editor.py Saved/DairV10/head_curve.py`).
In game, head world steps over the charge (dair_whiff frames.jsonl): V9 +29/+35/-12 cm, V10 +5.5..+11 (follows the jump arc). Arms, stomp key 15, hit 2 f, active 15-17, FAF 42 unchanged.
Landing (stomp-atterraggio.md): TL_Overlord_DAirV10 = duplicate of TL V9 + 3 Landing tracks LandingLag (internal 0-2 light 4 f, 3-30 heavy 16 f, 31-42 light 4 f;
designer f1-3/f4-31/f32-43), no landing hit; clips AS/AM_OverlordDAirLandHeavy (16 f) / LandLight (4 f), Antigravity plans + curve sheets next to the cards.
Script: Tools/GAS/setup_overlord_dair_v10.py (log Saved/DairV10/setup_overlord_dair_v10.txt). Body Capsule re-measured on V10.
**Dropkick bounce**: BASE: the user is never downed (TL_Overlord_Dropkick has no Combat.Move.KnockdownOnLanding window, and KIT_Test_Playtest's defence
extension has no rules while the standard combat rules carry none -> no defence data; diag log `Saved/DropkickBounce/diag3/run.log.ue.log`); the montage Land section plays.
Variant chain (owner's kit untouched): TL_Overlord_DropkickTB (+window, +BounceAbility), GA_Overlord_DropkickTB, KIT/WPN_Test_PlaytestTB (rules DA_DefenceRules_Test),
GA/AM/AS_OverlordDropkickTechBounce (15 f, card overlord_dropkick_techbounce, r1 retimed to the measured hop). Script Tools/GAS/setup_dropkick_tech_bounce.py.
Hop measured: +22 cm, peak contact +8, ground +12 (variant3 dropkick_whiff frames.jsonl); GroundHit (Knockdown rule) takes over at the second touchdown.
**C++** (generic): FGSTN_SelfLandingBounceTrack::BounceAbility (move's own bounce ability, falls back to the kit's GroundBounce when not registered),
SelfKnockdown out param, UGSTN_DefenceContentLibrary::SetSelfLandingBounceAbility, UGSTN_TimelineUtils::AddLandingTrack/RemoveLandingTracks, rare-event logs
(LogGSTNDefence: armed/disarmed/landed, "runs a move but has no defence data"). Tests GSTNDefence.NP.Landing.SelfBounceOwnAbility, GSTNCombat.Core.Landing.AuthoringHelpers.
Build: `Tools\CI\Run-Timed.ps1 -Preset Build` OK (Saved/Logs/ci/build_dair_bounce_4.log). Tests: `Run-Timed.ps1 -Preset Test -Tests GSTN` 1743 Success / 0 Fail
(`grep -c "Result={Success}" / "Result={Fail"` on Saved/Logs/ci/test_dair_bounce_1.log.ue.log).
Tools: Tools/GASNPPTest/pie_capture_timed.ps1 (timed Play capture, vd2), pie_capture_pair.py (base/variant same frames), new sequences dropkick_low_*, dair_hit_rise.
**Open** (to the coordinator):
1. GAME: dropkick contact frame: body ~60 cm under the floor for 1 frame (Body Capsule mesh offset lags the cancel) -> code queue.
2. GAME: dair V10 falling press: first frame still drops ~39 cm world (V9 49): Body Capsule anchor root -46 -> -88 cm in 2 frames.
3. DECISION: the owner's KIT_Test_Playtest has no defence data at all (no knockdown, no hop for the player): give it DA_DefenceRules_Test? Add the window to TL_Overlord_Dropkick?
4. Capture: no dair air hit with the current jump (stomp passes over a standing dummy; dair_hit lands at anim ~10 -> V10 heavy lag, no hit). Hit grids show that.
5. AM_OverlordDAirV10 is fresh (blend 2/8 f): the owner's local edit on AM_OverlordDAirV9 is not carried over. KIT_Test_PlaytestTB copies the owner's on-disk KIT_Test_Playtest.
6. Bounce clip frames 12-14 never show (GroundHit starts at clip f11); the clip's peak height adds ~12 cm to the game's 22 cm.
Attempts: dropkick variant 2 (TIMEOUT setup2 = 1, rules fix = 2, OK). Lock released at the end.
