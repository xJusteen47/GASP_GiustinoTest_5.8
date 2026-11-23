# Bibliotecario della documentazione: giro del 2026-11-23

Primo giro, sola lettura. Branch `claude/docs-bibliotecario-2026-11-23` (parte da `claude/indice-progetto`). Niente è stato
cancellato o spostato; nessun file di GASP o locale dell'owner toccato; nessun artefatto modificato.

## Rapporto in 10 righe
1. `main` non è cambiato dalla fotografia dell'indice (a5b20950, PR #37 del 22-11): nessuna voce di codice da aggiornare.
2. Oggi sono nati 5 documenti, tutti solo su branch: cosmetica skin, riordino Content, mappa input, presa al collo, ricerca rete 1v1. Ora sono nell'indice (§6). Nuovi anche `docs/STATO-ATTUALE.md` (fonte unica delle decisioni vive) e `docs/BRIEFING-ANTIGRAVITY.md` (una pagina per agenti esterni).
3. Gli artefatti pubblicati sono 11: 3 vanno aggiornati, 1 fuso in un altro, 6 sono superati e andrebbero segnati come archivio (sezione E). Ora sono in cima all'indice.
4. Il caso peggiore: "Cose da fare a mano" (13-11) dice ancora di pushare su `feature/gas-npp-5.8`, cosa che oggi la guardia git blocca.
5. "Mosse di esempio GSTN" è la guida per i designer: il titolo va bene, ma il suo stato (rami, numero di test) è del 16-11 e la copia nel repo è del 14-11.
6. 130 documenti su 167 in `docs/` non compaiono in nessun indice; 11 non li cita proprio nessuno (sezione A).
7. Le voci dell'indice puntano tutte a file esistenti; le 4 che sembrano mancare esistono sui loro branch (sezione B).
8. Ho trovato 9 contraddizioni; le due che possono far sbagliare un thread: le regole in `CLAUDE.md` puntano al file del 22-11 (Antigravity coordina, DeepSeek lavora) mentre le regole di progetto del 23-11 dicono il contrario; e la mappa input propone "server dedicato per il ranked" mentre la ricerca di oggi parte da "niente server dedicato" (sezione D).
9. 103 handoff su 152 sono vecchi (prima del 19-11) e nessuno li cita più: proposti per un archivio (sezione C).
10. `handoffs/INDICE.md` ora ha una riga per ognuno dei 70 thread di `costi.csv`, con lo stato del branch; spesa stimata: 312 $ nella settimana del 16-11, 25,5 $ registrati finora questa settimana.

## Decisioni per l'owner (max 3)
1. **Artefatti superati.** A (consigliata): un thread Sonnet (~2-3 $) aggiorna "Mosse di esempio", "Architettura plugin" e "Networking", fonde "Guida mosse Blueprint" in "Mosse di esempio", e i 6 superati restano online ma segnati "ARCHIVIO" nell'indice. B: solo il segno nell'indice, nessun aggiornamento (0 $). C: cancellare i 6 superati (non si torna indietro). Intanto non resta fermo niente.
2. **Handoff vecchi.** A (consigliata): spostare i 103 file in `handoffs/archivio/2026-11/` (reversibile, ~0,5 $). B: lasciarli dove sono, segnati solo qui. Intanto non resta fermo niente.
3. **Una sola fonte per le regole.** A (consigliata): `CLAUDE.md` punta alle istruzioni di progetto del 23-11 e `regole-progetto-2026-11-20/22.md` vengono segnate come superate (~0,5 $, PR verso main). B: lasciare così. Finché manca, un thread che legge solo `CLAUDE.md` può seguire le regole vecchie.

## A. Documenti orfani

Metodo: ogni `.md` sotto `docs/` (esclusi `handovers/` e `reviews/`, 167 file) cercato per nome in `docs/INDICE-PROGETTO.md`, `docs/DOVE-METTO-MANO.md`, `docs/notes/audit-codice.md`, `CLAUDE.md`, `docs/notes/README.md`, `Plugins/README.md`, `Artifacts/README.md`. **130 non compaiono in nessun indice.** Tra parentesi: quanti altri documenti li citano. Le date git non aiutano (quasi tutti risultano 21-11 per un commit di massa).

**Orfani veri (11, nessuno li cita):** `docs/notes/assistant/2026-11-19_q1.md`, `docs/notes/assistant/2026-11-19_q3.md`, `docs/notes/assistant/2026-11-19_q4.md`, `docs/notes/assistant/2026-11-19_q5.md`, `docs/notes/assistant/2026-11-19_q6.md`, `docs/notes/camera-doc-deltas.md`, `docs/notes/depth-marker-doc-deltas.md`, `docs/notes/depth-marker-pseudocode.md`, `docs/notes/interaction-doc-deltas.md`, `docs/notes/owner-requirements-2026-10-23.md`, `docs/notes/test-hub-audit.md`. Proposta: i `*-doc-deltas.md` e `assistant/2026-11-19_q*.md` in `docs/notes/archivio/`; `owner-requirements-2026-10-23.md` va citato da `INDICE-PROGETTO` §7 (sono i requisiti originali dell'owner); `depth-marker-pseudocode.md` va citato da `docs/architecture/depth-marker.md`.

**Citati solo da altri documenti (non da un indice):**
- `docs/architecture/`: combat-bp-state-events.md (1), combat-loadout-mods.md (19), combat-m1-core-input.md (4), combat-m2-sim-input.md (5), combat-m5-m9.md (7), combat-m9-stock-gas.md (2), depth-marker.md (12), game-design-direction.md (29), game-modes.md (15), gravity-volumes.md (6), interaction.md (24), level-activation.md (8), many-enemies-replication.md (13), open-questions-m4-m10.md (22), spin-drill.md (24), wall-slide-v2.md (5)
- `docs/architecture/plans/`: fp-forward-prediction.md (4), fwd-prediction-2026-11-07.md (2), input-opzioni-per-giocatore.md (3), m11-kit-piano-a-fette.md (10), m11-kit.md (14), m12-modi-livelli.md (3), m13-spedizione.md (3), m14-pvp.md (3), m4-hits-plan.md (7), m4b-parts-plan.md (8), m5-editor-plan.md (4), precisione-weakpoint-2026-11-09.md (4), resto-m3-m4b-m5-t.md (10), sequenza-2026-11-06.md (9), spin-drill-plan.md (6), test-maps.md (7), traversal-surfaces-plan.md (7)
- `docs/knowledge/`: INDEX.md (17)
- `docs/knowledge/gasp/`: gasp-structure-5.9.md (3)
- `docs/knowledge/gasp/evidence/`: assistant_replies.md (3)
- `docs/notes/`: abp-architecture.md (3), additive-lean-1.md (1), anim-process.backup-2026-11-22.md (1), anim-process.backup.md (2), anim-process.md (4), anim-setup-audit.md (2), audit_setup_agentico.md (2), balloon-knockback.md (1), camera-tiltshift-1.md (1), corsa-mario-piano.md (1), crouch-veloce-1-brief.md (1), decisioni-2026-11-02.md (1), foot-lock-and-slides.md (1), gasp-curves.md (2), independence-audit.md (3), input-memory-audit.md (3), loadout-table-swap-design.md (2), m2-agent-instructions.md (1), m3-doc-deltas.md (6), m3-plan.md (5), m3-spike-report.md (3), m4-doc-deltas.md (6), m4-spike-report.md (3), m4b-doc-deltas.md (6), m4b-spike-report.md (4), m5-spike-report.md (2), m6-doc-deltas.md (2), m6-spike-report.md (4), m7-doc-deltas.md (5), m7-spike-report.md (6), m8-doc-deltas.md (4), m8-spike-report.md (6), networking-spiegato.md (2), owner-manual-todo-artifact.md (1), perf-debt.md (11), pie-perf-probe-2.md (2), plugin-split-checklist.md (1), regole-progetto-2026-11-20.md (1), rendering-profiles.md (1), review-m3-actions.md (2), review-pr3-actions.md (1), run-stages-s1-test-plan.md (1), speed-up.md (5), spike-sfp-plan.md (1), spike-sfp-report.md (1), spin-drill-doc-deltas.md (3), spin-drill-spike-report.md (2), test-content-names.md (1), traversal-doc-deltas.md (3), ue59-adoption.md (2)
- `docs/notes/anim-theory/`: animazione-picchiaduro-action.md (4), principi-animazione-corporea.md (4)
- `docs/notes/assistant/`: 2026-11-19_q2.md (1), 2026-11-20_pivot.md (2), 2026-11-20_pivot3.md (1), 2026-11-20_reversal.md (2)
- `docs/notes/briefs/`: jumpland-identity.md (2), runstart-identity.md (3), skid-identity.md (1)
- `docs/notes/camera-platforming/`: camera-p0-brief.md (4), p1a.md (3), ricerca-mario-astro.md (1)
- `docs/notes/plans/`: m11-l7c-dome-plan.md (1)
- `docs/notes/polish-m1/`: abp-wallsliding-export.md (1)
- `docs/notes/reference-dossiers/`: dash-ganon.md (10)
- `docs/notes/research/`: unreal-assistant-crouch-pop.md (1), unreal-assistant-haptics-audio.md (2), unreal-assistant-reface-starts.md (1), unreal-assistant-stance-width.md (2), unreal-assistant-torso-lean-pivot.md (2), unreal-assistant-turn-yaw.md (1)
- `docs/notes/review/`: critico-estetico.md (5), dossier-riferimento.md (7), dossier-template.md (5), gstn-move-animation-edits-2026-11-19.md (1), skills-portabilita.md (2), ue59-strumenti-per-skill.md (3)
- `docs/notes/skills/`: gstn-move-animation-clipcheck-edit.md (1)
- `docs/notes/stile/`: stile-sheet-vfx-review.md (8), vfx-stile-v3-design-sheet.md (1)
- `docs/notes/vfx-theory/`: agy-vfx-theory-peer-review-prompt.md (1), gstn-vfx-reviewer.SKILL.md (5), principi-vfx.md (7)

Proposta: `docs/architecture/` intero (33 file) va elencato in `INDICE-PROGETTO` §7 come tabella "doc di sistema"; `docs/architecture/plans/` e gli `*-spike-report`/`*-doc-deltas` di M3-M8 sono storia di milestone chiuse: archivio.

## B. Voci dell'indice senza file
Metodo: ogni percorso tra backtick o link in `docs/INDICE-PROGETTO.md`, `docs/DOVE-METTO-MANO.md`, `CLAUDE.md`, `docs/notes/README.md`, cercato nel repo (anche con i prefissi brevi `Core/`, `NP/`, `GAS/`, `Prj/`).
Nessun percorso rotto in `main`. Quattro voci esistono solo su branch, come l'indice già dice: `docs/notes/mappa-cartelle.md` (`project-thread-f4ovn9`, INDICE riga 101), `docs/notes/notazione-mosse.md` (`anim-inventario-2026-11-23`, riga 129), `docs/notes/skin-cosmetici-modulari.md` (`skin-cosmetici-ricerca`, riga 165), `skin-proporzioni-analisi.md` (`project-thread-4pj7kf`, riga 165). Verificate con `git cat-file -e`.
Nei project files: il brief parlava di `handoffs/content-riorganizzazione.md`; esiste, ma il documento vero è `docs/notes/content-riorganizzazione.md` sul branch `claude/content-riorganizzazione`.
Artefatti: `Artifacts/README.md` elenca 3 copie; "Guida mosse Blueprint" ha la copia solo su `claude/artifacts` (`Artifacts/Designer/guida-mosse-blueprint.html`) e manca dal README; gli altri 7 artefatti non hanno copia.

## D. Contraddizioni tra documenti
| # | Cosa | Dove (file:riga) | Contro | Proposta |
|---|---|---|---|---|
| D1 | Priorità di `IMC_GASNPP_Test` (contesto dei tasti di combattimento) | `Tools/GAS/setup_player_input_contexts.py:9` dice 10 | `docs/notes/handovers/movimento.md:27` dice 5; la mappa input lo lascia aperto (`docs/notes/input-azioni-mappa.md:149`, branch `project-thread-uvefj5`) | verificarlo in editor (passo P0 della mappa input), poi correggere il file sbagliato |
| D2 | L3 | schema tasti dell'owner in memoria: "L3 dash" | `input-azioni-mappa.md:53-54`: oggi L3 = run boost, il dash non ha tasto sul pad | decisione dell'owner già in coda nella mappa input |
| D3 | Regole di lavoro | `CLAUDE.md:61` rimanda a `docs/notes/regole-progetto-2026-11-22.md` §3 (Antigravity coordina, DeepSeek per build e codice) | istruzioni di progetto aggiornate il 23-11 (regola 3: coordinatore Sonnet/Opus, Fable solo dopo 2 errori di Opus; crediti Antigravity finiti); in `docs/notes/` convivono anche `regole-progetto-2026-11-20.md` | decisione 3 |
| D4 | Branch di lavoro | `CLAUDE.md:69` "Branch `feature/gas-npp-5.8`, base `main`" | `INDICE-PROGETTO.md` §6: `feature/gas-npp-5.8` non è antenato di main, non partire da lì; la guardia git blocca i push su `feature/*` | correggere `CLAUDE.md` insieme alla decisione 3 |
| D5 | Test e versione | `docs/architecture/testing.md:7` "94 tests", `:10` percorso `UE_5.8`, `:38` "in 5.8" | `CLAUDE.md` (UE 5.9), `INDICE-PROGETTO.md` §4 (1614 test il 21-11) | aggiornare `testing.md` (lavoro Haiku, <0,5 $) |
| D6 | Server per il 1v1 | `input-azioni-mappa.md:170` decisione 3A "server dedicato per il ranked"; memoria di progetto `input-azioni-mappa` | `server-dedicato-ricerca.md:3-5` (branch `ricerca-server-dedicato`): l'owner ha precisato che all'inizio non ci sarà server dedicato | segnare la 3A della mappa input come superata dalla ricerca |
| D8 | Run start 29w3 | memoria: "29w3 APPROVED" (23-11) | nessun tag `approved/runstart-v29w3` sul remoto (`git ls-remote --tags`); riga solo sul branch `runstart-lock-2026-11-23` | pushare il tag da un PC (già in coda) |
| D9 | Nomi degli artefatti | memoria: "DESIGNER GUIDE" e "MANUAL-TODO" | titoli reali "Mosse di esempio GSTN" e "Cose da fare a mano" (il secondo superato) | usare i titoli reali; vedi `docs/STATO-ATTUALE.md` |
| D7 | Lanci della presa | `INDICE-PROGETTO.md` §5.6: oggi lanci Su/Avanti/Indietro/Giù | owner 23-11 e branch `grab-link`: N., F., Cr., Cr.F. | non è un errore (branch non unito); l'indice ora lo dice |
Altre differenze già note e non ripetute qui: `Plugins/README.md` in parte vecchio (audit-codice), nome "Test" sul contenuto vero (audit A1).

## C. Handoff vecchi da spostare in archivio (proposta, nessuno spostamento fatto)

Criterio: file in `handoffs/` (project files) modificati prima del 19-11, che nessun file più recente del 21-11, l'indice della memoria o `docs/INDICE-PROGETTO.md` cita. Candidati: **103** su 152. Destinazione proposta: `handoffs/archivio/2026-11/` (spostamento reversibile, i nomi restano uguali). Motivo comune: lavoro chiuso o superato, già riassunto in un handover nel repo o in memoria.

**Bugpass, polish e feel (lavori 16-18/11 chiusi)** (45): `air-crouch-1.md` (11-16), `air-momentum-1.md` (11-18), `back-hit-turn.md` (11-14), `balloon-knockback.md` (11-16), `bugpass-1-handover.md` (11-18), `bugpass-1.md` (11-18), `bugpass-2.md` (11-18), `bugpass-3.md` (11-18), `bugpass-4.md` (11-18), `bugpass-desktop-checklist.md` (11-18), `crouch-ref` (11-18), `crouch-veloce-1-handover.md` (11-18), `crouch-veloce-1.md` (11-18), `dodge-fallback-1117.md` (11-17), `feel-defaults-piano.md` (11-17), `feel-f1-dodge-lag-brief.md` (11-17), `feel-f2` (11-17), `feel-f2-knockback-smash-brief.md` (11-17), `feel-f3-run-per-mode-brief.md` (11-17), `feel-f4-desktop-brief.md` (11-17), `gravita-a-1.md` (11-16), `gravita-audit-1.md` (11-16), `ground-bounce-1.md` (11-16), `hitstop-sakurai-1.md` (11-18), `jump-hold-gravity.md` (11-18), `landing-capsule-bounce-fixes.md` (11-17), `landing-hard-cut-regola.md` (11-18), `launch-light-kit-1.md` (11-16), `ledge-check-1.md` (11-16), `m3-air-turn.md` (11-17), `montage-half-rate.md` (11-15), `polish-m0-brief.md` (11-17), `polish-m0.md` (11-17), `polish-m1-brief.md` (11-17), `polish-m1.md` (11-17), `polish-m2-brief.md` (11-17), `polish-m4a-brief.md` (11-17), `polish-m4a.md` (11-17), `polish-m5-brief.md` (11-17), `polish-movimento-vfx-piano.md` (11-17), `qa-jump-air-brief.md` (11-17), `review-feel-defaults.md` (11-17), `stomp-atterraggio.md` (11-17), `tumble-recovery-1.md` (11-16), `wall-ceiling-bounce-1.md` (11-17)

**Dropkick e dair (round superati da K5/K6)** (7): `dair-v10-dropkick-bounce-3.md` (11-17), `dair-v10-dropkick-bounce-4.md` (11-17), `dropkick-air-timeout-1.md` (11-16), `dropkick-capsule-1.md` (11-15), `dropkick-land-1.md` (11-15), `dropkick-tech-bounce-anim-brief.md` (11-17), `jab-dropkick-r1-desktop-brief.md` (11-15)

**Corsa, run start e skid (superati da runstart-generic e v29w3)** (8): `corsa-mario-p1-brief.md` (11-16), `corsa-mario-p1-handover.md` (11-16), `corsa-mario-p2-p3-handover.md` (11-16), `corsa-mario-piano.md` (11-16), `preset-mario3dw.md` (11-16), `ref-smash-run` (11-18), `run-start-skid-import-piano.md` (11-16), `run-start-stop-skid-brief.md` (11-18)

**Camera (superati da camera-p1a e piano astrobot)** (4): `camera-anchor-1.md` (11-17), `camera-c0-brief.md` (11-17), `camera-tiltshift-1-handover.md` (11-18), `camera-tiltshift-1.md` (11-18)

**VFX, stile e silhouette (VFX in pausa)** (10): `bounce-vfx-capture-1.md` (11-17), `depth-marker-smoothing-1.md` (11-17), `ref-astrobot-vfx` (11-18), `silhouette-vfx-ricerca.md` (11-15), `stile-astrobot-nintendo-handover.md` (11-17), `stile-astrobot-nintendo.md` (11-17), `stile-sheet-vfx-review-proposta.md` (11-17), `vfx-clarity-ricerca.md` (11-17), `vfx-stile-v2-astrobot.md` (11-17), `vfx-stile-v3-categorie.md` (11-18)

**Ricerche chiuse** (7): `anim-physics-pass-ricerca.md` (11-17), `attack-aiming-research.md` (11-14), `reaction-anim-sources.md` (11-13), `reazioni-ricerca.md` (11-16), `ritorno-idle-ricerca.md` (11-16), `rivals2-animazione.md` (11-16), `rivals2-studio.md` (11-16)

**Pipeline animazione e skill (superati da regole 20-23/11)** (13): `agy-check-prompt.md` (11-15), `anim-finish-brief.md` (11-13), `anim-layer-skid-wallslide-piano.md` (11-16), `anim-pipeline-transizione-brief.md` (11-01), `anim-skill-revisore-esecutore.md` (11-15), `anim-skill-riferimenti.md` (11-15), `cp2-cp4-desktop-brief.md` (11-03), `look-v1-brief.md` (11-17), `m6-f15-followups-brief.md` (11-01), `motion-tools-1.md` (11-17), `organizzazione-thread-2026-11-02.md` (11-01), `schede-mosse-1.md` (11-15), `spin-drill-d4-brief.md` (11-01)

**Timeline, hitbox, BP** (8): `bp-exposure-audit-scan.py` (11-16), `bp-exposure-audit.md` (11-16), `bp-state-events-p1.md` (11-16), `hitbox-editor-ux-plan.md` (11-15), `move-timeline-decoupling.md` (11-16), `pushbox-plan.md` (11-15), `timeline-nodes-1.md` (11-16), `timeline-nodes-2.md` (11-16)

**Altro** (1): `riorganizzazione-cartelle-mappa.csv` (11-16)

**Da tenere** (vecchi ma ancora citati): `attacchi-stick-reverse-1.md` (citato da memoria/indice); `mappa-cartelle.md` (citato da memoria/indice); `riorganizzazione-cartelle-piano.md` (citato da ./content-riorganizzazione.md).

## E. Artefatti (11 pubblicati)
Letti uno per uno il 2026-11-23 (i 4 documenti Claude Docs del 24-31/10 solo da titolo e metadati: "da verificare"). Nessun artefatto modificato; le copie, se servono, vanno solo su `claude/artifacts`.
| Artefatto | Data | Contenuto reale | Copia / doppione | Vecchio o in contrasto con | Proposta |
|---|---|---|---|---|---|
| [Mosse di esempio GSTN](https://claude.ai/artifact/DyiBrsycoRfXNSMoZUBxun) | 16-11 | guida designer: 4 mosse esempio, campi, Combat Move, legenda asset. È la "DESIGNER GUIDE" della memoria: titolo e uso coincidono, ma è diventata più larga del titolo | `Artifacts/Designer/mosse-di-esempio.html` è del 14-11, senza Combat Move | cita ramo `integ-cp3`, "579 test", "non ancora compilato" | AGGIORNARE stato e copia; fondervi la Guida mosse Blueprint |
| [Architettura plugin combattimento](https://claude.ai/artifact/CLq2srEECQnd6bKDbBmm72) | 15-11 | moduli combat, flusso colpo/knockdown, passi designer, CP1-CP5 | `Artifacts/Combattimento/architettura-plugin.md/.html` (14-11) | rami `project-thread-*` vecchi; sezione rete "solo listen server" da allineare alla ricerca del 23-11 | AGGIORNARE |
| [Guida mosse Blueprint GSTN](https://claude.ai/artifact/La2oqPzCkPdcqL2kERy9D6) | 15-11 | passo passo Combat Move, validatore | solo su `claude/artifacts`; ripete Mosse di esempio §1-2 | "ramo fxjn45, da compilare", "CI 5.8/5.9" | FONDERE in Mosse di esempio |
| [Analisi regole di lavoro](https://claude.ai/artifact/1LEUDPK4VjC1bRgKj8qYqP) | 15-11 | 8 proposte sulle regole, approvate quella notte | testo approvato in `decisioni/istruzioni-progetto-2026-11-15.md` (project files) | superata dalle regole 20-11 e 23-11 | ARCHIVIO |
| [Silhouette multi-angolo e VFX](https://claude.ai/artifact/MySdFXExJmvrDviYWkatdb) | 15-11 | regole di review della silhouette, checklist VFX | `anim-theory/silhouette-multi-angolo-e-vfx.html` (project files); vicino a `docs/notes/vfx-theory/principi-vfx.md` | VFX in pausa; da confrontare con regola 10 | TENERE |
| [Cose da fare a mano](https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk) | 13-11 | passi manuali dell'owner: push di SHA su `feature/gas-npp-5.8`, prove PIE | `Artifacts/Progetto/cose-da-fare-a-mano.html` | push su feature oggi bloccati dalla guardia; percorsi e Rider vecchi; la memoria lo chiama ancora "MANUAL-TODO" | ARCHIVIO; un elenco nuovo solo se serve |
| [Networking di GASP Giustino](https://claude.ai/artifact/J6v4JZMLmDAMuemuXYxJhn) | 08-11 | rete spiegata al designer: predetto/interpolato, rollback, buffer | doppione di `docs/notes/networking-spiegato.md` | scrive "5.8"; non conosce la ricerca 1v1 del 23-11 | AGGIORNARE (5.9, rimando a `server-dedicato-ricerca.md`) |
| [Strumenti AI per animare le mosse](https://claude.ai/artifact/LsLBLG476QD5dZ8pcvsFtd) | 31-10 | confronto di strumenti (da verificare) | nessuna | scelta fatta: Cascadeur + animpipe + skill | ARCHIVIO |
| [Cascadeur per animare le mosse](https://claude.ai/artifact/E7KV71LLBD47zLF4HBAXzc) | 30-10 | uso di Cascadeur (da verificare) | in parte `docs/architecture/anim-pipeline.md` | probabilmente senza animpipe e Live Link | ARCHIVIO (vale anim-pipeline.md) |
| [Pipeline agentica per animare le mosse](https://claude.ai/artifact/V3EDNzjL6oQ56Y8Tos4nii) | 29-10 | pipeline di agenti (da verificare) | `docs/notes/anim-process.md` (SUPERSEDED 22-11), skill executor/reviewer | Antigravity senza crediti | ARCHIVIO |
| [Depth marker diegetico: opzioni](https://claude.ai/artifact/SXR2GcJHbrVTnR7p9SvE3d) | 24-10 | opzioni di design del depth marker (da verificare) | `docs/architecture/depth-marker.md`, `docs/notes/depth-marker-pseudocode.md` | scelta fatta e già nel plugin | ARCHIVIO |
Doppioni: i tre di fine ottobre sull'animazione si ripetono tra loro; Mosse di esempio, Architettura §10 e Guida mosse Blueprint ripetono la Combat Move.

## F. handoffs/INDICE.md
Riempito nei project files: una riga per ognuno dei 70 thread di `handoffs/costi.csv`, totale settimanale, stato del branch
(4 in main, 40 aperti, 11 non più sul remoto, 15 senza branch nella riga). Mancano i thread di oggi non ancora in `costi.csv`.
Copia della versione scritta in questo giro: `docs/notes/bibliotecario/handoffs-INDICE-2026-11-23.md`.
Nota: 19 righe di `costi.csv` hanno meno colonne del previsto (es. le righe `runstart-v3-*` del 20-11 e `anim-inventario-*` del 23-11); conviene una sola forma.
