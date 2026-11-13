# M11 slice 2 (K2): Smash-style kit throws (desktop, 2026-11-11)
Branch `claude/desktop-m11-2` from `3cbf4b68`. Doc: `Plugins/GSTNLink/README.md` "Kit throws", `Plugins/GSTNHeld/README.md` step 18, `combat.md` §12b.
**Done**: loadout extension `GSTNLinkThrows` (GSTNLinkNP) runs `UGSTN_LinkSlots::ApplyKitThrows` on every kit ASSET that binds `Command.Link.Grab`, every machine.
4 throws per kit = `FGSTN_LinkKitExtension::Throws` (replaces the kit's own rows), else the kit's rows, else `UGSTN_LinkSettings::FallbackThrows` (new native
`UGSTN_GA_LinkThrow{Forward,Back,Up,Down}`, placeholder arcs 40/140/88/80 deg, no montage). `ThrowInputMode` (decision 2, both shipped): **Stick** (default:
stick entering forward/back sector = throw, Jump = up, Crouch = down, Attack = pummel), **StickPlusSpecial**, **Buttons** (F12 rows). Special + Crouch held also
grabs (`Command.Link.Grab.Special`, bound to the kit's grab ability). Liftable enemy (decision 4): `ThrownEnemies.KitThrowsWhileLifting` = NotStickOnly (default,
the stick aims the M10 throw, Jump/Crouch throws stay) / None / All, handed to GSTNLink as throw blockers at startup (GSTNLink names no Held tag).
**Choices (rule 6)**: up/down in Stick mode = Jump/Crouch (a 3D stick has no up/down); forward/back fire on the stick ENTERING the sector (a stick held into
the grab does not throw); the special grab is its own child command (a 2nd `Command.Link.Grab` row with the same requirement made the loadout validator refuse
the kit's link rows: seen in PIE, fixed); lifted enemy keeps the non-stick throws (plan said "M10 flow only" = the `None` option); existing `Combat.Held.NoLift`
used, no new `Combat.Grab.NotLiftable` tag; kits built in memory by code keep their table (as K1).
**Input audit (owner question)**: no physical key is in C++. Keys/pad -> logical `Combat.Button.*` = Input Actions + mapping contexts (`Content/Input`, IC input
config, `AbilitySystemMappingContexts` in DefaultGame.ini). Which logical button makes which command: F12 link rows = slot set asset (built by `UGSTN_LinkSlots::Build`
via setup script, editable); K2 throws = C++ shapes per mode, each replaceable by data (`ThrowSlotOverrides`, Project Settings); M10 held (B pickup/drop, RT
throw/ThrowEnemy) and K1 kit swap (Release of B after `HoldSeconds`) = shapes FIXED in `UGSTN_HeldSlots` C++, only timings/kits/aim in DefaultGSTNHeld.ini.
To move them to data: a `SlotOverrides` array in `UGSTN_HeldSettings` read by `AppendKitBindings`/`AppendKitSwapBinding` (K2 pattern), one small slice ~$3-5.
**Gate**: builds b1 (test include) b2 b3 b4 b5 ok, each 5-60 s. Headless GSTN **1505/0** after fixes (+4 `GSTNLink.Core.KitThrows.*`, +1 `GSTNHeld.Core.KitThrowBlockers`;
tag count 40->41), 0 ensures. Harness (new `Tools/GASNPPTest/batch_driver.py`): `kit_throw_stick_forward/_back/_lifted` PASS listen 0 %, listen 2 %, dedicated 0 %;
`grab_throw, grab_mash, grab_interact, grab_gate, throw_at_enemy` PASS listen 0 %. SUITE_DEFAULT one PIE **50/50** (854 s).
**Not verified**: the arcs (harness pawns have no knockback move and the harness kit binds one test throw to all four, so B lands where it lands; direction ->
command is unit-tested); special grab in PIE; the fallback set in play (no kit uses it yet: KIT_Test_Link binds its own four identical F12 test throws).
**Nobody has seen it in PIE with a pad.**
**Next essential slice K3** (3 kits light/medium/heavy on the hub pads): give each kit a `FGSTN_LinkKitExtension` with its own `Throws` (GASP placeholder montages,
Blueprint children of `UGSTN_GA_LinkThrow*`) and a grab row, set `MaxCarryWeight`/`Weight` so some enemies are not liftable (test values make every standard fighter
liftable, which hides the stick forward/back throws by default). Start from this branch.
Cost: ~150k context, Opus, estimated ~$11-14. Build 5-60 s, headless ~1 min, kit batch ~2.5 min, suite ~13 min.
