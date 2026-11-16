# Piano: skid e wall slide come anim layer (2026-11-16, solo piano)
Thread cloud, Opus high, budget ~5 $. Fonti: corsa-mario-piano.md §5, docs/architecture/anim-layers.md + wall-anim.md (feature d258c41c), branch claude/stance-offset d67011e4, listato in sola lettura di GaspMoverProject/Content sul fisso.

## 1. Cosa c'è già (non si rifà)
Il 5.8 ha già il pezzo "niente ABP monolitico": plugin GSTNAnimation (ABP principale figlio di UGSTN_CharacterAnimInstance, layer collegati con LinkAnimClassLayers, anim set per tag Anim.Set.*, varianti per skin/tratti come ABP figli) e il wall layer (ALI_GSTN_Wall, ABP_GSTN_WallLayer, stato FGSTN_WallAnimState Idle/Left/Right/Skid con clip segnaposto). Mancano: più di un layer di override (oggi un solo bool), l'aggancio al kit (B3) e le clip vere.

## 2. Architettura (scelte applicate)
- **ABP base** = solo locomozione GASP + "prese" generiche, non nomina mai una feature: presa OVERRIDE corpo intero (davanti a Pose History), presa ADDITIVA busto (stance, già su claude/stance-offset; poi reazioni ai colpi), foot lock.
- **Override a slot**: IGSTN_OverrideLayerSource dà anche uno slot; il bool diventa Blend Poses by int (ActiveOverrideSlot). Una interfaccia per feature: ALI_GSTN_Wall, ALI_GSTN_Skid (poi Spin, Drill). Una sola modifica all'ABP base per interfaccia nuova, zero per variante. Vince la priorità (wall > skid).
- **Guidato dalla sim, niente bool BP**: ogni layer ha un parent C++ che copia lo stato di presentazione di Mover (sync state, tag Movement.State.Skid / JumpWindow) in una struct; le regole del grafo sono Property Access su enum. Il layer non scrive mai nella sim.
- **Clip dai dati**: le clip le sceglie l'anim set del kit/skin (ABP figlio con asset override), mai cablate nel grafo base. Kit->LocomotionAnimLayer confluisce nello stesso sistema (B3).
- **Root motion spento** su tutte le clip portate: il movimento lo fa Mover; skid con distance matching sulla distanza di stop che la sim conosce dal freno (curva Distance aggiunta alla COPIA della clip, la posa non si tocca).
- **Mani sul muro**: il Control Rig IK sta DENTRO il wall layer (dopo la macchina a stati), così l'ABP base non sa nulla di muri. Bersagli delle mani = proiezione sul piano del muro (normale + punto di contatto aggiunti a FGSTN_WallAnimState), niente trace per frame. Peso IK per stato (Idle/Left/Right pieno, Skid ridotto) come dato.
- **Stance (owner 17:28Z, ambito: stance)**: la sceglie la GAMEPLAY ABILITY attiva, non il kit. L'abilità porta un tag Anim.Stance.* (tra i tag che concede da attiva, quindi predetto e ripristinato dalla resim); la presentazione legge i tag dell'ASC in sola lettura e li mappa in una tabella tag -> stance (anim set). Il nodo additivo busto di claude/stance-offset resta la presa; cambia solo chi sceglie (oggi kit extension). Senza tag = stance di default del personaggio. Stance animate in futuro = layer ALI_GSTN_Stance, stessa presa.
- **Skid**: layer a 3 fasi Enter/Loop/Exit dalla fase scritta dalla sim (P2 corsa), mirror per piede, uscita con inertialization verso corsa o salto dallo skid. La svolta a 180 (SS_180_Turn) e il solo skid diventano un'opzione per kit (regola "due comportamenti = impostazione").

## 3. Fasi (stime, non costi reali)
| # | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| L0 | Copiare dal 5.7 (originali intatti) in Content/Blueprints/Animation/Ported57/: Skid/{Skid_Revised, Driving_Skid_Revised, SS_180_Turn(+1,2)}, WallSlide/{WallSlideIdle, Driving_WallSlideIdle, WallSlideLeft/RightPose, CR_WallIkControlRig}; ABP_WallSliding solo come riferimento. Stesso scheletro UEFN. Root motion off, curva Distance sulle copie skid. Commit LFS | fisso, editor 5.8 libero, Sonnet medium | ~2 $ |
| L1 | C++: slot di override, campi normale/contatto nel wall state, script setup_wall_anim_layer.py -> setup_anim_layers.py (int blend, wall layer con clip vere + nodo CR), test | cloud, Opus medium | ~5 $ |
| L2 | Build, esecuzione script, cattura wall slide (regola 10) + review | fisso Haiku/Sonnet + review cloud | ~3 $ |
| L3 | Skid layer: FGSTN_SkidAnimState, ALI_GSTN_Skid, ABP_GSTN_SkidLayer, distance matching, mirror, uscite; DOPO P2 della corsa | cloud Opus medium + fisso | ~5 $ |
| L4 | B3: collection di anim set nel kit/arma/skin applicata dal loadout + stance scelta dal tag dell'abilità | cloud Sonnet medium | ~4.5 $ |
Totale ~19.5 $. Ordine: L0 e L1 subito (indipendenti), L2, poi L4, L3 dopo P2.

## 4. Vincoli
- ABP base binario: lo toccano già claude/stance-offset (non in feature) e ora L1. Prima entra stance-offset (PR unico dopo il playtest), poi L1 rigenera il grafo con lo script. Un solo thread alla volta sull'ABP base.
- Due copie di CR_WallIkControlRig nel 5.7 (GiustinoMoves/ e GiustinoMoves/WallSlide/): L0 porta quella usata da ABP_WallSliding.
- Nessuna animazione dell'owner viene modificata: solo copie, curve dati sulle copie.

## 5. Decisioni
1. Stance: decisa dall'owner (17:28Z), scelta dall'abilità attiva (vedi §2). Costo: +~1.5 $ dentro L4 (selezione per tag invece che per kit), nessuna fase nuova.
