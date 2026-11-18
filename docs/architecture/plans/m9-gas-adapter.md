# Implementation plan — M9 stock GAS adapter (`GSTNCombatStockGAS`)

Spec: [`../combat-m9-stock-gas.md`](../combat-m9-stock-gas.md) (outline in [`../combat-m5-m9.md`](../combat-m5-m9.md)). Plan only, no code.
Same method as the other plans: one gate = one commit, a short agent session per gate (cheapest model that fits), handover in
`docs/notes/handovers/`, deviations in `docs/notes/m9-doc-deltas.md`. Written 2026-11-03 against `feature/gas-npp-5.8`.

## 1. Recommendation: defer (no-go), keep the guard

**Defer M9.** The spec's go/no-go (§2) needs a named trigger and none exists: no second consumer, no store/console release that
cannot ship the fork, fork re-sync cost not measured. M9 is a way out of the fork, not a feature. Reasons:

- **The scope grew since the spec.** The spec assumed `GSTNCombatNP` + `GSTNInputNP`. Today six feature plugins carry NP modules
  too (table below, ~49k lines of NP code), and their `.uplugin` files depend on NetworkPrediction. A stock adapter that leaves
  Defence, Link, Parts, SpinDrill and Piggyback out gives a game without grabs, defence, latch/drill or mounts. Covering them
  multiplies the 4-6 week estimate of the spec (§2).
- **It makes the feel worse, not better.** No rollback: the victim sees a late snap (~RTT/2), parry/tech windows below 5 frames
  degrade, the attacker's hit can be undone. Our wall modes and platform jump stay NP-only. The owner's characteristic movement
  would not exist in the stock project.
- **The expensive part is cheap to protect now.** The boundary is already clean: no `#include` of NetworkPrediction, Mover,
  AbilitySystemSimulation or `FSync*` in `GSTNCombat`, `GSTNCombatGAS`, `GSTNInput` or the `*Core`/`*GAS` modules of Link, Parts,
  Defence (checked 2026-11-03, grep, zero hits), and `GSTNCombatNP`/`GSTNInputNP` are already separate plugins. `Tools/Modules/check_modules.py`
  enforces module edges. What is missing is cheap: the plugin-level edges (below) and a canary build.

**Do now (small, useful even if M9 never happens):** gate M9.A (boundary hardening, ~0.5 day) and M9.B (S0 audit, ~0.5 day).
**Re-check the decision** after M7 is complete and the first playtest over real latency, or when the owner names a trigger.

## 2. Where the fork is touched (S0 preview, from the repo today)

| Area | Modules | Files | Lines | NP edge |
|---|---|---|---|---|
| Combat simulation adapter | `GSTNCombatNP` (own plugin) | 105 | 21.1k | NetworkPrediction, Mover, AbilitySystemSimulation, `GSTNInputNP` |
| Input adapter | `GSTNInputNP`, `GSTNInputNPDebug` | 25 + debug | 3.8k | same |
| Links (grab, latch, mount base) | `GSTNLinkNP` (inside `GSTNLink`) | 58 | 11.3k | `GSTNLink.uplugin` lists NP |
| Defence | `GSTNDefenceNP` (inside `GSTNDefence`) | 40 | 6.3k | `GSTNDefence.uplugin` lists NP |
| Parts | `GSTNPartsNP` (inside `GSTNParts`) | 26 | 3.7k | `GSTNParts.uplugin` lists NP |
| Spin and drill | `GSTNSpinDrillNP`, `GSTNSpinDrillGAS` | 13 + GAS | 3.1k | `GSTNSpinDrill.uplugin` lists NP |
| Piggyback | `GSTNPiggybackRuntime`, `GSTNPiggybackTests` | few | — | Build.cs and `.uplugin` list NP/Mover |
| Platform / movement | `GSTNPlatformRuntime` (+ project `GASNPPTest`, `GAS/GSTN_GA_Jump`, wall modes) | 37+ | — | Mover; **stays NP-only** (spec §1 Out) |
| Tooling | `GSTNCombatEditor` (inline mode, E13), `GSTNCombatDebug` | 4 + 0 | — | depend on `GSTNCombatNP` / `GSTNInputNP` (plan m5 R-B exception) |
| Test scaffolding | `GSTNExtensionTest` (Defence, Link) | 2 modules | — | NP edge |
| Fork itself | `Plugins/GASNPP/PATCHES.md` | 70 lines | — | rollback, input buffer, hitch patches (count the items needing manual work at the next engine upgrade: spec trigger B) |

Neutral and clean today: `GSTNCombatCore`, `GSTNCombatGAS` (6.4k lines), `GSTNInputCore`, `GSTNLinkCore/GAS`, `GSTNPartsCore/GAS`,
`GSTNDefenceCore/GAS`. Unverified: that M2's tick and M4's hit logic are written against host interfaces (spec §1 assumption).
That is what the S0 audit (M9.B) answers; the 21k lines of `GSTNCombatNP` hold the tick, so extraction (spec §5) is the real cost.

## 3. Gates

| Gate | Content | Gate criterion | Cost (agent) |
|---|---|---|---|
| **M9.A** boundary hardening (do now) | Move the NP edge of Link/Defence/Parts/SpinDrill/Piggyback out of the neutral plugins: each `*NP` module becomes its own plugin (`GSTNLinkNP`, ...) like `GSTNCombatNP`; neutral `.uplugin` files lose NetworkPrediction/Mover. Extend `check_modules.py` with the `.uplugin` rule and the forbidden-identifier rule (`UNpGameplayAbility`, `FSync*`). | `check_modules.py` green; full build with plugins on/off; headless suites unchanged (1285/0 on, 1260/0 off style); no C++ module renamed | Sonnet, ~1 session, ~$3-6 |
| **M9.B** S0 audit (do now) | List every NP/Mover/`UNp*` call and include in the combat tick (M2) and hit code (M4) inside `GSTNCombatNP`; table in `docs/notes/m9-s0-audit.md`; estimate extraction in lines and risk. Also count `PATCHES.md` items needing manual work. | Table committed; trigger B number given; this plan's estimates updated | Sonnet, ~1 session, ~$2-4 |
| *-- decision point: owner names a trigger or says go --* | | | |
| M9.C canary project | Minimal project with only neutral plugins, no `GASNPP`. Loads and passes `GSTNCombat.Core.*` and boundary tests. | Loads; weekly/on-demand build documented | Sonnet, ~$3-5 |
| M9.D extraction | `FGSTN_CombatState`, `FGSTN_CombatStepper`, `IGSTN_CombatEnvironment` in Core (spec §5); NP adapter implements them with no behaviour change | Every M2-M4 scenario and test unchanged on NP | Opus medium, 2-3 sessions, ~$25-45 |
| M9.E spikes 1-4 | Payload race, movement bridge, lag history, time base (spec §3) | Numbers recorded in `docs/notes/m9-spike-report.md`; go/no-go re-evaluated | Opus, ~$10-15 |
| M9.F stock adapter | Component, `UGSTN_GA_Base` (stock), prediction keys, activation payload, CMC bridge (spec §6-8) | Three-hit string between two clients at 60/15 ms and 2 % loss; payload ≤ 70 B | Opus, ~5 sessions, ~$60-100 |
| M9.G hits and correction | Server hurtbox history, lag compensation, GAS-style correction, listen-server run (spec §9-10) | Spec §14 criteria 3-4; degradation table measured | Opus, ~3 sessions, ~$35-60 |
| M9.H feature plugins | Stock modules for Defence, Link, Parts, SpinDrill, Piggyback — only the ones the owner wants | Per-plugin harness scenarios on the stock adapter | Opus, ~$50-150 per plugin (Link is the largest) |
| M9.I reparent tool + docs | `reparent_abilities.py`, CoreRedirects, README per plugin, spec deltas | Sample ability Blueprint moves across adapters | Sonnet, ~$5-10 |

Total if everything is built: roughly $190-350 and 6-10 weeks of calendar time, with M9.H the biggest unknown. Do now: ~$5-10 total.
Order after the decision point: C, D, E (stop here if the spikes disappoint), F, G, then H for chosen plugins, I.

## 4. Rules for the work

- Epic coding standard, plugin README updated at each gate, native tags only (the `Combat.Net.*` payload keys are stock-adapter owned, per modularity §4.2).
- Gates M9.A and D touch `GSTNCombat*` and the feature plugins: only on the laptop, one code thread at a time, after the thread working there has merged.
- Both adapters enabled in one project stays unsupported and fails at startup with a clear log (spec §5b).

## 5. Open questions for the owner (Italian)

1. Esiste (o è pianificato) un secondo progetto, una distribuzione (store, console, altro team) che non può includere il fork GAS-NPP? Senza un caso concreto la raccomandazione resta "rimanda".
2. Va bene fare subito solo M9.A e M9.B (separare i moduli NP in plugin propri, controlli automatici, audit, circa 0,5 giorno ciascuno)? Servono anche se M9 non parte mai.
3. Quanto ti è pesato aggiornare il fork alla 5.8 (giorni, patch di `PATCHES.md` fatte a mano)? Sopra circa 5 giorni a ogni aggiornamento M9 diventa una via d'uscita sensata.
4. Se un giorno si fa, quali plugin di funzionalità devono funzionare anche con GAS standard (difesa, link, parti, spin/drill, piggyback), o va bene solo il combattimento base senza le prese?
5. Accetti che con GAS standard chi subisce un colpo veda il contraccolpo in ritardo (circa metà ping) e che parry/tech sotto i 5 frame diventino più larghi? Il gioco resta co-op e listen server.
