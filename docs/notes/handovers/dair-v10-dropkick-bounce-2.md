# dair V10 + dropkick bounce, ROUND 2 (pose round 1/3), thread dair-dropkick-anim (desktop, _ue59, 2026-11-17), branch claude/project-thread-98dml5
Review r1 (Fable) and my answers: docs/notes/reviews/overlord-dair-v10/review-r1.md, dropkick-tech-bounce-v1/review-r1.md. New packages in */r2/ (8 grids + frames.md each).
**Game bug fixed (C++, generic)**: the standing capsule came back with the Body anchor (mesh root kept): after any landing on a Body Capsule move
the pawn stood sunk by the root gap for good (dair V9 27 cm, V10 52 cm: the "seiza" of review r1 was the squat drawn 52 cm into the floor) and the
dropkick contact frame sat 33 cm under the floor. FGSTN_BodyCapsule::GroundedRestore: on the floor the standing capsule grows from the floor.
Evidence: end actor z 88.1 in every V10R2 sequence (round 1 35.8) `Saved/DairV10/variant_r2/*/frames.jsonl`; dropkick contact pelvis 7.5 cm (was -25.7)
`Saved/DropkickBounce/variant_r2/dropkick_whiff/frames.jsonl`. Test GSTNCombat.Core.BodyCapsule.GroundedRestore.
**Dair R2** (kit KIT_Test_OverlordDAirV10R2, TL_Overlord_DAirV10R2 = dup of V10; charge = round 1 AS/AM V10, PASS): heavy clip AS/AM_OverlordDAirLandHeavyR2
(card overlord_dair_land_heavy_r2): arms crossed at the sternum, elbows out, stance 48 cm, lowest f3, +4 cm settle f6, rise f10-15 head first.
"Pelvis up 30-35 cm" NOT applied (rule 13e: it was the floor bug). Light: AM_OverlordDAirLandLightR2 with 1 f blends (round 1's 4-frame clip was shorter
than its blends and never showed); now plays on the contact frame (dair_land_early v86).
**Dropkick R2** (KIT_Test_PlaytestTBR2 chain, card overlord_dropkick_techbounce_r2): squash f1-f2 (pelvis -4/-5, heels down to ~10 deg, knees give),
head back f3, pelvis leads f3-f5 with legs lagging, ~20 deg at f6, arms down from f9, GroundHit f0 at f11 (hand-off at clip f10.3-11, no pop).
**Owner option A (applied by script Tools/GAS/setup_playtest_dropkick_knockdown.py)**: KIT_Test_Playtest defence rules DA_DefenceRules_Test (shield/
get-ups kept), TL_Overlord_Dropkick + KnockdownOnLanding window [0,91). Verified: Saved/DropkickBounce/playtest_kd (downed, hop to actor 116.3, generic
GroundBounce/GroundHit). TODO after a Fable PASS of the bounce clip: set BounceAbility on TL_Overlord_Dropkick and GA_Overlord_DropkickTechBounceR2 in
KIT_Test_Playtest's TriggeredAbilities (setup_dropkick_tech_bounce.py does it on the TB chain). NOTE: the committed KIT_Test_Playtest also carries the
owner's uncommitted local edits that were on disk (the script loads and saves the on-disk asset).
Build `Run-Timed.ps1 -Preset Build` OK (Saved/Logs/ci/build_dair_bounce_r2_1.log). Tests `-Preset Test -Tests GSTN`: 1744 Success / 0 Fail
(`grep -c "Result={Success}"` / `"Result={Fail"` on Saved/Logs/ci/test_dair_bounce_r2.log.ue.log).
**Open**: 1. code queue: the landing clip starts 2 frames after touchdown (dair_land v86 -> v88, dair_hit v84 -> v86): ground fact latency (attempts 0/2).
2. pose: a landing during the tucked charge (f4-12) jumps from the ball to the clip's f0 (legs under him, as after the stomp): the heavy f0 fits the
stomp hold, not the charge. Option: a third window f4-12 with a tucked-start variant of the heavy clip (decision for the reviewer/owner).
3. capture: with this jump the dair never lands in f32+ (the move hangs): the late light window is unverified; no air hit on a standing dummy either.
4. AM_OverlordDAirV10 does not carry the owner's local edit of AM_OverlordDAirV9.
Attempts this round: capsule fix 1 (OK); light montage 1 (OK). Lock released at the end.
