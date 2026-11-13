# M11 slice L4: reach scaling on ReachAxis, sweet/sour follow the blade (laptop, 2026-11-12)
Branch `claude/laptop-m11-l4` from `319aad5a` (L3). Code `74eec907`. Plan `docs/architecture/plans/m11-kit-piano-a-fette.md` §2 L4,
card gate G4a, spec `combat-loadout-mods.md` §8 (now carries a *Built (M11 L4)* note).
**Done**:
1. `UGSTN_HitReachUtils` (GSTNCombatCore `Hits/`): `ScaleHitboxReach(Spec, ReachScale)` multiplies the shape centre's coordinate
   along `ReachAxis` (in the hitbox's space) and, with `bScaleExtentWithReach`, the extent along it (factor per shape axis by the
   axis' components: exact for an axis-aligned blade). Without the flag the shape only moves: a tip sweetspot follows the new tip
   at its size. Keys: written into each key (scale, then offset so the centre lands right); no keys: into `Shape.Local`.
   `ScaleTimelineReach` / `ApplyReachScale(Data, Scale)` = every hitbox track of a timeline (the call L5's derived timeline makes).
   Baked, never in the simulation; double math on authored data only, deterministic. Projectile and `None` hitboxes untouched;
   safety clamp [0.25, 4] (kit `ModLimits` clamp tighter in L5).
2. Timeline validator warnings: a hit group mixing reaching and non-reaching hitboxes (sweetspot left behind), a reach axis that
   cannot move its hitbox (centre on the space origin, extent kept), a reach axis on a projectile.
3. Tests `GSTNCombat.Core.Reach.{StretchesBlade,SweetspotFollowsTip,KeyedHitbox,TimelineAndValidation}`. README row, spec note.
**Decisions taken (recommended option)**: baked socket tracks are NOT transformed (spec said "the same way"): they are character
bones, the blade length lives in hitbox offsets from the grip socket; a hitbox authored on a tip socket with zero offset gets the
validator warning instead. No game-feel choice involved (owner already decided sweet/sour follow the blade, 2026-10-23).
**Gate (laptop)**: build ok; first headless run 3 red on a too tight float tolerance (1.3f = 1.29999995), test fixed, second run
GSTN **1527/0**, 0 ensures (+4; `Saved/M11L4/headless-t2.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l4-all.txt`).
**Not covered**: no caller yet (mods arrive in L5), so nothing visible in PIE; no content timeline has `ReachAxis` set yet (the
sword timelines need it authored: L5/L10 data, or the setup script); M5 bounds check vs the blade mesh is L8; Game/Shipping not built.
**Owner questions**: none new. Still open: M11.2/M11.3/M11.4 (kit swap keeps), M11.6 default, special swap input (from L3).
**Next in order**: L6, L9 (then L5 after them; L5 must not run in parallel with another GSTNCombatCore slice).
No manual step for the owner; artifact not changed.
Cost: ~70k context, Opus, estimated ~$4-6. Wall clock ~35 min (2 builds, 2 headless runs, suite ~12 min).
