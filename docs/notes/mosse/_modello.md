# Scheda mossa: <kit> <mossa> (<versione di lavoro>)
Regola 13 del progetto. Una scheda per mossa, in `docs/notes/mosse/<kit>-<mossa>.md`. La scheda è la FONTE per chi anima (esecutore) e per chi giudica
(revisore): si legge PRIMA di toccare o guardare la mossa. Ogni dato dice chi l'ha deciso: `[OWNER]` deciso dall'owner, `[DATI]` letto da file del repo
(citato), `[DA CONFERMARE]` dedotto, finisce anche nella lista in fondo. Nessun "da confermare" resta solo qui: lo raccoglie il coordinatore.

## 1. Identità (2-3 tratti, il revisore li controlla PER PRIMI)
| # | Tratto | Riferimento (immagine/video/pose/card) | Chi l'ha deciso |
|---|---|---|---|
| 1 | | | |
Cambiare un tratto = decisione dell'owner PRIMA della build. I vincoli di gioco non piegano la posa: si spostano manichino o hitbox (regola 13e).

## 2. Fasi e frame (frame di animazione a 60 Hz, il primo = 0)
| Fase | Frame | Note |
|---|---|---|
Chiavi dell'owner (bloccate, mai spostare): ...  Convenzione: i CSV di timing contano da 1, le card da 0.

## 3. Foglio delle curve (accanto alle pose; lo scrive chi anima, la base buona fa da base)
| Segmento | Timing (frame) | Easing / spaziatura | Overshoot | Follow-through (parte, ritardo in frame) |
|---|---|---|---|---|
Fonte: `spacing` e `part_delays` della card; il resto va scritto da chi anima a ogni iterazione.

## 4. Hitbox e frame attivi: DECISI DALL'OWNER (qui solo registrati, nessun thread li cambia)
| Cosa | Valore attuale | Fonte |
|---|---|---|
Hitbox: forma, posizione, finestra attiva, FAF, hitstop. Se il valore sta solo nei dati del kit (uasset) e non è leggibile dal cloud: `[DA CONFERMARE]`.

## 5. Versione approvata e blocchi
- **Approvata dall'owner** (regola 13b): tag `approved/<mossa>-<versione>` + riga in `docs/notes/anim-approvate.md`. Nessun thread la sovrascrive.
  Stato: ...
- **Buone** (checkpoint, vedi `docs/notes/mosse/README.md`): una riga per ogni giro buono.
| n | Tag | Commit | Card | Data | Cosa è buono | Cosa manca |
|---|---|---|---|---|---|---|

## 6. Domande aperte (all'owner, via coordinatore)
1. ...

## 7. Storia sintetica (max 8 righe, con commit o file)
