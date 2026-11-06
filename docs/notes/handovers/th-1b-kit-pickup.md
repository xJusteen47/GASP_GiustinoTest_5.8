# Handover TH-1b: kit a terra (portatile, headless, 2026-11-06)

- Branch `claude/project-thread-0t6tfg`, base `origin/feature/gas-npp-5.8` (60da6fa). Non dipende da M7-15 né da SD-16.
- Nessun brief TH-1b dedicato: partito dalla riga D1 di `docs/architecture/plans/sequenza-2026-11-06.md` (branch `claude/project-thread-4kdr4u`).
- Consegnato in `Plugins/GSTNCombatGAS` (cartelle `Pickups/`):
  - `AGSTN_KitPickup`: attore replicato (always relevant) che tiene un `FGSTN_LoadoutData` intero; mesh (nessuna di default) +
    etichetta con il nome del kit; vuoto = nascosto e non scambiabile. `InteractRadius` 150 cm.
  - `UGSTN_KitSwapSubsystem` (world, solo server): ascolta l'evento pubblicato `Input.Event.CommandExecuted` con
    `Command.Interact.PickUp` (tieni premuto interagisci, già nel set di movimento) e scambia il loadout del giocatore con il kit
    a terra più vicino in raggio via `AcceptLoadout`; il vecchio kit resta nel pickup. Rifiuto delle regole → `ClientLoadoutRejected`.
  - `DropKit(...)`: hook del drop alla morte per M12 (regola di modalità); nessuno lo chiama ancora.
- Scelta (regola 6): niente codice nella simulazione. L'hold è deciso dallo strato input deterministico, lo scambio è una scrittura
  server fuori banda come il cambio loadout da menu; arriva alla sim come nuova `LoadoutRevision` (confine di frame già esistente).
- Vincolo: l'evento esiste solo se il kit lega `Command.Interact.PickUp` a un'abilità (il binding set di test lo lega a
  `GSTN_GA_TestPickUp`). I kit messi in Z0 di `L_Test_Hub` devono averlo.
- Doc: `combat.md` §10.6, un punto.
- Test: `GSTNCombat.GAS.Loadout.KitPickupSwap` (pickup vuoto, raggio, più vicino, scambio con stats 800→500→800, kit rifiutato
  non cambia nulla, hook drop). Gate headless on: build ok, **981/0**, 0 ensure (script `Saved/TH1B/build.ps1`). Gate off non
  rilanciato (il cambio non tocca SpinDrill/Piggyback).
- Non fatto (fuori dal portatile/headless): fila di kit in Z0 di `L_Test_Hub` (serve TH-1 sul fisso: lo script del livello piazza
  `AGSTN_KitPickup` con mesh e `Loadout` impostati) e prova PIE listen dello scambio con l'input vero. Prossimo: farlo nel thread
  TH-1 o in un TH-1c Sonnet sul fisso (~$2-4).
- Costo stimato: ~110k token di contesto, ~$4-5.
