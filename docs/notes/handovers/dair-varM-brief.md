# Brief: dair variante M (per sessione locale `claude`, Sonnet) — 2026-11-04
Leggi SOLO questo file, `Tools/AnimPipeline/cards/README.md` (regole owner), `docs/notes/reviews/dair-varL/review.md` e gli ultimi 2 capitoli di `docs/notes/handovers/anim-pipeline.md` (K, L). Cartella: `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_feature`, ramo `claude/project-thread-0pafeo` (2f5ac29). Lavora da `Tools/AnimPipeline`. `ANIMPIPE_PROJECT` = cartella del progetto principale (non il .uproject).

## 1. Obiettivo (parole dell'owner, 04/11)
"Nei frame f30 e f31 il corpo si irrigidisce per un po' prima di iniziare il movimento di ritorno. Dovrebbe fluire direttamente in f32. Probabilmente è il molleggio delle gambe: essendo un attacco in aria, le gambe non devono molleggiare così tanto all'impatto. Le braccia e la testa sì."
**M = L con meno rimbalzo di gambe/bacino dopo lo stomp e transizione continua f28-32 verso il ritorno.** Testa e pugni mantengono il ritardo di K/L (bacino fermo f19-23, testa/pugni scendono fino a f23).
Chiarimento owner (04/11): "Sembra più che stiano incontrando il terreno così, invece che un semplice molleggio dovuto all'estensione rapida". Quindi le gambe in L sembrano ATTERRARE (accovacciata tenuta in basso, bacino che affonda e resta basso). M deve leggersi come calcio in aria: un rinculo breve e piccolo delle gambe dovuto all'estensione rapida dello stomp, NON un atterraggio/piega tenuta; bacino e piedi non si fermano in basso. Testa e pugni mantengono il ritardo visibile.
Frame = frame di ANIMAZIONE (0-based). Ipotesi (da verificare nel dump prima di toccare): la risalita del bacino/piedi/ginocchia (body_ctrl z, `leg_*` ik, piega ginocchia, picco 22) arriva a un plateau a f28-31 e poi riparte → "rigido". Modifica unica: ridurre l'ampiezza della piega/rimbalzo di piedi+bacino e raccordare f28→f32 senza plateau.

## 2. Vincoli duri
- Frame 0-17 intatti. Key owner 31/37/42 mai toccate (mai spostare key dell'owner). Finestra attiva [15,17], key 15 = posa d'impatto. FAF 37 bloccato. Pugni chiusi.
- Ginocchia davanti entro la larghezza dei fianchi (poli ginocchio di L, f19-26: tienili; se riduci la piega, riduci anche il delta in proporzione, non tornare a "rana").
- Contatto hand_l|spine_03 −0,57 cm @23 tollerato (noto da K/L) salvo se si cancella facilmente.
- Tocca solo parti/frame coinvolti (gambe, bacino, piedi f19-31 circa); mai il corpo come blocco. Una variante sola, max 2 tentativi, poi FERMATI e scrivi cosa serve. Diagnosi di 3 righe scritta prima di ogni build.

## 3. Passi (da Tools/AnimPipeline)
1. Scheda L: `cards/overlord/overlord_dair_v4_varL.json` (generata da `dair_varL_make.py`, a partire da `overlord_dair_v4_varK.json`; se manca rigenerala con `python dair_varL_make.py`). Copia script → `dair_varM_make.py`: parte da varL.json, `move`/`anim`/`source_anim` VarL→VarM, `author.take` = `AS_Pilot_OverlordDAir_V4_VarM`, `variant` = {of: overlord_dair_v4, letter: M, why, base_sha}, salva `cards/overlord/overlord_dair_v4_varM.json`. Le modifiche di K sono nei suoi `edits` (vedi `dair_varK_make.py`/`dair_varK_open.json`: body_ctrl z, piedi = bacino + piega, picco 22).
2. Guarda prima il dump di L f19-34 (z bacino, piedi, ginocchia): trova dove si ferma f30-31. Poi modifica le key di bacino/piedi/piega f20-31 (meno ampiezza, discesa del peso più dolce, risalita monotona fino a f32).
3. `animpipe author cards/overlord/overlord_dair_v4_varM.json` → `animpipe build overlord_dair_v4_varM` → `animpipe check overlord_dair_v4_varM` (output su file, stampa ≤5 righe).
4. Req: copia `cards/overlord/dair/review/overlord_dair_v4_varL_req.json` → `..._varM_req.json`, `variant` = M. Tieni il ritardo testa/pugni vs bacino ≥3 (delay min 3; il max 4 di L può salire), tenuta bacino 19-23 e head/hand "sinks" 20-23. **`pelvis_dip_held` (min 10, max 16 su f17-31) va abbassato** al nuovo valore voluto (meno rimbalzo). Il verify NON ha un tipo "smoothness": aggiungi, se facile, un `amplitude` di bacino/piede z su finestra [28,32] con max piccolo (es. variazione entro 1,5 cm per frame non esprimibile → usa amplitude su [30,31] max ~1), altrimenti annota nell'handover che la fluidità f28-32 si giudica solo da video. Poi `animpipe verify overlord_dair_v4_varM --req=cards/overlord/dair/review/overlord_dair_v4_varM_req.json`.
5. `animpipe render overlord_dair_v4_varM --standalone --azs=90,0,45` (usa `--force` solo se l'unico FAIL del check è hand_l|spine_03 ≈ −0,57 cm; sta in standalone, non tocca l'editor aperto dell'owner).
6. In `docs/notes/reviews/dair-varM/` metti i 3 mp4 di M e le griglie base-vs-M stessi frame **f12-32 e f35-42** per az 90/0/45 (etichette `v<N> (f<n>)`, controlla che f30-32 siano allineate). Commit + push sul ramo (`git push -u origin claude/project-thread-0pafeo`).
7. Handover ≤20 righe in coda a `docs/notes/handovers/anim-pipeline.md` ("Dair variante M"): modifica fatta, verify, check, tentativi, costo (chiamate/contesto/$).

## 4. Regole
Output delle macchine su file, stampa ≤5 righe, mai log interi. Non applicare M sulla card principale (`overlord_dair`); non toccare l'editor aperto dell'owner; niente lavoro extra.

## 5. Dopo
L'owner guarda lui stesso gli mp4 az90 e az0 (regola 10: se possibile un revisore cloud prima) e decide "applica/scarta". Applica = copia a mano sulla card principale (`variant apply` rifiuta le key 37/42 dell'owner), poi merge di `0pafeo` in `feature` (porta il fix di cattura 3f3d01e e la PR #28). Costo atteso: ~25-35 chiamate, ~$3-5 con Sonnet.
