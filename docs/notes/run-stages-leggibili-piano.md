# Stadi di corsa leggibili (riferimento Mario 3D World / Luigi) - piano di design

Owner 2026-11-20 20:37Z (ambito: stadi di corsa di tutti i personaggi giocabili). Solo piano, niente implementazione.
Nota owner 20:38Z: in 3D World il passaggio corsa -> sprint è quasi istantaneo perché si tiene X: quel fattore (tempo
guidato dal tasto) si IGNORA. Si prende la LEGGIBILITÀ: ogni stadio è un gradino visibile, con velocità, animazione ed
effetti che cambiano INSIEME.

## 1. Cosa mostra il video (IMG_9588.mov, 30 fps, ripresa da telefono: Luigi ~40 px, tempi +-1 frame)
Griglie: docs/notes/reference-dossiers/run-stages-mario3dw/ (3 immagini, regola 4). Tempi assoluti nel video.

| Fase | Corsa A | Corsa B | Cosa si vede |
|---|---|---|---|
| Corsa base | 1,4-3,0 s | dopo lo skid 6,2-6,4 | bracci che oscillano normali, busto dritto, polvere rara |
| Sprint 1 (stadio 2) | da ~3,73 | 6,5-6,9 | cambio di POSA netto: busto avanti, braccia aperte e tirate indietro; uno sbuffo di polvere A OGNI passo |
| Tell dello sprint 2 | 3,80-4,13 (10 f = 0,33 s) | 6,97-7,33 (11 f = 0,37 s) | scintille arancioni dal tallone a ogni appoggio (2-3 passi), polvere più grossa |
| Pausa / carica | 4,13-4,37 (~0,23 s) | 7,33-7,43 (~0,1 s) | in B Luigi si butta quasi orizzontale (posa di lancio = anticipazione) |
| Scoppio (entrata stadio 3) | 4,40 | 7,50 | lampo giallo 2-3 frame, poi anello di polvere che CRESCE (4,47-4,73) o scia gialla dietro (7,57-7,83, ~8 f) |
| Sprint 2 | dopo 4,47 | dopo 7,57 | velocità sale in 1-2 frame (Luigi si stacca dalla nuvola che resta ferma), gambe sfocate = frenesia |

Principio estratto (generale, non ricetta): **stadio 2 = cambio di posa + passo di velocità, nessun lampo;
stadio 3 = annuncio (tell ~0,33 s) -> carica breve -> scoppio, e nello stesso frame velocità, clip ed effetti cambiano.**
Il contrasto tra i due è voluto: lo sprint 2 è "l'evento", lo sprint 1 no.

## 2. Tensione da segnalare (regola 12)
Il run start attuale (v4r5, bloccato) usa una rampa CONTINUA alla Toad: stadio 1 rampa 62 f / 0,15, stadio 2 rampa
30 f / 0,25 (runstart-accel-1, divergenza 13). Il riferimento chiede gradini con velocità istantanea. Proposta: la rampa
resta SOLO per partire da fermo (ambito della richiesta Toad = run start); i passaggi 1->2 e 2->3 diventano gradini.
Decisione 1 sotto. Il v4r5 non si tocca (regola 13b).

## 3. Design per stadio (Overlord = stile BRUTO, principio run start: motore che va su di giri)
| | Stadio 1 Corsa | Stadio 2 Sprint 1 | Stadio 3 Sprint 2 |
|---|---|---|---|
| Entrata sim | rampa da fermo (attuale) | gradino: velocità a MaxSpeed in ~3 frame sim | gradino: in ~2 frame sim, nello stesso frame dello scoppio |
| Clip | v4r5 -> loop Run stock | loop sprint pesante: testa giù, spalle avanti, pugni che pompano | 0,1 s posa di carica (tuffo in avanti) -> loop sprint 2 frenetico (toro che carica, braccia dietro) |
| Tell | - | - | ultimi ~20 frame sim (0,32 s) dello stadio 2: scintille dai talloni a ogni appoggio + lean additivo che si approfondisce |
| VFX | polvere passi attuale | sbuffo a ogni appoggio, poco più grande | scoppio: lampo 2-3 frame + anello d'urto a terra (polvere che cresce, mai ristretta) + scia ~0,25 s; poi polvere continua |
| Lettura | normale | "si è piegato" | "è partito un motore" |

## 4. Mappa sui dati e su GASP stock (regola 14)
- **Run Stages (FGSTN_RunStage, DA_Locomotion_Default)**: campi nuovi per stadio, tutti a 0 = comportamento attuale:
  `EntryBurstFrames` + `EntryAccelMultiplier` (riusa la logica del Boost Dash L3, già esistente, sul cambio di stadio
  naturale); `TellFrames` (frame prima di FramesToNextStage in cui la sim espone "tell attivo"). Il tell è deterministico
  (contatore già in sync state), quindi in rete arriva uguale. Getter BP nuovi su UGSTN_RunPresentationComponent:
  IsRunStageTell, RunStageEnteredFrame. Rampa: AccelRampFrames dello stadio 2 a 0 (dati, 0 $).
- **Clip = righe chooser**: GASP stock ha la colonna Gait (Walk/Run/Sprint) e cambia loop col pose match. Stadio 1 =
  Run, stadi 2 e 3 = Sprint stock; colonna variante nostra `RunStage` nel chooser annidato per kit (con override per skin)
  SOPRA la riga stock (come da LOCO CLIP SELECTION), fallback = sprint stock. La posa di carica dello stadio 3 = riga di
  transizione (come le start in Run F Loco Transitions) con colonna `IsStageEntry`.
- **Divergenze da scrivere prima del commit** (in gasp-divergenze.md): (a) blend corto (~0,1 s) sul cambio di stadio
  invece del default stock, perché il gradino deve leggersi; (b) riga di transizione "stage entry" che lo stock non ha
  (stock: il cambio gait passa solo dal pose match sul loop). Prima: chiedere all'Unreal Assistant come GASP stock gestisce
  Run->Sprint e se esiste una via stock per un'entrata con clip di transizione.
- **VFX = eventi del presentation component** (stessa tabella del run start dust): RunStage.Tell.Footstep,
  RunStage.Enter.2, RunStage.Enter.3, RunStage.Trail.3. Ogni VFX si descrive PRIMA all'Unreal Assistant:
  - Tell: "scintille arancio-oro dal tallone dello stivale corazzato a ogni appoggio, 6-10 particelle brevi (0,15 s),
    traiettoria all'indietro e in alto, più qualche sassolino scuro; stile DK Bananza: comportamento realistico, palette
    stilizzata; deve leggersi da camera alta a 15 m".
  - Scoppio: "lampo caldo 2-3 frame centrato sui piedi, un solo anello d'urto a terra, polvere che cresce e sfuma ad
    alfa 0 senza mai restringersi, scia di 0,25 s dietro il personaggio; nessun bagliore oltre 3 frame".
  - Scia sprint 2: "polvere continua a ogni appoggio, più larga di quella dello stadio 2".

## 4b. Tutto regolabile (owner 20:43Z: "tutto regolabile come al solito")
Ogni valore del §3 è un campo dati, nessun numero nel codice o nelle clip: per stadio in DA_Locomotion_Default > Run Stages
(MaxSpeed, FramesToNextStage, AccelRampFrames, EntryBurstFrames, EntryAccelMultiplier, TellFrames, blend del cambio
stadio, profondità del lean del tell nel LocomotionAnimSet); VFX in riga di tabella eventi (sistema, scala, durata,
attivabile per stadio). Tutti a 0/vuoti = comportamento attuale. Ogni campo con categoria e tooltip (CLAUDE.md, FUTURE).

## 5. Nuovo vs esistente
Esiste: 3 stadi + FramesToNextStage + NextStageSpeedFraction; Boost Dash (burst di accelerazione); lean additivo
(LocomotionAnimSet); eventi VFX del presentation component; run start v4r5. Nuovo: campi tell/burst per stadio, 2 getter,
colonna RunStage + riga stage entry, 2 loop sprint e 1 posa di carica per l'Overlord, 3 VFX.

## 6. Fasi proposte (dopo le decisioni)
S1 codice + dati (Opus medium, cloud + build fisso, ~8 $). S2 VFX via Unreal Assistant (fisso, ~10-12 $, review Fable
high). S3 clip Overlord (Cascadeur, fisso, ~20-25 $ con dec. 3B, ~35 $ con 3A; brief con tratti d'identità, regola 13a).
Totale ~40-45 $.

## 7. Decisioni owner (max 3)
1. **Velocità tra gli stadi.** A) rampa solo da fermo, 1->2 e 2->3 a gradino (consigliata: leggibile, non tocca il v4r5);
   B) gradini ovunque, anche la partenza (via la rampa Toad); C) rampa continua ovunque, cambiano solo anim ed effetti.
2. **Annuncio dello sprint 2.** A) scintille dai talloni + lean che si approfondisce per ~0,33 s, poi 0,1 s di posa di
   carica (consigliata: come il riferimento); B) solo scintille, nessuna posa di carica (più semplice, meno anticipo);
   C) come A più un mini-rallentamento sim prima dello scoppio (più "carica", ma si sente un freno).
3. **Clip dello sprint 1.** A) loop nuovo dell'Overlord per entrambi gli sprint (~35 $ per S3); B) sprint stock GASP per
   lo sprint 1 + lean additivo marcato, clip nuove solo per carica e sprint 2 (consigliata, ~20-25 $; si passa ad A se
   lo stock non si legge abbastanza piegato in review).
Intanto resta ferma solo l'implementazione; nessun altro lavoro dipende da queste risposte.

## 8. Decisioni owner 2026-11-20 20:47Z: 1A 2A 3B
- 1A: rampa solo da fermo (stadio 1); 1->2 e 2->3 a gradino (stadio 2 AccelRampFrames -> 0, burst per stadio).
- 2A: tell = scintille dai talloni + lean che si approfondisce (~0,33 s) + 0,1 s di posa di carica, poi scoppio.
- 3B come PONTE: sprint stock GASP + lean marcato finché le clip nuove non ci sono.
- Clip richieste dall'owner (3 loop Overlord, riferimento in arrivo dall'owner; lettura scelta: "fase 2" = stadio 2):
  stadio 1 = corsa LENTA alla Ganon; stadio 2 (sprint 1) = corsa a BRACCIA APERTE; stadio 3 (sprint 2) = sprint un po'
  più veloce. Ciascuna = riga con colonna RunStage sopra la riga stock (fallback stock). S3 parte quando arriva il riferimento;
  la posa di carica dello stadio 3 resta una clip corta a parte. Se "fase 2" voleva dire sprint 2, si scambiano solo le righe.
