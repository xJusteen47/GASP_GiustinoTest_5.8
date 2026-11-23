# Neck hold desync, giro 2: log Mover (2026-11-23, fisso, Opus medium)
OBIETTIVO: campo del Mover che riconcilia con la presa al collo, causa, fix. Branch `claude/project-thread-5kb8vz` (+ merge locale content-reorg), worktree `GASP_Giustino2_netgrab`.

## Fatto
- `Plugins/EngineOverrides/Mover/.../MoverNetworkPredictionLiaison.cpp`: cvar `gstn.Mover.LogReconcile 1` (no Shipping) = riga `GSTNMoverReconcile` con campo, delta/base/velocità per DefaultSyncState e ToString locale/autorità per gli altri struct. Non cambia il verdetto. Build OK (`Saved/Logs/ci/build-moverlog2.log`).
- Run listen `link_spike_client_grabs_host`: G x3 col collo, H x2 col collo, H x2 senza (HoldByNeck 2). Report `Saved/Logs/netgrab/batch-{G-moverlog,H-neck,H-noneck}.log`.

## Risultato (conteggi: Select-String sui `Saved/Logs/ci/netgrab-<run>.log.ue.log`)
- Chi riconcilia: solo il pawn autonomo del client che afferra (role=2). Campi: DefaultSyncState (posizione, base sempre None: NON è una collisione o una base sotto i piedi), GSTN_RunStageSyncState, SmoothWalkingState. H-neck 36/65/26 righe, H-noneck 3/16/2.
- Le velocità locale e autorità hanno direzioni diverse (es. local (0,372) auth (546,-334)): il server simula il grabber con un input DIVERSO, non con la stessa fisica.
- Perché: il server resta senza input del client. `Gap in input stream detected on server` H-neck 57, H-noneck 0 (G 6054 con la macchina più carica, 8-10 fps). `ReconcileFromInputOffset` sale fino a 157 frame col collo (95 senza) e `Ignoring server state ... older than the 63 frames of local history` H-neck 27, H-noneck 0.
- Quindi: col collo qualcosa allunga il frame/ritarda il server, l'anticipo del client sfora la storia, il server riusa input vecchi e il Mover del grabber diverge. La posizione del preso NON entra nel movimento del grabber (ipotesi del brief scartata).
- Rollback camminata: G 11/27/29, H-neck 10/27, H-noneck 4 (3 nelle finestre forzate). Il carico del PC (45 % CPU senza editor) peggiora tutto: confronti solo dentro la stessa sessione.

## Prossimo passo (non fatto: budget, regola 5)
1. Con `stat NetworkPrediction`/`np.Debug` e conteggio resim per frame: verificare se col collo il modello combat del PRESO resimula ogni frame (ShouldReconcile del preso legge l'AnchorOverride: float confrontati senza tolleranza?) e quindi gonfia il costo del frame. Se sì, fix = tolleranza/quantizzazione dell'anchor nel ShouldReconcile del preso.
2. Rerun H-neck/H-noneck nella stessa sessione, poi i 4 scenari.
## Warning con 3 PIE (riepilogo in `Saved/Logs/netgrab/warnings-3pie.txt`, run H-neck)
- Legati al desync: Gap in input stream (57), Lag Compensation Rewind Exceeded (55), Ignoring server state (27), ReconcileFromInputOffset (14).
- Rumore: ABP_GSTN_Locomotion chiamate non thread-safe (82: GetAnimOwnerBoostDashTime, RunStartTime, IsJumpSquatting, IsSkidding: servono metadati BlueprintThreadSafe), PoseSearch indici async (10), Oodle dictionaries (8), SaveGames mancanti (6), GSTNSurfaceTypes all'ingresso (4), SmartObjects S_SmartObject mancante, DA_GSTN_SurfaceResponse_Default non trovato (riordino contenuti?), enum duplicati in Python. Stima pulizia: ~1-2 $ Sonnet, 1 thread.
## Giro 3 (stessa sera, sola lettura, 0 min desktop)
- Ipotesi "ShouldReconcile del preso sull'anchor senza tolleranza" FALSA: `GSTN_LinkFragment.cpp:156` ha già la tolleranza `PredictedPlacementToleranceCm`, e StepNeckHold scrive l'anchor una volta sola per link. Le run F (LogFragmentReconcile) davano 2 righe di fragment per run. Nessun fix applicato.
- Dato nuovo, stessa sessione H: fps dell'editor 11,5/12 col collo contro 17,9/16,9 senza (numeri di frame tra `play setup` e `timeline done` nei .ue.log). Col collo il frame costa ~30 % in più; con anticipo di ~100 frame ogni rollback risimula ~100 frame, quindi un primo errore innesca la cascata (fps giù, input in ritardo, altri rollback).
- RunStage riconcilia anche da fermo e PRIMA della presa (`SinceRelease` locale 6 contro autorità 33): sintomo di frame saltati dal server, c'è anche senza collo.
- Terzo giro sullo stesso obiettivo: per la regola 5 serve una DIAGNOSI DI SISTEMA. Proposta: una traccia Unreal Insights (`-trace=cpu,frame` + `np` stat) di una run col collo e una senza, ~10 min fisso, per trovare cosa costa il 30 % (presentazione del preso sulla mano viva? hurtbox piazzata?).
## Giro 4: traccia Insights (fisso, ~4 min editor)
- CORREZIONE: il "30 % di frame in più" del giro 3 era un errore di misura (il numero di frame nei log UE è modulo 1000). Traccia `Saved/Profiling/neck/T-{neck,noneck}.utrace`, export GameThread (`UnrealInsights -ExecOnAnalysisCompleteCmd=TimingInsights.ExportTimerStatistics`): Frame 5704 col collo contro 5789 senza in ~80 s (~70 fps entrambi), FrameTime 79,99 contro 79,87 s. Nessun timer di gioco costa di più col collo.
- L'unica differenza grande è WinPumpMessages 5,3 s contro 0,04 s: messaggi di Windows (attività esterna sul PC), non il gioco. Rollback NP 21 contro 17.
- In queste due run: camminata 5 rollback col collo (3 forzati) contro 4 (2 forzati), 0 buchi di input in entrambe: la cascata non si è presentata.
- Lettura: la cascata (buchi di input, anticipo oltre la storia) sembra legata a intoppi della macchina (carico esterno 45 % CPU, G con 6054 buchi a macchina carica), non a un costo della presa al collo. Il legame col collo resta statistico (H: 57 buchi contro 0) e non spiegato. Prossimo passo: N=5 per lato a macchina scarica (owner fuori dal PC) prima di altro codice.
SUBAGENTI: 0
COSTO: ~7,5 $ (giro 2 ~4, giro 3 ~1,5, giro 4 ~2) | ~26 min desktop | 0 token review
