# Unreal Assistant (2026-11-21, rule 14): torso lean additive; pivot/turn blend and re-selection jitter

## Q: In stock Game Animation Sample (GASP, Mover variant, UE 5.9): what is the stock way to add a tunable forward/backward torso lean (pitch the spine more upright or more leaning, without re-authoring clips) on top of a run start clip, weighted per clip over time? Does GASP's Simple Additive Lean (BS_Relaxed_Lean additive blendspace) or any other stock node support a pitch offset driven by a curve and a single scalar? If not, what is the stock-friendly approach: Transform (Modify) Bone on spine_01..spine_05 in local space, an additive pose, or a Control Rig? Name exact nodes, settings and where in the ABP chain it should go (before or after Offset Root Bone / Leg IK).

**Short answer:** stock GASP has no forward/back torso pitch node. The **Simple Additive Lean** is lateral only. The approach that fits GASP best is a **local-space additive pitch pose**, applied with an **Apply Additive** node whose alpha comes from an anim curve, right after the existing lean. **Transform (Modify) Bone** and a **Control Rig** node also work, with tradeoffs listed below.

## 1. Can the Simple Additive Lean do it?

No. The GASP docs describe it as leaning the character *left and right based on the lateral acceleration of the character*, applied to all locomotion. Its input comes from `GetLeanAmount` / `CalculateRelativeAccelerationAmount`, not from a curve.

To double-check your copy, open `BS_Relaxed_Lean` and look at its samples. If none of them is a forward/back spine pose, no input value will give you pitch. Don't reuse it for pitch either way: it's tied to acceleration and would fight your curve.

No other stock GASP node gives you "pitch offset = curve × scalar" out of the box. Several engine nodes get close, because they accept an anim curve as their alpha:

| Node | Curve-driven alpha | Notes |
|---|---|---|
| **Apply Additive** | **Alpha Input Type** = Anim Curve, **Alpha Curve Name** | Local space; respects **LOD Threshold** |
| **Transform (Modify) Bone** | Same skeletal control alpha options | Works in component space, one bone per node |
| **Control Rig** | A float variable can be set to **Use Curve** (signed, not clamped to 0..1) | Also has node alpha |
| **Modify Curve** | n/a | **Apply Mode** = Scale, multiplies a curve by your scalar |

## 2. Authoring the per-clip weight

1. In each run start `AnimSequence`, add a float curve such as `TorsoPitch_Alpha` (0..1 over time). Clips without the curve read 0, so nothing changes in them.
2. Motion Matching's blend stack blends curves across active clips, so the value fades in and out with pose transitions. Montages don't carry the curve, so the lean fades out under the `DefaultSlot` without extra logic.
3. Apply your global scalar with a **Modify Curve** node, placed just before the consumer:
   - **Apply Mode** = `Scale`
   - Add a `TorsoPitch_Alpha` pin and bind it to an ABP float such as `TorsoPitchScale`, set through Property Access or a thread-safe update function.

If you need *signed* pitch per clip (some clips more upright, others leaning further), either use two curves (`TorsoPitchFwd_Alpha`, `TorsoPitchBack_Alpha`) or use the Control Rig option, which takes a signed curve directly.

## 3. Options, in recommended order

### A. Additive pose (recommended)
- **Assets:** one or two single-frame additive sequences, e.g. `AS_TorsoPitch_Fwd` and `AS_TorsoPitch_Back`:
  - Spread the rotation across `spine_01..spine_05`, about 2 to 4° each, with more toward the upper spine.
  - **Additive Anim Type** = `Local Space`
  - **Base Pose Type** = `Reference Pose` (or a selected frame of a neutral run)
- **Graph:** a **Sequence Evaluator** (fixed time 0) for each pose, feeding the **Additive** pin of an **Apply Additive** node:
  - **Alpha Input Type** = Anim Curve
  - **Alpha Curve Name** = `TorsoPitchFwd_Alpha` (and a second node for Back)
- **Why:** it's the same pattern as GASP's lateral lean, an artist controls how the bend is spread along the spine, it's the cheapest option, and there's no space conversion.

### B. Transform (Modify) Bone (fastest to prototype)
- Use 3 to 5 nodes (`spine_02..spine_04` is often enough):
  - **Rotation Mode** = `Add to Existing`
  - **Rotation Space** = `Bone Space`
  - **Translation Mode** and **Scale Mode** = `Ignore`
  - Expose **Rotation** as a pin bound to `TorsoPitchDeg * perBoneWeight`
  - **Alpha Input Type** = Anim Curve with your curve name
- The node works in component space, so it needs **Local to Component** before it and **Component to Local** after (or place it in an existing component-space section).
- The forward/back bend axis depends on the bone orientation. On UE mannequin spines it's usually the bone's local Z, so the **Yaw** field of the rotator. Check this in the Skeleton editor with the local-space gizmo before tuning.

### C. Control Rig
- Create a rig with a float variable `TorsoPitch` (instance-editable) and use **Distribute Rotation** across `spine_01..spine_05`.
- In the ABP **Control Rig** node, set the variable to **Use Curve**, pointing at a signed `TorsoPitch` curve, or use **Use Pin** with the curve value × scalar.
- Choose this when you need signed values per clip or extra logic such as keeping the head steady. It costs the most of the three.

## 4. Where it goes in the chain

```
Motion Matching (blend stack: orientation warping, steering)
  -> Simple Additive Lean (Apply Additive)
  -> [Modify Curve: scale TorsoPitch_*]
  -> [Torso Pitch: Apply Additive x2  |  or LtoC -> Transform Bone xN -> CtoL  |  or Control Rig]
  -> Simple Aim Offset
  -> Slot 'DefaultSlot'
  -> Offset Root Bone
  -> Leg IK
  -> Output Pose
```

- **Before Offset Root Bone and Leg IK.** Offset Root Bone only moves `root`, and the spine doesn't affect leg or foot positions, so the result doesn't depend on the order. Keeping pose edits upstream keeps IK as the final step, which is the GASP convention.
- **Before the Aim Offset**, so the camera-facing offset stays on top of the lean.
- **Before the Slot**, so traversal montages override it. The curve alpha already fades it out under montages anyway.
- **Pose History:** check whether your Pose History node sits downstream of this insertion and whether your schema samples spine bones. If it does, the leaned pose will feed the Motion Matching query. With the stock feet/pelvis channels this doesn't matter.

## 5. Mover / Network Prediction

This is purely a cosmetic, local animation change. The curves are sampled at animation time on each game frame, so it doesn't interact with the 62.5 Hz Network Prediction fixed tick, Mover state, or GAS, and you don't need any cvar for it. To debug, use **Rewind Debugger** (Animation Insights) to see the blended curve values and node weights, or `ShowDebug Animation` for a quick on-screen check of curves.

Sources:
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Transform Bone](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-transform-bone-in-unreal-engine)
- [Blend Nodes (Apply Additive)](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine)
- [Control Rig in Animation Blueprints](https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-animation-blueprints-in-unreal-engine)

## Q: In stock GASP Mover (UE 5.9) turnarounds/pivots (180 and 90 degree reface starts): the arms jitter during the turn. What controls the blend into a pivot clip (Blend Stack blend time, inertialization, Motion Matching / Pose Search re-selection cooldown, Pose Search database continuing-pose cost bias, MaxActiveBlends)? What are the stock values, and which setting would you change to reduce re-selection jitter (e.g. blend 0.14-0.18 s plus a 5-8 frame lock) with the least divergence from stock? Name exact properties.

# Turnaround e pivot in GASP con Mover: cosa controlla il blend e cosa conviene cambiare

**In breve:** abbassare il **Blend Time** a 0.14-0.18 s non riduce il jitter da riselezione. Lo rende più visibile, perché ogni cambio di posa si completa più in fretta. Con la minima divergenza da stock conviene bloccare la riselezione, non cambiare il blend.

**Prima una nota sulle fonti.** La documentazione ufficiale elenca queste proprietà ma non ne riporta i valori. I default qui sotto vengono dal codice del plugin PoseSearch, a memoria: verificali in **Details**. La freccia gialla di reset accanto a una proprietà indica che l'asset GASP usa un valore diverso dal default del motore.

## 1. Cosa controlla il blend verso un pivot

### Nodo Motion Matching (`FAnimNode_MotionMatching`, in `ABP_SandboxCharacter`)

| Proprietà | Default motore | In GASP | Effetto |
|---|---|---|---|
| **Blend Time** | `0.2` | Collegato alla funzione `Get_MMBlendTime`. A terra restituisce circa `0.2`; in aria e all'atterraggio valori diversi (circa `0.5` / `0.15`), da verificare. | Durata del crossfade del blend stack verso il nuovo segmento selezionato |
| **Max Active Blends** | `4` | Lasciato come in stock | Numero massimo di segmenti in blend insieme. Con `0` il blend stack si disattiva e il nodo chiede un'inertializzazione, che richiede un nodo **Inertialization** o **Dead Blending** più a valle |
| **Blend Profile** / **Blend Option** | nessuno / `Linear` | | Distribuzione del blend sulle ossa |
| **Pose Reselect History** | `0.3` | | Impedisce di riselezionare pose già scelte negli ultimi N secondi |
| **Pose Jump Threshold Time** | `(0, 0)` | | Esclude salti verso pose dello *stesso* segmento entro questo intervallo dalla posa continua |
| **Search Throttle Time** | `0.0` | | Tempo minimo tra due ricerche. Con `0` cerca a ogni update: è il vero "cooldown" |

### Modalità di interrupt

L'interrupt mode non è una proprietà del nodo. La imposta `Update_MotionMatching` tramite **Set Databases To Search**, usando il valore restituito da `Get_MMInterruptMode`. In stock, a memoria, restituisce `InterruptOnDatabaseChange` quando cambia stato o andatura e `DoNotInterrupt` negli altri casi.

**Attenzione:** se il chooser `CHT_PoseSearchDatabases` alterna da un frame all'altro il database dei pivot e quello di movimento, ogni cambio di database scarta la posa continua e forza una nuova selezione. Questo avviene se l'ingresso nel pivot oscilla al confine tra due righe, per esempio una condizione su velocità o accelerazione che alterna vero e falso.

### Database (`UPoseSearchDatabase`)

- **Continuing Pose Cost Bias**: default `-0.01`. Bonus di costo per continuare il clip corrente.
- **Base Cost Bias**: default `0.0`.
- **Looping Cost Bias**: default `-0.005`.

## 2. Cosa cambierei, in ordine di divergenza crescente

### Opzione A: solo dati, nessuna modifica all'ABP (consigliata)

1. Apri i clip di pivot e di reface start a 90 e 180 gradi.
2. Aggiungi il notify state `UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias` (categoria Pose Search in **Add Notify State**).
3. Fallo coprire dall'inizio del clip fino a circa 0.08-0.13 s. Corrisponde a 5-8 frame a 60 fps, oppure a 5-8 tick a 62.5 Hz.
4. Imposta **Cost Addend** su un valore negativo e tara quel valore nel **Rewind Debugger**.
   - Confronta il costo della posa attiva con quello del candidato migliore.
   - Scegli un addend appena superiore alla differenza tipica: abbastanza da vincere, non così tanto da ignorare un input opposto.

Il blocco agisce solo sui clip di pivot, lascia invariate le impostazioni del nodo e del database e non tocca il resto della locomozione.

**Limite:** funziona solo se la posa continua resta valida. Se in quei frame `Get_MMInterruptMode` restituisce un interrupt perché il database cambia, il bias viene ignorato. Vedi il punto C.

### Opzione B: blocco a livello di nodo, solo nei pivot

1. Esponi **Search Throttle Time** come pin e collegalo a una funzione.
2. Restituisci circa `0.1` quando il personaggio è nel pivot, per esempio quando `IsPivoting` è vero o quando il database corrente ha il tag dei pivot. Restituisci `0.0` negli altri casi.

**Da evitare:** un valore globale su **Search Throttle Time** riduce la reattività di tutta la locomozione.

### Opzione C: interrupt e chooser

Se il Rewind Debugger mostra cambi di database durante il pivot, fai in modo che `Get_MMInterruptMode` restituisca `DoNotInterrupt` per la durata del pivot. In alternativa, aggiungi isteresi alla condizione del chooser.

### Cosa non cambierei

- **Blend Time:** `0.2` è già nel tuo intervallo. Se vuoi comunque 0.14-0.18 s, cambialo solo nel ramo pivot di `Get_MMBlendTime`.
- **Max Active Blends:** passare a `0` cambia il modello di blend da blend stack a inertializzazione. È una divergenza grande e non risolve la riselezione.
- **Pose Reselect History:** alzarla aiuta poco nei pivot. Il jitter di solito nasce dal salto tra *segmenti diversi*, non dal ritorno a pose già scelte.

## 3. Prima di toccare il Motion Matching: verifica la causa

Le braccia che tremano possono venire anche da nodi che agiscono dopo la selezione. Con Mover e Network Prediction a tick fisso di 62.5 Hz è un caso frequente.

1. Apri **Tools > Debug > Rewind Debugger** e il traccia **Pose Search**.
   - Se durante il pivot vedi più selezioni o blend sovrapposti, il problema è la riselezione e valgono le opzioni A, B e C.
   - Se gira un solo clip di pivot, il jitter nasce dopo il Motion Matching.
2. Candidati dopo il Motion Matching in GASP:
   - **Simple Additive Lean.** `CalculateRelativeAccelerationAmount` calcola l'accelerazione da delta di velocità. Con Mover a tick fisso la velocità cambia a gradini se il frame di render non coincide con il tick, quindi l'accelerazione ha picchi che fanno oscillare il busto e le braccia. Interpola il valore del lean o calcolalo dallo stato sincronizzato di Mover.
   - **Aim Offset.** `Enable_AO` può attivarsi e disattivarsi in alternanza durante una rotazione di 180 gradi.
   - **Orientation Warping e Steering** nel Blend Stack Graph (doppio clic sul nodo Motion Matching).
3. Il comportamento del tick fisso dipende da come leggi lo stato di Mover nell'ABP. Incolla `UpdateEssentialValues` o la parte dell'ABP che legge velocità e accelerazione da Mover e ti indico dove interpolare.

**CVar:** nessuna cvar controlla questi parametri. Sono tutte proprietà del nodo, del database o dei notify.

## Riferimenti

- [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)
- [Motion Matching Debugging](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-debugging-in-unreal-engine)
- [Game Animation Sample Project](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)
- [Blend Nodes / Inertialization](https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine)
