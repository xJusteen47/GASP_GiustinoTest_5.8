# Presa, lanci e spin con il nemico in mano: numeri di Ganondorf (2026-11-25)

Richiesta owner (thread "Perfezionare grab e lancio dei nemici"): nel kit di playtest la forza di presa, pummel e lanci viene dalle prese di Ganondorf
su Ultimate Frame Data; con un nemico in mano, girando lo stick lo si usa come arma (fa male ai vicini e a lui), poi lo si lancia; il nemico si libera
da solo dopo un po'. Script: `Tools/GAS/setup_grab_ganon_playtest.py`. Codice: GSTNLink (README, sezione "Grab spin").

## Fonti (lette il 2026-11-25)
- Frame e danni: Ultimate Frame Data, https://ultimateframedata.com/ganondorf (sezione Grabs/Throws). Il sito non dà BKB/KBG/angolo.
- Knockback dei lanci e del pummel: SmashWiki, pagine `https://www.ssbwiki.com/Ganondorf_(SSBU)/Forward_throw`, `/Back_throw`, `/Up_throw`,
  `/Down_throw`, `/Pummel` (tabelle hitbox, colonne BK, KS, FKV; angolo dall'icona). Pagine comunitarie, possono non seguire l'ultima patch.
- Durata della presa: SmashWiki, https://www.ssbwiki.com/Grab, "Grab time": da Brawl in poi `90 + 1,7 × %` (arrotondato in basso), ogni input
  toglie 8 frame (stick; i tasti 14,4 in Smash 4/Ultimate), minimo 19, nessun massimo. La pagina non lo dice esplicito per Ultimate.

## Ganondorf (Smash Ultimate)
| Mossa | Startup / rilascio | Totale | Danno | BKB | KBG | Angolo |
|---|---|---|---|---|---|---|
| Presa da fermo | 8 (attiva 8-10) | 38 | - | | | |
| Presa in corsa | 11 (11-13) | 46 | - | | | |
| Presa pivot | 12 (12-14) | 41 | - | | | |
| Pummel | 2 | 21 (14 di hitlag) | 1,6 | 0 (FKV 30) | 100 | 80 |
| Lancio avanti | 11 colpo / 13 rilascio | 39 | 5 + 8 | 68 | 70 | 43 |
| Lancio indietro | 12 / 14 | 49 | 5 + 5 | 30 | 130 | 43 (indietro) |
| Lancio su | 11 / 13 | 43 | 10 + 3 | 70 | 105 | 90 |
| Lancio giù | 23 | 39 | 7 | 70 | 50 | 65 |
| Colpo collaterale (avanti, indietro) | | | 5 | 30 | 100 | 80 / 70 |

## Conversione nel progetto
- Frame 1:1 (Smash a 60 Hz, nostro tick fisso a 62,5 Hz: tutto il 4 % più veloce). Rilascio = frame della chiave Throw, totale = TotalFrames e FAF.
- Danno = colpo + lancio (il nostro lancio ha un solo colpo sul preso).
- BKB × 22,5 = cm/s (UnitScale del modello Smash, handover feel-f2), KBG uguale. Angolo: elevazione dal davanti del lanciatore, sopra 90 = dietro.
- Notazione (modalità Notation, opzione 1A consigliata): F. = lancio avanti, N. = lancio su, Cr. = lancio giù, Cr.F. = lancio indietro mandato in
  avanti (quello che uccide), B. = lancio indietro (solo con BackThrow).
- Presa: base 90, 1,7 per %, minimo 19, massimo 600 (a 300 % sono 600 frame), 8 frame per input (opzione 2A).
- Presa in corsa e pivot: non abbiamo una mossa separata; restano da fare se l'owner le vuole.

## Spin con il nemico in mano (codice, valori di default = opzioni 3A 4A 5A 6A consigliate)
| Cosa | Valore | Da dove |
|---|---|---|
| Input | giro completo dello stick (360° in 30 frame), uno per verso | condizione Spin di GSTNInput, come la spin di GSTNSpinDrill |
| Un giro | 24 frame, 1 rotazione; dal frame 12 si concatena un altro giro, i lanci, il rilascio e il put-down | nostro, segnaposto |
| Colpo ai vicini | 5 %, BKB 30 (675 cm/s), KBG 100, 45°, una volta per giro, una sfera di 70 cm a 90 cm davanti (dove pende il nemico) | colpo collaterale dei lanci di Ganon |
| Colpo al nemico tenuto | 1,6 % per giro, senza reazione | pummel di Ganon |
| Costo | ogni giro toglie 20 frame alla presa (si libera prima) | nostro |
| Fine | il mash del preso continua; i lanci normali cancellano il giro | |
Interruttore: `bGrabSpin` in DefaultGSTNLink.ini, cvar `gstn.Grab.Spin` (0 progetto, 1 acceso, 2 spento) nel menu GSTN Debug.
