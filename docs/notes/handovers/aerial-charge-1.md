# Handover aerial-charge-1: caricare gli aerial (2026-11-25)

Branch `claude/project-thread-uyhp3p`, base `main` 152d25e6. Thread cloud, **NON compilato** (niente engine nel cloud). Owner 2026-11-25: 1C 2A 3A,
"porzione dello startup e gestione delle pose come le cariche degli smash di Smash Ultimate", "la tolleranza di leggera angolazione anche qui, alla conferma".

## Cosa fa
- Traccia di timeline `FGSTN_TimelineTrack_ChargeHold` (Core, `Timeline/GSTN_ChargeHold.h`): se il tasto della mossa è tenuto **senza interruzioni dall'inizio**,
  la timeline si ferma sul fotogramma `StartFrame` (posa di anticipazione) fino a `MaxHoldFrames` (60, ~1 s come la carica piena di Ultimate); al rilascio
  prosegue con il resto dello startup. Un tap non cambia nulla. In aria si continua a cadere e a spostarsi con lo stick.
- Posa: montage in pausa sul fotogramma (come la hitstop, nel pre-tick del combat model) oppure loop di `LoopSection` guidato dall'abilità.
- 2A: `bLockDirection` taglia la finestra di reverse al fotogramma di carica e tiene fermo lo yaw dello steering durante la carica; dopo il rilascio lo
  steering riprende nel cono della mossa (la "leggera angolazione alla conferma").
- 1C: `FullChargeDamageScale` per mossa (1 = spento, default; Ultimate 1,4): danno scalato in base a `FGSTN_CombatSyncCore::ChargeFrames`
  (nuovo campo di sync, 1 bit se 0), nel tick dell'attaccante; il knockback cresce col danno tramite la formula.
- Script `Tools/GAS/setup_aerial_charge_hold.py`: trova tutti i binding Airborne di Command.Attack/Strong in tutti i KIT_/WPN_ sotto /Game/GSTN (3A),
  mette la traccia a ~1/3 dello startup (`CHARGE_HOLD_FRACTION`, frazione da tarare a occhio, NON verificata sui frame di Ultimate) e un cono di
  steering di 15° dal fotogramma dopo la carica dove la mossa non ne aveva. **Prova a secco di default**; `run(apply=True)` scrive.
- Test: `GSTNCombat.Core.ChargeHold.*` (Clock, HeldSinceStart, DamageScale, Direction, Validate).

## Passi sul fisso (un thread, lock `_ue59`)
1. Editor chiuso. Merge di `claude/project-thread-uyhp3p` nel branch di integrazione.
2. `.\Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` (nuovi .h/.cpp, UPROPERTY, campo di sync: build vera).
3. `.\Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test.log -Tests GSTNCombat.Core.ChargeHold` poi `GSTNCombat` intero: 0 fallimenti attesi.
4. Editor: `s.run()` dello script, leggere `Saved/AerialCharge/setup_aerial_charge_hold.txt` (elenco aerial e fotogrammi). Se ci sono timeline inline
   su Blueprint di abilità condivisi, avvisare l'owner prima di `s.run(apply=True)`.
5. PIE capsula: tap = aerial normale; tenere = posa ferma, deriva con lo stick, rilascio = resto dello startup; stick indietro durante la carica non gira
   la mossa; atterrare in carica = landing lag normale. Harness di rete (`gasnpp_test.py`) per il rollback della carica.
Massimo 2 tentativi di fix per lo stesso errore (regola 2).

## Non fatto / da confermare
- Bagliore e suono di carica: dati (una finestra cosmetica su [StartFrame, StartFrame+1) resta viva per tutta la carica), non messi dallo script.
- Tremolio di Ultimate: serve una sezione di loop nel montage (`LoopSection`); con la capsula la posa è ferma.
- Frazione dello startup e tetto: da tarare in gioco con l'owner.

ESITO: PARZIALE codice, test e script scritti nel cloud, non compilato né provato sul fisso
SUBAGENTI: 0
COSTO: ~9 $ | 0 min desktop | 0

## Fisso, 2026-11-27 (thread M8, worktree GASP_Giustino2_3dyy2t, detached su 0c5cadfe)
- Build OK; `-Tests GSTNCombat` 740/740 PASS (Saved/Logs/ci/test.log.ue.log).
- Script: `_tag` falliva in 5.9 (`GameplayTag` non ha `request_gameplay_tag` né la keyword `tag_name`): ora `import_text`, come gli altri
  script. In commandlet serve `search_all_assets(True)` prima di `run()` (in editor no). A secco: 18 binding, 16 timeline asset, nessuna
  inline su Blueprint; tutte "hold frame 5 of startup 15, max 60, damage x1"; nessun cono aggiunto (le timeline sterzano già).
- PIE (KIT_Test_OverlordDropkick, hub, host + 2 client; traccia applicata SOLO in memoria, nulla salvato; `pie_capture_move.py` con
  Attack tenuto 40 frame su dropkick_whiff via boot, tap su dropkick_hit): tenuto = montage fermo al frame 5 da v44 a v76, il personaggio
  continua a salire/cadere e deriva con lo stick (x +220 cm), al rilascio il resto dello startup; tap = startup normale. Dati Saved/M8/charge.
- NON fatto: salvataggio delle 16 timeline (vedi sotto), harness di rete per il rollback della carica, landing in carica.
- Da decidere prima di `run(apply=True)`: 9 timeline (Dropkick, TB, TBK5-K8, TBR2-R4) sono anche nel commit 354c556c della PR #57
  (binari, conflitto sicuro se si salvano qui); `TL_Overlord_DAirV9` è della dair v9 approvata (bloccata). Proposta: unire prima #57,
  poi rilanciare lo script su main via `Save-Assets.ps1` in `_ue59`, escludendo la V9 o su una sua variante.

ESITO: PARZIALE build, 740 test e PIE della carica OK (in memoria); timeline non salvate per conflitto con PR #57 e dair V9 bloccata
SUBAGENTI: 0
COSTO: ~5 $ | ~15 min desktop | 0

Aggiornamento 2026-11-27 16:16 UTC (dopo il merge della #57, coordinatore): branch allineato a main, script con CHARGE_SKIP (default TL_Overlord_DAirV9) e scansione del registry; Save-Assets in _ue59 ha scritto 15 timeline, V9 intatta. Riletto: carica presente, finestra KnockdownOnLanding della #57 conservata sul dropkick. Commit 38be3b21.

## Aggiornamento 2026-11-27 sera: rete e atterraggio in carica
- Rete (harness, server dedicato + 2 client, 60 ms): dropkick caricato 30 frame, 0 reconcile su ChargeFrames/TimelinePausedFrames, ChargeFrames 25 su
  server, owner e osservatore, montage in pausa sul frame 5 anche sul client remoto. Log sul fisso: Saved/Logs/ci/aerial_charge_net.log,
  Saved/AerialChargeNet/{charge,land}.jsonl, summary.json.
- Atterraggio in carica: gioco corretto, ma il montage restava in pausa per tutto l'atterraggio (il resume scattava solo al rilascio). Fix:
  `FGSTN_TimelineOutput::bChargeHoldResume` (fine della carica per rilascio o per il salto di Hold Until Landed), usato dal combat model per il
  resume; `bChargeHoldRelease` resta solo per la posa di loop. Test `GSTNCombat.NP.ChargeHold.Release` e `.LandDuringHold`. Non compilato.
- Da ricontrollare dopo il fix: l'atterraggio in carica durava 29-30 frame contro 27-28 dopo un rilascio.

ESITO: PARZIALE carica in main e provata in rete, fix del montage all'atterraggio in carica scritto ma non compilato
SUBAGENTI: 0
COSTO: ~4 $ | ~25 min desktop | 0
