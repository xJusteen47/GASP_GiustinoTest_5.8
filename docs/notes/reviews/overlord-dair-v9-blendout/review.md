# Review: overlord dair v9, prove di blend-out — Verdetto: FAIL per tutte e due le varianti; lo scatto è lato gioco (confermato dai frame) — 2026-11-14
Regola 10, cloud. Frame estratti da `dair_whiff_base.mp4`, `dair_whiff_variant.mp4` (tentativo 1) e `dair_whiff_variant2.mp4`
(tentativo 2): 61 frame ciascuno, v40-v100. Guardati uno per uno v74-v82 di LATO (az270) e da DIETRO (az180), griglia
`rv_exit_v74-v82.jpg` (tre righe di lato, tre da dietro). Colpo, hitstop e atterraggio non sono in queste catture: restano giudicati
dalla review `overlord-dair-v9/review.md` (la clip passa). Misura mia: pixel del personaggio cambiati tra due frame video (ritaglio
280x288, soglia 20); i valori assoluti sono più bassi di quelli di `numbers.md`, i picchi cadono sugli stessi frame.
**Catena di cattura: valida.** Le etichette danno la fine della timeline (FAF 42): base m0:42 a v78, tentativo 1 a v79, tentativo 2 m0:41 a v78.

## Cosa si vede
| corsa | montage esce | frame dello scatto | lato | dietro | cosa cambia nel frame |
|---|---|---|---|---|---|
| base (8 f, auto) | v71 | v78 | 2687 | 2455 | gambe da raccolte sotto a sedute in avanti, braccia da 45 gradi a croce |
| tentativo 1 (8 f, parte alla fine) | v80 | v80 e v81 | 3970, 3888 | 3069, 2898 | gambe da raccolte dietro a sedute in avanti in 2 frame; le braccia invece si aprono piano (a v82 sono a metà) |
| tentativo 2 (16 f, auto) | v63 | v79 | 2185 | 3881 | solo le gambe: da piedi sotto il corpo a sedute; le braccia erano già a croce da v74 |
Frame normali attorno: 900-1500 di lato, 800-1400 da dietro.

## Confermato: non è il blend-out del montage
- Lo scatto segue la fine della timeline, non l'inizio del blend: spostare l'inizio del blend da v71 a v80 sposta lo scatto da v78 a v80-81.
- **Prova più pulita, tentativo 2.** Il montage è uscito 16 frame prima: da v74 a v78 si vede già la posa del gioco, con le braccia a
  croce della caduta ma le gambe con i piedi sotto il corpo. A v79, primo frame senza timeline, cambiano solo le gambe. Quindi è la posa
  del gioco stessa a cambiare quando l'abilità finisce: finché la timeline è attiva in aria le gambe del gioco non sono quelle della caduta.
- Tentativo 1: le gambe arrivano in 2 frame, le braccia seguono il blend di 8. Le gambe non seguono il blend del montage dopo la fine
  dell'abilità.
- Nessuna impostazione del montage può toglierlo. Le due varianti sono peggiori della base (tentativo 1: scatto più grande e su 2 frame;
  tentativo 2: da dietro 3881 e 16 frame di posa di gioco con le gambe sbagliate). Montage da lasciare a 8 frame, trigger -1.
- Non accertato: quale stato cambia. L'Anim BP è binario e non l'ho aperto; nel codice non ho trovato il punto.

## Correzione lato gioco (thread di codice piccolo, niente pose)
1. Diagnosi nell'editor, cattura `dair_whiff`: registrare per frame, da v70 a v85, la modalità di movimento del Mover, le variabili che
   l'Anim BP legge per lo stato in aria o caduta, i tag di combattimento e il peso dello slot. Trovare cosa cambia nel frame m0:42.
2. Cura attesa: durante una timeline in aria la posa del gioco sotto il montage deve essere già quella di caduta (l'Anim BP non deve
   ricevere dati da terra o congelati mentre l'abilità gira), così il blend-out di 8 frame atterra su una posa che alla fine non cambia.
3. Se quel cambio di stato deve restare: dargli una transizione di 8-10 frame invece di un taglio, sulle gambe.
4. Verifica: ricatturare `dair_whiff`, nessun frame sopra 2400 px di lato (maschera di `numbers.md`) da v69 a v85. La finestra va
   allargata oltre v77 perché lo scatto cade a v78-v81.
Stesso gruppo del difetto dell'accosciata dopo il jab (`overlord-jab-v3-r1/review.md`): in tutti e due i casi è la posa del gioco sotto
il montage a essere sbagliata quando il montage esce.
