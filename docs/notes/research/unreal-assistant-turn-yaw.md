# Unreal Assistant: turnaround start yaw vs capsule (2026-11-21, rule 14, review v27 r1 item 1)

Asked from a timed editor boot (Tools/UE59/ask_assistant_boot.py), conversation . Raw replies, not verified against our assets.

## Q. In GASP Mover, when a start/pivot clip carries authored root yaw (180 to 0 deg in 8 game frames at 60 fps, i.e. about 22 deg per frame), what limits how fast the mesh follows it: the Offset Root Bone node (rotation mode Accumulate, Max Rotation Error, Clamp To Rotation Velocity, Rotation Speed Ratio), the Steering node, or the Mover orient/turning rate (UCommonLegacyMovementSettings Turning Rate / Turning Boost)? In our capture the capsule turns about 10.5 deg per 60 fps frame (about 630 deg/s) and the visible body turn follows the capsule (180 deg in about 17 frames) instead of the clip's 8 frames. How do the stock reface clips reach their authored yaw (their root yaw goes 180 to 0 in about 40 game frames, slower than the capsule)?

La documentazione ufficiale descrive l'**Offset Root Bone** e lo **Steering** solo a livello concettuale. Le singole proprietà (**Max Rotation Error**, **Rotation Speed Ratio**, ecc.) e i parametri Mover `TurningRate`/`TurningBoost` non sono documentati nel dettaglio. Quello che segue unisce ciò che dice la documentazione GASP con quello che so del comportamento dei nodi. I punti non confermati sono segnati come ipotesi da verificare.

## Risposta breve

Con **Rotation Mode = Accumulate**, nessuno dei tre elementi dovrebbe di per sé legare la rotazione della mesh a quella della capsula. La documentazione GASP dice: *"the root bone is allowed to rotate independently from the capsule, so that rotation is entirely controlled with root motion, which gets steered within the motion matching node to always align toward the target"*.

Quindi lo yaw della mesh è circa *yaw del frame precedente + delta dello yaw di root motion dopo lo Steering*. La capsula conta solo se:

1. l'offset supera **Max Rotation Error** (clamp attorno alla capsula);
2. il target dello **Steering** deriva dalla rotazione corrente della capsula;
3. il delta di root motion che arriva all'Offset Root Bone *non è più* quello del clip, per esempio per il blend o per la selezione del Motion Matching.

I clip stock reface vi dicono che il caso 1 probabilmente non vi limita. Il loro yaw (~40 frame) è più lento della capsula (~17 frame), quindi la mesh resta *dietro* la capsula di oltre 100° e raggiunge comunque lo yaw del clip. Se **Max Rotation Error** fosse stretto, anche loro verrebbero trascinati dalla capsula.

## Ruolo di ciascun elemento

| Elemento | Effetto sullo yaw della mesh in Accumulate |
|---|---|
| Mover `TurningRate` | Ruota solo la capsula. Sulla mesh agisce solo in modo indiretto: con il clamp di **Max Rotation Error**, tramite il target dello Steering o tramite la traiettoria interrogata dal Motion Matching. |
| Mover `TurningBoost` | Accelera il cambio di direzione della *velocità* (anti-sliding), non l'orientamento. Non è un fattore qui. |
| **Max Rotation Error** | Limite massimo dell'offset tra mesh e capsula. Se lo yaw del clip supera la capsula di più di questo valore, la mesh resta a capsula ± errore e quindi gira alla velocità della capsula. |
| **Clamp To Rotation Velocity** / **Rotation Speed Ratio** | Per quanto ne so limitano la velocità con cui l'offset viene *recuperato* in modalità Interpolate, in proporzione alla velocità di rotazione dell'animazione. In Accumulate non dovrebbero avere effetto. Verificatelo sulla vostra build. |
| **Steering** | Aggiunge una correzione alla root motion perché l'orientamento futuro del clip coincida con il target. Se il clip arriva già al target, la correzione è circa zero. Se il target è la facing *attuale* della capsula, la correzione frena il clip fino a riportarlo alla velocità della capsula. |

## Perché i clip stock arrivano allo yaw scritto nel clip e i vostri no

Lo yaw dei clip stock è distribuito su circa 0,67 s. Quando il Motion Matching seleziona il clip, il blend in ingresso del Blend Stack copre solo la parte iniziale della rotazione. Quasi tutto lo yaw del clip arriva quindi intatto all'Offset Root Bone. Lo Steering compensa solo il piccolo errore residuo verso il target.

Nel vostro clip tutta la rotazione (8 frame, ~0,13 s) avviene *dentro* la finestra di blend. Ecco i meccanismi più probabili, in ordine di sospetto:

1. **Root motion attenuata dal blend.** L'attributo di root motion viene mescolato con il clip di corsa precedente, che ha yaw circa zero. Gran parte dei 180° si perde. Il resto viene recuperato dalla correzione dello Steering (guadagno proporzionale verso il target), che ha una dinamica simile a quella della capsula. Questo è compatibile con i circa 17 frame che misurate.
2. **Selezione del Motion Matching.** Il clip potrebbe essere selezionato da un frame successivo al primo (yaw già in parte consumato), oppure il Motion Matching potrebbe saltare a un altro asset durante la rotazione. Le traiettorie cambiano molto in pochi frame e il clip non vince più il confronto dei costi.
3. **Target dello Steering.** Se nella variante Mover il target è legato alla rotazione corrente o vicina della capsula, lo Steering frena attivamente un clip che anticipa la capsula. I clip stock, che restano dietro la capsula, non vengono mai frenati.
4. **Max Rotation Error** basso. È improbabile per quanto detto sopra, ma è il controllo più veloce da fare.

## Come isolare la causa

Cambiate una variabile alla volta e misurate di nuovo i frame della rotazione:

- **Rewind Debugger + Pose Search debugger.** Controllate il frame di ingresso del clip, se resta attivo per tutti gli 8 frame e i pesi del Blend Stack durante la rotazione. Questo è il test più importante.
- **Bypass dello Steering** (alpha 0 o condizione per tag del database, come fa GASP per i turn in place). Se la mesh segue il clip, il limite era lo Steering.
- **Max Rotation Error** molto alto o disattivato. Se non cambia nulla, escludetelo.
- **Blend Time del Motion Matching** ridotto per queste transizioni. Se la rotazione si avvicina agli 8 frame, la causa era l'attenuazione del blend.
- **`TurningRate`** più alto. Se la mesh accelera insieme alla capsula, conferma che esiste un legame indiretto (Steering, clamp o selezione).
- Comandi console: digitate `a.AnimNode.OffsetRootBone` e `a.AnimNode.Steering` e usate l'autocompletamento per trovare i cvar di debug e di attivazione. Per l'orientation warping è documentato `a.AnimNode.OrientationWarping.Debug`.

## Possibili soluzioni

- **Distribuire lo yaw su più frame.** È la scelta dei clip stock: la rotazione dura più del blend. Con 12-20 frame di gioco ne conservate gran parte senza toccare il grafo.
- **Blend più breve** o blend profile dedicato per le righe di transizione.
- **Steering ridotto o disattivato** durante questi clip, se è lui a frenare.
- **Allineare la capsula.** Se volete davvero una rotazione completa in 8 frame, alzate `TurningRate`, così la capsula non è più il riferimento lento per lo Steering e per la traiettoria.

Se mi incollate i valori del nodo Offset Root Bone, il calcolo del target dello Steering e il **Blend Time** del nodo Motion Matching nella vostra AnimBP, posso dirvi con più precisione quale di questi fattori vi limita.

**Riferimenti:**
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Motion Matching and the Game Animation Sample (Unreal Fest 2024)](https://dev.epicgames.com/community/learning/talks-and-demos/0y9w/unreal-engine-motion-matching-and-the-game-animation-sample-in-ue-5-4-unreal-fest-2024)
- [Pose Warping](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine)
- [Your First 60 Minutes With Motion Matching](https://dev.epicgames.com/community/learning/tutorials/lwlG/unreal-engine-your-first-60-minutes-with-motion-matching)

## Q. Same setup: what is the least-divergent stock-friendly way to let a turnaround start clip rotate the visible body faster than the capsule turning rate for its first 8-10 frames (Offset Root Bone settings per clip via a curve, Steering settings, a higher Mover Turning Rate or Turning Boost while starting, or something else)? Name exact node properties, where they are set in the GASP AnimBP (functions like Get_OffsetRootRotationMode), and the stock values if you know them. If the answer is the Mover turning rate, what do Turning Rate and Turning Boost mean exactly in UCommonLegacyMovementSettings?

## Risposta breve

L'opzione più stock-friendly è **nessuna delle quattro così come le hai elencate**. Con la configurazione GASP, la yaw visibile del corpo già *non* dipende dalla velocità di rotazione della capsula. Viene dalla yaw della root motion della clip più la correzione dello **Steering**. Quindi la leva corretta è far sì che la yaw della clip arrivi intatta nei primi frame:

1. Controlla che **Offset Root Bone** non abbia un clamp sulla rotazione (**Max Rotation Error**).
2. Accorcia il blend Motion Matching verso queste clip (`Get_MMBlendTime`).
3. Se serve, regola **Animated Target Time** dello **Steering** per clip, nel Blend Stack graph.

Alzare il turning rate di Mover è la scelta *meno* efficace per il corpo visibile.

## Perché

La [documentazione GASP](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine) descrive `Get_OffsetRootRotationMode` così:

- **Accumulate** (locomozione normale): la root *controruota* ogni cambiamento di rotazione della capsula. Così la rotazione del corpo è "entirely controlled with root motion, which gets steered within the motion matching node". La doc lo indica proprio per "turn starts, and turn pivots".
- **Release**: usato solo quando c'è un montage nel Default Slot. Riporta l'offset a zero e poi aggancia la root alla capsula.

La funzione è collegata al pin **Rotation Mode** del nodo **Offset Root Bone** nell'AnimGraph di `ABP_SandboxCharacter`. **Translation Mode** e **Translation Halflife** sono collegati a `Get_OffsetRootTranslationMode` e `Get_OffsetRootTranslationHalfLife`.

Conseguenza: in Accumulate la capsula può girare a 500°/s o a 50°/s e il corpo visibile gira comunque alla velocità della clip. Se nei primi 8-10 frame il corpo gira più lentamente della clip, la causa è a valle della root motion, non nella capsula.

## Cosa fare, in ordine di minima divergenza

### 1. Verifica che non ci sia un clamp (Offset Root Bone)
Seleziona il nodo **Offset Root Bone** nell'AnimGraph e controlla:
- **Max Rotation Error**: se è > 0, la root non può allontanarsi dalla capsula oltre quell'angolo. In quel caso è davvero la capsula a limitare il corpo. Il default del nodo è `-1` (nessun limite). La doc GASP lascia intendere che lo stock non abbia limiti di rotazione: la nota sul turn in place in aiming dice che "Additional limits need to be applied". Verifica comunque nel tuo progetto.
- **Clamp to Rotation Velocity** / **Rotation Speed Ratio**: se attivo, limita la velocità di recupero dell'offset.
- **Rotation Halflife** conta solo in **Interpolate**/**Release**. In Accumulate non accelera nulla.

Per questo *non* consiglio una curva per clip che cambi il **Rotation Mode** in `Get_OffsetRootRotationMode`. L'unica alternativa ad Accumulate che ha senso qui è Interpolate, e Interpolate *riporta* il corpo verso la capsula, cioè lo rallenta.

### 2. Blend time verso la clip di start (`Get_MMBlendTime`)
Durante il blend del Blend Stack la root motion (yaw inclusa) viene pesata tra la clip uscente (idle, yaw ~0) e quella entrante. Un blend di 0,2-0,3 s copre esattamente i tuoi primi 6-10 frame a 30 fps e "diluisce" la yaw iniziale. È la causa più probabile del sintomo che descrivi.
- In `Get_MMBlendTime` aggiungi un ramo per questi start: usa i tag del database corrente (lo stesso pattern già usato con `CurrentDatabaseTags`) oppure `IsStarting`, e restituisci un blend più corto.
- In alternativa usa un blend profile che privilegi root e lower body.

È una modifica locale a una funzione che GASP già usa per questo scopo.

### 3. Steering per clip (Blend Stack graph)
Fai doppio clic sul nodo **Motion Matching** per aprire il graph per-animazione. GASP ha già due rami di **Steering**, scelti in base al tag del database (turn in place sì/no), più `Enable Steering` e `GetDesiredFacing`. Le proprietà rilevanti del nodo sono:
- **Animated Target Time**: tempo in cui lo steering porta l'orientamento al target quando la clip *ha* rotazione nella root motion (questo è il tuo caso).
- **Procedural Target Time**: lo stesso, per clip *senza* rotazione nella root motion.
- **Enable Target Smoothing** e relativi parametri di smoothing.

Se la clip da sola non ruota abbastanza in fretta verso la facing desiderata, puoi abbassare **Animated Target Time** solo per queste clip. Leggi una curva dalla selezione corrente, come GASP già fa per disabilitare l'Orientation Warping per clip, e collegala al pin. Non ho trovato nella documentazione i valori stock di GASP per queste proprietà: leggili direttamente sui due nodi.

### 4. Autorialità della clip
Visto che in Accumulate il corpo segue la yaw della root, la soluzione più "pulita" resta concentrare la yaw nei primi frame della clip stessa. `Force Root Lock` con `REF_POSE` non interferisce: blocca la root nella posa, ma la root motion viene comunque estratta dalla traccia della root.

## Perché non alzare il turning rate di Mover

- Con Accumulate, una capsula più veloce viene semplicemente controruotata: il corpo visibile non cambia.
- Cambia invece la traiettoria e la facing predetta usate dalla query e da `GetDesiredFacing`. Questo può cambiare *quale* start viene selezionato, e quindi diverge di più dallo stock.
- Varrebbe la pena solo se volessi che la *capsula* (camera, gameplay, rete) sia allineata prima.

## `TurningRate` e `TurningBoost` in `UCommonLegacyMovementSettings`

La documentazione Mover ([Mover Features and Concepts](https://dev.epicgames.com/documentation/unreal-engine/mover-features-and-concepts-in-unreal-engine)) non descrive queste proprietà. Quanto segue viene dall'header del plugin: verificalo in `CommonLegacyMovementSettings.h` della tua build.

| Proprietà | Significato | Default plugin |
|---|---|---|
| `TurningRate` | Velocità massima di rotazione della capsula verso l'orientamento desiderato, in **gradi/secondo**. Un valore negativo significa rotazione istantanea (snap). Agisce solo sull'**orientamento**. | `500` |
| `TurningBoost` | Moltiplicatore che rende più rapido il cambio di **direzione della velocità** (non della facing) quando l'input cambia direzione, per ridurre lo scivolamento. Non aumenta la velocità e non ruota la capsula. | `8` |

Quindi `TurningBoost` è del tutto irrilevante per la rotazione visibile. `TurningRate` agisce solo sulla capsula, sul tick fisso di Network Prediction a 62,5 Hz. Il valore effettivo nel GASP Mover può essere sovrascritto nell'asset o nell'istanza di settings del personaggio: controllalo lì.

Se mi incolli i valori attuali di **Max Rotation Error**, dei due nodi **Steering** e il contenuto di `Get_MMBlendTime`, posso dirti con più precisione quale dei tre è il collo di bottiglia. Approfondimento su Steering e Offset Root Bone: [Motion Matching and the Game Animation Sample (Unreal Fest 2024)](https://dev.epicgames.com/community/learning/talks-and-demos/0y9w/unreal-engine-motion-matching-and-the-game-animation-sample-in-ue-5-4-unreal-fest-2024).
