# Implementation plans M4, M4b, M5 (draft for owner review)

Plans only, no code. Each plan mirrors `Saved/Notes/m3-plan.md` (local M3 plan, C0..C15): ground rules, dependencies on M3,
checkpoints (one checkpoint = one commit), spikes with stop-and-ask conditions, assumptions for unanswered owner questions,
Blueprint/shared-asset edits that need the owner's OK, risks and parallelisation.

| Plan | Milestone | Checkpoints | Spec |
|---|---|---|---|
| [m4-hits-plan.md](m4-hits-plan.md) | M4 Hits (`GSTNCombatCore/GAS/NP`) | H0..H13 | `../combat-m4-hits.md` |
| [m4b-parts-plan.md](m4b-parts-plan.md) | M4b destructible parts (`GSTNParts*`) | P0..P16 | `../combat-parts.md` |
| [m5-editor-plan.md](m5-editor-plan.md) | M5 editor and debug (`GSTNCombatEditor`, `GSTNCombatDebug`) | E0..E19 | `../combat-m5-editor.md` |
| [traversal-surfaces-plan.md](traversal-surfaces-plan.md) (added 2026-10-22 evening) | Traversal surfaces: wall-slide / trampoline surfaces on level objects and enemy body parts (`GSTNSurface`, `GSTNPlatform`, `GSTNPlatformParts`) | T0..T9 | `../traversal-surfaces.md` |

`../game-design-direction.md` wins over every spec and plan. `../modularity-and-tags.md` §3.7-3.9, §4, §5b are binding.

### Traversal surfaces (T) on the other plans (added 2026-10-22 evening)

| T needs | Ready at |
|---|---|
| H11 contact record and `Surface.Type.*` rows, H11b launch layered move and `FGSTN_SurfaceLaunchParams` (declared in `GSTNSurfaceCore` after T1, pending owner TS.1) | H11, H11b → T4 |
| H10 lag-comp history (composed transform of based players) | H10 → T7 |
| P7 confirmed `BrokenMask`, P7b `Combat.Event.Part.Restored` | P7b → T6 |
| `GSTNPlatform` skeleton (M8 §4d.5 / many-enemies §4; no plan yet: T3 delivers the fighter minimum if none has started), `GSTNPlatformMinion` + P16 | T3 (own), M8 + P16 → T8 |
| Wall movement core: T5 approved (TS.2, 2026-10-22 22:14); T2 is subclass-only | now → T0, T1, T2 |
| M5 E10 validation mechanism | E10 → T9 |

## Cross-plan dependency map

The M4b and M5 plans name M4 capabilities (D-ids, "M4-*" names) instead of H-numbers. This table maps them.

### M4b (parts) on M4

| Parts need | Capability | Ready at |
|---|---|---|
| D1 | victim view + pipeline step classes, hit resolution context | H4 |
| D2 | reaction modifiers + OutcomeExtensions in the context | H2 (types), H4 (context) |
| D3 | `HurtboxIndex` + hurtbox `Priority`, attacker picks | H1 (types, `PickHurtbox`), H6 (inbox entry), H8 (attacker) |
| D4 | outcome applier registry, state at F+1 | H4 (interface), H9 (invoked) |
| D5 | attacker outcome seam, `RicochetResponse`, recoil | H4 (seam), H8 (evaluated), H12 (GA_BaseCombat response) |
| D6 | `PercentCapped` damage, WeightDriven/CappedPercent knockback | H2, H4 |
| D7 | hit data `ExtensionData` + override entry | H1 (data), H4 (override) |
| D8 | tick extension "before candidates", `HitGroupMask` | H4 (M3 C3b hook points) |
| D9 | snapshot extension POD, lag history, hurtbox override hook | H6, H10 |
| D10 | sync fragment for victim state | M3 C3a + H6 |
| D11 | attacker hitstop predicted from static data | H2 (Auto/Override), H8 |
| D12 | outbox events with hit envelope, KO | H9 |
| D13 | harness hit scenarios, test character hurtbox set | H13 (early scenarios from H8/H9) |
| D14 | lag compensation consistent with `HurtboxIndex` | H10 |

So: parts pure work P1-P3 can start any time after M3 C3/C5; P4 after H4; P6 after H6; P7 and later after H9 (seams
complete) and H10; P8 after H12; P15/P16 wait for M8.

### M5 (editor) on M4 / M4b

| M5 need | Ready at |
|---|---|
| M4-shapes, M4-hitbox, M4-hitdata, M4-hurtbox (incl. `Priority`) | H1 |
| M4-baked (`BakedSockets`, declared by M3 C2, filled by `bake_sockets.py` in H13 until E8 replaces it) | M3 C2 + H1 |
| M4-knockback (`UGSTN_KnockbackModel`, `FGSTN_ReactionTuning`) | H2 |
| M4-snapshot (shape queries for debug draw) | H6 |
| M4-hitstop-state, M4-reaction (readable sim state) | H7, H9 |
| M4-events (hit events) | H9 |
| M4b P-hit-extension (`FGSTN_PartHitData` in `ExtensionData`) for E12/E19 hooks test | P2 |

M5 never references `GSTNParts`; `GSTNPartsEditor` (P13) is a client of the M5 hooks (spec §12b). P13's validators need
only the M5 validation mechanism (E10); its hits-to-break report can land before or after E12.

## Suggested order and parallel threads after M3

1. Thread "M4 core": H0 spikes, then H1 → H2 → H3 → H4 (pure Core, no world; good for a cheaper agent per checkpoint).
2. Thread "M5 editor shell" in parallel from the end of M3: E0 (S1, S2, S4), E1, E2, E4, E5, E10, E14.
3. Thread "Parts core" in parallel: P0b/P0d spikes, P1-P3 (pure), then P4 once H4 lands.
4. Thread "M4 sim" (needs the editor and the listen-server harness on the PC): H5 → H6 → H7 → H8 → H9 → H10 → H11 → H12 → H13.
5. After H1: E3 final, E5b, E6-E9, E11, E12; after H6/H9: E15-E17. After H9/H10: P5-P12, P14.

Only one thread at a time should touch `GSTNCombatNP` and the harness on the PC (one PIE scenario at a time).

## Approval status (2026-10-22)

The owner allowed self-approval of these plans where they hold no architectural question and no new feature. Everything
in the three plans is approved on that basis (assumptions in §5 are spec proposals or tunable data defaults) **except the
items below, which were owner decisions**. All three are now answered:

1. ~~Trampoline actor and `GSTNLevelObjects` plugin~~ **Approved 2026-10-22 18:27**: an adjustable box (height, depth,
   width) with per-instance mesh, size, launch direction and tilt. Planned as M4 H11b; the plugin depends only on `GSTNCombatCore`.
2. ~~`IsMovementLocked` extension point in the wall-movement core~~ **Approved 2026-10-22 18:25**: H7 adds the protected
   virtual to `UGSTN_WallMovementModeBase` (default: Mover has `Combat.Mover.Frozen`), so hitstop freezes on walls too.
3. ~~Parts never heal~~ **Answered 2026-10-22 18:22**: no self-regeneration, but explicit restores (e.g. a healer enemy)
   exist. Planned as M4b checkpoint P7b; no longer blocking.

Owner-OK lists in §6 of each plan (shared Blueprints, `.uproject`, ini, wire changes) still apply as written: they are
announced before the step happens, as with M2/M3.
