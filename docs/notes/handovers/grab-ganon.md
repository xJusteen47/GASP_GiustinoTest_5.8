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
  conteggio tag 44 -> 47.

## Scelte all'owner (messaggio nel thread, applicate le consigliate 1A 2A 3A 4A 5A 6A)
1 mappa lanci (Cr.F. = back throw in avanti), 2 durata presa stile Smash, 3 input giro stick, 4 danni 5 % / 1,6 %, 5 ogni giro -20 frame, 6 lanci normali dopo lo spin.

## Prossimo (fisso, editor chiuso)
1. `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log`.
2. Test: `GSTNLink`, `GSTNSpinDrill`, `GSTNInput` (Check-TestLog.ps1). Se `FillAndSlots` sbaglia il verso dello script `@angolo`, va corretto il test, non lo slot.
3. Script headless: `setup_grab_ganon_playtest.py`; in `Saved/Logs/grab-ganon/setup.log` la riga `grab:` deve dire changed=True al primo giro (False = la
   presa del kit non ha hitbox Combat.HitType.Grab: guardare quale abilita' lega il kit).
4. PIE con KIT_Test_Playtest e due manichini: presa (B), giro dello stick -> il manichino gira in tondo e colpisce l'altro; due giri di fila; F. lancia;
   senza lanciare si libera prima di 90 + 1,7 x % frame. Rischio: se il grabber non gira, la SetFacing del timeline viene sovrascritta dall'orientamento
   di Mover (lo steering funziona, quindi non dovrebbe).
5. Rete: scenari link di `gasnpp_test.py` (rollback del grabber; nota neck-hold-desync).

## Prove
Nessuna: niente build ne' test (cloud). Script Python: sintassi controllata con ast.

SUBAGENTI: 3 (lettura web UFD, SmashWiki lanci, SmashWiki durata presa; web-fetch)
ESITO: PARZIALE codice e script scritti, non compilato ne' provato in gioco
COSTO: ~9 $ | 0 min desktop | 0 token review
