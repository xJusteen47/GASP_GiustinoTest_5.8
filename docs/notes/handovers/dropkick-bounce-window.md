# Dropkick: tech bounce troppo presto (2026-11-25, cloud, Opus)
Branch `claude/project-thread-pkioys` da `main` (152d25e6). NON compilato, NON provato in editor (fisso e portatile offline).
Riuso: Body Capsule track e modificatore, `ReadEvaluatedTimeline`, `FGSTN_TumbleLanding::SelfKnockdown`, `UGSTN_DefenceContentLibrary`.

## Causa (lettura codice)
1. `UFallingMode::SimulationTick` (fork Mover, invariato): con velocità verticale di partenza ESATTAMENTE zero e mossa verso il basso cerca
   il pavimento fino a `FloorSweepDistance` (40 cm) e atterra subito, senza contatto. Dopo ogni hitstop la velocità è zero (freeze
   OverrideAll): un dropkick che colpisce vicino al suolo atterrava al primo frame dopo l'hitstop, fino a 40 cm sopra il pavimento.
2. La finestra `Combat.Move.KnockdownOnLanding` copriva tutta la mossa [0, total) e si armava con UN frame in aria: un decollo che sfarfalla
   o un saltello di pochi cm = bounce.
3. Capsula del dropkick (non cambiata): in aria hh 35, raggio del pawn (30 sui pawn di test, quindi non una sfera esatta), centrata in
   orizzontale sul torso. Pin `head` (default dal 23/11): fondo ~1 m sopra la radice della mesh; pin `torso`: fondo ~20 cm sotto la schiena.
   Quale sia nell'asset: `Saved/AnimR2/setup_dropkick_body_capsule.txt` sul fisso.

## Fix (owner: G + aggancio solo con hitbox attiva, estendendo la classe, niente modifiche al fork)
- `UGSTN_FallingMode` (GSTNCombatNP, figlio di `UFallingMode`): con un'hitbox d'attacco attiva nel frame, una velocità verticale iniziale
  esattamente zero arriva al padre come 0,01 cm/s verso il basso, così l'aggancio non scatta. Copia dei parametri, sync state intatto.
  `Tools/GAS/setup_gstn_falling_mode.py` riassegna il padre di `BP_MovementMode_Falling` (GASP, condiviso: backup in Saved/Backups/GAS;
  `GSTN_FALLING_UNDO=1` torna allo stock). gasp-divergenze riga 28.
- F: `FGSTN_GroundBounceRules::SelfKnockdownMinAirFrames` (default 6, 1..31): servono N frame in aria nella finestra per armare; un
  atterraggio prima è normale e il conteggio riparte. Conteggio nei bit 3-7 di `HitFlags` del fragment (nessun cambio del formato in rete).
- E: `UGSTN_DefenceContentLibrary::OpenSelfKnockdownWindowAtFirstHitbox` + `Tools/GAS/setup_dropkick_bounce_window.py`: ogni timeline
  con la finestra la apre al primo frame dell'hitbox d'attacco (fine invariata). Trova da solo TL_Overlord_Dropkick, TB, R*, K5, K6.
- Test: `GSTNCombat.NP.FallingMode.*`, `GSTNDefence.NP.Landing.SelfKnockdownNeedsMinAirFrames`, `.SelfKnockdownWindowOpensAtFirstHitbox`
  (i test vecchi della finestra usano 1 frame, come prima).

## Da fare sul fisso (dopo build)
1. Build (`Run-Timed.ps1 -Preset Build`), poi `GSTNCombat.NP.FallingMode`, `GSTNDefence.NP.Landing` tutti verdi.
2. Editor: `setup_gstn_falling_mode.run()`, poi `setup_dropkick_bounce_window.run()` (prima con `BOUNCE_WINDOW_DRY=1` per vedere la lista).
3. PIE: dropkick che colpisce un bersaglio vicino al suolo, dropkick a vuoto, dropkick dal suolo piatto. Log `SelfKnockdown:` (armed dopo
   N frame, landed) e `show collision`. Atteso: niente bounce alla fine dell'hitstop finché non c'è contatto vero, niente bounce nello startup.
4. Giro rapido delle altre aerial (dair, fair) e del walk-off da un gradino (deve restare a terra come prima: senza hitbox è lo stock).

## Fisso, 2026-11-26 00:11-00:27 UTC (_ue59)
- Build OK (624 s), test 23/23 PASS (FallingMode, Landing, BodyCapsule). Nessuna correzione al codice.
- Script: finestra KnockdownOnLanding aperta al frame 15 su 9 timeline (Dropkick, TB, TBK5-K8, TBR2-R4); BP_MovementMode_Falling
  reparentato su GSTN_FallingMode (backup Saved/Backups/GAS). Commit 354c556c (BP + 9 timeline).
- Backup asset residui di _ue59: claude/backup-ue59-2026-11-26 (4b97e9fa, i due greybox). CHT_GSTN_StockMover_PoseMatch bloccato dalla
  guardia come file locale dell'owner: lasciato modificato su _ue59. L'owner (25/11 11:40) dice che non è suo: al prossimo turno su _ue59
  salvarlo su claude/backup-ue59-2026-11-26 (la guardia va capita, non aggirata), non scartarlo. I binari di _ue59 ora sono di questo branch: il prossimo thread ricompila.
- Saved/AnimR2/setup_dropkick_body_capsule.txt assente ovunque: AIR_PIN dell'asset da verificare.
- PIE NON fatto (lancio di pie_capture_move.py negato dai permessi della sessione): restano dropkick colpito vicino al suolo, a vuoto,
  dal suolo, discesa da un gradino, log "SelfKnockdown:".

## Owner 2026-11-26: aggancio mai in aria tranne il primo frame
- "Con super armor e hitstop, o vento dal basso, snappiamo comunque?" Sì: lo snap scatta a ogni velocità verticale esattamente zero in
  aria. Scelta A: `UGSTN_FallingMode::FallStartSnap = FirstAirFrameOnly` (default): snap solo se il frame prima finiva a terra
  (`Mover.IsOnGround`, come lo legge il modificatore della capsula). Restano `NotWhileHitboxActive` e `Stock` come opzioni. Test
  `GSTNCombat.NP.FallingMode.SnapPolicy`. Da ricompilare e riprovare sul fisso (il BP Falling non va rilanciato: il default sta nella classe).

## Owner 2026-11-26: "A, B"
- A (codice, non compilato): il rimbalzo del self knockdown (dropkick) ha l'altezza dalla velocità di caduta reale invece di
  `SelfKnockdownImpactSpeed` fisso (900). Ogni frame in aria nella finestra scrive in `FGSTN_DefenceFragment::SelfFallSpeedQ` (gruppo nuovo
  del fragment, bit 12 della maschera, 2 byte) la velocità verso il basso di fine frame precedente (voce dello step snapshot); all'atterraggio
  si usa quella (il frame d'atterraggio ha già la verticale azzerata da Mover). Regola `GroundBounce.bSelfKnockdownUseFallSpeed` (default on);
  spenta o senza caduta registrata = 900 come prima. Il momento orizzontale resta zero (nessun cambio di feel non chiesto). Log
  `SelfKnockdown: ... (fall N cm/s, bounce judged at M)`. Test `GSTNDefence.NP.Landing.SelfKnockdownRecordsFallSpeed`, Fragment 2 + 95 byte.
- B (dati, da lanciare in editor): `Tools/GAS/setup_aerial_body_capsules.py` generalizza `setup_dair_body_capsule.py` a tutte le aerial
  (timeline con Hold Until Landed o Landing, con montage, senza Body Capsule: dropkick e dair v9 restano i loro). Fit puro in
  `Tools/GAS/body_capsule_fit.py` (test `python Tools/GAS/tests/test_body_capsule_fit.py`, verde qui). Rispetta setup_guard.
  Sul fisso: prima `AERIAL_CAPSULE_DRY=1`, guardare `Saved/AnimR2/setup_aerial_body_capsules.txt`, poi `run()`; commit TL + ledger.

ESITO: PARZIALE build e 23/23 test OK su 354c556c; regola dello snap (26/11), rimbalzo dalla caduta (A) e script capsule aerial (B) non compilati né lanciati; prova in gioco non fatta
SUBAGENTI: 0
COSTO: ~8 $ | 25 min desktop | 0 token review

## Fisso, 2026-11-27 (thread M7, worktree GASP_Giustino2_3dyy2t, detached su e1c7b284)
- Build OK (23 azioni: Defence Core/GAS/NP, CombatNP, CombatGAS). Test 24/24 PASS (`GSTNCombat.NP.FallingMode`, `GSTNDefence.NP.Landing`,
  `GSTNDefence.NP.Fragment`), tra cui `SnapPolicy`, `SelfKnockdownRecordsFallSpeed`, `RoundTripIdleOneBit` (Saved/Logs/ci/test.log.ue.log).
- B (capsule aerial): a secco con ROOT `/Game` e `/`: 0 timeline da fare. Le aerial con atterraggio di questo branch (Dropkick, TB*, DAirV10*)
  hanno già la loro Body Capsule. Le aerial nuove dell'Overlord (M1, branch sznhpw) andranno coperte dopo il loro merge.
  In commandlet (-run=pythonscript) lo script trova le timeline solo dopo `search_all_assets(True)`; in editor aperto non serve.
- PIE (`pie_capture_move.py`, KIT_Test_OverlordDropkick, L_Test_Hub, host + 2 client; log `SelfKnockdown:` del ue.log):
  - press a 79 cm in caduta (low_hit / low_whiff) e a 367 cm (sweet_hit / sweet_whiff): "grounded after 1-2 airborne frames: not armed,
    ordinary landing". Nessun rimbalzo con meno di 6 frame in aria (regola F).
  - press a 520 e 650 cm: "armed (6 frames)" poi "landed inside the window: downed (fall 1500 cm/s, bounce judged at 1500)". Il rimbalzo
    usa la caduta vera (A).
  - press in salita a ~250 cm (whiff / hit, apice ~10 m): armato, poi "disarmed, window closed" prima di toccare terra: atterraggio normale.
  - colpo vicino al suolo con hitstop NON riprodotto: il kit salta ~9 m e cade a ~24 cm/frame; con il manichino a 150-327 cm nessuna
    delle 6 prove l'ha colpito (2 tentativi di taratura, poi fermato). Serve un caso dedicato (manichino sotto la traiettoria).
  - discesa laterale dal gradino di 45 cm (Z5, sequenza solo nel boot, non nel repo): ~10 frame di caduta e atterraggio normale, nessuno
    snap a mezz'aria. Il gradino di 30 cm si sale e basta: in avanti i gradini dell'hub sono attaccati.
- Dati: Saved/M7/ (frames.jsonl e png per sequenza) nel worktree 3dyy2t.

ESITO: PARZIALE build, 24/24 test e PIE OK per regola F, rimbalzo A e gradino; colpo vicino al suolo con hitstop non riprodotto; B niente da fare
SUBAGENTI: 0
COSTO: ~6 $ | ~20 min desktop | 0
