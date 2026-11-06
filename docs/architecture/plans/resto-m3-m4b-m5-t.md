# Remaining work on M3, M4b, M5 and traversal (T): gate plan (2026-11-06)

Plans only, no code. These four milestones already have full checkpoint plans (`../../notes/m3-plan.md`,
[m4b-parts-plan.md](m4b-parts-plan.md), [m5-editor-plan.md](m5-editor-plan.md), [traversal-surfaces-plan.md](traversal-surfaces-plan.md)).
This file does **not** repeat them: it lists what is still open on `feature/gas-npp-5.8` (audit of git log and doc deltas at
`60da6fa`), regroups it into gates with a cost, a machine and a model, and says what goes in Blueprint and what in C++.
Sequence and totals: [sequenza-2026-11-06.md](sequenza-2026-11-06.md).

## 0. Ground rules (same as the M8/M10 plans)

- One gate = one thread started from a brief, one or a few commits, handover ≤ 30 lines with cost. Max 2 fix attempts, then stop.
- Every gate ends with build green, headless `Automation RunTests GSTN` green and the suites the gate names, listen host + 2 clients.
- **Designer rule**: everything tunable from the editor (data assets, `UPROPERTY(EditAnywhere)`, CVars); examples and moves in
  Blueprint; nothing the designer has to run by hand (setup happens at Play or at editor startup). C++ holds frameworks, sim code and
  anything rolled back. **No aesthetics before mechanics**: presentation stays placeholder.
- Costs use the scale of the M6/M7 threads: S $3-6, M $6-10, L $10-18 (Opus), roughly half with Sonnet, a third with Haiku.

## 1. Audit: what is done

| Milestone | Done on feature | Open |
|---|---|---|
| M3 | C0-C19 (HANDOVER 14) | suite not "two clean passes" (30 scenarios), harness arena/lane, `spike_triggers` T3, O-CameraYaw, S-MaxSpeed, canary build |
| M4b | P0-P9, P7b, P13, P14 | P10-P12 presentation, P15/P16 minions (need M8), P17 stations, P18 openings, P19 screws |
| M5 | E1-E14, E5b | E15 Gameplay Debugger, E16 debug draw + BakeCheck, E17 trace sink, E18 Rewind Debugger (stretch), E19 closing |
| T | T1, T3, T5; T2 paused | T2 finish, T4 launch from moving surfaces, T6 parts bridge, T7 lifecycle + lag comp, T8 platform minions, T9 content |

The M11 seams M3 left open (typed loadout extensions, compile extension implementations, attachment providers) are planned in
[m11-kit.md](m11-kit.md) G1, not here.

## 2. Gates

### R3 — M3 leftovers (laptop, Sonnet)
- **R3-1 Suite stability** (M, $5-8). Re-check which HANDOVER 14 items M4/M6/M7 already fixed, then: harness lane/arena fix (pawns off the
  platform edge), jump apex read from non-resimulated ticks only, `spike_triggers` T3, O-CameraYaw (command yaw 0 vs 64 in reconcile
  dumps: find the writer or record it as harmless). Gate: 30-scenario M3 suite **two consecutive clean passes** listen, one dedicated.
  C++ only in the harness component and, if O-CameraYaw is real, in the input source. Nothing in Blueprint.
- **R3-2 Canary build** (S, Haiku, $1-3). `Tools/Canary/make_canary.py` fixed and run once; output to file.

### M4b — parts (mechanics now, looks later)
- **P17 Parts station in `L_Test_Hub`** (S, Sonnet, desktop, $3-5): a zone with the P14 sample enemy, spawned at Play by the startup
  script (no saved map edits by Claude). BP: the sample enemy stays a Blueprint child of the C++ parts component.
- **P18 Hurtbox openings** (S, Opus, laptop, $4-7) and **P19 Screws** (S, Opus, laptop, $4-7): as written in the M4b plan. Both touch
  `GSTNCombatCore`: never at the same time as M8 G1/G5 or M10 G1-G3. Example screwed enemy in Blueprint (part set data asset only).
- **P10-min Hidden bones and placeholder tint** (S, Sonnet, $4-7): only the part of P10 that is gameplay feedback (a broken limb hides,
  a damaged part tints). Damage materials, stump caps (P11) and death shatter (P12) wait for the aesthetics phase (decision D6).
- **P15 Minion interface** (S, Sonnet, $3-5) after M8 G2; **P16 full minion path** (L, Opus, $10-18) after M8 G12.

### M5 — editor and debug (laptop, Sonnet; editor needed only for E16 screenshots)
- **E15 Gameplay Debugger category** (M, $5-8): const accessors, ring of the last hits per pawn.
- **E16 Debug draw + BakeCheck** (S, $3-6): first record what the test hub's `gstn.Combat.Debug.Hitboxes/Hurtboxes` already draws; add
  only the missing BakeCheck overlay.
- **E17 Trace channel and sink** (S, $3-6). **E19 Extension procedure, parts hooks, designer scenario, docs** (M, $5-8).
- **E18 Rewind Debugger track**: not planned (stretch; decision D6). Total M5 rest $16-28.

### T — traversal surfaces
- **T2 finish** (S, Sonnet, $3-6): close the paused re-entry work, enemy-surface scenario green.
- **T4 Launch from moving surfaces** (M, Opus, $6-10): base velocity on exit (the T3 gap `bAddBaseVelocityOnExit` adds nothing).
- **T6 Parts bridge `GSTNPlatformParts`** (M, Opus, $6-10): part state → shape mask; P7b is done so it can start now.
- **T7 Lifecycle under a player + lag compensation** (M, Opus, $6-10). **T9 Content, validators, harness, docs** (M, Sonnet, $5-8):
  example surfaces as Blueprint children (bouncy wall, slide-only wall) placed by the test hub startup script.
- **T8 Platform minion shapes** (S/M, Sonnet, $4-8): after M8 G12 and P16.

## 3. Totals and order

| Block | Gates | Cost |
|---|---|---|
| Now, second code slot while M8 runs | R3-1, R3-2, P17, P18, P19, P10-min, E15, E16, E17, E19, T2, T4, T6, T7, T9 | $63-109 |
| After M8 | P15, P16, T8 | $17-31 |
| Aesthetics phase (not counted) | P11, P12 | $12-20 |

Order inside the "now" block (cheapest risk first, no two gates on the same plugin at once): R3-1 → T2 → E15/E16 → P17 → T4 → T6 → T7 →
P18 → P19 → E17 → E19 → T9 → P10-min → R3-2. Conflicts: P18/P19 and T4 touch `GSTNCombatCore`/`GSTNCombatNP` (wait for M8 G1 and G5 to
merge); T6/T7 touch `GSTNPlatform` (M8 G10 also does: not in parallel).
