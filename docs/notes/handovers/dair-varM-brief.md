# Brief: dair variante M (sessione locale `claude`, Sonnet) — 2026-11-04, corretto 12:15Z
Leggi SOLO questo file, `Tools/AnimPipeline/cards/README.md` (regole owner) e `docs/notes/reviews/dair-varL/review.md`. Cartella: `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_feature`, ramo `claude/project-thread-0pafeo`. Lavora da `Tools/AnimPipeline`. `ANIMPIPE_PROJECT` = cartella del progetto principale (non il .uproject).

## 1. Problema (owner, 04/11 12:15Z, CORREGGE la richiesta precedente)
"Il problema non dipende dal frame 28-32. Dipende dal molleggiamento eccessivo e troppo lungo. Le gambe dovrebbero rientrare di qualche cm, e solo per 1-2 frame dal frame di stomp, e il primo frame di stomp stesso dovrebbe essere quello più in basso."
Le gambe in L sembrano ATTERRARE (piega profonda e tenuta). Deve leggersi come calcio in aria.

## 2. Specifica di M (= L con questa sola modifica alle gambe)
- **Anim 15 (stomp, primo frame attivo) = punto PIÙ BASSO di gambe/bacino/piedi.** Nessun frame successivo scende sotto.
- Dopo: rinculo di gambe/bacino di **pochi cm, solo anim 16-17** (1-2 frame), poi ritorno continuo. Niente piega profonda, niente tenuta in basso.
- Braccia e testa: restano come L (ritardo K/L, bacino fermo f19-23 NON più valido per le gambe: il bacino segue la regola sopra; testa/pugni scendono fino a f23 col loro ritardo, min 3 frame).
- Ginocchia davanti: tieni i poli ginocchio di L (f19-26), riduci il delta in proporzione, non tornare a "rana".
- La rigidità f30-31 NON è un obiettivo: deve sparire come conseguenza. Si giudica solo dal video az90, non con metriche.

## 3. Vincoli duri
Frame 0-15 intatti (posa d'impatto key 15 invariata). Key owner 31/37/42 mai toccate. Finestra attiva [15,17]. FAF 37 bloccato. Pugni chiusi. Tocca solo gambe/bacino/piedi (e di conseguenza i frame 16-31 che li raccordano); mai il corpo come blocco. Contatto hand_l|spine_03 −0,57 cm @23 tollerato. Una variante sola, max 2 tentativi, poi FERMATI e scrivi cosa serve. Diagnosi di 3 righe scritta prima di ogni build.

## 4. Passi (da Tools/AnimPipeline)
1. Parti da `cards/overlord/overlord_dair_v4_varL.json` (se manca: `python dair_varL_make.py`). Copia lo script in `dair_varM_make.py`: `move`/`anim`/`source_anim` VarL→VarM, `author.take` = `AS_Pilot_OverlordDAir_V4_VarM`, `variant` = {of: overlord_dair_v4, letter: M, why, base_sha}, salva `cards/overlord/overlord_dair_v4_varM.json`.
2. Prima del dump di L f14-32: z bacino, piedi, ginocchia. Trova dove le gambe scendono sotto il valore di f15 e per quanti frame. Riscrivi le key di bacino/piedi/piega f16-31: f15 minimo, rinculo piccolo f16-17, poi risalita monotona.
3. `animpipe author` → `animpipe build overlord_dair_v4_varM` → `animpipe check` (output su file, stampa ≤5 righe).
4. Req: copia `cards/overlord/dair/review/overlord_dair_v4_varL_req.json` → `..._varM_req.json`, variant M. Sostituisci `pelvis_dip_held` con un requisito che valga la regola: bacino/piedi z minimo a f15 e risalita ≤ pochi cm in f16-17 (amplitude). Tieni ritardo testa/pugni ≥3 e "sinks" 20-23. Poi `animpipe verify overlord_dair_v4_varM --req=<file>`.
5. `animpipe render overlord_dair_v4_varM --standalone --azs=90,0,45` (`--force` solo se l'unico FAIL è hand_l|spine_03 ≈ −0,57 cm).
6. In `docs/notes/reviews/dair-varM/`: 3 mp4 di M e griglie base-vs-M stessi frame f12-32 e f35-42 per az 90/0/45 (etichette `v<N> (f<n>)`). Commit + push sul ramo.
7. Handover ≤20 righe in coda a `docs/notes/handovers/anim-pipeline.md` ("Dair variante M"): modifica, verify, check, tentativi, costo.

## 5. Regole e dopo
Output macchine su file, stampa ≤5 righe. Non applicare M sulla card principale; non toccare l'editor dell'owner. Poi il coordinatore apre la revisione cloud sul video (regola 10); solo PASS va all'owner per "applica/scarta". Domande aperte da L (esenzione hand_l|spine_03; braccia FAF f39-42) restano da chiedere all'owner.
