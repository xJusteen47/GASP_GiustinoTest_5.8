# M5 leftovers: E15 Gameplay Debugger category, E16 BakeCheck (laptop, 2026-11-06)

Branch claude/project-thread-wlggpb (after the T2 close-out). Only `Plugins/GSTNCombatDebug` changed: no GSTNCombatNP/Core edit, so no
conflict with M8 G1/G5. Plugin 0.4; README has the user-facing description.

E15 (spec 10.2, 10.2b): `FGSTN_GameplayDebuggerCategory_Combat` ("GSTNCombat", slot 6, on in game and simulate), registered by the module
under `WITH_GAMEPLAY_DEBUGGER_MENU` (`SetupGameplayDebuggerSupport`). Collects on the authority from the existing const accessors
(`GetSyncState`, `GetDebugString`, `GetFacingYaw`, `GetGateClosedFrames`): state, timeline frame, hitstop/hitstun left, weapon, revisions,
input notation, last 8 hits, facing arrow and head label. Hit ring = `UGSTN_CombatDebugHitRecorder` (world subsystem) on the HitLanded/
HitReceived outbox events, never the sync state (`gstn.Combat.Debug.HitRing`, default 1). New edge: GSTNCombatGAS (event tags).
Not shown yet (spec 10.2): buffered command and resolve reason, open windows (cancel/override/hurt state), lock tags, adoption pending.
E16 (10.3): hitbox/hurtbox draw already existed in GSTNCombatNP (`gstn.Combat.Debug.Hitboxes/Hurtboxes`), left there. Added only
`gstn.Combat.Debug.BakeCheck` (0 off, 1 draw, 2 draw + log), `UGSTN_CombatBakeCheck` tickable world subsystem: baked socket placed as the
attacker step does (baked * actor transform) vs the live mesh socket, on machines that simulate the pawn.

Checks: build green first try both times. `RunTests GSTNCombat.Debug+GSTN` 1295/1295 (E15), `GSTNCombat.Debug` 12/12 after E16
(6 new tests). PIE listen host + 2 clients 60/15/0 headless with E15 loaded: hitstop, hit_basic, hit_trade, hit_host, hit_lagcomp all PASS.
hit_basic with BakeCheck 2: PASS, 126 BakeCheck lines (the test move's `hand_r` bake is synthetic, so its ~250 cm gap is expected).
Not verified: the category on screen (needs a rendering editor: open PIE, apostrophe, select a pawn, key 6) and BakeCheck on a real
baked move (the 10.2/E7 open item: blend-in and the first-frame offset). Owner or a desktop thread with a rendered PIE.

Next in plan order for a laptop thread: E17 trace channel + sink (S, $3-6) touches GSTNCombatNP (sink call in the sim loop): only after
M8 G1/G5 are merged; otherwise T6 parts bridge (M, Opus, $6-10; GSTNPlatform, fine while M8 is before G10). E19 after E17.
Cost of this thread (T2 + E15 + E16): ~$8-11 estimated (Opus, ~110k context, 4 builds/test runs, 3 PIE runs).
