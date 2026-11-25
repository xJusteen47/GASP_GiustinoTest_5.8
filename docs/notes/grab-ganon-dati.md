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
| Lancio indietro (non usato) | 12 / 14 | 49 | 5 + 5 | 30 | 130 | 43 (indietro) |
| Lancio su | 11 / 13 | 43 | 10 + 3 | 70 | 105 | 90 |
| Lancio giù | 23 | 39 | 7 | 70 | 50 | 65 |
| Colpo collaterale (avanti, indietro) | | | 5 | 30 | 100 | 80 / 70 |

## Conversione nel progetto
- Frame 1:1 (Smash a 60 Hz, nostro tick fisso a 62,5 Hz: tutto il 4 % più veloce). Rilascio = frame della chiave Throw, totale = TotalFrames e FAF.
- Danno = colpo + lancio (il nostro lancio ha un solo colpo sul preso).
- BKB × 22,5 = cm/s (UnitScale del modello Smash, handover feel-f2), KBG uguale. Angolo: elevazione dal davanti del lanciatore, sopra 90 = dietro.
- Notazione (modalità Notation, risposta owner 1, 10:21Z): F. = lancio avanti, Cr. = lancio su, Cr.F. = lancio giù, nessun lancio indietro.
  N. non nominato: resta il lancio su (proposta 1A). Lo slot Cr. è `Command.Link.Throw.Down`, quindi nelle modalità classiche il "giù" è il lancio su.
- Ogni lancio gira verso lo stick prima del rilascio (risposta 6): 180° in 8 frame (1406°/s, steering del combat core con sorgente stick di movimento,
  `UGSTN_GA_LinkThrow::ApplyStickTurn`), finito prima del rilascio più presto (13). Stick fermo: la direzione resta quella attuale, cioè dove sta il
  nemico tenuto (consigliata sul punto aperto; l'altra era l'ultimo movimento dello stick). Niente scatto istantaneo.
- Presa: base 90, 1,7 per %, minimo 19, massimo 600 (a 300 % sono 600 frame), **nessun mash** (risposta 2: l'input non toglie frame).
- Presa in corsa e pivot: non abbiamo una mossa separata; restano da fare se l'owner le vuole.

## Spin con il nemico in mano (codice, valori = risposte owner 3, 4, 5 del 2026-11-25)
| Cosa | Valore | Da dove |
|---|---|---|
| Input | giro completo dello stick (360° in 30 frame), uno per verso | condizione Spin di GSTNInput, come la spin di GSTNSpinDrill |
| Una mossa | **due giri** in 48 frame (24 a giro), poi 12 frame di assestamento solo estetico (coda cancellabile da tutto, stick compreso; la capsula sbanda di 15° e torna) | risposta 3 |
| Catene | dal frame 12 lanci, rilascio e put-down; dal frame 36 un altro giro di stick concatena altri due giri | nostro, segnaposto |
| Colpo ai vicini | **8 %**, BKB 30 (675 cm/s), KBG 100, 45°, una volta per mossa, sfera di 70 cm a 90 cm davanti (dove pende il nemico) | risposta 4 ("per il personaggio 8 e 3"), knockback dal colpo collaterale di Ganon |
| Colpo al nemico tenuto | **3 %** per mossa, senza reazione | risposta 4 |
| Sbattuto al muro | 4 % al nemico tenuto con 8 frame di hitstop (4 su chi gira), una volta per mossa, al primo frame in cui una sfera di 30 cm dal grabber a 110 cm davanti tocca geometria statica | risposta 4, numeri nostri |
| Costo | ogni mossa toglie 20 frame alla presa, regolabile (`GrabTimeCostFrames`) | risposta 5 |
| Fine | il tempo di presa del preso continua (si libera da solo); i lanci normali cancellano lo spin e girano verso lo stick | risposta 6 |
Danni "dipende da che nemico": per ora un solo valore per mossa (spec `FGSTN_GrabSpinMoveSpec`); un moltiplicatore per nemico è da fare quando serve.
Interruttore: `bGrabSpin` in DefaultGSTNLink.ini, cvar `gstn.Grab.Spin` (0 progetto, 1 acceso, 2 spento) nel menu GSTN Debug.
