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

## Stato fisso (2026-11-26, verifica completata su `GASP_Giustino2_grabganon`)
- **Fix applicati (reversibili):**
  1. `Plugins/GSTNLink/Source/GSTNLinkCore/Private/Tests/GSTN_LinkTests_KitThrows.cpp`: corretto `TrackYaw` (`Two.Turns=2.f` su `48` frame -> `120.f` a frame `6`, e `Errors.Reset()` prima del secondo `Validate`).
  2. `Plugins/GSTNLink/Source/GSTNLinkCore/Private/Tests/GSTN_LinkTests_Tags.cpp`: aggiunti `Combat.HitType.GrabSpin` e `Combat.HitType.GrabSpin.Wall` alla lista dei subtree di `GSTNLink` in `NoForeignRootDeclared`.
  3. `Tools/GAS/setup_grab_ganon_playtest.py`: aggiunto `seed_link_bindings(kit)` idempotente che copia da `KIT_Test_Link` gli slot `CommandSet.AddedSlots` e i binding `Command.Link.Grab` / `Command.Link.Release` (`Role.Grabber`) se mancanti su `KIT_Test_Playtest` (altrimenti `write_grab` e `ApplyKitThrows` saltavano tutto).
  4. `Plugins/GSTNLink/Source/GSTNLinkNP/Private/Harness/GSTN_LinkTestAbilities.cpp` e `GSTN_LinkContentLibrary.cpp`: aggiunti `Throw_Neutral`, `Throw_CrouchForward`, `Release_PutDown`, `Spin_Clockwise`, `Spin_CounterClockwise` alla `CancelWindow` `OnHit` di `UGSTN_GA_LinkTestGrab` (e propagati in `WriteGrabMove`) per consentire lo spin e i lanci `N.`/`Cr.F.` durante i 38 frame della presa.
- **Note residue:**
  - `write_throws` in `setup_grab_ganon_playtest.py` rilancia `WriteThrowMove` + `WriteThrowKnockback` + `WriteThrowStickTurn` e `SetKitThrows` + `SetKitThrow`, quindi al rerun segna `changed=True` sui lanci mentre `grab:` e `mash rule` risultano `changed=False` (asset salvati coerenti e determinati).
  - La rete `SUITE_LINK` (`link-listen-0pct.log`) mostra `PASS` su `link_spike_client_grabs_client` (18/18), `link_rollback_client_grabs_host` (6/6), `link_rollback_client_grabs_client` (6/6), `grab_throw` (12/12), `grab_interact` (9/9), `grab_gate` (12/12), `throw_at_enemy` (7/7), e nessun `neck-hold-desync` su `link_spike_client_grabs_host` (solo 1 picco su observer `world 2`: `max 99.4 cm`, p95 `17.1 cm`). Restano pre-esistenti/noti i fail su `link_spike_host_grabs_client` (4 rb, p95 `26.1/32.3`), `link_rollback_host_grabs_client` e 1 check su `grab_mash` (`{0: [549], 1: [555], 2: [549]}`).

## Prove (fisso 2026-11-26)
- **Build:** `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build-grab-cancel.log` -> `ESITO: OK` (build iniziale `build.log`: `197s`, testfix `build-testfix.log`: `30s`, cancel fix `build-grab-cancel.log`: `16s`, `REGRESS: OK 8 locked versions, 13 files unchanged`).
- **Test automatici UE (`Check-TestLog.ps1`):**
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-link-final.log -Tests GSTNLink` + `Check-TestLog.ps1 -MinTests 100` -> `ESITO TEST: PASS ok=132 fail=0 totale=132` (`AllRegisteredAndDocumented` = 48 tag, `FillAndSlots`, `TrackYaw`, `NoForeignRootDeclared`, `ThrowStickTurn` tutti verdi).
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-spindrill.log -Tests GSTNSpinDrill` + `Check-TestLog.ps1 -MinTests 30` -> `ESITO TEST: PASS ok=32 fail=0 totale=32`.
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-input.log -Tests GSTNInput` + `Check-TestLog.ps1 -MinTests 100` -> `ESITO TEST: PASS ok=139 fail=0 totale=139`.
- **Setup headless (`setup_grab_ganon_playtest.py`):** `Saved/Logs/grab-ganon/setup.log` -> `seeded command_set slot Command.Link.Grab/Release`, `seeded kit_binding Command.Link.Grab/Release`, `grab: GA_Ganon_Playtest_Grab_C ... changed=True`, `saved 7: GA_Ganon_Playtest_Throw{Forward,Up,Down,Neutral,CrouchForward}, GA_Ganon_Playtest_Grab, KIT_Test_Playtest`.
- **PIE 2 manichini (`L_Test_Hub`, `KIT_Test_Playtest`, `Saved/Logs/grab-ganon/pie_verify.txt`, `Saved/Logs/ci/pie-grab-ganon.log`):** `OVERALL: PASS` (`ESITO: OK`):
  1. Spin: `total_sweep=722.0deg` (2 giri in 48f @ 15 deg/f + settle), `d0` pummel `50->53%` (`+3%`) e costo `-20f` (`1023->1003`), `d1` colpito `0->8%` (`+8%`, spostamento `324.2cm`, `Combat.HitType.GrabSpin`).
  2. Muro (`Z1_Wall`, `ECC_WorldStatic`): `d0` `100->107%` (`+4%` `Combat.HitType.GrabSpin.Wall` + `+3%` pummel), hitstop `host:4f`, `d0:8f`.
  3. Lancio `F.` stick indietro: `turn_frames=8` (`0->22.5->...->180.0deg`), `dmg=0->13%`, lancio `dx=-139.0cm`.
  4. Lancio `N.` stick neutro: `yaw 0.0->0.0`, `dmg=0->13%`, `dz=658.2cm`.
  5. Durata presa senza lancio (`ReducePerPress=0`): `0%` = `90f` (mash non accorcia, fuga automatica al frame `533` con `reason=8`), `100%` = `260f` (`90 + 1.7*100`, mash non accorcia).
- **Rete (`Saved/Logs/grab-ganon/link-listen-0pct.log`, `Saved/Logs/ci/link_net.log`):** `7/11 PASS` (`link_spike_client_grabs_client`, `link_rollback_client_grabs_{host,client}`, `grab_throw`, `grab_interact`, `grab_gate`, `throw_at_enemy`).

BRANCH: claude/project-thread-h46ltf
SUBAGENTI: 3 (cloud: web UFD, SmashWiki lanci, SmashWiki durata presa) + worker desktop Antigravity (build, test, setup, PIE 2 manichini, rete)
ESITO: PASS build, test GSTNLink (132/132)/GSTNSpinDrill (32/32)/GSTNInput (139/139), setup asset, 5/5 criteri PIE (2 manichini) e 7/11 scenari rete verificati su claude/project-thread-h46ltf
COSTO: ~13 $ | 28 min desktop | 0 token review
