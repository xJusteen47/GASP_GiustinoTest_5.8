# Piano: polish animazioni di movimento, meccaniche base e VFX (2026-11-17, solo piano)
Thread cloud, Opus, budget piano ~4 $. Fonti: richiesta owner 12:30Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNT2rJwwRJnZ6GXvXGp3J9fm), handoffs/anim-layer-skid-wallslide-piano.md, run-start-skid-import-piano.md, ground-bounce-1.md, reaction-anim-sources.md, qa-movement-1117.md, handover launch-impacts/hit-react-directional (branch claude/qa-movement-1117 180164f3).
INDICE.md NON trovato (né in project files né su claude/artifacts): stime calibrate sui costi reali scritti negli handover: thread di codice Opus 7-16 $ (stance-offset 7, night-footlock 9, ground-bounce 9, night-knockdown 12-15, qa-movement 16); thread mossa Fable+agy 30-50 $ (anim-finish-3 ~30, jab-dropkick-r1 45-50); review regola 10 Fable 7-9 $.

## 0. Cosa c'è già (non si rifà)
- Locomozione = motion matching GASP (mocap: idle/walk/run/sprint già "AAA" come movimento, non stilizzati). Corsa a 3 stadi, skid sim, rampa accel, preset Mario 3DW: fatti (P1-P3, R1).
- Hook di presentazione: override layer (wall), additivo busto (stance), reazioni direzionali placeholder, GA con timeline per GroundBounce/GetupRoll/GetupNeutral.
- VFX: nel repo NON esiste nessun sistema Niagara nostro (solo handler MCP Niagara nel plugin bridge). Il solo "effetto" è l'hitstop. Quindi oggi niente polvere, scintille, linee di velocità.
- Muri: escono solo con splat (tabella superfici, Force Splat). Nessun rimbalzo di default su muri/soffitti (vedi M5).

## 1. Regole che valgono per ogni checkpoint
Regola 10 (video in-game guardato, PASS/FAIL, max 3 giri di posa), 13 (2-3 tratti d'identità per clip nel brief, versioni approvate bloccate, sheet curve condiviso), feel rule (frame 1 cambia posa), clip dell'owner solo copie. Ogni animazione = blocking → test in gioco → polish (workflow owner). Ogni VFX è un DATO (tabella evento → effetto per kit/skin), mai cablato: si sostituisce senza codice. Niente nella sim: eventi letti dallo stato sincronizzato lato presentazione (rollback: dedupe per frame).

**REVIEW CON FABLE 5.2 (owner 2026-11-17 14:26Z, cmsg_01CRvUdrWm1dJrM4q54YDBLNP9GzDDKrH4RAiM2buVfk14; ambito: ogni animazione e ogni VFX del piano: M1 diagnosi ossa, M2, M3, M4, M6, M7, M8, M9).** Fable 5.2 high (a) da director decide le pose chiave e lo sheet delle curve prima del blocking (regola 3: Fable giudica e decide le pose; agy/Haiku/Sonnet solo catture, import, build) e (b) fa la review video in-game regola 10 (PASS/FAIL, frame uno per uno attorno a hit/FAF/contatto, VFX off poi on). Nessun altro modello firma un PASS; solo PASS arriva all'owner. Costo della review Fable (~7-9 $ per giro) già nelle stime dei checkpoint.

## 2. Checkpoint, in ordine di impatto sul feeling
| # | Cosa | Assorbe dalla coda | Macchina, modello | Stima |
|---|---|---|---|---|
| M0 | Fondamenta presentazione: (a) slot di override a int (L1); (b) tabella "evento di presentazione → Niagara + suono + camera shake + additivo breve" per kit/skin. Eventi: Jump(tipo), Land(velocità d'impatto), RunStart, StageUp, Skid enter/loop/exit, Turn180, Hit(forza), Splat, WallBounce, GroundBounce, Knockdown, GetUp, Roll, Dodge, Shield on/hit/break. Test sulla logica pura | L1 anim-layer, "hit cue kit data" | cloud, Opus high | ~13 $ |
| M1 | Import asset: skid/SS_180/wall slide da GaspMoverProject (copie), review ABP owner tieni/scarta, controllo retarget clip reazione base (ossa staccate) | R3=L0, R4, "skid/wall slide clip import", "hit reaction base bones fix" (diagnosi) | fisso (editor) Sonnet medium + review cloud | ~8 $ |
| M2 | Kit VFX v1 in Niagara, stile pulito/leggibile (checklist VFX approvata 11-15): polvere partenza e skid (scia), sbuffo salto, anello atterraggio scalato dalla velocità, linee di velocità a sprint 2, scintille colpo 3 taglie, impatto muro/soffitto/terra, scia tumble. Review VFX off/on | "decal arrows" no (resta in coda) | portatile (editor) Opus medium + review cloud Fable | ~17 $ |
| M5 | Rimbalzo muri/soffitti di DEFAULT (codice, vedi §3) + evento WallBounce | — | cloud, Opus high | ~10 $ |
| M3 | Salto/aria/atterraggio: jump squat 3f, salita, apice, caduta, atterraggio leggero/pesante (da velocità), salto corto vs pieno, giro in aria. Prima: fix codice giro in aria 300°/s (difetto 3 di qa-movement) | QA jump/air del piano debug | fisso, Fable director + agy; fix cloud Opus | ~35 $ (+~6 $ fix) |
| M4 | Corsa: partenza (R2), skid layer 3 fasi + 180 (L3), wall slide con clip vere (L2), lean e cambio stadio, posa sprint 2 | R2, L2, L3, "run-start clip" | cloud Opus (codice ~10 $) + fisso Fable+agy (~25 $) | ~35 $ |
| M6 | Reazioni: hitstun per forza (pose laterali vere), tumble, rimbalzo terra/muro/soffitto (UNA clip orientata alla normale), knockdown a terra, get-up, roll; fix ossa base | "hit reaction base bones fix", opzione B reazioni | fisso, Fable+agy | ~45 $ |
| M7 | Difesa/utilità: crouch terra+aria, dodge e salto lungo, scudo (alza/tieni/colpito/rotto) | QA crouch/defence | fisso, Fable+agy | ~35 $ |
| M8 | Idle/walk: idle con stance additiva, camminata a stick basso, ritorno a idle con foot-lock | "return to idle" | fisso, Fable+agy | ~20 $ |
Totale ~218 $ (+ ~6 $ fix aria). **Prima ondata** per giudicare il feeling: M0+M1+M2+M5 (~48 $, in parallelo: cloud ×2, portatile, fisso) poi M3 (~41 $): ~89 $.
Ogni checkpoint di animazione = 1 thread di blocking + 1 di polish, review cloud inclusa nella stima. Posti (regola 7): fisso = animazione (agy), portatile = VFX, cloud ≤2 = codice/review.

## 2b. Strati procedurali: Control Rig e animazione fisica (owner 12:36Z, domanda)
- **Control Rig** (IK, non fisica): il CR del wall slide dell'owner entra in M4 (dentro il wall layer, mani proiettate sul muro, piano anim-layer). Stesso strumento, nello stesso checkpoint della clip: foot IK su pendii e skid (M4), lean/aim del busto (M8), mani sul bordo nel ledge (futuro). Costo già nelle stime.
- **Animazione fisica stile Fortnite in volo/caduta** = nuovo **M9** (dopo M6, serve la clip di tumble): (a) moto secondario con nodo RigidBody/AnimDynamics nell'ABP (braccia, gambe, mantello/capelli) in caduta, tumble, salto lungo; (b) Physics Control con blend parziale verso la posa animata per il "dimenarsi" in aria e l'impatto del knockdown/rimbalzo; (c) **stato a terra (downed/knockdown, owner 12:37Z)**: corpo che si adagia sul terreno irregolare (pendii, gradini) e arti che reagiscono ai colpi presi da a terra, blend fisico parziale sopra la clip "lying"; il get-up/roll riparte dalla posa fisica con inertialization. Mai ragdoll completo (la posa deve restare leggibile, regola silhouette). Cloud Opus high (codice) + fisso (tuning, cattura, review regola 10): ~22 $.
- **Online**: solo presentazione, mai nella sim, mai replicata (ogni macchina la calcola dal suo stato: piccole differenze visive accettate). VINCOLO: le hurtbox che seguono le ossa (hurtbox-follow-bones) devono leggere la posa PRIMA della fisica, sennò un client vede/colpisce un corpo diverso: va verificato e fissato in M9.
- **PC toaster**: fisica attiva solo negli stati che la usano (aria, tumble, a terra), corpi limitati (~6-10), LOD: spenta oltre una distanza e per personaggi fuori camera; impostazione grafica on/off. Costo misurato con stat anim prima della review.
Totale piano con M9 e M4a: ~245 $.

## 3. Rimbalzo muri/soffitti (owner 12:30Z, ambito: vittima che tocca muro o soffitto)
Applicato (regola 6), da confermare (13d):
- Default per OGNI superficie non-pavimento: vittima in hitstun/tumble che tocca muro o soffitto rimbalza (riflette la velocità normale) con una clip breve. Lo SPLAT (tabella superfici / Force Splat) e il rimbalzo FORZATO dalla hitbox (Force Bounce) restano cose separate e vincono sul default.
- Ampiezza scalata dall'HITSTUN RIMASTO (parole dell'owner): poco hitstun = rimbalzo piccolo piccolo (minimo in cm, mai zero se in hitstun); scala alternativa per velocità d'impatto come impostazione (regola "due comportamenti = impostazione"). Dati in CombatRules > Wall Impact, stesso stile di GroundImpact (restituzione, min/max, limite di catena).
- Fuori hitstun: nessun rimbalzo, collisione normale (lettura mia).
- Una clip valida per muri e soffitti, orientata alla normale; presentazione via evento WallBounce di M0.

## 4. Decisioni per l'owner (risposta tipo "1A 2B 3A")
1. **Stile della locomozione.** A (consigliata) base mocap GASP per idle/walk/run, stile solo dove il gioco lo chiede (salto, atterraggio, skid, dodge, reazioni) + VFX e additivi: costo come sopra. B set stilizzato fatto a mano che sostituisce il GASP (stile Mario): +~60 $, si perde/rifà il motion matching. Fermo con B: M8 diventa il checkpoint più grande.
2. **Chi dà le pose chiave.** A ogni animazione parte da pose chiave dell'owner (suo workflow; ~25 clip, owner bloccante). B (consigliata) gli agenti fanno il blocking da fonti esistenti (GASP, clip 5.7, pack CC0/Mixamo ritargettati, riferimenti Smash/Mario) e l'owner dà le pose solo per le clip firma: salto, atterraggio, dodge/salto lungo, knockdown/get-up. Nessun costo extra; con A si ferma ogni checkpoint finché non arrivano le pose.
3. **Polish contro piano debug (regola 12: conflitto con l'ordine approvato 11-17).** A (consigliata) per ogni meccanica prima il QA automatico (già previsto, ~8-12 $), poi subito il suo polish: niente animazioni su meccaniche rotte (es. giro in aria 300°/s). B polish subito, QA dopo: rischio di rifare clip. C finire tutto il QA, poi polish: il feeling si vede più tardi.

Applicate senza owner (rimangono reversibili): VFX fatti in casa in Niagara, nessun pack a pagamento (si sostituiscono via tabella); scala del rimbalzo muri per hitstun; M2 sul portatile per non bloccare agy sul fisso.

## 5. Risposta owner 12:52Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNA6Ni3ZyMxWBLC37QG8edXc): 1A 2B 3A approvate + requisiti locomozione
Video di riferimento dell'owner: riferimenti/mario3dw-toad-start-skid-owner-1117.mov (Mario 3D World, Toad: partenza, skid con inversione, sbuffi di polvere a partenza e skid).
- **STILE (ambito: tutte le animazioni del piano)**: smooth / cartoon alla Fortnite / Astro Bot. Entra nello sheet delle curve (regola 13c): ease morbidi, overshoot e follow-through generosi, niente pose rigide. Rischio (regola 12) con 1A: la base GASP è mocap realistica; lo stile arriva da additivi, curve e clip nostre. Se dopo M4 la base sembra ancora "realistica", si riapre la decisione 1 (B, +~60 $).
- **Run-start (ambito: partenza da fermo)**: clip di partenza stile Astro Bot/Mario OBBLIGATORIA in M4 (R2 non è più opzionale/bassa priorità). Frame 1 cambia posa, sbuffo di polvere (M2).
- **Frenata (ambito: corsa a terra)**: due casi distinti. Stick lasciato = frenata morbida (ReleaseBrake per stadio, già nella sim) con clip di stop. Stick tenuto indietro oltre una soglia di dot regolabile = skid (già nella sim: `ReverseStickCos` in GSTN_RunSkidLogic.cpp:17, dato del kit). M4 aggiunge la presentazione dei due casi e verifica che rilasciare lo stick non faccia mai skid.
- **Ripartenza dopo lo skid (owner: "magari")**: opzione per kit `bResumeStageAfterSkid`: dopo l'inversione si riparte dall'ultimo stadio raggiunto invece che da fermo. Applicata (regola 6) con default ON perché compensa il tempo dello skid; da confermare (13d).
- **L3 (CAMBIA la regola precedente "L3 = stadio 3", decisione 3C del piano corsa; ambito: boost di corsa)**: L3 porta allo stadio INTERMEDIO (sprint 1), non all'ultimo. Implementazione: dato `BoostTargetStage` (default = stadio 2), non costante; oggi il codice va dritto all'ultimo (GSTN_RunStageLogic.cpp:198).
- **Lean (ambito: corsa)**: lean ADDITIVO guidato da accelerazione e velocità di svolta, a molla (smooth), ampiezza cartoon; sostituisce il placeholder di RunPresentationComponent. Niente clip di corsa "monolitica" fissa.
- Nuovo checkpoint di codice **M4a** (cloud, Sonnet/Opus medium, ~5 $): BoostTargetStage, bResumeStageAfterSkid, test. Va prima del polish di M4.

## 6. Ondata 1 (avviata 2026-11-17): un checkpoint per thread, brief in handoffs/
- M0 fondamenta presentazione: handoffs/polish-m0-brief.md (cloud)
- M5 rimbalzo muri/soffitti: handoffs/polish-m5-brief.md (cloud)
- M1 import asset: handoffs/polish-m1-brief.md (fisso, editor: quando il fisso è libero)
- M2 kit VFX v1: handoffs/polish-m2-brief.md (portatile, editor; dopo M0, che dà la tabella eventi)
- M4a regole corsa: handoffs/polish-m4a-brief.md (cloud, al primo posto cloud libero)
Poi, per la decisione 3A: QA salto/aria → M3; QA corsa/skid → M4.
