# Unreal Editor AI assistant: replies on GASP Mover 5.9 (2026-11-20, thread gasp-structure-1)

Asked with Tools/UE59/ask_assistant_boot.py (one conversation). The assistant cannot inspect our assets; it answers from engine
code/docs plus the context below. Replies are in Italian, verbatim. Question 7 (copy pitfalls) timed out (run limit 15 min).

Context sent:

Project: UE 5.9, Game Animation Sample (GASP) Mover variant: SandboxCharacter_Mover_ABP with DDCvar.LocomotionSetupMover=1. AnimGraph: BlendListByInt index 0 = MotionMatching node (database chooser CHT_PoseSearchDatabases_Mover), index 1 = TwoWayBlend of the 'State Controller' state machine (outputs no pose) and a Blend Stack node (tag StateMachineBlendStack). Each state's On State Entry calls SetBlendStackAnimFromChooser(StateName): EvaluateChooser on CHT_MoverCharacterAnimations_PoseMatch (params: ABP context, output struct S_CHT_MoverCharacterAnimations_OUT {StartTime, BlendTime, BlendCurve, BlendProfile, Tags}, PoseHistoryReference from the PoseSearchHistoryCollector node), then ForceBlendOnNextUpdate on the Blend Stack. Root table keys on StateName; nested tables (Locomotion Transitions -> Run F Loco Transitions etc.) filter with Speed2D, FutureFacingDelta, IsPivoting, MovementDirection_Recent and end with a PoseSearchColumn whose cells reference internal databases (subobjects 'PSD Locomotion Transitions', 'PSD Locomotion Stops', 'PSD Locomotion Spin Transitions', 'PSD Locomotion Loops'). Turn In Place tables have no PoseSearchColumn. Exit rules: IsAnimationAlmostComplete (non-looping and time remaining <= 0.75 s), NoValidAnim, NotifyTransition_ToLoop / Re-Transition bools set by 'Transition:' anim notifies, Re-Enter aliases on direction/gait/stance change, IsPivoting, FutureFacing delta > 60 deg. Character uses Mover 2.0 + Network Prediction at 62.5 Hz. We add our own stylised (cartoony, Smash-like) clips. Answer concretely, name exact classes, properties, curves, notifies, cvars.

## Q: 1) In this setup, what exactly does a PoseSearchColumn in a Chooser Table do at evaluation? Among the rows that pass the filter columns, does it pick both the row (asset) AND the start time by pose search against the pose history? Where does the matched start time end up (the OUT struct StartTime column is 0 in every row, yet the Blend Stack starts mid-clip)? How are the chooser's internal PoseSearch databases built, which schema do they use, and what do the cell options bUseSingleSample, SamplingRange, BranchInId, bDisableReselection and MirrorOption change?

## 1. Cosa fa `FPoseSearchColumn` in fase di valutazione

La documentazione ufficiale non descrive in dettaglio la colonna **Pose Match** (`FPoseSearchColumn`, plugin PoseSearch, `PoseSearch/Chooser/PoseSearchChooserColumn.h`). Quanto segue si basa sul codice del plugin: confrontalo con l'header della tua build 5.9, perché la colonna è marcata *Experimental*.

- È una colonna di **scoring**, non di filtro booleano. Riceve l'insieme di righe che ha superato le colonne precedenti (`Speed2D`, `FutureFacingDelta`, `IsPivoting`, `MovementDirection_Recent`).
- Costruisce una query a partire dal `FPoseHistoryReference` (il pin **PoseHistoryReference** che passi dal nodo `PoseSearchHistoryCollector`), usando lo **schema del database interno**.
- Esegue la ricerca solo sugli asset delle righe ancora valide e prende il campione con costo minimo.
- **Sì, sceglie entrambe le cose:** la riga (l'asset) *e* il tempo del frame migliore dentro quell'asset. Produce anche l'eventuale flag di mirror e il costo.

Le tabelle Turn In Place non hanno questa colonna. Lì vince la prima riga che passa i filtri, e lo `StartTime` è quello della colonna di output (0).

## 2. Dove finisce lo start time

La colonna ha dei binding di **output** propri, visibili nel pannello **Details** quando selezioni l'header della colonna: start time, mirror e cost.

Nel Chooser gli output della riga selezionata vengono scritti **in ordine di colonna**. In GASP il binding dello start time della colonna Pose Match punta allo stesso `S_CHT_MoverCharacterAnimations_OUT.StartTime`. Siccome la colonna sta in fondo, **sovrascrive lo 0** scritto prima dalla colonna di output `StartTime`. Poi `SetBlendStackAnimFromChooser` copia il valore nell'input start time del Blend Stack, e il clip parte a metà.

Per verificarlo:
- controlla il binding sull'header della colonna;
- nel **Rewind Debugger** guarda la traccia Chooser (riga scelta) e la traccia Pose Search (frame e costo).

**Conseguenze pratiche:**
- Se sposti la colonna Pose Match *prima* della colonna `StartTime`, torni a 0.
- Se il mirror output non è collegato a un campo dell'OUT struct e al Blend Stack, un risultato mirrored verrà riprodotto *non* mirrored: la posa non coinciderà.

## 3. Come sono costruiti i database interni

- Ogni colonna possiede un `UPoseSearchDatabase` **istanziato come subobject del Chooser Table**. Sono i tuoi `PSD Locomotion Transitions`, `PSD Locomotion Stops`, `PSD Locomotion Spin Transitions` e `PSD Locomotion Loops`, uno per colonna nelle tabelle annidate.
- Ogni cella della colonna è una voce `FPoseSearchDatabaseAnimationAsset` di quel database. Punta all'asset risultato della riga.
- L'indicizzazione è identica a quella di un database normale: campionamento al **Sample Rate** dello schema, normalizzazione, DDC asincrono. Quando aggiungi o modifichi righe, il database si reindicizza. Durante la build può non restituire risultati, cosa che nel tuo grafo si manifesta come `NoValidAnim`.
- **Schema:** è la proprietà **Schema** del database interno. La vedi espandendo il database interno nel pannello Details della colonna; non c'è uno schema implicito. I canali (Pose, Trajectory, Velocity, Heading, Phase) devono essere ricavabili dalla pose history. I bone del canale Pose devono essere tra quelli raccolti dal `PoseSearchHistoryCollector`, e la traiettoria deve essere generata dal collector o dal predictor di Mover. Anche la **Mirror Data Table** dello schema è necessaria per il mirroring.
- Valgono le stesse opzioni del database: **Pose Search Mode**, **Base Cost Bias**, **Looping Cost Bias** e così via.

## 4. Opzioni di cella

| Proprietà | Effetto |
|---|---|
| `SamplingRange` (`FFloatInterval`) | Intervallo di tempo indicizzato dell'asset. Con `0,0` viene indicizzato tutto il clip. **È il controllo principale su dove può partire il clip.** |
| `MirrorOption` | `UnmirroredOnly` / `MirroredOnly` / `UnmirroredAndMirrored`. Con l'opzione combinata indicizza entrambe le versioni e il match può restituire mirror = true. Richiede la Mirror Data Table nello schema e il binding del mirror output. |
| `bDisableReselection` | Evita di riselezionare pose dello stesso asset quando l'asset corrente è quello in riproduzione (logica "continuing pose"). In una valutazione one-shot all'entrata di uno stato ha poco o nessun effetto. Verificalo col debugger. |
| `BranchInId` | Diverso da 0 solo per le voci generate automaticamente da `UAnimNotifyState_PoseSearchBranchIn`: il `SamplingRange` diventa la finestra del notify. Per le voci manuali lascialo a 0. |
| `bUseSingleSample` | Rilevante per i **Blend Space**: campiona un solo punto (`BlendParamX/Y`) invece della griglia (`bUseGridForSampling`, `NumberOfHorizontal/VerticalSamples`). Su una Anim Sequence non ha effetto. |

## 5. Consigli per i clip stilizzati (stile Smash)

- **Limita `SamplingRange`** di transitions e stops alla parte iniziale. Così il match non salta l'anticipation e non parte oltre il notify `Transition:`. Se lo start time cade dopo il notify, `NotifyTransition_ToLoop` non scatta mai e resti nello stato fino a `IsAnimationAlmostComplete`. Ricorda anche che quei 0,75 s si contano dal punto di partenza effettivo.
- Applica `UAnimNotifyState_PoseSearchBlockTransition` sui frame di smear e squash and stretch estremi, dove non vuoi entrare.
- Usa `UAnimNotifyState_PoseSearchModifyCost` per favorire o penalizzare zone del clip senza toccare lo schema. Esiste anche `UAnimNotifyState_PoseSearchExcludeFromDatabase`.
- Con pose molto esagerate, considera uno **schema dedicato** per i tuoi database interni: più peso a traiettoria e velocità del root, meno ai bone di posa. Altrimenti la distanza di posa rispetto alla history, spesso generata da clip realistici, domina e sceglie frame strani.
- Con Mover e Network Prediction a 62,5 Hz, la pose history viene campionata al tick dell'AnimInstance. Controlla che traiettoria e velocità nella history siano quelle smussate e non a scatti. Una query rumorosa produce start time instabili.

## Riferimenti
- [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine) (schema, canali, database)
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Motion Matching and the Game Animation Sample in UE 5.4](https://dev.epicgames.com/community/learning/talks-and-demos/0y9w/unreal-engine-motion-matching-and-the-game-animation-sample-in-ue-5-4-unreal-fest-2024) (Branch In, chooser annidati, Blend Stack)
- [Choosing How Your Game Animation Feels](https://dev.epicgames.com/community/learning/talks-and-demos/234j/unreal-engine-choosing-how-your-game-animation-feels-unreal-fest-2024) (colonne di filtro e di scoring)

Se mi incolli i binding della colonna (header **Details**) e lo schema dei database interni, posso dirti se il mirror output è davvero collegato e quali canali conviene ripesare per i tuoi clip.

## Q: 2) Root motion: GASP clips carry root motion while the Mover capsule moves by its own simulation. How do OffsetRootBone, Orientation Warping, Stride Warping, Steering and the dynamic play rate (MoveData_Speed curve, Enable_Warping curve) reconcile the clip root with the capsule? What breaks, concretely, when a custom start/stop/pivot clip has root displacement much smaller than the capsule's (for example a stop clip that travels 125 cm while the capsule brakes over 360 cm, or a start clip with 0 cm root motion)? What is the recommended rule for authoring custom clips' root motion?

## Il modello: chi comanda cosa

Nel setup Mover di GASP il **capsule è l'unica verità**. Mover 2.0 e Network Prediction simulano la posizione a 62.5 Hz e *non consumano* la root motion delle clip di locomozione (vale solo per i montage). La root della clip quindi non muove nulla: è un **piano** che dice al grafo come il corpo *vorrebbe* muoversi. Ogni frame, la differenza tra piano (root della clip) e realtà (delta del capsule) viene assorbita a strati.

| Strato | Cosa compensa | Come | Limite strutturale |
|---|---|---|---|
| **Selezione** (Pose Search, PoseSearchColumn, trajectory channel) | Sceglie clip e frame la cui traiettoria futura somiglia alla query del capsule | Costo trajectory e pose | Se nessuna clip ha la traiettoria giusta, sceglie "la meno peggio" con costo alto |
| **Dynamic play rate** (`MoveData_Speed`) | Differenza di *velocità scalare* | PlayRate ≈ `CapsuleSpeed / MoveData_Speed(t)`, con clamp | Non funziona più quando `MoveData_Speed` tende a 0. Accelerare la clip anticipa anche la sua fine |
| **Stride Warping** | Differenza di velocità residua, sulle gambe | `StrideScale = LocomotionSpeed / RootMotionSpeed` (dalla documentazione), con **Clamp Result** | Stessa divisione per ~0. Oltre il clamp la differenza resta |
| **Orientation Warping** | Differenza di *direzione* (root vs. velocità del capsule) | `RotationBetween(RootMotionDirection, LocomotionDirection)`, ridistribuito su spine e IK feet | Serve una direzione della root: con velocità della root ~0 la direzione non esiste e il warp si spegne |
| **Steering** | Differenza di *facing* | Aggiunge rotazione alla root motion per raggiungere la facing desiderata entro il tempo target | Agisce sulla root motion. Senza root motion (sotto la soglia di velocità del nodo) non c'è niente da ruotare |
| **Offset Root Bone** | *Tutto il resto*, in posizione e rotazione | Integra la root motion del grafo e tiene la mesh separata dal capsule | Raggio massimo di traslazione. Senza collisioni. In stato fermo fa Release |
| `Enable_Warping` (curva) | Esclude i frame in cui il warp rovina la posa (plant del pivot) | Gate sull'alpha dei warp nel Blend Stack Graph | Mentre vale 0 l'errore finisce tutto su Offset Root Bone |

Dettagli chiave sull'**Offset Root Bone**, dalla documentazione GASP:
- **Rotation mode = Accumulate**: la root contro-ruota i cambi di yaw del capsule, quindi la rotazione della mesh viene *solo* da root motion più Steering.
- **Translation mode = Interpolate**: la root può allontanarsi dal capsule entro un raggio fisso e torna sempre verso il centro con un'emivita (`Get_OffsetRootTranslationHalfLife`).
- **Release** quando non c'è movimento, in aria o durante un montage: l'offset sfuma e la mesh torna al centro del capsule.

Un punto importante: l'Offset Root Bone *non annulla* un errore che cresce sempre. Lo nasconde finché resta entro il raggio. Oltre il raggio, la mesh viene trascinata alla velocità del capsule mentre le gambe animano un'altra velocità, e quello è foot sliding.

## Cosa si rompe: stop da 125 cm contro frenata da 360 cm

Esempio: capsule a 600 cm/s, frenata da 360 cm, quindi decelerazione ≈ 500 cm/s² e circa 1.2 s di frenata. La clip arriva a zero in circa 0.5 s.

1. **Selezione**: la query vuole 360 cm di traiettoria residua, la clip ne offre al massimo 125. Il PoseSearchColumn prende il frame iniziale con un costo trajectory alto. Se nella tabella ci sono alternative, rischi entrate incoerenti o `NoValidAnim`.
2. **Play rate**: il capsule va più veloce della root, quindi il rapporto supera 1 e satura sul massimo del clamp. La clip finisce *ancora prima*.
3. **Stride Warping**: la scala supera 1, satura sul clamp e il pelvis scende (**Pelvis Adjustment**). Ottieni falcate lunghe in una posa di arresto. Verso la fine `RootMotionSpeed` scende sotto la soglia e il warp si spegne di colpo, con un pop.
4. **Offset Root Bone**: la root frena prima del capsule, quindi la mesh resta indietro. In pochi decimi di secondo raggiunge il raggio massimo, poi viene trascinata. Risultato: piedi piantati che pattinano per oltre 200 cm.
5. **Uscita dallo stato**: `IsAnimationAlmostComplete` (≤ 0.75 s rimanenti) scatta mentre il capsule frena ancora. Vai in Idle con capsule in movimento, la traslazione passa in **Release** e la mesh "scivola" al centro.
6. **Steering e Orientation Warping**: a fine clip la root è sotto le soglie, quindi l'eventuale errore di facing resta e viene recuperato dalla clip successiva con uno scatto.

## Cosa si rompe: start con 0 cm di root motion

1. `MoveData_Speed = 0`: il play rate non è definito e il clamp lo porta al massimo. L'anticipazione cartoon viene riprodotta accelerata e il timing va perso.
2. **Stride e Orientation Warping**: senza velocità della root non hanno né scala né direzione, quindi non fanno nulla. Se l'input è a 90° dal facing, la start parte nella direzione sbagliata.
3. **Steering**: sotto la soglia di root motion non ruota. Con rotation **Accumulate** la mesh *non gira affatto* verso il movimento, poi la clip seguente fa uno scatto di rotazione.
4. **Offset Root Bone**: il capsule parte, la mesh resta ferma, l'offset satura in circa 0.1-0.2 s e poi la mesh viene trascinata in posa di windup.
5. **Pose Search**: la traiettoria piatta della clip ha costo molto alto contro una query che accelera. Se la clip non è forzata, non viene scelta in modo affidabile.

## Regola di authoring consigliata

**La root della clip deve riprodurre il capsule: stessa distanza, stesso profilo di velocità e di yaw, con la stessa durata. Lo stile si mette sul corpo, non sulla root.**

In pratica:

1. **Registra il capsule reale** con i tuoi parametri Mover (in `UCommonLegacyMovementSettings`: **MaxSpeed**, **Acceleration**, **Deceleration**, **TurningRate**, **GroundFriction**) per ogni situazione: start, stop, pivot, per gait e direzione. Usa il Rewind Debugger e importa quella curva come root track nel DCC. Anima il corpo sopra quella root.
2. **Oppure adatta il movimento alle clip.** Per uno stop da 125 cm partendo da 600 cm/s serve `a = v² / 2d = 600² / 250 ≈ 1440 cm/s²`. Per uno stile "alla Smash" la soluzione più coerente è spesso questa: movimento snappy *e* clip snappy.
3. **Tolleranze**:
   - errore di posizione ben dentro il raggio dell'Offset Root Bone (indicativamente meno di metà raggio);
   - rapporto di velocità dentro il clamp di play rate e Stride, senza saturarlo mai.
4. **Nessuna root a velocità zero mentre il capsule si muove.** Per l'anticipazione hai due opzioni:
   - un piccolo "creep" in avanti nella root, con velocità non nulla e coerente con l'accelerazione;
   - un ritardo del movimento *lato gameplay*, ad esempio una fase di accelerazione iniziale bassa in Mover, così il capsule è davvero fermo durante il windup.
5. **Direzione e facing**: nelle clip direzionali la velocità della root deve puntare nella direzione del movimento. Lo yaw della root deve seguire quello del capsule (in orient-to-movement) o restare costante (in strafe), così Steering corregge solo residui piccoli.
6. **Stop**: il momento in cui restano 0.75 s di clip deve arrivare *dopo* che il capsule è fermo. In altre parole, la coda di settle deve durare almeno 0.75 s dopo l'arresto della root.
7. **Pivot**: lo zero di velocità della root (frame di plant) deve coincidere in tempo e distanza con l'inversione di velocità del capsule. Usa `Enable_Warping = 0` solo per pochi frame attorno al plant.
8. **Dopo ogni modifica alla root**, rigenera `MoveData_Speed` con il `MotionExtractorModifier` e ricostruisci i database interni (`PSD Locomotion Stops` ecc.). Marca con **Pose Search: Block Transition** le parti in cui non vuoi entrare a metà.

## Verifica

- Fai una registrazione con il Rewind Debugger e confronta, per la stessa sequenza di input, la traiettoria del capsule con quella della clip. Usa anche il canale trajectory nel **Channel Breakdown**.
- Per vedere il comportamento dei warp usa le cvar `a.AnimNode.OrientationWarping.Debug` e `a.AnimNode.StrideWarping.Debug`. Per l'Offset Root Bone guarda il cerchio di debug: se tocca il bordo, la clip è sbagliata rispetto al capsule.

Una nota sui nomi: la documentazione descrive formule e modalità (Accumulate, Interpolate, Release, Clamp Result), ma non riporta i nomi esatti delle soglie interne di Steering, Orientation Warping e Offset Root Bone. Controlla quindi nel pannello **Details** dei nodi i nomi delle proprietà per soglia di velocità della root, raggio massimo ed emivita.

Un altro punto da controllare: perché le clip custom siano trattate come quelle del Motion Matching, il Blend Stack `StateMachineBlendStack` deve avere nel suo Blend Stack Graph la stessa catena Orientation Warping e Steering del nodo Motion Matching. Se vuoi, incolla uno screenshot di quel grafo e lo verifico.

**Fonti:**
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Pose Warping](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine)
- [Motion Matching and the Game Animation Sample (Unreal Fest 2024)](https://dev.epicgames.com/community/learning/talks-and-demos/0y9w/unreal-engine-motion-matching-and-the-game-animation-sample-in-ue-5-4-unreal-fest-2024)
- [Your First 60 Minutes With Motion Matching](https://dev.epicgames.com/community/learning/tutorials/lwlG/unreal-engine-your-first-60-minutes-with-motion-matching)
- [Motion Matching Debugging](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-debugging-in-unreal-engine)

## Q: 3) Transitions and notifies: which anim notifies / notify states inside GASP clips drive this state machine (e.g. 'Transition: ...' notifies setting NotifyTransition_ToLoop, PoseSearchBlockTransition, PoseSearchBranchIn) and which ones must a custom transition clip (start, stop, pivot, landing) carry so the hand-off to the loop or idle is clean? Also explain the role of IsAnimationAlmostComplete with 0.75 s and the blend times (0.2-0.5 s) and blend profiles like FastFeet_FastRoot in the OUT struct.

## Premessa

La documentazione ufficiale tratta il GASP e il Motion Matching, ma **non** descrive il setup sperimentale State Machine + Blend Stack né i notify "Transition:" del Mover ABP. La parte sui notify di logica si basa quindi sul funzionamento del motore (AnimNotify, Pose Search, Blend Stack). Prima di replicarla, apri una start GASP (es. Run F Start) e controlla nel pannello **Details** di ogni notify la classe e il nome esatti.

## 1. Due famiglie di notify, due consumatori diversi

| Famiglia | Classe | Chi la legge | Effetto |
|---|---|---|---|
| Notify di logica ("Transition: ToLoop", "Transition: Re-Transition"...) | `AnimNotify` (Skeleton Notify o notify Blueprint custom) | Event Graph dell'ABP, che imposta i bool letti dalle transition rule | Runtime: decide *quando* uscire dallo stato |
| Notify state Pose Search | `UAnimNotifyState_PoseSearchBlockTransition`, `UAnimNotifyState_PoseSearchExcludeFromDatabase`, `UAnimNotifyState_PoseSearchModifyCost`, `UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias`, `UAnimNotifyState_PoseSearchBranchIn` | Indicizzazione dei database, compresi i PSD interni della `PoseSearchColumn` | Decidono *quali frame* il pose match può restituire come asset + `StartTime` |

I notify state Pose Search **non** impostano bool e non guidano la state machine. Controllano soltanto cosa restituisce la colonna `PoseSearchColumn` del chooser e cosa può scegliere il nodo Motion Matching (indice 0).

## 2. Notify di logica: cosa deve avere ogni clip custom

Funzionamento:

1. Il notify scatta.
2. L'evento `AnimNotify_<Nome>` gira nell'Event Graph (game thread) e imposta per esempio `NotifyTransition_ToLoop = true`.
3. La transition rule lo legge all'update successivo.
4. L'On State Entry dello stato seguente lo azzera.

| Clip | ToLoop | Re-Transition | Note |
|---|---|---|---|
| **Start** | Sì, sul frame in cui velocità del root e fase dei piedi coincidono con il loop (di solito dopo il 2° appoggio) | Sì, dopo il punto di "commit" (primo passo) | In uno stile snappy cade presto, circa 0.15-0.25 s |
| **Pivot** | Sì, dopo l'appoggio del piede di perno, a direzione raggiunta | Sì, dopo il perno | Evita il ToLoop prima del cambio di direzione del root |
| **Stop** | No (va in Idle) | Facoltativo, per permettere una ripartenza durante lo stop | Uscita tramite `IsAnimationAlmostComplete` |
| **Land in movimento** | Sì, dopo l'impatto | Facoltativo | Il land da fermo va in Idle per completamento |
| **Turn In Place** | No | No | Nessuna `PoseSearchColumn`, uscita per completamento |

Proprietà da impostare su questi notify:

- **Trigger Weight Threshold**: alzala, per esempio a `0.5`. Il Blend Stack fa crossfade, quindi una clip in uscita a peso basso può ancora far scattare il suo ToLoop e attivare una transizione spuria.
- **Can Be Filtered Via Request**: disattivala. Il filtro dei notify (**Should Filter Notifies** sul nodo Motion Matching) non deve mai scartare un notify di logica.

**Trappola tipica:** se il pose match sceglie uno `StartTime` *successivo* al notify ToLoop, il notify non scatta mai e lo stato resta fino a `IsAnimationAlmostComplete`. La soluzione è nella sezione 3.

## 3. Notify state Pose Search nelle clip di transizione

- **Pose Search: Block Transition**: i frame restano riproducibili ma non possono essere scelti come punto d'ingresso. Mettilo:
  - sulla coda delle start, dal ToLoop in poi;
  - sulla coda degli stop già fermi, così il pose match non salta direttamente al pose finale;
  - sulla parte del pivot dopo il perno;
  - sui frame del land dopo l'impatto.
- **Exclude From Database**: i frame non vengono indicizzati affatto. Usalo per maniglie e frame tenuti che non devono mai essere candidati.
- **Modify Cost**: un costo negativo sui frame di anticipazione fa preferire ingressi più "carichi", utile per lo stile cartoon.
- **Override Continuing Pose Cost Bias**: riguarda soprattutto il nodo MM (indice 0), dove decide quanto restare sulla clip corrente.
- **Branch In**: delimita la parte di una sequenza indicizzata per un database specifico, tramite la sua proprietà database. In questo modo la stessa clip si riproduce per intero ma è cercabile solo in quella finestra. Verifica la proprietà esatta nella tua build 5.9.

Requisiti di base per i database:

- La documentazione del Motion Matching richiede root motion nelle clip, con **Enable Root Motion** attivo sulle sequenze.
- Aggiungi ogni clip al PSD interno giusto (`PSD Locomotion Transitions`, `PSD Locomotion Stops`, `PSD Locomotion Spin Transitions` o `PSD Locomotion Loops`).
- Aggiungi anche la riga nella tabella annidata giusta, con gli intervalli di `Speed2D` e `MovementDirection_Recent`.

## 4. `IsAnimationAlmostComplete` e i 0.75 s

La condizione è vera quando la clip corrente del Blend Stack non è in loop e il tempo rimanente è ≤ 0.75 s. Serve per tre motivi:

1. **Passaggio prima della fine:** una clip non in loop arrivata in fondo resta bloccata sull'ultimo frame. Il root motion va a zero e i piedi scivolano rispetto alla capsula Mover.
2. **Margine sul blend:** 0.75 s ≥ il blend più lungo (0.5 s) più un margine. Così il blend verso loop o idle si completa mentre la clip in uscita ha ancora movimento.
3. **Latenza:** i bool impostati dai notify vengono letti all'update successivo. L'AnimInstance si aggiorna a frame rate, non ai 62.5 Hz del Mover.

**Problema per lo stile Smash:** una clip lunga meno di circa 0.75 s (al netto dello `StartTime`) soddisfa la condizione già al primo update. Il risultato è un'uscita immediata o un ciclo di re-entry. Le soluzioni:

- Aggiungi una coda di assestamento di durata ≥ blend time e coprila con Block Transition.
- Oppure rendi la soglia dipendente dalla clip, per esempio `min(0.75, BlendTimeUscita + 0.1)` oppure `clamp(0.3 * Length, 0.1, 0.75)`.
- Aggiungi un tempo minimo nello stato: salva il tempo nell'On State Entry e richiedi per esempio > 0.1 s prima di valutare l'uscita.

## 5. Valori della struct OUT (`S_CHT_MoverCharacterAnimations_OUT`)

- **`BlendTime`**: durata del crossfade del Blend Stack quando `SetBlendStackAnimFromChooser` inserisce la nuova clip. Il Blend Stack valuta entrambe le pose per tutta la durata, a differenza dell'inertialization. Nel GASP i blend verso start e pivot sono corti (~0.2 s) e quelli verso loop e idle più lunghi (0.3-0.5 s).
- **`BlendCurve`**: dà forma all'alpha del blend. Per uno stile snappy usa una curva con ease-out che prende peso subito.
- **`BlendProfile`**: un `UBlendProfile` in modalità **Weight Blend Profile**. Secondo la documentazione, `1.0` è la velocità normale e `2.0` è due volte più veloce. `FastFeet_FastRoot` accelera catena delle gambe, piedi e root:
  - i piedi si piantano subito, senza scivolare sotto il movimento del Mover;
  - l'orientamento del root adotta in fretta quello della nuova clip;
  - busto e braccia arrivano con il tempo pieno, il che produce un follow-through naturale.
  
  Per uno stile cartoon, crea per esempio un `FastFeet_FastRoot_Snappy` con colonna e braccia a `1.0`, gambe a `3-4` e root a `4`.
- **`StartTime`**: con `PoseSearchColumn` lo determina il pose match. Senza (Turn In Place) arriva dalla tabella.

Punti di partenza per lo stile cartoon. Sono valori da tarare, non quelli del GASP:

| Hand-off | BlendTime | Profile |
|---|---|---|
| Idle → Start | 0.1-0.15 | FastFeet_FastRoot |
| Loop → Pivot | 0.1-0.15 | FastFeet_FastRoot |
| Start/Pivot → Loop | 0.2-0.3 | FastFeet_FastRoot |
| Stop → Idle | 0.25-0.4 | default o upper-body lento |
| Air → Land | 0.05-0.1 | FastFeet_FastRoot |

## 6. Note specifiche Mover / Network Prediction

- I notify sono cosmetici e vengono valutati localmente su ogni macchina, proxy simulati inclusi. Non usarli per guidare lo stato del Mover.
- Sui proxy simulati, `Speed2D` e `FutureFacingDelta` derivano da stato interpolato. Prevedi isteresi sulle soglie, per esempio 60° in ingresso e 50° in uscita, per evitare re-entry continui.
- Per il debug usa il **Rewind Debugger**: tracce dei notify, selezione Pose Search e valutazione del chooser.

**Documentazione:**
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine) (filtro dei notify, requisiti root motion)
- [Blend Masks and Blend Profiles](https://dev.epicgames.com/documentation/unreal-engine/blend-masks-and-blend-profiles-in-unreal-engine)
- [Blend Nodes / Inertialization](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine)

Se incolli la classe e i nomi esatti dei notify "Transition:" e il grafo di `IsAnimationAlmostComplete` della tua copia, posso adattare la soglia per singola clip in modo preciso.

## Q: 4) Foot locking and leg IK in GASP Mover: how do the Procedural_Feet layer (CR_Biped_FootPlacement / Foot Placement node), AllowFootPinning, and curves such as disable_legik / disablefootlock / contact_l / contact_r / phase work? Which curves must a custom clip have so feet do not slide or pop on entry and exit, and what happens when they are missing (a missing curve reads 0)?

La documentazione ufficiale non descrive il layer `Procedural_Feet`, `CR_Biped_FootPlacement`, né le curve `AllowFootPinning`, `disable_legik`, `disablefootlock`, `contact_l`, `contact_r` e `phase` di GASP. La pagina di GASP parla solo di un "Simple Leg IK" che fissa i piedi alle ossa IK dopo il Pose Warping. Quindi qui sotto separo tre cose: le regole documentate, quello che si deduce dai nomi e dalla logica del grafo, e come controllarlo nel tuo progetto. Non ti do nomi di proprietà o cvar che non posso confermare.

## 1. Regole documentate

- **Una curva assente vale `0.0`.** Anche una curva aggiunta ma senza keyframe vale `0.0`.
- **Una Metadata Curve vale sempre `1.0`** (sola lettura). Si crea con **Add Curve… > Create Curve**, oppure con **Convert To Metadata** su una curva esistente.
- **Le curve si fondono come le pose.** Il Blend Stack, la TwoWayBlend, la BlendListByInt e l'inertialization/Dead Blending fondono anche i valori delle curve. Quindi se il clip in uscita ha una curva a `1` e quello in entrata non ce l'ha, ottieni una rampa da 1 a 0 lunga quanto il `BlendTime` dell'output del chooser.

Fonti: [Animation Curves](https://dev.epicgames.com/documentation/unreal-engine/animation-curves-in-unreal-engine), [Blend Nodes / Inertialization](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine).

## 2. Il punto chiave: il verso della curva

Un 0 di default è innocuo o pericoloso a seconda del significato della curva:

| Curva | Verso (dal nome) | Se manca (= 0) | Rischio per i tuoi clip |
|---|---|---|---|
| `disable_legik` | 1 = IK gambe spento | IK gambe **sempre attivo** | Basso a terra. Alto in aria (salti, hop, squash): l'IK tira i piedi verso target non validi |
| `disablefootlock` | 1 = lock spento | Lock **sempre consentito** | Il piede resta bloccato quando dovrebbe scivolare (skid, slide stilizzati). Si vede uno "strappo" al rilascio |
| `AllowFootPinning` | 1 = pinning consentito | Pinning **disattivato** | Alto: il clip slitta perché il layer non blocca mai i piedi |
| `contact_l` / `contact_r` | 1 = piede a terra | Piede **mai a terra** | Alto: nessun lock e sliding. Se il clip precedente aveva `1`, durante il blend il valore scende e il lock si sgancia a metà blend (pop) |
| `phase` | Valore ciclico del passo | Fase ferma a 0 | Dipende da chi la legge: se guida il lock o la predizione del passo, si comporta come "inizio ciclo" fisso |

**In pratica:** le curve `disable_*` possono mancare solo se va bene l'IK/lock sempre attivo. Le curve "positive" (`AllowFootPinning`, `contact_*`) vanno *sempre* scritte sui clip in cui i piedi devono restare piantati.

## 3. Cosa causa sliding e pop in entrata e in uscita nel tuo setup

1. **Entrata (`ForceBlendOnNextUpdate`):** il Blend Stack fonde le curve del clip vecchio con quelle del nuovo durante il `BlendTime`. Se i valori di `contact_*`/`AllowFootPinning` ai primi frame del tuo clip non coincidono con quelli del clip GASP precedente, il lock cambia stato a metà blend e il piede salta.
2. **Uscita con `IsAnimationAlmostComplete` (≤ 0.75 s rimanenti):** il frame di uscita reale è circa `durata - 0.75 s`, non l'ultimo. I valori delle curve in *quel* punto devono essere coerenti con l'inizio del clip successivo. Ad esempio, per stop → idle servono entrambi i piedi a `contact = 1` e pinning consentito.
3. **Uscite via notify `Transition:`** (`NotifyTransition_ToLoop`/`Re-Transition`): stessa regola. Metti il notify in un frame in cui il piede d'appoggio ha `contact = 1` stabile, non a cavallo di un cambio di stato.
4. **Passaggio BlendListByInt 0 ↔ 1** (Motion Matching ↔ Blend Stack): anche qui le curve si fondono. I database PSD interni e i clip della state machine devono avere lo *stesso set* di curve.
5. **Turn In Place senza PoseSearchColumn:** il clip parte da `StartTime` fisso, senza pose matching. Se i piedi a `StartTime` non sono piantati, il lock non ha nulla da tenere. Servono `contact_* = 1` sui frame iniziali e sul piede perno.

## 4. Checklist di authoring per i clip cartoon

- **Copia il set di curve da un clip GASP equivalente** (loop, start, stop, pivot, TIP, land) e mantieni nomi e maiuscole identici. Il nome deve coincidere esattamente con quello letto dal grafo o dal rig.
- **`contact_l` / `contact_r`:** generali dalla velocità del piede. Usa **Window > Animation Data Modifiers > Add Modifier > MotionExtractorModifier** su `ball_l`/`ball_r` (o `foot_*`) con **Motion Type** `Translation Speed`, **Axis** `XYZ`. Poi converti in 0/1 con una soglia. Con smear e anticipazioni cartoon alza la soglia o ritocca a mano ([Fix Foot Sliding with IK Retargeter](https://dev.epicgames.com/documentation/unreal-engine/fix-foot-sliding-with-ik-retargeter-in-unreal-engine)).
- **`disable_legik = 1`** su tutti i frame aerei (hop, salti, squash/stretch con gambe fuori range), con rampe brevi (2-4 frame) invece di salti netti.
- **`disablefootlock = 1`** nei momenti in cui vuoi uno scivolamento *intenzionale* (skid stile Smash, slide, stop "a pattino").
- **`AllowFootPinning`:** se un clip deve avere il pinning sempre attivo (idle, TIP), usa una **Metadata Curve**, che vale costante `1.0` senza keyframe.
- **`phase`:** prima di scriverla verifica chi la legge. Se nessun nodo la usa, non serve.
- **Valori costanti ai bordi:** primi e ultimi frame (e il frame di uscita a -0.75 s) con valori stabili, senza transizioni a cavallo dei punti di blend.
- **Root motion coerente:** la velocità del root deve corrispondere al passo, altrimenti il lock "tira" contro l'Offset Root Bone. Con la simulazione Mover a 62.5 Hz e la pose interpolata per frame, uno sliding residuo si nota soprattutto se root motion e trajectory divergono.

## 5. Come verificare la semantica reale nel tuo progetto

1. Apri `CR_Biped_FootPlacement` e cerca i nodi **Get Curve Value**: ti dicono quali curve legge, con quale nome e come le usa (come alpha, come soglia o invertite).
2. Nel layer `Procedural_Feet` dell'ABP controlla quali curve vengono lette con **Get Curve Value** e passate al Control Rig o al nodo Foot Placement come alpha o variabili.
3. In PIE usa il **Rewind Debugger** (traccia delle curve) o `showdebug animation` per vedere i valori di `contact_*`/`AllowFootPinning` frame per frame durante una transizione che fa pop. Di solito si vede subito la rampa causata dal blend.

Se mi incolli uno screenshot dei nodi **Get Curve Value** in `CR_Biped_FootPlacement` e nel layer `Procedural_Feet`, ti preciso il comportamento esatto di ogni curva, inclusa `phase`.

Riferimento generale: [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine).

## Q: 5) Best practice to extend this Mover setup with custom locomotion clips (a fast cartoony run start, a run stop, a 180 degree skid/pivot, a dash) without breaking transitions. Option A: add our clips as rows in the existing nested tables (e.g. Run F Loco Transitions with IsPivoting for the skid, Stand Stops for the stop) so they get the PoseSearchColumn start frame. Option B: add our own states plus aliases in the State Controller that call SetBlendStackAnimFromChooser with our own state names and fixed start time rows. Which does Epic intend and why? What data must each clip have (pose search channels sampled, foot curves, root motion consistent with the simulation speed curve, tags) and what must be rebuilt after editing (internal databases)?

## Risposta breve

Usa **l'opzione A** per avvio corsa, stop e skid/pivot, e **l'opzione B** solo per il dash. La documentazione non dichiara esplicitamente quale approccio Epic preveda per il setup Mover. Questa raccomandazione si basa su come Epic ha presentato lo state machine sperimentale di GASP (talk *Choosing How Your Game Animation Feels*, Unreal Fest 2024):
- Lo state machine è puramente logico, non produce pose e consente il re-entry.
- *"The state machine is basically just telling the chooser what to do"*.
- La selezione vera (asset, start time, mirror, cost limit, blend time/profile, tag) è dati nel chooser.

Quindi i nuovi clip si aggiungono nel chooser come righe. Nuovi stati servono solo per nuova *semantica* di gameplay.

## Perché A per start, stop e skid

| Aspetto | A (righe nelle tabelle esistenti) | B (stati nuovi + start time fisso) |
|---|---|---|
| Frame d'ingresso | La `PoseSearchColumn` sceglie lo `StartTime` confrontando con la `PoseHistoryReference`, cioè fase dei piedi e traiettoria correnti | Lo `StartTime` fisso ignora la fase del ciclo di corsa, quindi rischi blend con piedi incrociati o foot slide |
| Regole di uscita | Ottieni gratis `IsPivoting`, Re-Enter, `NotifyTransition_ToLoop`, la soglia di 60° e `NoValidAnim` | Devi duplicarle; uno skid di 180° fa scattare `FutureFacingDelta > 60` e rientra in loop |
| Manutenzione | Solo dati | Alias, enum `StateName` e righe radice da tenere sincronizzati |

Il dash è diverso: è un'azione di simulazione, non una variante di locomozione (vedi sotto).

## Dati richiesti per ogni clip

### Root motion
- Abilita **Root Motion** sulla sequence. Il sample è guidato dalla capsula, ma il root motion serve comunque:
  - per indicizzare il canale **Trajectory** del database;
  - per **Offset Root Bone**, steering e orientation warping (la rotazione del root la controlla il root motion).
- Il profilo di velocità del root deve combaciare con la simulazione Mover:
  - **Start:** tempo per arrivare a regime = `MaxSpeed / Acceleration`.
  - **Stop:** distanza di frenata ≈ `v² / (2 · Deceleration)`.
  - **Pivot:** deve rallentare fino a 0 e invertire la direzione nel tempo in cui lo fa la simulazione.
- Se lo stile cartoon è più rapido dei dati mocap, regola prima le impostazioni di movimento Mover (accelerazione, decelerazione, turning rate). Non lasciare che sia l'Offset Root Bone a compensare: non fa collision check.

### Canali dello schema
- Il clip deve contenere tutte le ossa campionate dallo schema del database interno, almeno `foot_l` e `foot_r` con position e velocity, e ogni canale Phase o Curve presente.
- Deve durare abbastanza per l'orizzonte history/prediction della traiettoria. Altrimenti si estrapola; controlla **Additional Extrapolation Time** sulla entry.
- Imposta la **Mirror Option** sulla entry coerente con le righe vicine, se la tabella scrive il mirror in output.

### Notify state Pose Search
- Il frame scelto dalla `PoseSearchColumn` non deve finire a metà del gesto. Per i frame di anticipazione cartoon o per la seconda metà dello skid usa:
  - **Pose Search: Exclude From Database**, oppure restringi il **Sampling Range** della entry, se il gesto deve partire dall'inizio;
  - **Pose Search: Block Transition**, per i tratti in cui non si deve entrare.
- Per orientare la scelta senza escludere frame, usa **Modify Cost** o **Override Continuing Pose Cost Bias**.

### Curve
Non ho conferma documentale dei nomi esatti delle curve GASP. Il metodo più sicuro:
1. Apri il clip GASP che stai "sostituendo" (stesso stato, stessa gait).
2. Replica **tutti** i suoi nomi di curva.
3. Le curve di velocità dei piedi per **Foot Placement** si generano con un anim modifier. Per esempio **Motion Extractor Modifier** con **Motion Type** = Translation Speed sulle ossa del piede, poi **Apply All Modifiers**.

### Notify di transizione
- Copia i notify `Transition:` del clip di riferimento e posizionali sul frame in cui la falcata è in fase con il loop.
- Attenzione a `IsAnimationAlmostComplete`: se a uno start di 0,5 s restano già ≤ 0,75 s, lo stato esce al primo update. Le soluzioni:
  - clip più lunghi della soglia più la durata utile;
  - un `ToLoop` esplicito;
  - una soglia per-stato.

### Tag e filtri
- Compila le celle di output (`StartTime`, `BlendTime`, `BlendCurve`, `BlendProfile`, `Tags`) copiandole da una riga vicina. Così la logica ABP che legge `Tags` (blend profile, warping) tratta il clip come i nativi.
- Verifica che i range dei filtri (`Speed2D`, `MovementDirection_Recent` ecc.) coprano le velocità cartoon. Altrimenti ottieni `NoValidAnim`.

## Il dash (opzione B)
- **Simulazione:** il dash va in Mover come **Layered Move**, accodato dal percorso di input. Così è predetto e soggetto a rollback a 62,5 Hz. Non va mai guidato da notify o dall'ABP, che sono solo cosmetici.
- **Animazione:** aggiungi uno stato `Dash` nel **State Controller**, che entra quando la simulazione segnala il dash attivo. Aggiungi poi una riga radice `StateName = Dash` senza `PoseSearchColumn`, con `StartTime = 0`, come le tabelle Turn In Place.
- **Alternativa:** un montage nel **Default Slot**, come per il traversal.

## Cosa ricostruire dopo le modifiche
- **Database interni:** sono sotto-oggetti del chooser. Salva il chooser e attendi che il loro indice venga ricostruito. Non ho conferma documentale di un pulsante di rebuild manuale.
- **Modifiche ai clip** (curve, notify, root motion, sampling): invalidano l'indice di ogni database che li contiene, interni ed esterni.
- **Database esterni:** se vuoi i clip anche sul ramo Motion Matching (`CHT_PoseSearchDatabases_Mover`, indice 0), aggiungili anche ai database `PSD_*` corrispondenti.
- **Verifica** con il **Rewind Debugger**:
  - traccia *Chooser Evaluation*: controlla che la riga passi i filtri;
  - traccia *Pose Search*: controlla candidati e scomposizione dei costi per canale (**Channel Breakdown**).

Per le cvar: la documentazione non le elenca oltre `DDCvar.LocomotionSetupMover`. Puoi vedere le altre digitando `DDCvar.` nella console e scorrendo l'autocompletamento.

Fonti: [Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine), [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine), [Motion Matching Debugging](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-debugging-in-unreal-engine), [Choosing How Your Game Animation Feels](https://dev.epicgames.com/community/learning/talks-and-demos/234j/unreal-engine-choosing-how-your-game-animation-feels-unreal-fest-2024), [Mover Features and Concepts](https://dev.epicgames.com/documentation/unreal-engine/mover-features-and-concepts-in-unreal-engine), [Fix Foot Sliding with IK Retargeter](https://dev.epicgames.com/documentation/unreal-engine/fix-foot-sliding-with-ik-retargeter-in-unreal-engine).

## Q: 6) Where is Motion Matching still used when LocomotionSetupMover = 1 (state machine setup)? Is the MotionMatching node (index 0) evaluated at all, what do PoseSearchHistoryCollector, MMDatabaseLOD, CHT_PoseSearchDatabases_Mover and the trajectory (Trj_FutureFacing, Trj_IsCircling) still provide? When would you switch to setup 0 (pure MM) instead?

## In breve

Con `DDCvar.LocomotionSetupMover=1`, il nodo **Motion Matching** non viene eseguito. Il pose matching però continua a funzionare, solo in un punto diverso: dentro la **Pose Search Column** delle chooser table `CHT_MoverCharacterAnimations_PoseMatch`, e viene valutato una sola volta all'ingresso di ogni stato. Il setup 1 è quindi un sistema "state machine + pose search su richiesta", non un sistema senza MM.

Nota sulle fonti: la documentazione ufficiale descrive l'AnimGraph MM di GASP e il ruolo di Pose History e Chooser ([Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine), [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)). La variante Mover con state machine invece non è documentata. Quello che segue si basa sul grafo che hai descritto e sul comportamento standard dei nodi; per i punti specifici del tuo asset ti indico cosa verificare.

## 1. Il nodo MotionMatching (indice 0) viene valutato?

**No, a regime no.** `Blend Poses by Int` (`FAnimNode_BlendListByInt`) aggiorna e valuta solo i figli con peso diverso da zero.

- **Quando viene saltato:** con indice 1 attivo e la blend completata, il ramo 0 non riceve né `Update` né `Evaluate`. Di conseguenza:
  - la funzione **On Update** del nodo (`Update_MotionMatching`) non gira;
  - `CHT_PoseSearchDatabases_Mover` non viene valutata;
  - **Update_MotionMatching_PostSelection** non gira e `CurrentDatabaseTags` non si aggiorna.
- **Quando invece gira:** solo durante la blend di switch, se cambi la cvar a runtime. Con **Transition Type = Inertialization** il ramo vecchio viene scartato subito. Se **Reset Child on Activation** è attivo, il nodo MM si reinizializza quando torna attivo.
- **Da controllare:** ogni funzione dell'ABP che legge `CurrentDatabaseTags`, per esempio la half-life dell'Offset Root o i controlli sui pivot. Nel setup 1 la fonte dei tag giusta sono i `Tags` dello struct `S_CHT_MoverCharacterAnimations_OUT` passati al Blend Stack, non il database MM. Se una funzione legge ancora i tag MM, nel setup 1 riceve valori vecchi.

## 2. Cosa forniscono ancora i singoli pezzi

| Elemento | Ruolo nel setup 1 |
|---|---|
| **PoseSearchHistoryCollector** | **Indispensabile.** Sta a valle del `BlendListByInt`, quindi registra sempre la posa finale. Fornisce il `PoseHistoryReference` che la Pose Search Column usa per costruire la query: canali pose (posizioni e velocità dei piedi, secondo lo schema) e canale trajectory (passato/futuro). Senza di lui la colonna non ha una query valida. |
| **Pose Search Column** (nelle tabelle annidate) | È il "motion matching" del setup 1. Fa la ricerca solo sulle righe sopravvissute ai filtri (`Speed2D`, `FutureFacingDelta`, `IsPivoting`, `MovementDirection_Recent`) e solo nei database interni (`PSD Locomotion Transitions`, `Stops`, `Spin Transitions`, `Loops`). Sceglie l'asset e il frame di ingresso, e quel tempo finisce in `StartTime`. Controlla nella colonna quale output è collegato a `StartTime`. |
| **Database interni (subobject)** | Sono veri `UPoseSearchDatabase` con uno schema. Richiedono indicizzazione e rispettano le notify di Pose Search (vedi sezione 3). |
| **CHT_PoseSearchDatabases_Mover** | **Nessun ruolo**: viene valutata solo dalla On Update del nodo MM. |
| **MMDatabaseLOD** | Conta solo dove una colonna di chooser è collegata a questa variabile, di solito solo in `CHT_PoseSearchDatabases_Mover`. Se nelle tabelle PoseMatch non è collegata, nel setup 1 è inerte. Puoi verificarlo con **Find References** sulla variabile e controllando i binding delle colonne. |
| **Trajectory** (`Trj_FutureFacing`, `Trj_IsCircling`) | **Ancora centrale**, per tre motivi: (a) `FutureFacingDelta` filtra le righe nelle tabelle Transitions; (b) la exit rule "FutureFacing delta > 60°" usa lo stesso dato; (c) il canale trajectory dello schema dei database interni usa la traiettoria contenuta nell'history collector, quindi la qualità della predizione Mover influisce direttamente sul costo del match. `Trj_IsCircling` serve dove è collegato, di solito per evitare pivot o transizioni quando la traiettoria è un arco stretto. Controlla le sue referenze: se nessuna colonna o regola lo usa nel setup 1, è inerte. |
| **Turn In Place** (senza PoseSearchColumn) | Selezione deterministica: asset e `StartTime` vengono dalle colonne di output. La qualità della blend dipende solo da `BlendTime`, `BlendCurve` e `BlendProfile`. |

**Mover a 62.5 Hz:** la traiettoria è campionata dalla simulazione a tick fisso, mentre l'history campiona al frame rate dell'animazione. Nel setup 1 questo pesa meno che nel setup 0. La query infatti parte solo all'ingresso di uno stato, quindi il rumore sulla traiettoria può influire su quella singola selezione ma non causa cambi di animazione continui.

## 3. Implicazioni per le tue clip stilizzate

- **Clip nelle tabelle con PoseSearchColumn:** aggiungile al database interno corretto, non solo come riga della tabella. Le notify di Pose Search restano valide:
  - `AnimNotifyState_PoseSearchBlockTransition` sui frame di anticipation, così la ricerca non salta l'anticipo stilizzato entrando a metà clip;
  - `AnimNotifyState_PoseSearchExcludeFromDatabase` per le porzioni da escludere;
  - `AnimNotifyState_PoseSearchModifyCost` e `AnimNotifyState_PoseSearchOverrideContinuingPoseCostBias` per pesare le scelte;
  - **Sampling Range** sulla entry del database per limitare la finestra di ingresso.
- **Clip in stile Smash con timing autoriale** (start a frame 0, hold): mettile in righe senza Pose Search Column, come i Turn In Place, con `StartTime = 0`. Pose pesantemente "squash/stretch" producono distanze di feature poco significative per lo schema.
- **Notify `Transition:`** che impostano `NotifyTransition_ToLoop` / Re-Transition: aggiungile anche alle tue clip, altrimenti l'uscita avverrà solo con `IsAnimationAlmostComplete` (0.75 s).

## 4. Quando passare al setup 0 (solo MM)

Conviene il **setup 0** se:
- hai una **copertura densa** (molti angoli, archi, velocità, pivot a vari gradi) e vuoi transizioni che emergono da sole, senza scrivere regole di uscita;
- il gameplay produce traiettorie continue e imprevedibili (strafe circolare, curve, cioè i casi in cui `Trj_IsCircling` è spesso true), dove una state machine richiederebbe tanti alias Re-Enter;
- vuoi usare le funzioni di livello nodo: continuing pose bias, **Search Throttle Time**, LOD dei database tramite `MMDatabaseLOD`, steering e warping nel Blend Stack Graph del nodo MM.

Resta sul **setup 1** se:
- il materiale è **stilizzato, a chiavi, con poche clip per gait**. Il MM con dati sparsi tende a cambiare clip troppo spesso e a fare scelte basate solo sul costo, che rompono il timing di anticipation e hold;
- ti servono controllo deterministico per stato (blend time, curve, profile e tag per singola riga) e ricerche solo all'ingresso degli stati, cosa che di solito costa anche meno della ricerca periodica del nodo MM;
- vuoi un comportamento più robusto sui simulated proxy, dove la traiettoria Mover è interpolata e più rumorosa.

**Raccomandazione per uno stile cartoon/Smash:** setup 1, con Pose Search Column solo su stops e transizioni di locomozione, dove scegliere il frame di ingresso aiuta davvero. Per starts e turn usa righe deterministiche. Passa al setup 0 solo se aumenti molto la copertura delle animazioni.

Per il debug, nel **Rewind Debugger** la traccia **Pose Search** mostra anche le ricerche lanciate dalle chooser. Così puoi verificare il costo e il frame scelto per ogni ingresso di stato.
