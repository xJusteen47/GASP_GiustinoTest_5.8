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

ESITO: PARZIALE build OK, 23/23 test, dati applicati (354c556c); prova in gioco non fatta
SUBAGENTI: 0
COSTO: ~6 $ | 25 min desktop | 0 token review
