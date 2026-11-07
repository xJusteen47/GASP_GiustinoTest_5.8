# M8 doc deltas (G0-G12a)

What the build did differently from `docs/architecture/combat-m8-minions.md`, and the folds into `docs/architecture/combat.md` still to do in G12b.
Source: the G sections of `docs/notes/handovers/m8-minion.md` only. One bullet each.

## Deltas against combat-m8-minions.md

- G0b/spike 1: the idle AI fighter costs ~0.5 ms per fixed step on the laptop (10x over target), so the store-based minion goes ahead; the numbers are in `m8-spike-report.md`.
- G1: the post-fixed-tick hook is a fork patch (`OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)`, inert unbound); `PostToInbox` gained a generation; `FindEntity` returns `FGSTN_FoundEntity`.
- G2: `GSTNCore` (shared pose history, net primitives) is a new plugin; health is 5 bits on the wire (fraction x 31); minimal net item 119 bits (spec ~121); generation 0 is the null handle and wraps 65535 -> 1.
- G3: the batch has stages 0 spawn queue (`gstn.Minion.SpawnsPerFrame` 4), 1 LOD, 2 drain, 3 timeline, 4 movement, 5 publish; pooled `AGSTN_MinionActor`; the subsystem activates only when NetMode is not Client.
- G4: spike 3 picked backend B (lightweight sweep and slide); Mover backend A is not built; the course is built in code, not as `L_MinionCourse`; robustness is not at 0 failures (a trapped-minion case at the stair block side face stays open).
- G5/X4: minions use the world subsystem history ring through the provider snapshot (no separate ring for the fighters' rewind); the cheap candidate query exists but is not wired into GSTNCombatNP (rewind cost still grows with N); MeterGain on minion victims is skipped.
- G6a: the item type is sent on every update (9 bits, type-on-add needs a per-connection baseline); "token holder" is approximated as "member running a move"; `MaxRelevantPerClient` is a cvar; the push-model ini and the bake-off (G6b) are not done.
- G7a: the Animation Budget Allocator is skipped (plugin not enabled); `UGSTN_HitPresentationComponent` cannot present minion victims, so the attacker-side pause and local cue do not play for minion hits (needs a GSTNCombatNP hook, X4 forbids); the presentation reuses a tracker on the local pawn's sync state.
- G7a closing: the player -> minion scenarios needed the bench `gstn.Minion.Bench.HurtboxSet` cvar and a swinger kit; the client puppet overshoots the server path at Far and Dormant (interpolator extrapolation, open).
- G8: the brain is one `FStateTreeExecutionContext` per brained slot run by `UGSTN_MinionBrainSubsystem`, not a ticking `UStateTreeComponent` per actor; tier rates are cvars, not the LOD batch intervals; StateTree is a plugin dependency.
- G9a: no GAS attribute write on the promoted fighter (the health fraction is on the component, the kit applies it in G9b); `Event_MinionPromoted` is only logged; acceptance item 3 (type and wave as assets without C++) is deferred.
- G10: platforms and parts bridges are inert seams only (`SetPlatformActor`, `IGSTN_MinionPublishObserver`, per-slot `MaxLODTier`, `bExemptFromCombatCap`); `FGSTN_MinionNetFormat` gained an `ExtTypes` table filled process-wide (open risk for P16: a pack before the registry).
- G11a: host cap `gstn.Minion.MaxActiveT0` (48) replaces the per-definition `MaxActiveCombat` (left unused); horde and bytes scenarios are laptop numbers (-nullrhi), not the section 11 measurement; the demoted T0 slot is pulled back by the stepper (perf-debt).
- G12a: store default capacity 128 -> 160 (the 150 population fitted only 128); a single wave stays capped at 128; there is no registry asset (the table is first-spawn order, checked by hash), so the registry rule is a function run over a wave's types; debug draw `Hitboxes` draws the move reach sphere, not the timeline shapes.

## Folds into combat.md in G12b

- Section 3 (modules): add GSTNCore, GSTNMinion with Core, Runtime, Editor, Tests and its dependency edges (no GSTNPlatform, no GSTNParts, no GSTNDefence/GSTNLink); fork patch of G1 in the fork list.
- Section 13: minion hit flow (minion -> player through the inbox, player -> minion through the provider and lag comp history), attack tokens, the X4 limits above.
- Section 15 (bytes): the 119-bit net item, 5 bit health, 16-item packs, type on every update, the `ExtTypes` table and the v1 layout when no type has an ext.
- Section 17 (tests and harness): `SUITE_MINION`, `SUITE_MINION_NET`, `SUITE_MINION_BENCH`, the G12 forced-reconcile scenario and the `GSTNMinion.*` test families, provisional laptop numbers flagged as such.
- Section 18 (roadmap): M8 done through G12a; open: G6b, G7b, G9b, G11b on the desktop, the trapped-minion case, Far/Dormant puppet overshoot, the GSTNCombatNP hooks X4 blocks.
