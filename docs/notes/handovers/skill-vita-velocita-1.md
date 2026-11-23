# Skill animazione: misura della vita e della velocita' (opzione A, 2026-11-23, cloud, Sonnet->Opus 5.5, budget 4 $)
Branch claude/project-thread-wwnerm da btrk7b 7af96c4a + cherry-pick vjbryc 276709b1 (build spline + stop_and_go). Solo testo e script.
Riuso: animpipe/curves.py (stop_and_go, long_stills, soglie), motion_overlay, review_pack, capture_move; no_stop_gate di 6blrlz reso generico.
## Fatto
1. `gstn_anim_mcp/no_stop_gate.py` (CLI + funzione, matematica di curves.py, nessuna copia): ogni clip della run (startup, loop, settle,
   atterraggio) nel cancello di `anim_capture_move` (`no_stop`, sempre bloccante, dump mancante = blocco). Clip condivise con la base
   = "ereditato": rigiudicate e bloccano (K7 resta bloccata, si corregge in variante). Il settle K6 (~6 frame fermo) ora FALLISCE: segnalato.
   `animpipe check` `stop_and_go` blocca su ogni carta (anche "tangents"); `stop_and_go` non si eredita mai (NON_INHERITABLE).
2. `motion_overlay`: freccia per ogni frame, pallini rossi STOP su testa/bacino, striscia velocita' per frame sotto le scie (mani, piedi,
   testa, bacino, nero = corpo intero, tratteggio = soglia, banda rossa = STOP); parametri out, holds, stop_cm, still_run.
3. Ogni cattura scrive `slowmo_<seq>_az90.png` (startup 1 tick per cella, base sopra), `velocity_<variante>_az90/az180.png`, `pack.json`
   (ordine: slowmo, schede, TUTTE le griglie + mp4 slow4x). Con 1 sequenza = 6 immagini. Niente si taglia: oltre 8 -> `over_budget`.
4. Revisore: guarda prima slow4x mp4 (o il riquadro slowmo come campioni in sequenza), poi schede, poi griglie (project.md, cheap SKILL).
5. Rubrica: `carica_viva` ora blocca, nuova voce `statua` (bloccante, anche clip ereditate).
Test: `tests/test_life_gate.py` 11 nuovi; suite AnimPipeline 168 pass / 11 fail, tutti gia' rotti prima (file locali mancanti, pwsh);
5 test di capture_move rotti da btrk7b (cvars hitbox) riparati nel fixture. Comando: `python -m pytest -q tests/ --ignore=tests/test_casc_roundtrip.py`.
## Da sapere / aperto
- Regola owner 24/11 (niente compromessi sui falsi positivi) contraddice il punto 3 di btrk7b ("blocchi ereditati non bloccano") per gli
  altri blocchi (legibility, contact_faf, hitstop_snap): qui l'ho tolto solo per le fermate (ambito del brief). Serve decisione/thread.
- Prima cattura reale: servono i dump di TUTTE le clip del montage (startup, loop, settle) via clip_joints_dump_ue.py, mai provato in UE.
- Taratura: soglie 3,5 cm/frame e run > 3 passi mai provate su clip GASP/approvate in gioco; se un falso allarme, correggere curves.py.
- Copie skill in project files `anim-skills/*.SKILL.md` e skill account `gstn-anim-*` non aggiornate (sono copie: le aggiorna l'owner/coordinatore).
- `cards/.../k8/no_stop_gate.py` (6blrlz) superato dal generico; `docs/INDICE-PROGETTO.md` non su questo branch: voce "skill animazione" al merge.
SUBAGENTI: 0
COSTO: ~3,5 $ (stima) | 0 minuti desktop | 0 token review
