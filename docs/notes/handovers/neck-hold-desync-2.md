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
SUBAGENTI: 0
COSTO: ~4 $ | ~22 min desktop (2 build 1 min, 3 batch 9 min) | 0 token review
