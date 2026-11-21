# Unreal Assistant 2026-11-21 (wc8ksn): run pivot mirror and retime

## Q1
In GASP 5.9 (Mover sample), how does stock GASP provide the LEFT vs RIGHT version of a run 180 pivot/turn? The stock clips M_Relaxed_Run_Turn_L_180 and _R_180 both exist as separate assets. I want ONE custom run pivot clip (authored for the right turn) plus its mirror for the left turn, as rows of a nested chooser table that uses a PoseSearchColumn (rows have result type PoseSearch chooser row data with AnimAsset and MirrorOption = UnmirroredOnly / MirroredOnly / UnmirroredAndMirrored, Database = PSD Locomotion Transitions). 1) Is setting MirrorOption=MirroredOnly on a second row with the same AnimAsset the stock-correct way, and what does it need (MirrorDataTable on the PoseSearchSchema of that database? which stock mirror table, e.g. MDT_UEFN_Mannequin?) 2) Does the Blend Stack / animation output actually play the clip mirrored when the chosen database entry is mirrored, or must I bake a mirrored copy of the sequence? 3) Are root motion, curves (contact_l/contact_r swap?) and notifies (foley L/R) handled for a mirrored entry, or should I bake a mirrored asset instead? Give the recommended stock approach.

## A1
**In breve:** GASP stock non usa il mirroring per il pivot di corsa a 180°. `M_Relaxed_Run_Turn_L_180` e `M_Relaxed_Run_Turn_R_180` sono due clip catturate separatamente, e nel database ciascuna sta come voce non mirrorata. Il motore però supporta il mirroring a runtime dentro Pose Search, quindi una sola clip destra più la sua versione mirrorata è un'opzione valida. Funziona solo se sono vere tre condizioni: lo Schema ha una Mirror Data Table, il flag "mirrored" arriva al Blend Stack e le notify con lato L/R sono gestite.

La documentazione ufficiale tratta i Mirror Data Table e Pose Search a livello generale. Non descrive il collegamento tra la riga del Chooser con PoseSearchColumn e il Blend Stack. Per questo i punti segnati come *da verificare* vanno controllati nel tuo progetto 5.9.

## 1) Riga con `MirrorOption = MirroredOnly` e cosa serve

**Due righe o una sola**
- Una sola riga con `UnmirroredAndMirrored` basta: la ricerca indicizza entrambe le versioni e sceglie quella mirrorata quando traiettoria e posa corrispondono a una virata a sinistra.
- Due righe con lo stesso `AnimAsset` (`UnmirroredOnly` + `MirroredOnly`) servono solo se altre colonne del chooser devono filtrare per lato, per esempio una colonna con la direzione della virata. Altrimenti sono ridondanti.

**Requisito obbligatorio: la Mirror Data Table sullo Schema**
- Va impostata sullo **Pose Search Schema** del database `PSD Locomotion Transitions`. Nelle versioni recenti si trova nell'array **Skeletons**: ogni voce ha **Skeleton**, **Mirror Data Table** e **Role**.
- Senza questa tabella le voci mirrorate non vengono indicizzate.
- Usa la `UMirrorDataTable` creata per *lo stesso skeleton* indicato nello Schema. Se lo Schema usa lo skeleton UEFN Mannequin, serve una MDT di quello skeleton (`MDT_UEFN_Mannequin` va bene solo se esiste davvero nel progetto ed è basata su quello skeleton). Non sono riuscito a confermare nella documentazione quale MDT spedisca GASP.

**Cosa controllare nella MDT** ([Mirroring Animation](https://dev.epicgames.com/documentation/unreal-engine/mirroring-animation-in-unreal-engine)):
- **Mirror Axis** = `X`.
- Righe Bone per tutte le ossa skinnate, comprese quelle centrali (`root`, `pelvis`, `spine_*`, `neck`, `head`) con **Name** = **Mirrored Name**.
- Righe per le ossa IK usate da Leg IK e Pose Warping (`ik_foot_l`/`ik_foot_r`, ecc.).
- Righe **Curve** e **Animation Notify**: vedi il punto 3.

Dopo la modifica, riapri il database e controlla che le voci mirrorate compaiano nella lista con l'anteprima mirrorata.

## 2) Il Blend Stack riproduce davvero la clip mirrorata?

Sì, a runtime, senza una copia baked. Però solo se il flag "mirrored" del risultato della ricerca arriva al nodo:
- Il risultato di Pose Search ha un flag mirrored (`bIsMirrored` in `FPoseSearchBlueprintResult`).
- Il nodo **Blend Stack** deve ricevere quel flag sull'input **Mirrored** e avere una Mirror Data Table disponibile. *Da verificare* nel **Details** del nodo nella tua 5.9: il pin potrebbe essere nascosto di default.
- Se il flag non viene collegato, il database sceglie la voce mirrorata ma la clip viene riprodotta *non* mirrorata, cioè il personaggio gira a destra quando dovrebbe girare a sinistra. È l'errore più comune.

**Come verificare:** apri il **Rewind Debugger**, traccia **Pose Search**. Nella tabella di selezione controlla che la posa attiva risulti mirrorata quando giri a sinistra, poi confronta con la posa visibile ([Motion Matching Debugging](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-debugging-in-unreal-engine)).

## 3) Root motion, curve e notify

| Elemento | Con mirroring a runtime |
|---|---|
| **Root motion / traiettoria** | Gestito. Il root viene mirrorato sull'asse della MDT, e sia l'indicizzazione Pose Search sia l'estrazione nel Blend Stack usano la versione mirrorata. Con Mover non cambia nulla: la capsula è guidata dal movement model, e il root motion serve al matching e a **Offset Root Bone**. |
| **Curve** (`contact_l`/`contact_r` e simili) | Si scambiano *solo* se la MDT contiene righe di tipo **Curve** con quella coppia. Le Find and Replace Expressions coprono i suffissi `_l`/`_r`, ma controlla che le righe esistano davvero. Le curve senza lato (distanza, peso del pivot) non vanno toccate. |
| **Sync marker** | Servono righe di tipo **Sync Marker** se usi marker con lato. |
| **Notify** | Le notify identificate per nome si scambiano tramite righe **Animation Notify** nella MDT. Le notify Blueprint con il lato salvato come *proprietà* (tipico del foley L/R) **non** si invertono da sole. Nella notify devi usare **Is Triggered By Mirrored Animation** e invertire il lato in quel caso. |

## Approccio consigliato

1. **Mirroring a runtime (consigliato se vuoi un solo asset):**
   - Usa una riga con `UnmirroredAndMirrored`, oppure due righe se devi filtrare per lato.
   - Assegna la MDT dello skeleton corretto nello **Skeletons** dello Schema.
   - Completa la MDT con le righe Curve e Notify.
   - Collega il flag mirrored al **Blend Stack**.
   - Correggi le notify foley con **Is Triggered By Mirrored Animation**.
   - Verifica tutto nel Rewind Debugger.
2. **Copia baked (fallback):** scegli questa strada se vuoi replicare esattamente il modello stock (due asset `UnmirroredOnly`), o se hai molte notify o curve con lato difficili da gestire. Paghi una copia da mantenere, ma ottieni comportamento identico alle clip stock e notify e curve modificabili a mano. La documentazione non descrive un comando di "bake mirror" diretto per le Animation Sequence. Di solito si fa nel DCC oppure registrando l'output di un nodo **Mirror** con Sequencer.

Il fixed tick di Network Prediction a 62.5 Hz non influisce sul mirroring: la selezione e la riproduzione dell'animazione sono locali e cosmetiche.

Se mi incolli uno screenshot o la descrizione dei pin del tuo nodo **Blend Stack** e di come esce il risultato del chooser con PoseSearchColumn, ti dico esattamente dove collegare il flag mirrored.

## Q2
In GASP 5.9 stock, the run 180 turn clips M_Relaxed_Run_Turn_R_180_Lfoot (4.1 s: run-in, ~0.8 s turn core where the root decelerates to ~25 cm/s and rotates 180 degrees, run-out) are played from the Blend Stack with a dynamic play rate. Our Mover capsule now reverses direction in ~0.3 s (fast cartoon reversal). I want the pivot animation to match: the visible turn core should last ~0.3 s instead of ~0.8 s, run-in and run-out unchanged. Which is the stock-compatible way: (a) bake a time-warped copy of the clip (turn core compressed ~2.7x, curves contact/phase/enable_warping/steeringtargettime resampled, movedata_speed recomputed from the new root), (b) keep the clip and raise its play rate (how? chooser row output PlayRate, Get_DynamicPlayRate with movedata_speed curve?), (c) something else (e.g. pose matching start time skipping into the turn core)? What does Get_DynamicPlayRate in the stock ABP do with movedata_speed when the capsule speed is near 0 during the reversal, and is it clamped? Recommend one approach and list what to check.

## A2
Nella documentazione ufficiale non trovo descritti né `Get_DynamicPlayRate` né il curve `movedata_speed` dello stock GASP 5.9. Non posso quindi confermarti cosa fa la funzione né quali valori di clamp usa. Se mi incolli il grafo (o una descrizione nodo per nodo), te lo verifico. Ti spiego intanto come ragionare e quale opzione ti consiglio.

## Cosa fa (probabilmente) `Get_DynamicPlayRate` vicino a 0

Una funzione di play rate dinamico basata su un curve di velocità segue in genere questo schema:

```
PlayRate = Clamp( CapsuleSpeed / CurveSpeed(movedata_speed), Min, Max )
// con un guard: se CurveSpeed < soglia  ->  PlayRate = 1 (o il valore precedente)
```

Nel grafo stock controlla tre cose:
1. **Il guard sul denominatore.** Se manca, con `movedata_speed` vicino a 0 il rapporto esplode o diventa instabile. Il pavimento a ~25 cm/s nel turn core fa pensare a un curve scritto apposta per non scendere mai a 0.
2. **I valori Min/Max del clamp** (se c'è un nodo `Clamp` / `FInterp To`).
3. **La sorgente di `CapsuleSpeed`.** Con Mover + Network Prediction a 62.5 Hz deve leggere la velocità dello stato interpolato o di presentazione, non lo stato del fixed tick grezzo. Altrimenti il rate fa scatti tra un tick e l'altro.

Il punto chiave è che **un rapporto di velocità serve ad allineare le velocità, non a comprimere una fase**. Con la tua inversione da 0.3 s la capsula passa per 0 per un istante, ma per il resto della finestra va molto più veloce dei ~25 cm/s del clip. Il rapporto quindi sbatte sul clamp Max per tutto il core e poi oscilla. Il core risulta compresso in modo uniforme e rumoroso, solo se Max ≥ ~2.7, e la cadenza dei piedi accelera allo stesso modo.

## Confronto delle opzioni

| | Durata del core | Selezione (PoseSearchColumn vs PSD) | Determinismo / rete | Note |
|---|---|---|---|---|
| **(a) Clip time-warped baked** | 0.3 s esatti | La traiettoria indicizzata corrisponde a quella del Mover | Totale, nessun rate a runtime | Richiede di rigenerare curve, notify e indice |
| **(b) Play rate** | Approssimativa, dipende dal clamp | Resta il mismatch: il PSD indicizza un turn da 0.8 s | Il rate dipende dalla velocità campionata | Utile solo come prototipo |
| **(c) Start time nel core** | Invariata (0.8 s) | La pose matching sceglie già il punto d'ingresso | | Salta il run-in, ma non accorcia il core |

L'opzione (c) non risolve il problema: il Pose Search Column sceglie già il frame d'ingresso migliore, e saltare il run-in non cambia la durata del core.

Una variante di (b) sarebbe un curve custom `PlayRateScale` (1 → 2.7 → 1) moltiplicato dentro `Get_DynamicPlayRate`. Prima però dovresti verificare che il pin di play rate del Blend Stack venga rivalutato ogni frame per il sample attivo, e non solo al push. Anche così il mismatch di traiettoria nel database resta.

## Raccomandazione: (a)

È l'unica opzione che sistema sia la **selezione** sia il **playback**. Il Pose Search Column confronta la traiettoria predetta del Mover (inversione in 0.3 s) con le feature indicizzate nel `PSD Locomotion Transitions`. Con il clip originale da 0.8 s il costo è sempre alto e lo start time scelto è un compromesso. Col clip compresso, query e database dicono la stessa cosa, e `Get_DynamicPlayRate` resta vicino a 1, quindi il suo comportamento vicino a 0 diventa irrilevante.

### Come fare il retime dentro UE
- Metti il clip in una Level Sequence su uno Skeletal Mesh del Mannequin.
- Sulla sezione Skeletal Animation aggiungi **Time Warp > Time Warp Curve** ([Time Warp Track](https://dev.epicgames.com/documentation/unreal-engine/time-warp-track-in-unreal-engine)). Imposta la mappatura a pezzi: identità nel run-in, pendenza ~2.7 nel core, identità nel run-out. La durata totale passa da ~4.1 s a ~3.6 s.
- Fai il bake in un nuovo Anim Sequence. In alternativa puoi farlo in un DCC.

### Cosa controllare
1. **Frame rate del bake.** A 30 fps un core da 0.3 s sono solo 9 frame per 180°. Valuta di esportare a 60 fps.
2. **Root motion.** Stesse impostazioni del clip originale (**Enable Root Motion**, **Force Root Lock**). Controlla che la rotazione di 180° e la decelerazione siano nel root, non nel pelvis.
3. **Curve:**
   - `movedata_speed`: ricalcolalo dal nuovo root, non ricampionarlo. La velocità nel core cambia di ~2.7x.
   - `contact` / `phase`: rigenerali dalle posizioni dei piedi, oppure rimappa i tempi con la stessa funzione di warp e verifica che i contatti restino sui frame giusti.
   - `enable_warping`: basta rimappare le chiavi nel tempo.
   - `steeringtargettime`: se è un valore *temporale* (tempo rimanente al target), anche i **valori** nel core vanno divisi per ~2.7, non solo le posizioni delle chiavi. È l'errore più facile da fare.
4. **Notify e notify state.** Il bake non li porta con sé. Ricrea e rimappa i Pose Search Block Transition / Branch In, i sync marker e i notify dei passi.
5. **Database.** Sostituisci il clip in `PSD Locomotion Transitions` (o aggiungi il nuovo clip ed escludi l'originale). Controlla **Sampling Range** e **Mirror Option**, ricostruisci l'indice e aggiorna le righe del Chooser che puntano all'asset.
6. **Debug.** Con Rewind Debugger + Pose Search Debugger verifica il costo, lo start time scelto e che si entri prima del core.
7. **Runtime:**
   - Durante il clip `Get_DynamicPlayRate` deve stare intorno a 1. Se no, controlla guard e clamp.
   - La rotazione di 180° in 0.3 s non deve essere frenata dallo Steering, né dalla finestra `enable_warping`.
   - L'Offset Root Bone in modalità Interpolate non deve accumulare deriva durante l'inversione.
   - Leg IK / foot lock: nessuno scivolamento nel core compresso.
8. **Variante mirror** (`_Rfoot` e Turn_L): applica lo stesso trattamento a tutte, altrimenti la selezione preferirà le versioni lente.

Riferimenti: [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine), [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine), [Time Warp Track](https://dev.epicgames.com/documentation/unreal-engine/time-warp-track-in-unreal-engine).

Se mi incolli `Get_DynamicPlayRate`, ti dico con precisione cosa succede vicino a 0 e se il clamp va ritoccato dopo il bake.
