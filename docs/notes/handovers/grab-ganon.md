# Handover grab-ganon (2026-11-25, cloud, Opus) - presa e lanci di Ganondorf sul kit di playtest, spin con il nemico in mano
Branch `claude/project-thread-h46ltf` da main 152d25e6. NON COMPILATO (cloud senza UE, fisso spento): la regola 1 vuole build e prova sul fisso in questo thread.
OBIETTIVO (owner 10:05Z): numeri di presa/pummel/lanci di Ganon (Ultimate Frame Data) su KIT_Test_Playtest; con un nemico in mano, girando lo stick
lo si usa come arma (danno ai vicini e a lui) e poi lo si lancia; il nemico si libera da solo dopo un po'.

## Fatto
- Riusato: GSTNLink (link, mash, watchdog, lanci, presa al collo: il preso sta nello spazio radice del grabber), condizione Spin di GSTNInput (come
  GSTNSpinDrill), richiesta SetFacing del combat core (come lo steering), modello knockback Smash (BKB x 22,5).
- Spin: tag `Command.Link.Spin.{Clockwise,CounterClockwise}`, `Combat.HitType.GrabSpin`; traccia `FGSTN_GrabSpinTrack` (Core, yaw puro dal frame);
  `FGSTN_LinkTickExtension::StepGrabSpin` (AfterExecute, richiesta SetFacing); abilita' `UGSTN_GA_LinkGrabSpin{Clockwise,CounterClockwise}` (un giro 24 f,
  sfera davanti 5 % ai vicini, pummel 1,6 % al preso che costa 20 frame di presa, catena e lanci dal frame 12); righe aggiunte dal kit throw fill
  (`bGrabSpin`, cvar `gstn.Grab.Spin`, `FallbackThrows.Spin*` in DefaultGSTNLink.ini) su ogni kit che prende.
- Costo presa: `FGSTN_LinkHitData::GrabTimeCostFrames` -> `FGSTN_LinkGrabTimeOutcome` + applier (sposta l'EndFrame del mash, mai prima del frame).
- Contenuti: `UGSTN_LinkContentLibrary::{SetKitThrow, SetKitGrabMashRule, WriteGrabMove, WriteGrabSpinMove}`; script `Tools/GAS/setup_grab_ganon_playtest.py`
  (lanci GA_Ganon_Playtest_Throw*, presa GA_Ganon_Playtest_Grab, mash 90 + 1,7/%, min 19, max 600, 8 per input); `setup_knockback_smash.py` salta GA_Ganon_*.
- Fonti e conversione: `docs/notes/grab-ganon-dati.md`. Test nuovi `GSTNLink.Core.GrabSpin.{TrackYaw,FillAndSlots}`, `GSTNLink.NP.Grab.SpinTurnCostsGrabTime`;
  conteggio tag 44 -> 48.

## Risposte dell'owner (10:21Z) applicate
1. Notation: F. = lancio avanti di Ganon, Cr. = lancio su, Cr.F. = lancio giu', niente lancio indietro (N. non nominato: resta il su).
2. Niente mash (`ReducePerPress` 0 sul kit), la durata 90 + 1,7 x % resta.
3. Un giro di stick = due giri (48 f), poi 12 f di assestamento solo estetico: coda cancellabile (`Timeline.Tail`) e la capsula sbanda di 15 gradi e torna
   (`FGSTN_GrabSpinTrack::TurnEndFrame`, `SettleOvershootDeg`). Lanci dal f12, catena del giro successivo dal f36. Lo spin ha `AimPolicy KeepFacing`
   (prima lo scatto all'avvio verso lo stick, che sta girando, avrebbe fatto saltare il nemico).
4. 8 % ai vicini, 3 % al tenuto. Muro: hitbox `Combat.HitType.GrabSpin.Wall` sul tenuto (gruppo 2, 4 %, hitstop 8 / 4 sul grabber), mascherata a
   BeforeCandidates da `FGSTN_LinkTickExtension::GetGrabSpinWallMask` tranne quando una sfera dal grabber al punto di presa tocca `ECC_WorldStatic`.
   Gruppi: anello 0, tenuto 1, muro 2. "Dipende dal nemico": per ora un valore per mossa.
5. -20 frame di presa per mossa, regolabile nello spec.
6. Lanci verso lo stick con girata: `UGSTN_GA_LinkThrow::ApplyStickTurn` (steering MoveStick, cono 180, 1406 gradi/s = 180 in 8 f, fino al rilascio),
   scritto dallo script con `WriteThrowStickTurn`. Stick fermo (punto aperto, scelta mia consigliata, reversibile): resta la direzione attuale = dove
   sta il nemico.

## Stato fisso (2026-11-25 12:00Z, pausa dell'owner fino a stasera)
Worktree `GASP_Giustino2_grabganon` su e1366071 (hook installati, Content in junction verso _ue59). Build interrotta a 246/537 con 0 errori di
compilazione (ESITO FAIL solo per l'interruzione): stasera riparte incrementale da li'. Test, script e rete non ancora fatti. Nessun commit dal fisso.

## Prossimo (fisso, editor chiuso)
1. `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log`.
2. Test: `GSTNLink` (nuovo `GSTNLink.NP.Grab.ThrowStickTurn`, conteggio tag 48), `GSTNSpinDrill`, `GSTNInput` (Check-TestLog.ps1). Se `FillAndSlots` sbaglia il verso dello script `@angolo`, va corretto il test, non lo slot.
3. Script headless: `setup_grab_ganon_playtest.py`; in `Saved/Logs/grab-ganon/setup.log` la riga `grab:` deve dire changed=True al primo giro (False = la
   presa del kit non ha hitbox Combat.HitType.Grab: guardare quale abilita' lega il kit).
4. PIE con KIT_Test_Playtest e due manichini: presa, giro dello stick -> due giri, il manichino colpisce l'altro; contro un muro il tenuto prende il
   colpo con hitstop; F. con lo stick indietro gira in 8 f e lancia; stick fermo lancia dove sta il nemico; senza lanciare si libera a 90 + 1,7 x %.
   Rischio muro: la sfera parte dal centro del grabber (MoveOrigin): se e' gia' dentro un muro conta sempre, se il tenuto e' piu' alto/basso va spostata. Rischio: se il grabber non gira, la SetFacing del timeline viene sovrascritta dall'orientamento
   di Mover (lo steering funziona, quindi non dovrebbe).
5. Rete: scenari link di `gasnpp_test.py` (rollback del grabber; nota neck-hold-desync).

## Prove
Nessuna: niente build ne' test (cloud). Script Python: sintassi controllata con ast.

SUBAGENTI: 3 (lettura web UFD, SmashWiki lanci, SmashWiki durata presa; web-fetch)
ESITO: PARZIALE codice e script scritti, non compilato ne' provato in gioco
COSTO: ~13 $ | 0 min desktop | 0 token review
