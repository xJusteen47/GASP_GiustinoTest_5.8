# Lanci e gameplay mancante: diagnosi (cloud, sola lettura del codice, 2026-11-24)
Branch `claude/project-thread-5yaz54` da main a5b20950. Niente compilato, niente cambiato nel gioco. Numeri delle mosse: `docs/notes/reviews/feel-f2/knockback_table.csv`
(F2, applicato sul fisso in F4 con `setup_knockback_smash.py`, `docs/notes/handovers/feel-f4-desktop.md`). Traiettorie: `handoffs/lanci-traiettorie.py` nei project files, gravità 980, passo 16 ms.

## Perché "non lanciava" (3 cause, in ordine di peso)
1. **Il lancio non è un "lancio" per il salto.** `FGSTN_LayeredMove_Knockback` deriva da `FLayeredMoveBase`, non da `FLayeredMove_Launch`
   (`GSTN_LayeredMove_Knockback.h:28`), quindi `UGSTN_MoverSimStateTracker::CaptureFrameFacts` (`GSTN_MoverSimStateTracker.cpp:94-98`) non lo segnala e
   `FGSTN_JumpLogic::Step` non cambia fase. Un bersaglio colpito **in aria mentre scende** resta in fase `Falling` e riceve `FallGravityScale` (~9 g, preset
   del salto 11-18) anche mentre sale: un lancio da 1000 cm/s sale 20-50 cm invece di 2-5 m. Nelle combo in aria succede quasi sempre.
2. **A 0-20 % le mosse di Overlord non lanciano per design dei numeri.** Dropkick K0 475 e DAir V9 K0 450 stanno sotto PushBackThreshold 540: stagger sul
   posto. Lanciano da ~19-21 %, tumble da ~88-95 %. Jab 1 non lancia mai (è voluto). In playtest il manichino sta quasi sempre a percentuale bassa.
3. **Sotto LaunchThreshold 750 il "push back" è solo orizzontale** e limitato a 600 cm/s (`GSTN_KnockbackModel.cpp`, tier PushBack): una mossa a 75° (dair)
   fa scivolare di lato, non solleva.

## Perché "lanciava e non vedevo dove"
4. **Gravità asimmetrica nel volo.** Colpito da terra: fase `Launched` = gravità x1 in salita (il salto del giocatore sale a x2.6), poi `Falling` = x9 in
   discesa. Il bersaglio sale lento e altissimo, poi cade a piombo. Strong a 40 %: sale 4.1 m, vola 20 m in 1.3 s; a 100 %: 13.9 m di altezza, 78 m.
5. **Nessun freno al lancio.** DecayPerFrame è 0 per i lanci (Smash toglie 0.051/frame: `docs/notes/balloon-knockback.md`), quindi la velocità orizzontale
   non cala mai: i lanci medi escono dall'inquadratura in meno di un secondo.
6. **La camera segue solo il proprio personaggio.** Gioco = camera GASP + `UGSTN_PlaytestCameraModifier` (spinta indietro, tilt-shift). La modalità Duel
   (`UGSTN_CameraMode_Duel`, inquadra i due) esiste in GSTNCamera ma non è su un pawn vero (M12 G5, solo branch). Nessun indicatore fuori schermo,
   VFX spenti: niente scia del lancio.

Tabella (stessi K, terra = gravità di oggi, "come il salto" = x2.6 su / x9 giù):
| Mossa | K, angolo | Da terra oggi | Colpito in aria mentre scende | Con la gravità del salto |
|---|---|---|---|---|
| Jab 2 a 0 % | 1296, 35° | alto 2.8 m, 11 m, 1.0 s | sale 0.26 m | 1.0 m, 4.8 m, 0.45 s |
| Dropkick a 40 % | 1080, 40° | 2.4 m, 7.9 m | 0.22 m | 0.9 m, 3.6 m |
| DAir a 40 % | 1023, 75° | 4.9 m, 3.8 m, 1.4 s | 0.47 m | 1.8 m, 1.6 m |
| Strong a 40 % | 1801, 30° | 4.1 m, 20 m | 0.39 m | 1.5 m, 8.5 m |
| Strong a 100 % | 3313, 30° | 13.9 m, 78 m, 2.7 s | 1.4 m | 5.3 m, 31 m |

Da verificare sul fisso (non visto da qui, asset binari): modello attivo in `GSTN_CombatRules_Standard` (F4 dice PercentScaling) e se il manichino
del playtest ha gli attributi del salto (se non li ha vola sempre a x1: ancora più lontano).

## Cosa manca per un platform fighter giocabile (dal codice, per impatto)
1. Sensazione dei lanci (sopra).
2. Ciclo di partita: KO zone c'è (`GSTN_KOZone.h`), ma respawn, vite e fine partita (GSTNRespawn, GSTNMission) sono SCRITTI, mai compilati, spenti.
3. HUD: nessun widget nel codice (nessun UUserWidget): percentuale e vite non si vedono a schermo.
4. Camera a due giocatori (Duel/condivisa) non collegata al pawn.
5. Recupero: nessuna presa del bordo (GSTN_Ledge è solo "non cadere dal bordo durante una mossa"), nessun up special. C'è il muro (wall slide/climb).
6. Moveset di Overlord: jab 1-2, dropkick, dair, presa; F.S, N.S, lanci N/F/Cr/Cr.F non fatti o su branch.
7. Piattaforme attraversabili dal basso: solo il tag di superficie, nessuna logica.
8. Avversario: nessuna CPU né manichino che si muove; solo il secondo client.
9. main non ricompilato dal merge del 22-11 (rischio su tutto il resto).

ESITO: diagnosi fatta, 6 cause nel codice con file e righe; nessun fix applicato (serve decisione owner).
SUBAGENTI: nessuno.
COSTO: ~3 $ | 0 min desktop | 0 token review
