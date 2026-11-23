# M11 slice 3 (K3): three test kit archetypes (desktop, 2026-11-11)
Branch `claude/desktop-m11-3` from `de12900b`. Doc: `Plugins/GSTNLink/README.md` "Kit throws" (content writers), this file.
**Done**: `KIT_Test_Light` / `KIT_Test_Medium` / `KIT_Test_Heavy` (copies of KIT_Test_Link: link rows, dodges) with their own weapon `WPN_Test_<X>` (jab on
Attack), special, four Smash throws in the link extension, grip (mash base 90/120/180 frames) and stats through the new `UGSTN_GE_TestKitArchetypeInit`
(WalkSpeed 760/600/430, Jump 820/720/620, Weight 80/100/140, MaxCarryWeight 60/90/200, Acceleration, AirControl, FallGravityScale, AirJumpCount 2/1/1).
Moves are Blueprint children in `/Game/GASNPPTest/Combat/Archetypes` (`GA_<X>_Jab/Special/Throw{Forward,Back,Up,Down}`), frames + GASP placeholder montages
(shoves, takedowns) written by `Tools/GAS/setup_test_kit_archetypes.py` (headless, idempotent: a rerun rewrites only what changed) through new writers
`UGSTN_LinkContentLibrary::{SetKitThrows, WriteThrowMove, SetKitGrabMashFrames}` and `UGSTN_TestKitContentLibrary::WriteStrikeMove` (parent
`UGSTN_GA_TestKitStrike`). Throw arcs: light flat/low (20/160/90/70 deg, 700-1500), medium classic (45/135/85/75, 1000-1900), heavy steep and far
(55/120/89/85, 1300-2600). Hub: `KIT_PADS` gets a row at x=800 (y -350/0/350), spawned by `test_hub.spawn_kits`; `DefaultGSTNHeld.ini` gives the three kits
the held bindings. The old `UGSTN_GE_TestKitInit` now resets the six extra stats to `DefaultGSTNCombatGAS.ini` values (swap heavy -> old kit is clean).
**Choices (rule 6)**: no new input or binding (moves sit on the existing Attack / Special / link rows, so nothing hardcoded); capacities make light and
medium unable to lift a standard fighter (stick throws visible) and heavy lift anyone (M10 lifted flow) = answer to the open "lower the default capacity?";
grip per kit added because the heavy needed it (copied 90 frames let B escape before a lifted throw: seen in harness, fixed with the 180 frame grip);
no map save, so the new row has no pad cylinder or sign in `L_Test_Hub` (the pickup shows its kit name); rerun `build_test_hub_map.py` to draw them.
**Gate**: build b1, b2 ok (11-17 s). Headless GSTN **1507/0** (+`GSTN.Test.KitArchetypes.{Content, StrikeTimeline}`), 0 ensures. Harness
`kit_scenarios.SUITE_ARCHETYPES` (stats on authority+owner, light stick forward, medium stick back, heavy lifts then crouch throw) **4/4 PASS** listen 0 %,
listen 2 %, dedicated 0 %. SUITE_DEFAULT one PIE **50/50**, 0 ensures (`m3_suite_driver.py` now has `R31_ALL=1` for the whole suite).
**Not verified / unseen**: nobody has seen the kits in PIE: arcs, montages (root motion of the walk takedowns), feel of the stats, the jab/special
hitboxes against a dummy. Harness pawns have no knockback move, so the arcs are only data. Every `-run=pythonscript` and batch editor exit ends with an
access violation at shutdown (exit 3, after saving; seen in K2 logs too, not from this slice).
**Open decisions**: the numbers above are placeholders for the playtest; whether kit grip length should scale with weight (shipped: yes).
Cost: ~110k context, Opus, estimated ~$8-10. Build 11-17 s, setup script 16-26 s, headless ~1 min, archetype batch ~3 min each, suite ~15 min.
