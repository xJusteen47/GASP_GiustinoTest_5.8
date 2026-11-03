# M9 implementation plan: stock GAS adapter (plugins `GSTNCombatStockGAS`, `GSTNInputStockGAS`), optional

Plan only, no code. Checkpoints are named **G0..G6** (local to this file). The spec is
[`../combat-m9-stock-gas.md`](../combat-m9-stock-gas.md) ("M9 §x"); owner questions are section M9 of
[`../open-questions-m4-m10.md`](../open-questions-m4-m10.md) (none answered yet).

Status: **draft for the owner** (2026-11-03). M9 is optional. The spec's recommendation stays: **no-go until the owner names a
trigger** (M9 §2: a second project or release that cannot ship the fork, a fork re-sync costing more than ~5 days, or a wish for
server-reconciled feel without rollback). Only **G0** is worth doing now: it is cheap and protects the option. G1-G6 are written so
that a "go" can start without a new planning thread.

## 1. What already exists (audit of `feature/gas-npp-5.8` at 60da6fa)

| Id | M9 item | M9 § | State |
|---|---|---|---|
| K1 | Plugin split: `GSTNCombatNP` and `GSTNInputNP` as their own plugins; `GSTNCombat.uplugin` without NP, Mover, AbilitySystemSimulation | 5b | **Done** (plugins on disk; `GSTNCombat.uplugin` lists `GSTNCombatCore`, `GSTNInput`, `GSTNSurface`) |
| K2 | Boundary check `Tools/Build/check_module_boundaries.py` + `module_rules.json` | 4 layer 1 | **Missing** (`Tools/Build` does not exist) |
| K3 | Reflection test `GSTNCombat.Boundary.NoAdapterTypes` | 4 layer 3 | **Missing** |
| K4 | S0 audit table (NP calls left in the M2 tick and M4 hit code) | 3 S0 | **Not done** |
| K5 | Pure combat stepper `FGSTN_CombatStepper` + `IGSTN_CombatEnvironment` | 5 | **Not done**; parts already pure (`FGSTN_TimelinePlayer`, hit resolution, victim view in `GSTNCombatCore`) |

## 2. Checkpoints

Cost estimates as in the M8 plan: S ≈ $3-6, M ≈ $6-10, L ≈ $10-18.

### G0 Boundary guard and S0 audit (recommended now, independent of go/no-go)

- `Tools/Build/check_module_boundaries.py` and `module_rules.json` (M9 §4 layer 1: forbidden deps, includes, identifiers, plugin deps,
  unknown modules), prints at most 5 lines, exit code 1 on a violation; run in every milestone gate from then on (one line added to
  the gate lists of the open plans).
- Reflection test `GSTNCombat.Boundary.NoAdapterTypes` (layer 3). Layer 2 (Build.cs self-check) only if UBT accepts the helper file.
- S0 audit: every NP/Mover/`UNp*` include and call reachable from the combat tick and the hit code, with an extraction estimate,
  written to combat.md §17.
- Gate: the script green on feature (or a list of real violations, which become follow-ups); build, headless counts unchanged.
- Model: Sonnet. Size **S** (~$3-6). Machine: either; the script runs without the engine.

### G1-G6 (only after an owner "go")

| Gate | Scope | Gate criteria | Size |
|---|---|---|---|
| **G1** Extraction | `FGSTN_CombatState`, `FGSTN_CombatStepper::Step`, `IGSTN_CombatEnvironment`, `IGSTN_AdapterCapabilities` in `GSTNCombatCore`; the NP adapter implements the environment with **no behaviour change** | Build, headless counts equal, SUITE_DEFAULT/PARTS/DEFENCE/LINK and the M7/M8 suites byte-identical at 0 % and 2 % (extraction proof, M9 §5) | L (~$12-18) |
| **G2** Adapter skeleton + spikes 1 and 4 | `GSTNCombatStockGAS`, `GSTNInputStockGAS` (component simulated per frame on owner and server, prediction keys for activations, input history with the activation payload, time base); both adapters enabled fails at startup with a clear log | Canary project (no `Plugins/GASNPP`) compiles and loads; spikes 1 (payload race at 5 % loss) and 4 (drift over 5 min) recorded | L (~$10-18) |
| **G3** Movement bridge + spike 2 | `IGSTN_MovementBridge` for `UCharacterMovementComponent`: hitstop freeze, knockback launch, root motion | Spike 2 recorded (corrections per hit on the victim, predicted `Frozen` mode); canary tests green | M (~$6-10) |
| **G4** Hits and lag compensation + spike 3 | Server-authoritative hits, own 80-frame hurtbox history, prediction of attacker consequences | Spike 3 disagreement rate at 60/15 ms recorded; canary hit scenarios at 0 % and 2 % | M (~$8-12) |
| **G5** GAS-style correction, listen host | Rejection, effect rollback by prediction key, hit ids; listen host (M9 §9b) | Canary scenarios in listen and dedicated; the degradations of M9 §11 measured and listed | M (~$6-10) |
| **G6** Sweep and docs | Canary in CI, M9 §14 acceptance, README "How to extend", combat.md, deltas | All gates green in both engine configurations (fork project, canary) | M (~$6-10) |

**Total if go**: about **$50-85** of tokens on top of G0 (the spec's own estimate is 4-6 weeks of solo work). Links, mounts and minions
get no stock adapter in this plan (open question M9 in the spec).

## 3. Stop and ask

- G0 finds a forbidden dependency that needs a design change to remove (not a simple include fix).
- G1 changes any byte of an existing suite (the extraction must be invisible).
- Spike 1 shows the payload can arrive after a rejected activation and still act.

## 4. Domande per l'owner (in breve)

1. Esiste o è previsto un secondo progetto, uno store o una console che non può usare il fork GAS-NPP? (No: M9 resta fermo.)
2. Faccio intanto solo G0, il controllo automatico dei confini tra plugin (~$3-6)? (Sì, protegge l'opzione e la modularità.)
