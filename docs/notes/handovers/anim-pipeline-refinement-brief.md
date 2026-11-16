# Brief: rifinitura pipeline animazione, punti senza editor (2026-11-02, owner approva PR #24)

Piano completo: `docs/notes/reviews/anim-pipeline-refinement-plan-2026-11-02.md` (leggere §4, §5, §6, §7).
Codice: `Tools/AnimPipeline/animpipe/` (`card.py`, `checks.py`, `__main__.py`, `poses.py`, `author.py`), `legibility.py`,
`casc_to_author.py`. Regole owner: `Tools/AnimPipeline/cards/README.md`. Nessun editor Unreal disponibile: tutto va
provato con dati sintetici (come già fa `checks.py`) o sui dati delle schede. Output degli script: file + max 5 righe.

## Lotto A (Sonnet): lock delle pose approvate, lint, approve, diff, regole README
1. `card.py`: blocco `approved` = lista di `{frames, parts, ref (scheda di riferimento, path relativo a cards/), by, date, why}`.
   Parti: `arm_l arm_r leg_l leg_r trunk head hand_l hand_r` mappate sui campi del blocco `author.poses` (arm_x, leg_x,
   pelvis+spine, head, fingers/wrist_out/hand_rot) e sui frammenti di controllo del rig (`PART_CTRLS` in `ue_rig.py`:
   aggiungere arm/leg/hand, es. `arm_l: ("upperarm_l", "lowerarm_l", "hand_l")`; ik `foot_l_ik`, `hand_l_ik`).
2. Guardia 1 in `load()` (o `animpipe card`): per ogni voce `approved`, le pose `author` della scheda ai frame bloccati, per
   le parti bloccate, devono coincidere con quelle della scheda `ref` entro 0,5 cm / 1°. Differenza -> `ValueError` con
   frame, parte, campo. `animpipe build/check/review` chiamano `load()` quindi si fermano da soli. `--unlock part@frame`
   in `__main__` salta la guardia per quella coppia e stampa una riga; la voce va comunque tolta dalla scheda con un `why`.
3. `animpipe approve <card> --frames=13,14 --parts=leg_l,leg_r --why="..."`: aggiunge la voce `approved` con `ref` = la
   scheda stessa e scrive `cards/<kit>/approved/<move>.json` con i valori `author` di quei frame/parti (snapshot) e, se
   esiste `Saved/.../<move>_build_ue.json` con le chiavi, anche le chiavi del rig (campo opzionale, non obbligatorio ora).
4. `animpipe diff <card> [--vs=<card>]` (default: la versione precedente nella stessa cartella, o `ref` degli approved):
   per frame x parte lo spostamento massimo delle pose `author` (cm per hand/foot/pole/loc, gradi per rot/spine/head),
   stampa ≤5 righe ("cambiato: arm_l 3,5,12; intatto (lock): leg_l 13-14"), file completo in `Saved/.../<move>_diff.json`
   (o `Tools/AnimPipeline/sheets/` se Saved non esiste). Se i dump ossa `sheets/raw/<take>_legbones.json` esistono per
   entrambe le schede, aggiungere la stessa tabella sulle ossa. Nessuna immagine ora.
5. Prova obbligatoria: scheda v4 attuale con lock `leg_l,leg_r @13,14` ref = se stessa -> PASS; copia con un polo del
   ginocchio spostato di 3 cm -> FAIL con messaggio chiaro. Lo script di prova va in `Tools/AnimPipeline/tests/test_locks.py`
   (puro Python, nessun editor) e deve girare con `python -m pytest` o `python tests/test_locks.py`.
6. Regole README come check in `checks.py` (blocchi dichiarati nella scheda sotto `rules`, assenti = non eseguiti, soglie
   nel blocco): `frame_0` (ogni parte al frame 0 dista dall'idle ≥ `min_cm` e ≤ `max_frac` del tragitto verso il frame
   `charge`), `knees_front` (frames: polo/ginocchio con y ≥ y bacino - `tol`), `feet_side_by_side` (frames: separazione
   laterale caviglie ≥ `min_cm`, nessun incrocio), `villain_hand` (frames: angolo polso-avambraccio ≥ `min_deg` sulle ossa).
   Funzionano sui dump ossa (`_bones`) come gli altri blocchi; prova sintetica per ognuno in `tests/test_rules.py`.
   `timing_source`: in `animpipe card` e `diff` stampare "timing: owner" se `anim_active` c'è, altrimenti "timing: CSV (avviso)".
   Le soglie restano da tarare sul desktop: scriverlo nel docstring.
7. Non toccare `ue_rig.py` oltre `PART_CTRLS` (il thread desktop ci sta lavorando). Non cambiare le schede esistenti
   salvo aggiungere `approved` alla dair v4 (lock gambe 13-14, ref se stessa, why "owner 02/11: ginocchia a rana giuste").
8. README delle schede: paragrafo "Disciplina di giro" (§8 del piano) e la riga per `approved`, `rules`, `diff`.

## Lotto B (Opus medium): legibility per mosse non da pugno, proporzioni Cascadeur -> UE (solo codice, verifica sul desktop dopo)
1. `legibility.py`: blocco `strike` della scheda (`limb`: arm_l|arm_r|leg_l|leg_r|legs, `hiders`: parti che possono coprire,
   `pose_checks`: lista tra `elbow_behind`, `closed_fist`, `rear_arm_cover`, default = tutti per i pugni, nessuno dei tre
   per i calci). Il dair (`strike_limb: leg_l`) non deve più fallire per controlli da pugno; il jab v12 deve dare lo stesso
   esito di oggi. `checks.py` passa il blocco. Senza editor: prova su dati sintetici o sui dump in `sheets/raw/` se presenti.
2. `casc_to_author.py`: lunghezze dei segmenti dei due rig (Cascy vs manichino UE: clavicola, braccio, avambraccio, mano,
   collo+testa, coscia, gamba) lette dai dati già salvati (`sheets/raw/*_frame0.json`, `idle_neutral_legbones.json`) e dal
   file pose Cascadeur; target IK di mani e piedi riscalati sulla catena UE mantenendo la direzione dalla spalla/anca;
   opzione `--no-rescale` per il comportamento attuale; stampa per segmento il rapporto applicato.
3. Pollice villain in `author.py`: chiave `thumb` nel blocco (default per `fingers`=1: disteso lungo l'indice; `"open"`
   per il pollice aperto), un solo punto da cambiare. Non toccare le schede.

## Per entrambi
- Massimo 2 tentativi per problema, poi scrivere cosa manca. Niente loop. Commit su branch `claude/project-thread-sysuzj`
  con messaggi chiari, senza identificativi di modello. Report finale ≤15 righe: file toccati, prove eseguite, cosa resta
  da verificare sul desktop, costo stimato.
