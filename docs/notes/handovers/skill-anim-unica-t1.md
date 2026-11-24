# Handover T1 skill di animazione unica (2026-11-24, cloud, Opus medium; brief handoffs/skill-anim-unica-brief-2026-11-24.md)
Branch `claude/anim-skill-unica` (da 7zhryd af9ccde2). Owner 1A 2A 3A applicate. Nessun editor, nessuna clip toccata.
FATTO
- Merge sgky92 (con mkedi4, 1uvcac, vjbryc), 3ufz10, jlb6i3, bazqb1, runstart-generic-desktop 230ad65a. Conflitti risolti:
  card.py (holds_without_reason accetta anche `why`), checks.py e anim-pipeline.md (versione life.py), audit_stop_table.py
  (versione sgky92, 0,4 cm), capture_move.py (cattura veloce + pacchetto livello 2; `final_round=True` forza la catena
  completa: il livello 2 non e' mai a finestra), i due SKILL.md cheap, costi.csv/INDICE.md (unione).
- life.py unica fonte: animpipe check, no_stop_gate, cancello di cattura la chiamano. 1A: `hold_reason` (blocca sempre),
  `hold_alive` (tenuta morta = metrica), `hold_guides` (2 tenute / 6 frame = misura). 2A: `life_gate.json` elenca le
  metriche con prova; lo scrive solo `life_calibrate.py --write-gate` su giro completo (passa tutte le PASS, boccia almeno
  una FAIL). Oggi vuoto: ogni metrica di vita e' misura `WOULD FAIL ...`. Fisici (15b, conservazione) restano bloccanti.
- Check di fase braccio-gamba scritto (runstart_pivot_gates.py `phase_arm_leg`, misura): w2 r=0,63, w3 r=0,67, braccia
  scambiate r<=0 (`python tests/test_phase_check.py`).
- Skill `gstn-anim-cycle` (sostituisce cheap): AUTORE (spline) e POLISH (strati, mai chiavi nuove), giro w3, tre sguardi,
  controlli in tre classi, profili standard/cheap/Antigravity, review finale uguale per tutti. INV-10 e INV-12 aggiornati.
PROVE (cloud, `bash runall.sh` = ogni `python tests/test_*.py`, 29 file): 20 OK, 9 FAIL tutti preesistenti:
  7 identici sul 7zhryd di partenza (carte cards/overlord/overlord_dair_v3|v4.json mancanti, casc, ps1) e test_verbs +
  test_vframes che falliscono gia' su sgky92 da solo ("2 cards found"). `python life_calibrate.py`: CALIB: FAIL 9/13
  (mancano dump K7, K8r2, K6 settle; dair_approved ERROR: carta ref overlord/overlord_dair_v4.json non trovata).
  Gate provvisorio sui 9 presenti: still_runs, stop_sync (non scritto: giro incompleto).
PER T3 (fisso): dump K7/K8r2/K6 settle (clip_joints_dump_ue.py), sistemare il ref della carta dair, poi
  `python life_calibrate.py --write-gate` e committare life_gate.json; prova di taratura della fase su un clip in fase vero.
  Reinstallare la skill: togliere gstn-anim-cycle-cheap (Antigravity sul fisso) e la vecchia gstn-move-animation dell'account.
APERTO: test_verbs/test_vframes (sgky92), jab v12 approvata ma FAIL vero (owner).
SUBAGENTI: 0
COSTO: ~5,5 $ | 0 minuti desktop | 0 token review
