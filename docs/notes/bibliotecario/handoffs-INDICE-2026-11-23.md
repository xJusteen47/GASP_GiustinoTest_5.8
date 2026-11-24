# Indice dei thread (handoffs/INDICE.md)

Una riga per thread, ricostruita il 2026-11-23 dal bibliotecario da `handoffs/costi.csv` (70 righe) e dai branch sul remoto (`git ls-remote`). Le righe di `costi.csv` senza tutte le colonne sono riportate come sono: i campi mancanti restano vuoti. "Stato branch": `in main` = unito (PR #37 e prima), `aperto` = esiste sul remoto e non risulta unito, `non sul remoto` = nome non trovato (rinominato o cancellato), `?` = nessun branch nella riga. Costi = stime dei thread, non fattura.

## Totale settimanale (stime, $)

| Settimana (dal lunedì) | Totale |
|---|---|
| 2026-11-16 | 312.5 |
| 2026-11-23 | 25.5 |

## Thread

| Data | Thread | Branch / obiettivo | Macchina | Modello | Esito | $ | Stato branch |
|---|---|---|---|---|---|---|---|
| 2026-11-19 | Audit agentic setup and costs | audit-setup | cloud | Opus | PASS | 4 | ? |
| 2026-11-20 | Build capture and review guard scripts | guard-scripts | cloud | Opus | PARTIAL (desktop checks pending) | 9 | ? |
| 2026-11-20 | Diagnose Cascadeur restyle of run stop | loco-runstop-diagnosis | cloud | Opus | PASS (cause found; option A recommended) | 2 | ? |
| 2026-11-20 | runstart-v3-review-1 | `claude/runstart-v3-review-1` |  |  | ~10 min desktop | 11 | non sul remoto |
| 2026-11-20 | runstart-v3-pose-1 | `claude/runstart-v3-pose-1` |  |  | ~14 min desktop | 22 | non sul remoto |
| 2026-11-20 | runstart-v3-pose-2 | `claude/runstart-v3-pose-2` |  |  | ~8 min desktop | 16 | non sul remoto |
| 2026-11-20 | runstart-v4r4-elbows | `claude/runstart-v4r4-elbows` |  |  | ~5 min desktop | 6 | non sul remoto |
| 2026-11-20 | Compare run start acceleration with Astro Bot | runstart-accel-1 | cloud+desktop | Opus | PARZIALE (rampa pronta non applicata) | 11 | ? |
| 2026-11-20 | runstart-v5-desktop | `claude/runstart-v5-desktop` |  |  | ~12 min desktop | 14 | non sul remoto |
| 2026-11-20 | Lay groundwork for forward rendering | `claude/project-thread-xmve9g` | cloud | Opus | FATTO (1A 2A applicati; non compilato) | 0 | non sul remoto |
| 2026-11-20 | Fix camera jitter | `claude/project-thread-hiq1zx` | cloud | Opus | PARZIALE (fix ordine tick misurato; rimbalzo a 2 livelli ancora da diagnosticare con gstn.Camera.Trace) | 0 | non sul remoto |
| 2026-11-20 | Design readable run stages from Luigi reference | `claude/project-thread-le0tc7` | cloud | Fable | FATTO (piano; 3 decisioni owner) | 0 | aperto |
| 2026-11-20 | Camera: capsule changes must not move it | `claude/project-thread-hiq1zx` | cloud | Opus | FATTO (non compilato; A/B Vertical 1/2 sul fisso) | 0 | non sul remoto |
| 2026-11-20 | Shorten run start to Luigi timing (prep v4r8) | `claude/project-thread-64vizj` | cloud | Opus | PRONTO (script; applicazione sul fisso) | 0 | aperto |
| 2026-11-20 | Review shorter Luigi-timed run start | runstart-v4r8 | cloud | Fable | FAIL | 2 | ? |
| 2026-11-21 | Review three run start variants | runstart-v4r8abc | cloud | Fable | a PASS / b c FAIL dati montage | 6 | ? |
| 2026-11-21 | Build Mario-style camera code P1a | camera-p1a | cloud | Opus | code+tests written not compiled (5509f341 + dead zone) | 11 | ? |
| 2026-11-21 | Plan Overlord run loop from Ganon refs | `claude/project-thread-l8warz` | cloud | Fable | plan + grids done (claude/project-thread-l8warz) | 3 | aperto |
| 2026-11-21 | Polish sprinter run start with footsteps | runstart-sprinter-s2 | desktop | Opus | s2 Variant 9 PASS + foley + debug menu | 6 | ? |
| 2026-11-21 | Build generic run start on sprinter weight shift (w1) | runstart-generic-w1 | desktop | Opus | w1 Variant 20 PASS approved locked | 4.5 | ? |
| 2026-11-21 | Build generic run start on sprinter weight shift (w2) | runstart-generic-w2 | desktop | Opus | w2 Variant 21 PASS slide fixed + menu tidy + PIE 1 | 7 | ? |
| 2026-11-21 | Give dropkick more impact and crossed arms | dropkick-k5 | laptop | Opus | K5 poses ready; blocked on laptop editor permission | 7 | ? |
| 2026-11-21 | Review dropkick K5 kick | dropkick-k5-review-1 | cloud | Fable | FAIL posa: arms not crossed; snap PASS; smug fall not verifiable | 3 | ? |
| 2026-11-21 | Make skid an organic speed change | `claude/project-thread-4adnj1` | cloud | Opus | code ready NOT compiled (claude/project-thread-4adnj1 588b378e); desktop build+tests+move_reversal A/B | 5 | non sul remoto |
| 2026-11-21 | Diagnose why Mario camera pitch locks | `claude/project-thread-aq82ak` | cloud | Opus | cause found (look feedback loop after control rotation write back); fix NOT compiled (claude/project-thread-aq | 4 | non sul remoto |
| 2026-11-21 | Review dropkick K5 round 3 | `claude/project-thread-98wtdn` | cloud | Fable | PASS posa (claude/project-thread-98wtdn 001a1a02): identity traits read; overshoot = montage owner call; a19 a | 3 | aperto |
| 2026-11-21 | Fix mesh flash on ZL crouch | `claude/project-thread-qncgfg` | cloud | Opus |  | 0 | non sul remoto |
| 2026-11-21 | Review dropkick K6 follow-through | `claude/project-thread-1akb6a` | cloud | Fable | FAIL posa round 1 (claude/project-thread-1akb6a 2bd9b2ef): identity + Big Band line PASS; arm press and settle | 3 | aperto |
| 2026-11-21 | Update milestone status from the repo | `claude/project-thread-yepa62` | cloud | Opus | DONE (claude/project-thread-yepa62 f237af8f): M10/M11 done; M12-M14 not started; parallel proposal cloud A M12 | 2 | aperto |
| 2026-11-21 | Add haptics and surface footstep audio (C++/data) | `claude/project-thread-c2dfra` | cloud | Opus | DONE code (claude/project-thread-c2dfra 8ff53b1a) not compiled; desktop steps 1-9 in docs/notes/surface-feedba | 5 | in main |
| 2026-11-21 | Widen run start feet with an additive | `claude/runstart-generic-desktop` | desktop | Opus | PASS approved v22 (claude/runstart-generic-desktop e44b6692) + crouch pop merged PASS | 8 | aperto |
| 2026-11-21 | Add weight and follow-through to dropkick | `claude/project-thread-jx1fv2` | laptop | Opus | built K6 r1 (claude/project-thread-jx1fv2 5a821c53); review FAIL posa | 7 | aperto |
| 2026-11-21 | Fix turnaround feet crossing and arms | `claude/runstart-generic-desktop` | desktop | Opus | DONE v23/24/25 + TorsoLean + toe fix (claude/runstart-generic-desktop 8cb52ea7); owner verdict pending | 16 | aperto |
| 2026-11-21 | Fix turnaround feet and speed up captures | `claude/runstart-generic-desktop` | desktop | Opus | DONE v26 + fast capture (claude/runstart-generic-desktop 84cc0057); owner redesign -> v27 | 14 | aperto |
| 2026-11-21 | Make dropkick arm press and settle readable | `claude/project-thread-l1dcng` | laptop | Opus | built K6 r2 (claude/project-thread-l1dcng 84b7c354); awaiting cloud review | 6 | aperto |
| 2026-11-21 | Code readable run stages S1 | `claude/project-thread-njoso3` | cloud | Opus | DONE code (claude/project-thread-njoso3 88e3424c) not compiled | 4 | in main |
| 2026-11-21 | Finish M10 and M11 leftovers | `claude/project-thread-476gv2` | cloud | Opus | DONE code (claude/project-thread-476gv2 ca4b2e8a) not compiled | 7 | in main |
| 2026-11-21 | Review dropkick K6 round 2 | `claude/project-thread-oqfyxp` | cloud | Fable | FAIL posa giro 2/3 (claude/project-thread-oqfyxp f6adaef8): settle PASS; startup arms pop + release hidden | 3 | aperto |
| 2026-11-21 | Build mission lives and checkpoints | `claude/project-thread-0f3p4m` | cloud | Opus | DONE code (claude/project-thread-0f3p4m 04a766c5) not compiled; D5 parts not done | 7 | in main |
| 2026-11-21 | Package the harness as a starter kit | harness-starter-kit | cloud | Opus | PASS (Windows scripts not run) | 3 | ? |
| 2026-11-21 | Review turnaround 180 left variant 27 | `claude/project-thread-lnbke8` | cloud | Fable | FAIL giro 1 (claude/project-thread-lnbke8 7f0002c5): pivot step v85-88 PASS; in-game yaw stalls side-on v89-97 | 4 | aperto |
| 2026-11-21 | Turnarounds pivot variant 27 desktop | `claude/runstart-generic-desktop` | fisso | Opus | 180 L round 2 + option A ready for review (claude/runstart-generic-desktop 22300411); r1 FAIL; 3 other directi | 15 | aperto |
| 2026-11-21 | Review turnaround 180 left v27 round 2 | `claude/project-thread-7q0nby` | cloud | Fable | PASS giro 2 (claude/project-thread-7q0nby a42d8c8a): r1 items 1-5 fixed; pivot v84-93 one beat; R foot lands v | 4 | aperto |
| 2026-11-21 | Turnarounds pivot variant 27 desktop | turn3-v27 | fisso | Opus | 180 L r2 + A + arms/foot: in game follows the clip; review r2 pending; style pass next thread | 18 | ? |
| 2026-11-21 | Style pass turnaround 180 L desktop | `claude/runstart-generic-desktop` | fisso | Opus | v28 Cascadeur keys + curve sheet; gates FAIL jitter after 2 tries; option A breakdowns next (claude/runstart-g | 6 | aperto |
| 2026-11-21 | Style pass turnaround 180 L desktop round A | `claude/runstart-generic-desktop` | fisso | Opus | v28 breakdowns gates OK + capture (claude/runstart-generic-desktop b49ec08a); review pending | 3 | aperto |
| 2026-11-21 | Review turnaround 180 L v28 style pass round 1 | `claude/project-thread-rdcazm` | cloud | Fable | FAIL pose (arms do not lead / hold / hand on hip / belly fist / right foot); review claude/project-thread-rdca | 3 | aperto |
| 2026-11-21 | Style pass turnaround 180 L desktop pose r2 | `claude/runstart-generic-desktop` | fisso | Opus | v28 pose round 2 gates+capture OK (claude/runstart-generic-desktop 41cbc8e9); review pending | 5 | aperto |
| 2026-11-21 | Review turnaround 180 L v28 pose round 2 | `claude/project-thread-orqz14` | cloud | Fable | FAIL pose round 2 of 3 (right foot still old heading v87-v90 pose-or-game / arms parked v96-v100); P1-P4 fixed | 3 | aperto |
| 2026-11-21 | Style pass turnaround 180 L desktop pose r3 | `claude/runstart-generic-desktop` | fisso | Opus | v28 pose round 3 gates+capture OK (claude/runstart-generic-desktop 3c779b08); P5 foot = game item queued | 4 | aperto |
| 2026-11-21 | Review turnaround 180 L v28 pose round 3 | `claude/project-thread-bjvo40` | cloud | Fable | FAIL pose round 3 of 3 (arms hand-over v95-v101 folded/fist in front of pelvis; right foot v86-v90 = game); pi | 4 | aperto |
| 2026-11-22 | Verify cheaper AI stack proposal | stack-economico | cloud | Opus | DONE verdict handoffs/stack-economico-verdetto-2026-11-22.md (1B 2B 3A + rule 3 rewrite) | 2 | ? |
| 2026-11-23 | Punti 5-7 lista deterministica (montaggio A/B | `claude/project-thread-nfhw4i` | cloud | Sonnet |  | 0 | aperto |
| 2026-11-23 | anim-inventario | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 2.5 | aperto |
| 2026-11-23 | anim-inventario-giro2 (roster 4 kit | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 1 | aperto |
| 2026-11-23 | anim-inventario-giro3 (nomi roster) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.3 | aperto |
| 2026-11-23 | anim-inventario-giro4 (fair/dair light) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.2 | aperto |
| 2026-11-23 | anim-inventario-giro5 (overlord strong) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.2 | aperto |
| 2026-11-23 | anim-inventario-giro6 (notazione) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.3 | aperto |
| 2026-11-23 | anim-inventario-giro7 (Cr.N.L Aerial) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.1 | aperto |
| 2026-11-23 | anim-inventario-giro8 (rimossi up/back) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.1 | aperto |
| 2026-11-23 | anim-inventario-giro9 (Cr.F.Sp) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.1 | aperto |
| 2026-11-23 | anim-inventario-giro10 (regola neutrale) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.1 | aperto |
| 2026-11-23 | anim-inventario-giro11 (curva su N.S) | `claude/anim-inventario-2026-11-23` | cloud | Sonnet medium |  | 0.1 | aperto |
| 2026-11-23 | skin-proporzioni-analisi | `claude/project-thread-4pj7kf` | cloud | Opus medium |  | 2 | aperto |
| 2026-11-23 | Map and propose Content folder reorganization | `claude/content-riorganizzazione` | cloud | Opus medium | PASS (fase 1 solo lettura) | 1.5 | aperto |
| 2026-11-23 | Build grab hold and throws link code | `claude/grab-link` | cloud | Opus medium | PARZIALE (codice non compilato) | 7 | aperto |
| 2026-11-23 | Build project index and code audit | `claude/indice-progetto` | cloud | Opus medium | PASS (sola lettura; 4 subagenti Sonnet) | 4.5 | aperto |
| 2026-11-23 | Map input bindings and check PvP netcode strategy | `claude/project-thread-uvefj5` | cloud | Opus high | PASS (sola lettura; 1 subagente Sonnet) | 3 | aperto |
| 2026-11-23 | Research 1v1 online without dedicated server (P2P/host) | `claude/ricerca-server-dedicato` | cloud | Opus high | PASS (sola lettura; 1 subagente web-fetch) | 2.5 | aperto |

## Righe precedenti (testo originale del file, 172 byte)

- 2026-11-21 | runstart-generic w1/w2 | claude/runstart-generic-desktop 5e585aa8 | PASS, w1 approved | 11.5 $ ; dropkick-k5 claude/project-thread-72dtra blocked perm | 7 $

Da completare a mano dal coordinatore: i thread di oggi senza riga in costi.csv (presa con revisione, rete in corso, MCP v2, integrazione sul portatile, bibliotecario) e lo stato "da unire / da buttare" per ogni branch aperto (regola 2).
