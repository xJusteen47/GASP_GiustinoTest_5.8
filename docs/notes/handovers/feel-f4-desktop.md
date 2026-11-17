# F4 desktop: dropkick recheck, F4 build, Overlord kit (desktop, _ue59, Opus, 2026-11-17), branch claude/project-thread-98dml5
**1. Dropkick (owner could not see the r3 claims)**: capture chain OK (AM_OverlordDropkickTechBounceR3 plays, pad idle heldButtons 0), but the mp4 was an
8-view mosaic of 0.67 s, the bounce is 12 f, the contact frame had the pelvis under the floor. Recapture KIT_Test_Playtest (Saved/DropkickBounce/f4_check)
+ Fable 5.2 high: **FAIL** vs the r3 claims (docs/notes/reviews/dropkick-tech-bounce-v1/f4_check/review-f4.md, videos side|back|game, 4x slow).
After the F4 build (ground contact fix on) the contact frame is above the floor: pelvis 16.7 / 20.5 cm (Saved/DropkickBounce/f4_after/*/frames.jsonl).
Pose problems stay (rigid pike float to 51 cm, no squash, GroundHit starts mid-air): next = a pose round, owner decides (max 3 rounds already used).
**2. Merges** into 98dml5: kfq87p 207ddd28, 8xfrpx 8b4b0e2a, z9mi0s 615bd9a5, 27htqa f57c552f, 1d70rm 0c177109 (conflict in GSTN_CombatSimComponent.cpp:
kept PresentationEvents + BindAirTurn; air-turn rate inside the HoldOrientationIntent branch, return kept). Build 2/2 OK (attempt 1: C4459 'Landed' in
GSTN_CombatTests_HoldUntilLanded.cpp, renamed). Scripts in order: setup_knockback_smash.py, setup_covered_landing_node.py, ground_contact_setup.py
(logs Saved/Logs/ci/f4_setup{1,2,3}.log*, ESITO OK). F4 brief data: MaxFallSpeed 1500, FastFallSpeed 2400 (ini + PRESET_STATS), PlaytestExtraDistance 550.
NOT done from the F4 brief: setup_run_modes.py (F3) does not exist on any branch; the measurements §3a-e and the L3 sprint setting note (budget).
**Dair tucked window**: f4-12 tried (DAIR_V10_TUCK_END=12): dair_land touched down on anim 10 and got HeavyR2, pelvis 45 -> 73 pop (Saved/DairV10/f4_r4b
v89-90). Back to f4-14 (default): the same case plays HeavyTuckR3, pelvis 41 -> 16 -> 23 (Saved/DairV10/f4_r4c v87-92). The montage still trails the
timeline: code queue. LightTuckR4 contact dip still there: pelvis 72 -> 22 -> 41 (f4_r4b dair_land_early v84-86).
**Tests** `Run-Timed.ps1 -Preset Test -Tests GSTN`: 1767 Success / 0 Fail (count: Select-String "Result={Success}" Saved/Logs/ci/test_f4.log*).
**3. Overlord kit (KIT_Test_Overlord, hue 9 = the orange one)**: run start and skid play the same as KIT_Test_Playtest in this build (they live in the
shared Mover ABP; grid docs/notes/reviews/f4-overlord-kit/skid_overlord_vs_playtest.jpg). The slide: the shared TL_Overlord_Dropkick asks for a
knockdown on landing, the kit had no defence extension, so the land part ran locomotion under planted feet (0 -> 451 cm/s, Saved/F4/loco_ov_walk70).
Fix, Tools/GAS/setup_overlord_kit_shared_f4.py: defence extension, the dropkick bounce ability, 9 spin/drill bindings, jump preset (700 -> 1240).
After: tech bounce + knockdown, no run under the land part (Saved/F4/loco_ov_walk70_fix). Other kits without defence (DAir V7-V9, jab kits): untouched.
**Captures**: pie_capture_move.py now parks other player pawns by key when the clients' worlds have no pawn yet (was "parked []"), new dropkick_land_walk.
Owner files untouched: AM_OverlordDAirV9 (uncommitted), DA_ReactionAnims and pie_wall_bounce.py equal to the merged branch versions (byte check).
Decisions applied (rule 6): F4 brief data applied though not in the coordinator list; tucked window kept f4-14; Overlord gets the dropkick bounce too.
Cost estimate ~25 $ (Opus, 2 builds, 9 captures, 1 Fable review).
