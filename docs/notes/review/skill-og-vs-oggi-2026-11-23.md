# Skill animazione: la OG contro quella di oggi, dove sono i buchi (2026-11-23, cloud, sola lettura)

**OG** = skill account `gstn-move-animation`, archiviata il 15/11 in `Tools/AnimPipeline/contract/archive/gstn-move-animation_2026-11-15.md` (righe citate "OG r.N"); è ancora installata e la copia sincronizzata ha aggiunte del 20/11.
**Oggi** = `skills-public/anim-reviewer|anim-executor` + `.claude/skills/gstn-anim-*/project.md` (branch btrk7b 7af96c4a), `Tools/AnimPipeline/contract/rubric.md`, `.claude/skills/gstn-anim-cycle-cheap/SKILL.md` ("cheap").

## In una frase
La OG **misurava la vita** (tenute, fermate, cascata, scie a ogni passata) e poi guardava. Oggi i numeri controllano solo la pulizia (lunghezze, compenetrazioni, pop) e la vita è lasciata all'occhio. Però l'occhio riceve poche immagini ferme attorno a colpo e FAF, a velocità reale. **Una fermata, in una griglia ferma, sembra una bella posa tenuta.**

## I buchi (con prova)
1. **Nessun controllo misura le FERMATE.** OG: "a perfectly frozen hold on a strong move is a defect" (OG r.203-205), tenuta "moving < 6% of reach" e cascata "arm spread >= 1 frame" (OG r.403-405). La copia del 20/11 voleva `kb_motion` con `stop_sync_share` e i frame morti come FAIL prima del video, ma `kb_motion.py` esiste solo sul branch m6s9y2, mai unito. Oggi `animpipe/checks.py` ha solo `co_motion`, cioè un arto incollato mentre il corpo gira (r.134-150), e non vede il corpo intero fermo. `no_stop_gate.py` esiste solo nel piano K8 r3 (3f950a23), non nella skill.
2. **I vettori di velocità ci sono, ma non vedono le fermate e non arrivano sempre al revisore.** `anim_motion_overlay` (cheap r.21-22) disegna scie e frecce, ma segnala solo i *pop*: velocità > 3x i vicini e > 30 cm/s (`motion_overlay.py` r.5, r.51). Una fermata, per costruzione, non la segnala. È stato fatto solo per K7 (`k7/motion_*.png`); nei pacchetti K8 r1 e r2 ci sono solo le silhouette. In K7 i 2 pop alle caviglie sono stati archiviati come "anche K6 ne ha" (k7/README). Il revisore (skills-public e project.md) non nomina mai le PNG motion.
3. **Le scie dell'esecutore coprono solo le chiavi toccate.** Executor SKILL r.39: prima/dopo "touched keys and neighbours only, motion trails on". Le voci della rubrica che usano le scie (`intermedio_manca`, `jitter`, rubric r.113-115) valgono per la griglia offline, non per la review del video in gioco.
4. **La finestra delle immagini salta lo startup.** Cheap r.19 e r.41 punto 7: "max 8 immagini attorno ad anchor e FAF". Reviewer SKILL r.47: quattro momenti (inizio +6, colpo, FAF, fine), quindi il centro dello startup (a6-a12, dove stavano le fermate) non è un momento. Il test K7 lo aveva già detto ("saltano l'inizio dello startup: aggiunte 2 griglie a mano", handover dropkick-k7-skill-test).
5. **Lo slowmo non viene guardato.** OG r.430 e r.465: mp4 a velocità normale e rallentato, e "guarda tu le immagini grandi". Oggi project.md r.60 dice di guardare il video a velocità reale e al 25 %, ma cheap r.30 dice "il revisore riceve SOLO le griglie, mai gli mp4". La review K8 r2 ha usato `ab_whiff_real` (review.md r.2), ha letto "porta da calcio a1-a9 leggibile" (r.25), cioè proprio i frame a1-a5 congelati, e ha giudicato lo stacco a0->a1 così: "a velocità reale regge" (r.30-31). Lo sguardo slowmo di qsgab4 ha trovato tutti e 6 i difetti con 5 immagini e circa 1,5 $.
6. **La rubrica non blocca un corpo fermo.** `carica_viva` ("frozen like a statue") è solo una nota (rubric r.102); `sinergia` copre un arto fermo mentre il corpo si muove (r.100), non tutto il corpo fermo.
7. **Continuità tra clip e basi ereditate.** `continuita` esiste solo per le reazioni (rubric r.35). I blocchi della base approvata sono "ereditati", quindi non fanno fallire la variante (project.md, sezione "After K8 round 1"). Così il settle K6, fermo per circa 6 frame a terra (v158-163), non viene più giudicato.
8. **La rubrica non è mai stata tarata sul video in gioco** (rubric r.123).

## Cosa è cambiato con i tagli di costo
- **31/10:** prima i numeri e poi solo le immagini dei frame segnalati. I numeri però includevano tenute, fermate, cascata e scie, quindi andava bene.
- **15/11:** la OG viene archiviata e la procedura con quei numeri sparisce. Il contratto dice "l'occhio cerca ciò che i numeri non vedono", e l'unica misura rimasta all'occhio sono i pixel cambiati (`--diff`).
- **20/11:** regola 4, una griglia per vista guardata una volta, max 8 immagini.
- **23/11:** il ciclo cheap sceglie le 8 immagini attorno ad anchor e FAF e non passa nessun mp4.
- Il risultato è che sono state tolte insieme le misure della vita e le immagini dello startup.

## Perché nessuno ha visto lo stop-and-go di K7/K8
1. Il build mette tangenti piatte su ogni chiave (diagnosi 50f3a529), quindi le fermate nascono nei dati.
2. Nessun cancello misura le fermate.
3. Le frecce segnalano solo i picchi.
4. Le griglie mostrano pose ferme lontane dalle fermate, oppure le mostrano come una "tenuta".
5. Il revisore guarda a velocità reale.

## Proposte (regole 4, 10, 12, 15 rispettate)
- **A (consigliata), circa 3-4 $, cloud, 0 minuti al PC in più.**
  1. `no_stop_gate` diventa un blocco di `animpipe check` su ogni clip della mossa, compresi settle e loop. Una fermata è tutto il corpo sotto 3,5 cm/frame fuori dalle tenute dichiarate nella carta; in più passo massimo/mediana <= 4x e arrivo simultaneo delle parti.
  2. `motion_overlay` segna anche le fermate, con un pallino rosso sul frame fermo, e sotto la griglia aggiunge una striscia con la velocità per frame di mani, piedi, bacino e testa.
  3. Il pacchetto ha 8 immagini: 3 griglie (az90, az180, game) scelte per evento (stacco, ogni chiave, colpo, FAF), 2 **schede velocità** con frecce e scie (az90, az180), 1 riquadro slowmo dello startup a 1 tick per frame, e 2 libere.
  4. Il revisore guarda prima il riquadro slowmo e le schede velocità; `carica_viva` blocca quando è fermo tutto il corpo.
  Questo produce le immagini a 0 token e costa circa 10k token di review in più.
- **B, circa 1,5 $.** Solo testo: il revisore deve guardare il riquadro slowmo e leggere a mano `no_stop_gate`. Costa poco, ma si affida di nuovo all'occhio, che ha già mancato.
- **C, circa 7-9 $.** A, più la reintegrazione delle metriche di vita della OG (`kb_motion`, da m6s9y2), più la taratura della rubrica sul video (jab v12 e ftilt v3.1 passano, dair v2 fallisce), più le chiavi spline per frame nel build (opzione C della diagnosi). È la più completa, ma tocca la pipeline di tutte le mosse.

**Nota (regola 12):** la skill account `gstn-move-animation` è ancora installata e cita strumenti che non sono nel repo (`kb_motion`, `skills-public/knowledge/`). Va tolta dall'owner o aggiornata, altrimenti un thread la segue per sbaglio.
