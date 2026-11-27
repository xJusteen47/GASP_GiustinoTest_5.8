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
  5. **Fix `C5c`/`C5d` (conflitto `Command.Movement.Spin.*` vs `Command.Link.Spin.*` post-timeline `Grab`, zero nuove dipendenze tra plugin pari, zero modifiche a `.Build.cs`/`.uplugin`/`.uasset`):**
     - **Opzione A** (`Plugins/GSTNSpinDrill/Source/GSTNSpinDrillGAS/Private/GSTN_SpinDrillLibrary.cpp`, `MakeBinding`): risolve `Combat.Link.Role.Grabber` via `FGameplayTag::RequestGameplayTag(..., false)` e lo aggiunge a `Binding.Requirements.IgnoreTags` (impedendo a `MakeKeys` 270° di rubare lo stick al termine dei 38f di `GA_Ganon_Playtest_Grab` prima che `Command.Link.Spin.*` raggiunga i 360°).
     - **Opzione C** (`Plugins/GSTNLink/Source/GSTNLinkNP/Private/GSTNLinkNPModule.cpp`): registra `FGSTN_LoadoutExtension` `"GSTNLinkSpinSuppression"` con `Priority = 100` (strettamente dopo `"GSTNSpinDrill"` a `Priority = 0`), speculare a `UGSTN_HeldSlots::ApplySizeBans`, aggiungendo idempotentemente `GSTNLinkTags::Role_Grabber` a `Requirements.IgnoreTags` per ogni binding `Command.Movement.Spin*` (kit e armi) che non lo richieda esplicitamente.
  6. **Test automatici aggiunti:**
     - `Plugins/GSTNSpinDrill/Source/GSTNSpinDrillGAS/Private/Tests/GSTN_SpinDrillGASTests.cpp`: `GSTNSpinDrill.GAS.Spin.DefaultBindingsAndSlots` verifica `Combat.Link.Role.Grabber` in `IgnoreTags`; nuovo `GSTNSpinDrill.GAS.Spin.GrabberBlocksMovementSpin` verifica su rotazione stick CCW 8 step (`0..8`) che con `Role_Grabber` a `k=6` (270°) `Movement.Spin.CCW` non scatti né consumi la stick history e a `k=8` (360°) vinca `Command.Link.Spin.CounterClockwise`, mentre senza `Role_Grabber` `Movement.Spin.*` (`MakeKeys`/stick e `A-S-D`/`D-S-A`) resti invariato.
     - `Plugins/GSTNLink/Source/GSTNLinkNP/Private/Tests/GSTN_LinkNPTests_Loadout.cpp`: nuovo `GSTNLink.NP.Loadout.SpinSuppression` verifica l'ordine di `GSTNLinkSpinSuppression` (`Priority = 100`), l'aggiunta idempotente di `Role_Grabber` a `IgnoreTags` sui soli binding `Command.Movement.Spin.*` (kit e armi) senza toccare gli altri binding.
- **Note residue:**
  - `write_throws` in `setup_grab_ganon_playtest.py` rilancia `WriteThrowMove` + `WriteThrowKnockback` + `WriteThrowStickTurn` e `SetKitThrows` + `SetKitThrow`, quindi al rerun segna `changed=True` sui lanci mentre `grab:` e `mash rule` risultano `changed=False` (asset salvati coerenti e determinati).
  - **`C5b` (`ThrowNeutral` da cancel mid-spin CCW — NON è un bug di `ThrowNeutral` / `LinkSpin`, nessuna modifica a `GSTNCombatNP`):** durante tutta la timeline di `ThrowNeutral` (`43f`), `UGSTN_CombatSimComponent::ProduceInput_Implementation` mantiene il facing corrente (`max_drift=0.41deg` su `21` sample nella nuova verifica PIE `P8b_ThrowNeutral_MidSpinCCW_C5b_InTimeline`). Il vecchio fail `yaw_post=21.1` del probe `chal_pie.py` campionava a `t_cancel+0.9s` (~`12f` *dopo* la fine di `ThrowNeutral`), quando GASP `SandboxCharacter_Mover` fuori timeline (senza `bHoldFacingAfterReaction` armato su timeline con `SetFacing`) riallinea la capsula verso l'ultimo `OrientationIntent` memorizzato (`0°`, dall'ultimo step `k=8` di `spin_steps`). Tema trasversale `GSTNCombatNP` lasciato invariato e fuori scope di M3.
  - La rete `SUITE_LINK` (`link-listen-0pct.log`) mostra `PASS` su `link_spike_client_grabs_client` (18/18), `link_rollback_client_grabs_host` (6/6), `link_rollback_client_grabs_client` (6/6), `grab_throw` (12/12), `grab_interact` (9/9), `grab_gate` (12/12), `throw_at_enemy` (7/7), e nessun `neck-hold-desync` su `link_spike_client_grabs_host` (solo 1 picco su observer `world 2`: `max 99.4 cm`, p95 `17.1 cm`). Restano pre-esistenti/noti i fail su `link_spike_host_grabs_client` (4 rb, p95 `26.1/32.3`), `link_rollback_host_grabs_client` e 1 check su `grab_mash` (`{0: [549], 1: [555], 2: [549]}`).

## Prove (fisso 2026-11-26)
- **Modularità (`Tools/Modules/check_modules.py`):** `check_modules: 24 problem(s)` pre-esistenti invariati (`GSTNMission`, `.uproject`, `GSTNMinion`, `GSTNHeldRuntime`, `GSTNParts*`); `0` violazioni in `GSTNLink*` e `GSTNSpinDrill*`, nessuna modifica a `.Build.cs`/`.uplugin`.
- **Build:** `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build-m3-c5cd.log` -> `ESITO: OK` (`23s`, `REGRESS: OK 8 locked versions, 13 files unchanged`; precedenti: `build.log` `197s`, `build-testfix.log` `30s`, `build-grab-cancel.log` `16s`).
- **Test automatici UE (`Check-TestLog.ps1`):**
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-link-m3-c5cd.log -Tests GSTNLink` + `Check-TestLog.ps1 -MinTests 100` -> `ESITO TEST: PASS ok=133 fail=0 totale=133` (`SpinSuppression`, `AllRegisteredAndDocumented` = 48 tag, `FillAndSlots`, `TrackYaw`, `NoForeignRootDeclared`, `ThrowStickTurn` tutti verdi).
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-spindrill-m3-c5cd.log -Tests GSTNSpinDrill` + `Check-TestLog.ps1 -MinTests 30` -> `ESITO TEST: PASS ok=33 fail=0 totale=33` (`DefaultBindingsAndSlots` e `GrabberBlocksMovementSpin` verdi).
  - `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test-input-m3-c5cd.log -Tests GSTNInput` + `Check-TestLog.ps1 -MinTests 100` -> `ESITO TEST: PASS ok=139 fail=0 totale=139`.
- **Setup headless (`setup_grab_ganon_playtest.py`):** `Saved/Logs/grab-ganon/setup.log` -> `seeded command_set slot Command.Link.Grab/Release`, `seeded kit_binding Command.Link.Grab/Release`, `grab: GA_Ganon_Playtest_Grab_C ... changed=True`, `saved 7: GA_Ganon_Playtest_Throw{Forward,Up,Down,Neutral,CrouchForward}, GA_Ganon_Playtest_Grab, KIT_Test_Playtest`.
- **PIE `L_Test_Hub` (`KIT_Test_Playtest`, `Saved/Logs/grab-ganon/pie_verify_m3_c5cd.txt`, `Saved/Logs/ci/pie-m3-c5cd.log`):** `SUMMARY: pass=10 fail=0 total=10` (`ESITO: OK`):
  1. `P1_SpinPostGrab_OpenSpace_CCW` (>38f di `Grab`, lontano dal muro): `role_grabber=True`, `sweep=-892.7deg`, `d0=53%` (`+3%` `Pummel`), `d1=8%` (`+8%` `GrabSpin`), `no_wall=True`, `no_mov=True` (zero `GSTN_GA_SpinCounterClockwise`).
  2. `P2_SpinPostGrab_Outside180cm_NoSlam_C5c` (>38f, 180cm da `Z1_Wall`): `d2=53%` (`+3%` `Pummel`), `no_wall=True`, `no_mov=True`.
  3. `P3_SpinPostGrab_Inside90cm_WallSlam_C5d` (>38f, 65cm da `Z1_Wall`): `d3=107%` (`+4%` `GrabSpin.Wall` + `+3%` `Pummel`), `wall=True`, `hs=(True,True)`, `no_mov=True`.
  4. `P4_ThrowDown_Cr_13pct` (`Cr.`): `dmg=13%`, `dz=559.2cm`, `throw=True`.
  5. `P5_ThrowCrouchForward_CrF_7pct` (`Cr.F.`): `dmg=7%`, `dx=328.8cm`, `dz=346.1cm`, `throw=True`.
  6. `P6_PutDown_Dodge_CleanRelease` (`PutDown`): `dmg=0%`, `role 1 phase 2 -> role 0 phase 0`, `nums=(0, 1281, 0, 1313, 0)`.
  7. `P7_ThrowForward_StickBack_13pct` (`F.`): `dmg=13%`, `dx=-1053.6cm`, `yaw_end=-180.0deg`, `throw=True`.
  8. `P8a_ThrowNeutral_Standing_13pct` (`N.`): `dmg=13%`, `dz=637.6cm`, `yaw0=-180.0deg`, `max_drift_in_tl=0.00deg` (`22` sample).
  9. `P8b_ThrowNeutral_MidSpinCCW_C5b_InTimeline` (`N.` da cancel mid-spin CCW entro timeline): `dmg=13%`, `throw_f=1709`, `yaw_start=62.1deg`, `yaw_rel=62.5deg`, `max_drift=0.41deg` (`21` sample).
  10. `P9_NonGrab_MovementSpin_CCW_Regression` (fuori presa): `Command.Movement.Spin.CounterClockwise` `+3% Normal` attivo regolarmente.
- **PIE precedente (`Saved/Logs/grab-ganon/pie_verify.txt`, `pie-grab-ganon.log`):** `OVERALL: PASS` (5/5 incl. durata presa `0% = 90f`, `100% = 260f` senza mash).
- **Rete (`Saved/Logs/grab-ganon/link-listen-0pct.log`, `Saved/Logs/ci/link_net.log`):** `7/11 PASS` (`link_spike_client_grabs_client`, `link_rollback_client_grabs_{host,client}`, `grab_throw`, `grab_interact`, `grab_gate`, `throw_at_enemy`).

BRANCH: claude/project-thread-h46ltf
SUBAGENTI: 3 (cloud: web UFD, SmashWiki lanci, SmashWiki durata presa) + worker desktop Antigravity (build, test, setup, PIE 2 manichini, rete) + 2 explorer + 1 worker M3 fix C5c/C5d (build, test, PIE 10/10)
ESITO: PASS build (build-m3-c5cd.log), test GSTNLink (133/133)/GSTNSpinDrill (33/33)/GSTNInput (139/139), fix C5c/C5d (Opzione A+C), C5b documentato, e 10/10 prove PIE L_Test_Hub verificati su claude/project-thread-h46ltf
COSTO: ~13 $ | 32 min desktop | 0 token review
