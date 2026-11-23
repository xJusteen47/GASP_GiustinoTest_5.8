# Audit del setup agentico, versione completa (19-11-2026)

## Sintesi per l'owner (leggi solo questa se hai poco tempo)

- **Dove sono andati i soldi.** Dal 18-11 alle 12:00 i thread hanno scritto **65 handover**. La VFX della polvere ne ha consumati circa **148 $**: 69 $ per la serie A (7 giri) e 79 $ per la serie v3 (6 giri). La locomotion ha richiesto **27 handover** e circa **11 h 40 di desktop**, più circa **1,4 milioni di token** di review Fable. In dollari la locomotion non è misurata, perché quasi nessun handover riporta i $.
- **La causa principale non è un errore singolo.** Il sistema non si ferma quando dovrebbe:
  1. quando manca lo strumento previsto (Cascadeur), il thread improvvisa;
  2. il limite di 2 tentativi vale per thread e non per obiettivo: lo skid ha avuto 4 tentativi in 3 thread, il run stop le versioni V1-V5, il run start r1-r3 più 3 thread in gioco;
  3. nessuno controlla che la cattura mostri davvero la clip giusta.
- **Il PASS sbagliato è costato più dei FAIL.** Fable aveva bocciato il run stop V3 come "posa di scivolata congelata". Poi lo splice automatico sugli 0,2 s iniziali e finali ha tolto vita alle clip, il check numerico è passato e Fable ha approvato (anim-setup-4). Così 4 thread hanno lavorato sopra clip rigide.
- **Cosa ti chiedo (3 decisioni, risposta tipo "1A 2A 3A").**
  1. Regole nuove R5-bis (tentativi per obiettivo), R6-bis (stop se manca lo strumento) e R14 (registro delle divergenze da GASP). **A: approva** (consigliata), B: solo R5-bis.
  2. Permessi sul fisso: incolla il blocco JSON della sezione 5. **A: sì** (consigliata, ti toglie i .bat a mano e i dialoghi bloccanti), B: no.
  3. Base della locomotion, cioè la decisione già aperta in "Make run start flow into the run": **A: copia stock GASP Mover + clip mocap GASP ristilizzate con pose matching** (consigliata dal thread e coerente con questo audit), B: Cascadeur.
- **Risparmio stimato** se applichiamo le sezioni 3-5: **30-45% della spesa settimanale** sul lavoro animazione e VFX, e circa **20-30 minuti al giorno del tuo tempo**. È una stima, non una misura: va verificata dopo una settimana con il registro costi della sezione 4.4.

Fonti: 65 handover del repo (tutti i branch claude/*, modificati dal 18-11 alle 12:00), lista dei 111 thread, chat di progetto (parte), memoria di progetto, misura costi del 31-10. I comandi sono a fine documento.

---

## 1. Numeri misurati

| Voce | Valore | Fonte |
|---|---|---|
| Handover scritti dal 18-11 alle 12:00 | 65 | `git log --all --since=2026-11-18T12:00 -- docs/notes/handovers/` |
| Handover senza alcuna riga di costo | 15 su 65 (23%) | grep "cost/costo" sui 65 file |
| Handover con il costo in $ | ~25; gli altri lo danno in ore o token | idem |
| VFX polvere, serie A (A1, A1b, A2, r2-r5) | ~69 $ in 7 giri | vfx-smoke-a*.md |
| VFX v3 (r1-r5 + hd1) | ~79 $ in 6 giri | vfx-v3-*.md |
| Locomotion: handover | 27 (finish 1-4, fix, anim-setup 1-4, run-stop 1-2, run-start 4, skid 4, crouch 5, routing, isolation, dash) | lista file |
| Locomotion: tempo desktop dichiarato | ~11 h 40 | righe "Cost:" |
| Locomotion: token delle review Fable | ≥1,4 M (600k, 370k, 320k, 124k...) | locomotion-finish-1/2, run-start-frantic-2 |
| Skid: tentativi | 4 in 3 thread, più skid-root/rootmotion/main-graph | locomotion-finish-2/3, skid-*.md |
| Run stop: versioni | V1→V5, 2 FAIL di posa, poi PASS dopo lo splice | locomotion-finish-1/3, anim-setup-4 |
| Root motion delle clip vs capsula | RunStart 0 cm; RunStop 125 vs ~360 cm; Skid 45 cm | run-start-handoff-1 §3 |
| Owner chiamato per passi manuali | ≥7 casi: 3 delete rifiutati, fix_clips_write.bat, 2 dialoghi PackageRestoreData (uno ha causato 2 timeout), restore crouch | locomotion-finish-1/2, anim-setup-3, skid-root-1, run-start-stop-skid-1, crouch-veloce-3 |
| Approvazioni di accesso al PC ripetute | 4 in un solo giorno (22-10) | chat di progetto |
| Costo misurato il 31-10 | 66% della spesa = rilettura della cache di contesto (480-850k) | memoria |

Limite: i $ sono **stime scritte dai thread**, non bollette. Nessun handover li confronta con INDICE.md (regola 8), e INDICE.md non è nella cartella del progetto (handoffs/INDICE.md non esiste lì).

---

## 2. Cause radice e soluzioni

Per ogni causa: **evidenza → perché è successo → soluzione concreta**. I testi pronti sono nelle sezioni 3, 4 e 5.

### C1. Il thread improvvisa quando manca lo strumento giusto
**Evidenza.** run-stop-2: "Cascadeur access was declined, so the clip is authored as numbers and solved by the pipeline's IK poser". run-start r3 = `author_runstart_r3.py` (27 chiavi a 30 fps, root bloccato). run stop V4/V5 = `run_stop_gen.py` dalle pose delle schede.
**Perché.** La regola 6 ("applica la consigliata, non aspettare") si applica anche alla scelta di pipeline. Nella skill nessun prerequisito dice "senza X non si parte".
**Soluzione.** R6-bis (§3) e una sezione "Prerequisiti" nella skill (§4.1). Il generatore Python resta solo per prototipi segnati `PROTO`, che non vanno mai in review né all'owner.
**Risparmio.** Evita la catena clip rigida → review → fix → nuovi giri. Sulla locomotion di questa settimana stimo **40-60 $**.

### C2. Fix automatici che distruggono quello che il check non misura
**Evidenza.** anim-setup-4 §3: splice "first/last 0.2 s to the neighbour pose" su dash, run start, run stop L/R e skid. Su una clip di 27 frame tocca metà del run start (run-start-handoff-1 §7).
**Perché.** Il fixer ottimizza la metrica (seam, pop, hand-off) senza un vincolo di conservazione: ampiezza, velocità e root motion.
**Soluzione.**
- Nel fixer, un **guard di conservazione**: per ogni passo confronta prima e dopo l'escursione angolare dei bracci e delle gambe, la velocità di picco e il root_travel. Se una cala più del 15%, il passo fallisce e non salva. Script in §4.3.
- Lo splice non supera mai il 10% della clip e mai 0,1 s.
- Ogni fix automatico produce una griglia prima/dopo nella stessa review.

### C3. Review che premiano la pulizia invece della vita
**Evidenza.** Fable ha bocciato il run stop V3 come "posa di scivolata congelata" (locomotion-finish-3), poi ha approvato le clip dopo lo splice (anim-setup-4 §8). Il critico cieco e la rubrica non avevano assi "vita" e "root". Sono stati aggiunti solo oggi (run-start-handoff-1 §4: rubric.md `vita` e `root`).
**Perché.** I check numerici erano tutti negativi (assenza di difetti). Mancava un criterio positivo, cioè la presenza di anticipazione, arco e peso. La review vedeva anche i numeri del check, che la ancoravano verso il PASS.
**Soluzione.**
- Ordine della review: **vita prima**, guardando il video senza numeri, poi i numeri.
- FAIL automatico se root_travel della clip differisce da quello della capsula di oltre il 20%, o se la clip è stata generata da `PROTO`.
- Testo pronto in §4.2.
**Risparmio.** 1-2 giri per mossa (5-20 $ ciascuno).

### C4. La cattura non dimostra quale clip sta suonando
**Evidenza.** Per un giorno le catture hanno mostrato lo stop di GASP, perché mancava `gstn.RunStop.Mode 1` (run-start-handoff-1 §7, anim-setup-4 §8). Ogni review di quel giorno giudicava la clip sbagliata.
**Soluzione.** La cattura scrive già `frames.jsonl` con `combat.anim` e i pesi. Basta uno script `capture_assert.py` che fallisce se la clip attesa non ha peso ≥0,8 nella finestra attesa. La review non parte senza `ASSERT: OK`. Testo in §4.3.
**Risparmio.** Un giorno di review a vuoto è costato circa **20-40 $**.

### C5. Deriva da GASP Mover stock
**Evidenza.** run-start-handoff-1 §6: "Our rows: fixed start time, no pose match". Uscite a tempo invece che a evento (§2). Root motion a 0 o sbagliato. Skid lasciato in un override layer invece di una riga del chooser (run-start-stop-skid-1). L'owner ora chiede di ripartire dallo stock.
**Perché.** Ogni deviazione era un fix locale ragionevole. Nessun registro le sommava e nessuno chiedeva "come lo fa GASP?" prima di cambiare.
**Soluzione.**
- **R14** con il file `docs/notes/gasp-divergenze.md`: una riga per deviazione, con motivo, alternativa stock e costo per tornare indietro.
- Prima di ogni nuova deviazione, una domanda all'Unreal Assistant e una riga nel registro.
- Default: pose matching, root motion dalla clip, transizioni a evento (contatto del piede).

### C6. Il coordinatore insegue sintomi clip per clip
**Evidenza.** Thread "Finish stop, skid, dash", poi "Finish run stop, skid and dash", "Fix failing clips and finish locomotion", "Finish skid, run start and air crouch", "Finish locomotion hand-offs and air crouch": 5 thread sullo stesso obiettivo in un giorno. Il problema di sistema (base non stock, clip senza root motion) è stato nominato solo alle 21:35.
**Perché.** La regola 5 limita i tentativi dentro un thread. Il coordinatore apre un thread nuovo e il conto riparte da zero.
**Soluzione.** **R5-bis**: dopo 2 thread falliti sullo stesso obiettivo, il successivo è obbligatoriamente un thread di **diagnosi di sistema** (Opus high, solo cloud, ~5-8 $). Il thread legge gli handover dell'obiettivo e risponde a "cosa hanno in comune i FAIL?". In INDICE.md ogni riga porta un `obiettivo:` per contare i tentativi.
**Risparmio.** 25-35% dei giri su locomotion e VFX, circa **30-60 $ a settimana**.

### C7. Le regole dell'owner restano testo e non diventano check
**Evidenza.** Polvere con puff ad alpha 1 e restringimento: il "mai restringere" era in memoria, ma il restringimento veniva dalla curva del template (a2r5: "r5c FAIL shrink = template curve"). Lo script `vfx_size_monotonic.py` esisteva, ma come log e non come cancello ("Make the template size module log a failing check"). Il r5 sembrava finto per un motivo noto in teoria VFX (unlit, un solo layer), scoperto solo dopo 6 giri.
**Soluzione.**
- `vfx_spec_check.py` come cancello prima della review: alpha finale = 0, dimensione monotona crescente, materiale lit, ≥2 layer (§4.3).
- Il brief di ogni giro VFX elenca i requisiti tecnici del riferimento **prima** del giro 1: luce, layer, motion stretch, variazione.
**Risparmio.** 2-3 giri su 6, circa **20-35 $**.

### C8. Contesto, handover e costi non misurati
**Evidenza.** 66% della spesa in rilettura (misura del 31-10). Thread chiusi a ~150k con subagenti Fable da 100-600k ciascuno. 23% degli handover senza costo. Costi solo stimati e mai confrontati.
**Soluzione.**
- **Registro costi**: a fine thread una riga in `handoffs/costi.csv` (thread, obiettivo, modello, token se noti, minuti di desktop, $ stimati, esito).
- Chiusura del thread a **120k**.
- Review Fable con input **solo griglie e criteri**: niente handover precedenti né codice. Target ≤80k per review.
- Il coordinatore ricontrolla a fine settimana le stime con l'uso reale della pagina di billing.
**Risparmio.** 20-30% della spesa di contesto.

### C9. Permessi e macchine: il tempo dell'owner come collo di bottiglia
**Evidenza.**
- Delete rifiutati 3 volte: `run_stop_gen.py`, `IN-USO.txt`, restore crouch.
- Il dialogo `PackageRestoreData.json` ha bloccato le catture 2 volte, con 2 timeout = tentativi bruciati (skid-root-1).
- Il salvataggio degli asset era bloccato anche con l'OK in thread. È **risolto** dalla regola `Save-Assets.ps1`, che in anim-setup-4 ha funzionato 6 volte.
- Approvazioni di accesso al PC ripetute 4 volte in un giorno (22-10).
- I thread su device richiedono un messaggio fresco dell'owner.
**Soluzione.** Blocco permessi in §5:
- delete solo nelle cartelle Saved/ e scratch;
- pulizia di PackageRestoreData dentro Run-Timed prima di avviare l'editor;
- git restore solo su file specificati.

Per i thread su device vale la regola 9: un solo messaggio di inizio sessione con la frase da incollare. Proposta: la frase copre esplicitamente "fino a 3 thread su device in questa sessione".

### C10. Editor e catture lente
**Evidenza.**
- Editor in background bloccato a 333 ms/frame, "not liftable" dopo 2 tentativi (locomotion-finish-4).
- 3 giocatori in un solo processo = clamp a 125 ms, ≤8 fps (anim-setup-1).
- La cattura renderizzava 7 viste e un PNG per frame, ridotte a 3 (anim-setup-2).
- `Run-Timed -unattended` risolve il throttle per le catture VFX (a2r3).
**Soluzione.** Rendere default ovunque: `-unattended`, 1 giocatore PIE, 3 viste, editor del playtest separato da quello delle catture. Lo script di cattura rifiuta di partire se trova `PIE worlds > 1`, salvo il flag `-Clients`.

### C11. Unreal Assistant gratuito poco utile così com'è
**Evidenza.** anim-setup-4 §7: non apre i nostri asset e ha rifiutato la domanda sull'architettura ABP. Ha però risposto bene sulle domande generali di GASP (q5-q6: uscita a evento, root motion necessario).
**Soluzione.** Usarlo per le **domande "come fa GASP stock?"** prima di ogni divergenza (R14), non per analizzare i nostri asset. Se l'owner imposta l'accesso in lettura, va ritestato con una sola domanda.

---

## 3. Testi pronti per le regole di progetto (da incollare se approvi)

**R5-bis (tentativi per obiettivo).** "Ogni riga di INDICE.md ha un campo `obiettivo`. Dopo 2 thread con esito FAIL o parziale sullo stesso obiettivo, il coordinatore non apre un terzo thread di esecuzione: apre un thread di diagnosi di sistema (cloud, Opus high, budget 8 $). Il thread legge solo gli handover di quell'obiettivo e scrive la causa comune e 2-3 opzioni. Poi decide l'owner se l'opzione cambia architettura, altrimenti il coordinatore."

**R6-bis (strumenti e pipeline).** "La regola 6 non copre il cambio di strumento o di pipeline. Se lo strumento previsto dalla skill o dal brief (Cascadeur, editor, salvataggio asset, macchina) non è disponibile, il thread si ferma al primo punto stabile e scrive cosa serve. Un generatore alternativo può produrre solo prototipi marcati PROTO, che non vanno in review né all'owner."

**R10-ter (catture e review).** "Una review parte solo se la cattura ha `ASSERT: OK` (clip attesa con peso ≥0,8 nella finestra attesa). La review giudica prima la vita (anticipazione, arco, peso, follow-through, root motion coerente con la capsula) guardando il video senza numeri, poi i check tecnici. Un fix automatico che riduce ampiezza, velocità o root motion di oltre il 15% è un FAIL."

**R14 (divergenze da GASP).** "Ogni deviazione dal comportamento stock di GASP Mover (righe del chooser, pose matching, root motion, uscite, layer) ha una riga in docs/notes/gasp-divergenze.md con motivo, alternativa stock e costo per tornare indietro, scritta prima del commit. Prima di aggiungere una deviazione, il thread chiede all'Unreal Assistant come la gestisce GASP stock."

**R2 (modifica).** "A 120k token chiudi" invece di 150k. "L'handover termina con la riga `COSTO: <$ stimati> | <minuti desktop> | <token review>` e la stessa riga va in handoffs/costi.csv."

**R4 (aggiunta).** "Un subagente di review riceve solo griglie, criteri e brief: niente handover precedenti, codice o log. Target ≤80k token."

---

## 4. Modifiche a skill e script (pronte da applicare in un thread di codice)

### 4.1 Skill gstn-move-animation, nuova sezione all'inizio
```
## Prerequisiti (stop se mancano)
- Cascadeur raggiungibile (o l'owner ha scelto per iscritto un'altra fonte: mocap GASP, AnimGen).
- Editor del checkout giusto chiuso o lock _ue59 libero; Save-Assets.ps1 consentito.
Se uno manca: fermati, scrivi cosa serve. Vietato generare la clip con Python/Blender interpolando pose
(solo PROTO, mai in review).
## Fix automatici
- Ogni passo del fixer passa il guard di conservazione (ampiezza, velocità di picco, root_travel: max -15%).
- Splice max 0,1 s e max 10% della clip; sempre griglia prima/dopo.
## Root motion
- La clip porta root motion; root_travel entro ±20% di quello della capsula nella stessa finestra.
```
La modifica dell'isolation check è già pronta su claude/project-thread-c0lfu3 e va proposta insieme a questa.

### 4.2 Skill gstn-anim-reviewer e critico estetico
```
Ordine obbligatorio:
1. Verifica ASSERT: OK nella cattura; se manca -> STOP "catena di cattura".
2. Senza leggere numeri: guarda il video a velocità reale e al 25%. Asse VITA: anticipazione presente?
   arco (non linea)? peso/ammortizzazione al contatto? follow-through e overlap su braccia e testa?
   Asse ROOT: il corpo avanza con la capsula o scivola/resta fermo?
   Se VITA o ROOT è FAIL -> FAIL "posa", senza guardare i check.
3. Solo dopo: check tecnici (piedi, seam, pop, hand-off).
4. Se la clip ha subito un fix automatico: confronta la griglia prima/dopo; perdita di vita = FAIL.
```

### 4.3 Script nuovi o da trasformare in cancelli (stima 10-18 $ in un thread Sonnet/Opus medium)
| Script | Cosa fa | Esito |
|---|---|---|
| `Tools/AnimPipeline/capture_assert.py <frames.jsonl> <clip> <finestra>` | legge `combat.anim` e i pesi da frames.jsonl | stampa `ASSERT: OK/FAIL`, exit code |
| `clip_check.py --conserve <prima> <dopo>` | escursione angolare per catena, velocità di picco, root_travel | FAIL se cala >15% |
| `clip_check.py --root-vs-capsule <frames.jsonl>` | confronta root_travel della clip con lo spostamento della capsula | FAIL oltre ±20% |
| `Tools/VFX/vfx_spec_check.py` (unifica `vfx_size_monotonic.py`) | alpha finale 0, dimensione monotona, materiale lit, numero di layer | FAIL bloccante |
| `Run-Timed.ps1` pre-start | rinomina `Saved/Autosaves/PackageRestoreData.json` in `.bak`; default `-unattended` | niente dialoghi bloccanti |
| `vfx_v3_capture.ps1` | rifiuta `PIE worlds > 1` senza `-Clients` | catture 2-3x più veloci |

### 4.4 Registro costi
`handoffs/costi.csv` con le colonne `data,thread,obiettivo,macchina,modello,minuti_desktop,token_review,usd_stimati,esito`. Il coordinatore somma per obiettivo e confronta con il billing ogni venerdì (il giorno del reset).

---

## 5. Permessi sul PC fisso (azione owner, solo se approvi la decisione 2)

In `C:\Users\jacop\.claude\settings.json`, dentro `permissions.allow`, unisci queste righe (non sostituire il file):
```json
"PowerShell(& .\\Tools\\CI\\Save-Assets.ps1 -Script Tools\\*)",
"PowerShell(Remove-Item .\\Saved\\*)",
"PowerShell(Rename-Item .\\Saved\\Autosaves\\PackageRestoreData.json *)",
"Bash(rm Saved/*)",
"Bash(git restore -- Content/GSTN/*)",
"Bash(git restore -- Tools/*)"
```
La prima riga dovrebbe esserci già (anim-setup-4 la usa). Le altre sono limitate a Saved/, Content/GSTN e Tools/ e non toccano `reset --hard`, `clean` o i branch feature e main. Non ho cambiato nessuna impostazione.

---

## 6. Cosa ha funzionato (da tenere)
- **Isolation check offline** (13/13 GASP PASS, calibrato): oggettivo, economico, gira in cloud. Ha trovato i difetti di dash e run start senza editor.
- **Save-Assets.ps1** con allow rule: 6 salvataggi senza owner. È il modello giusto per gli altri blocchi.
- **Run-Timed** con ESITO e TIMEOUT: nessun processo appeso.
- **Video prima del giudizio** e confronto base vs variante sugli stessi frame.
- **Studio frame per frame del riferimento** (studio_dk_clip_2059.md): ha trovato in un passaggio le cause della "polvere finta" che 6 giri non avevano isolato. Va fatto **prima** del giro 1, non dopo.
- **Regola 12 (guardia sull'owner)** e memoria condivisa: decisioni e ambiti sopravvivono tra thread.
- **Thread di diagnosi spontanei**: run-start-handoff-1 in 35 minuti ha trovato le cause vere (root motion, splice, cattura, righe senza pose match). È la prova che R5-bis funziona.

---

## 7. Classifica dei cambiamenti per impatto

| # | Cambiamento | Su cosa | Risparmio stimato |
|---|---|---|---|
| 1 | R5-bis: diagnosi dopo 2 thread falliti per obiettivo | costo, tempo | 30-60 $/settimana |
| 2 | R6-bis + prerequisiti nella skill | qualità, costo | 40-60 $ per episodio evitato |
| 3 | capture_assert + review con la vita prima | qualità, costo | 20-40 $ per giorno a vuoto evitato; 1-2 giri per mossa |
| 4 | Guard di conservazione nel fixer | qualità | evita regressioni silenziose |
| 5 | vfx_spec_check + studio del riferimento prima del giro 1 | costo VFX | 20-35 $ per serie |
| 6 | Permessi (§5) + pulizia PackageRestoreData | tempo owner, tentativi | 20-30 min/giorno owner; nessun timeout bruciato |
| 7 | Chiusura a 120k, review ≤80k, registro costi | costo | 20-30% della spesa di contesto |
| 8 | R14 registro divergenze GASP | qualità a lungo termine | non stimabile, è il debito più grosso |
| 9 | Default catture (1 PIE, -unattended, 3 viste) | tempo | catture 2-3x più veloci |
| 10 | Unreal Assistant solo per "come fa GASP stock?" | qualità | piccolo, gratuito |

Costo per applicare tutto: un thread di codice Sonnet/Opus medium per gli script (10-18 $), più una modifica delle skill (3-5 $). Si ripaga entro la prima settimana se R5-bis evita anche un solo giro.

---

Comandi usati (in sola lettura): `git log --all --since=2026-11-18T12:00 --name-only -- docs/notes/handovers/` su un clone di tutti i branch claude/*; `grep -iE "cost|costo|\$"` sui 65 file; `list_threads` dal 17-11; `read_project_chat`.
