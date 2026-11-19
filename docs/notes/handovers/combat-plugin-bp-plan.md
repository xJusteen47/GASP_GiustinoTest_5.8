# Piano: plugin di combattimento estendibile da Blueprint (CP0, solo piano, 2026-11-14)
Base codice `d27c77f5` (wkds3r). Obiettivo owner: quasi tutto estendibile e usabile solo da BP, con mosse complesse di esempio in BP.

## Cosa c'è già (verificato nel codice)
- Passi di risoluzione colpo, modello di knockback, resolver di relazione, hit extensions: già `BlueprintNativeEvent` (GSTNCombatCore).
- Il fork accetta abilità BP con variabili `FSync*` come variabili BP (rollback incluso) e `K2_FillMoveContainer` per i move creator.
- `UGSTN_GA_Base`: `WaitCombatFrames` + `K2_OnCombatFramesElapsed`, contesto di attivazione, mira. Catena jab1/jab2 = finestra Tags sulla timeline + binding con `RequireTags`.
- Mancano: eventi BP sulla timeline delle mosse (`OnTimelineBegan` è solo C++), un "spara proiettile" chiamabile da una mossa (oggi è un'abilità a parte con `FireDelayFrames`), query input da BP, ganci BP visivi, creazione di una mossa senza script.

## Ganci da esporre (regole per non rompere i 62.5 Hz)
1. **Eventi di simulazione su `UGSTN_GA_BaseCombat`** (BlueprintNativeEvent): `OnTimelineBegan`, `OnTimelineKey(Tag)` (chiave Event attraversata), `OnHitConfirmed` (lato attaccante: colpito/parato, letto dallo stato combat), `OnTimelineEnded(Motivo)`; `OnTimelineFrame` solo opt-in (costo BP per frame, PC toaster).
2. **Azioni "richiesta" chiamabili da BP**: Spara proiettile (da un data asset `ProjectileDefinition`), Invia evento, Applica movimento (GE con move), Apri finestra tag. Ogni azione passa dai percorsi esistenti (spawn una volta sola sull'host + ghost; GE in coda con `RequestFrame == F`): mai scrittura diretta di stato.
3. **Query pure**: `WasCommandPressedSince`, `IsButtonHeld`, direzione stick, frame timeline, random deterministico (seme = frame + entità). Nessun input cablato: si nominano comandi per tag.
4. **Validatore editor** (GSTNCombatEditor): segnala nei grafi di simulazione variabili non `FSync*` modificate, nodi latenti (Delay, Timer), query di Mover/attore, random non deterministico.
5. **Ganci visivi** (solo presentazione, `BlueprintCosmetic`, dati const): `OnReactionStarted/Ended` sul componente reazioni, `OnHitPresented`. Non possono chiamare le azioni del punto 2.
6. **Creazione mossa senza script**: factory "Combat Move" nel Content Browser (abilità BP + timeline), comando "Aggiungi al kit", poi guida designer.

## Cosa resta C++ per forza
Combat tick, inbox e pipeline colpi (il nucleo), fragment di sync NP, tick extension, `LayeredMove` (girano per frame), matematica balistica e serializzazione rete, il fork GAS+NPP. I passi per frame "caldi" restano C++ (regola owner). BP = decisioni per chiave/evento, non per frame.

## Esempio: special a doppia pressione (2 proiettili)
**Variante A, solo dati (consigliata, come jab1/jab2):** `GA_Shot1` timeline: chiave "Spara" al frame N, finestra Tags `Move.Chain.Shot` + finestra cancel verso `Command.Special`. `GA_Shot2` spara il secondo (angolo/velocità diversi). Arma: `Command.Special` → Shot1; `Command.Special` + `RequireTags Move.Chain.Shot` → Shot2. Seconda pressione fuori finestra = nuovo Shot1; pressione anticipata presa dal buffer. Tutto stato combat già in rollback.
**Variante B, grafo BP:** una sola abilità; `OnTimelineKey(Fire)` → nodo Spara; contatore `FSyncInteger` per il secondo colpo; mostra i ganci. Nota: forse la A è già fattibile oggi con chiave Event → `GA_FireProjectile` attivata da evento (non verificato).
Gli asset BP si creano da script Python (dati) e i grafi con gli handler Blueprint di UE_MCP_Bridge sull'editor vivo: niente cablaggio a mano dell'owner.

## Checkpoint (Opus high per il codice, build su una macchina; costi stimati)
| CP | Contenuto | Costo |
|---|---|---|
| CP1 | Eventi di sim BP + query input + validatore + test C++ | $20-30 |
| CP2 | Azioni richiesta + `ProjectileDefinition` + chiave "Spara" sulla timeline (nel plugin Projectile, il Core resta pulito) | $25-35 |
| CP3 | Mosse di esempio: doppio proiettile A e B + 1 mossa complessa (es. colpo che su hit-confirm lancia un dash); video in-game, revisione cloud | $20-30 |
| CP4 | Ganci visivi BP (reazioni, presentazione colpi) | $8-12 |
| CP5 | Factory "Combat Move", aggiungi al kit, guida designer e doc architettura aggiornati | $20-30 |
| CP6 | (decisione a parte) versione pubblica: esempi e tag dentro il plugin, installer, fork vs GAS standard | da stimare, $40+ |
Totale CP1-CP5: circa $95-140. Ordine consigliato: CP1 → CP2 → CP3 (l'owner prova) → CP4 → CP5.

## Decisioni per l'owner (un solo messaggio)
1. Approvare il piano e l'ordine (consiglio: sì, partire da CP1).
2. Il doppio proiettile: variante A (dati, consigliata) o entrambe A e B come esempio (consiglio: entrambe, B serve a mostrare i ganci BP).
