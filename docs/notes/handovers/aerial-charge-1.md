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
