# BACKUP - Processo Ufficiale di Animazione 3D e Pipeline Autoriale (GASP / Overlord)
# Data backup: 2026-11-06
# Creato su richiesta dell'Owner (Jacopo) come copia di ripristino congelata.

# Processo Ufficiale di Animazione 3D e Pipeline Autoriale (GASP / Overlord)

> [!IMPORTANT]
> **PRIORITÀ ASSOLUTA E DISCIPLINA DI MODIFICA**:
> 1. Questo processo **ha priorità assoluta su qualsiasi vecchio flusso, brief o procedura precedente** (inclusi i vecchi brief rig-points / Cascadeur roundtrip documentati in `docs/notes/handovers/` e `Tools/AnimPipeline/HANDOFF_2026-11-02.md`).
> 2. **NON MODIFICARE questo processo senza la preventiva approvazione esplicita dell'owner (Jacopo).**
> 3. Prima di qualsiasi modifica futura concordata ed esplicitamente approvata, **è obbligatorio creare una copia di backup** (es. `docs/notes/anim-process.backup.md` o copia versionata con timestamp) per poter ritornare indietro in qualsiasi momento.

---

## 1. Messaggio per Claude: Perché Questo Processo Sostituisce il Vecchio

Ciao Claude. Questo documento descrive in dettaglio le innovazioni metodologiche, artistiche e tecniche sviluppate per la pipeline delle animazioni di combattimento (focus su **Overlord** e mosse avanzate in Unreal Engine 5). 

Nelle sessioni precedenti, la pipeline si basava su un flusso rigido incentrato su punti nello spazio Cascadeur, solve indiretto verso ossa, check numerici rigidi e animazioni spesso esportate come blocchi monolitici. Questo causava:
1. **Clip monolitiche non responsive**: attacchi aerei (es. dropkick o down air) cotti in un unico blocco a fotogrammi fissi, rendendo impossibile gestire salti ad altezza variabile o cadute prolungate nel gameplay.
2. **Pose innaturali e prive di anticipazione**: pose che partivano al frame 0 con arti già contratti o conserti, creando salti visivi bruschi rispetto alla idle o alla guardia di combattimento.
3. **Secondary motion instabile o gommosa**: oscillazioni elastiche ripetute ("bounciness" da pupazzo di gomma) o, all'opposto, totale rigidità priva del senso di tonnellaggio e peso dell'armatura pesante.
4. **Falsi positivi nei check numerici**: controlli rigidi pensati per jab/pugni che fallivano su calci aerei e dropkick per occlusioni volute (es. braccia conserte durante un calcio).
5. **Blocchi operativi in Unreal Engine**: finestre modali di dialogo dell'editor ("Sovrascrivi l'oggetto esistente") che bloccavano l'engine loop e mandavano in timeout i comandi remoti.

Il nuovo processo risolve radicalmente ognuno di questi problemi. Di seguito trovi le regole, i principi e le istruzioni esatte da seguire.

---

## 2. I Pilastri del Nuovo Processo

### Pilastro A: Architettura Modulare a 3 Stati per Mosse Aeree / Dinamiche
Nessuna mossa aerea con impatto al suolo deve essere generata come clip unica cotta. Ogni mossa aerea complessa viene strutturata e compilata in **tre asset indipendenti**:

```
[ AS_<Move>_Startup ]  --->  [ AS_<Move>_Loop ] (In loop aereo)
(0.25 - 0.27s)               (10 - 12 frame, seamless f0 == fN)
                                   |
                             (Collisione Suolo)
                                   v
                             [ AS_<Move>_Land ]
                             (Crash + Rimbalzo Peso + Downed)
```

1. **`AS_<Move>_Startup` (12–16 frame)**:
   - **Regola del Frame 0**: La mossa **non deve mai** partire al frame 0 nella posa estrema del colpo. Frame 0 parte dalla guardia aerea o idle di transizione naturale (`arm_l` e `arm_r` aperti e reattivi).
   - **Coil & Shape Shift (Frame 4–8)**: Il bacino curva all'indietro, le ginocchia vengono richiamate verso il petto accumulando energia elastica, e gli arti si riposizionano rapidamente (es. sweep delle braccia che si incastrano con uno snap conserto sul petto a frame 8).
   - **Thrust / Hitbox Active (Frame 11–15)**: Esplosione rapida dell'arto d'attacco (spostamento rapido in $Y$, bacino orientato per dare direzione cinetica).

2. **`AS_<Move>_Loop` (10–12 frame, Seamless)**:
   - Stato di caduta / volo aereo libero.
   - **Invariante di continuità**: Il frame finale della clip coincide esattamente con il frame 0 ($f_0 == f_N$) per consentire un looping continuo senza hitch visivi.
   - **Secondary Motion da attrito dell'aria**: Include solo una micro-oscillazione ad alta frequenza e bassa ampiezza (es. $Z \pm 0.4\text{ cm}$, rotazione $\pm 0.5^\circ$) sul baricentro e sul busto a metà ciclo (frame 5–6), per comunicare la sensazione di un proiettile ad alta velocità che fende l'aria.

3. **`AS_<Move>_Land` (24–32 frame)**:
   - Triggerato dall'impatto con il pavimento.
   - **Frame 0–2 (Ground Crash)**: La piastra dorsale dell'armatura (o i piedi) si schianta sul terreno; la spina si appiattisce al suolo e la testa frusta per inerzia (`head = [12, 0, 0]`). Il personaggio mantiene la posa rigida senza aprire le braccia per attutire, dimostrando follia e brutalità marziale.
   - **Frame 5–8 (The Weight Bounce / Rimbalzo del Peso)**: Per comunicare l'enorme massa dell'armatura in piastre sul pavimento rigido, il busto e il bacino **rimbalzano verso l'alto con una quota marcata (+15 / +16 cm)** ($Z = 22\text{ cm}$), con la spina dorsale che si inarca verso l'alto (`spine = [16, 0, 0]`) e il mento serrato al petto.
   - **Frame 10–14 (Secondo Impatto & Assestamento)**: Ricaduta secca al suolo ($Z = 6\text{ cm}$) senza oscillazioni armoniche residue (smorzamento critico).
   - **Frame 15–32 (Sliding Friction -> Downed Rest)**: Decelerazione orizzontale da attrito e arresto completo disteso a terra a pancia in su, perfettamente collegabile allo stato logico di rialzo (*Getup / Tech Roll*).

4. **Assemblaggio AnimMontage (`AM_<Move>`)**:
   - I tre segmenti vengono inseriti nello slot di animazione (`DefaultSlot`) dell'AnimMontage tramite script Python Unreal (`unreal.AnimMontage`, `unreal.AnimTrack`, `unreal.SlotAnimationTrack`).
   - La sezione centrale `DiveLoop` è configurata per eseguire loop fino al momento in cui l'Ability di gameplay rileva il contatto con il suolo e salta alla sezione `Land`.

---

### Pilastro B: Authoring Diretto su Card JSON (`animpipe author`)
Non si generano file intermedi dispersi o punti rigidi. Tutto il design risiede nelle move card JSON (`Tools/AnimPipeline/cards/<kit>/<move>.json`):

1. **Coordinate e Spazio di Trasformazione**:
   - Spazio componenti Unreal: $+X$ = sinistra del personaggio, $+Y$ = avanti, $+Z$ = alto.
   - Rotazioni `[rx, ry, rz]` in gradi: $+rx$ inclina all'indietro (beccheggio positivo indietro; una flessione in avanti è $-rx$).
2. **Definizione Pose Chiave**:
   - `pelvis`: offset `loc: [x, y, z]` e orientamento `rot: [rx, ry, rz]`.
   - `spine`: rotazione totale del tronco distribuita su `spine_01..05`.
   - `head`: rotazione di `neck_01..02` e `head`.
   - `arm_l` / `arm_r`: target della mano `hand: [x, y, z]` e gomito `pole: [x, y, z]` nel frame del tronco (`spine_05`).
   - `leg_l` / `leg_r`: target della caviglia `foot: [x, y, z]`, ginocchio `pole: [x, y, z]` e orientamento `foot_rot: [rx, ry, rz]` nel frame del bacino (`pelvis`).
3. **Verifica Rigorosa delle Lunghezze Ossee**:
   - Il comando `python -m Tools.AnimPipeline.animpipe author <card.json>` deve sempre restituire:
     `read-back PASS bone lengths: 65 bones within 0.1 cm of the reference on every frame` (peggior errore concesso: $0.00\text{ cm}$).
   - Successivamente, `python -m Tools.AnimPipeline.animpipe build <move>` effettua il bake su Control Rig e produce `AS_<Move>` e `LS_<Move>`.

---

### Pilastro C: Trattamento della Fisica e Secondary Motion (Damped vs Bouncy)
- **Vietata la "Bounciness" da molla elastica**: Non usare oscillazioni armoniche sinusoidali che fanno sembrare il cavaliere un pupazzo o un giocattolo di gomma.
- **Profilo Fisico Impulse-Decay (Smorzamento Critico)**:
  $$\text{Impulso Diretto (Shock 1-2 frame)} \longrightarrow \text{Rebound Singolo del Peso} \longrightarrow \text{Assestamento Secco} \longrightarrow \text{Attrito Radente}$$
  - Il rimbalzo deve mostrare il **peso rigido dell'armatura**, non l'elasticità dei muscoli: una collisione ad altissima energia che rimbalza una sola volta in modo netto (+16 cm) e si ferma immediatamente sul secondo impatto con attrito.

---

### Pilastro D: Personalità e Appeal dell'Overlord
- **Lore & Stile di Combattimento**: L'Overlord è un cavaliere pesante (corporatura Ganondorf), instabile mentalmente ma marzialmente controllato. Combatte con violenza spietata, scagliando la propria corazza pesante come un ariete da sfondamento.
- **Le Braccia Conserte come Posa di Potere**: Mantenere le braccia conserte durante calci aerei, dropkick e cadute è una scelta stilistica di supremazia e disprezzo verso l'avversario. Tuttavia, l'entrata nella posa deve essere dinamica: parte da una guardia pronta e si serra con uno snap durante il caricamento.

---

### Pilastro E: Disciplina Operativa ed Esecuzione in Unreal Engine
1. **Nomi Asset e Isolamento**:
   - Usare sempre nomi chiari e univoci con prefissi e suffissi puliti (`AS_OverlordDropkick_Startup`, `_Loop`, `_Land`).
   - Non ricreare da zero gli asset con `AssetTools.create_asset` se esistono già: usare `unreal.load_asset` per evitare che l'editor sollevi il popup modale bloccante *"Sovrascrivi l'oggetto esistente"*.
2. **Move Cards e Locks**:
   - Quando si crea una nuova variante sperimentale, non inserire blocchi `"approved"` prima che l'owner abbia dato il consenso visivo, altrimenti `locks.py` rifiuterà qualsiasi modifica tramite `ValueError`.
3. **Controllo Visivo 360°**:
   - Generare sempre la visual sheet multi-camera per verificare la silhouette da tutte le angolazioni (profilo PvP, camera follow da dietro, visuale rialzata isometrica).

---

## 3. Checklist per Claude prima di ogni Erogazione

Prima di considerare concluso un asset di animazione, verifica che:
- [ ] La mossa aerea sia divisa nei 3 sottomoduli (`Startup`, `Loop`, `Land`) e integrata (o pronta per l'integrazione) in `AM_<Move>`.
- [ ] Il frame 0 dello `Startup` parta da guardia o transizione naturale, senza salti innaturali di posa.
- [ ] Il modulo `Loop` sia seamless ($f_0 == f_N$) e contenga solo micro-vibrazioni atmosferiche, senza deriva di traslazione.
- [ ] Il modulo `Land` contenga il rimbalzo cinetico pesante (+15/16 cm) e termini in stato *Downed* stabile.
- [ ] Il comando `author` verifichi 65 ossa entro 0.1 cm (PASS) senza popping articolare.
- [ ] Non vi siano popup modali pendenti nell'Unreal Editor.
- [ ] Qualsiasi proposta di modifica a questo processo sia stata **preventivamente approvata da Jacopo** e preceduta da un backup di questo file.
