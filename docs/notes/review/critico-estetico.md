# Il critico estetico (animazioni e VFX)

Owner 2026-11-19 (scelta 2A): oltre al giudice che dà PASS/FAIL, ogni review estetica ha un **critico** il cui lavoro
riesce solo se **trova cosa non è ottimale**. Ambito: tutte le review video di animazioni e VFX (regola 10). Non
sostituisce il giudice: lo mette alla prova. Le skill che lo applicano: `gstn-anim-reviewer` (§3b) e
`gstn-vfx-reviewer` (§3b). Questo file è il principio; le skill dicono come.

## Perché
Un giudice che deve dire PASS o FAIL tende a vedere ciò che si aspetta e a fermarsi al "va bene". Un ruolo che vince
solo trovando difetti cerca dove il giudice non guarda. Insieme alzano la barra senza che l'owner debba fare da critico.

## Principi (non ricette)
1. **Separato e non ancorato.** Il critico è un agente a parte (Fable 5.2 high). Vede il video, le griglie, i
   riferimenti, le regole dell'owner e i tratti d'identità della mossa (regola 13a). **Non** vede il verdetto del
   giudice, le note dell'esecutore né le review precedenti: deve guardare con occhi propri.
2. **Successo = difetti veri.** Elenca sempre **almeno i 3 difetti più pesanti**, ordinati per peso
   (`grave` / `medio` / `lieve`). Se davvero non ne trova tre, lo dice e scrive dove ha cercato. Inventare un difetto
   non è un successo: ognuno deve reggere al controllo del giudice.
3. **Ogni difetto si vede.** Frame e vista, cosa si vede, il riferimento o il principio che tradisce, l'etichetta
   (animazioni: `posa` / `dati montage` / `gioco`; VFX: le etichette della skill VFX), la correzione più piccola.
   Il gusto da solo non basta: serve un riferimento (owner, DK Bananza / Astro Bot, Smash, Ganondorf) o un principio.
4. **Il peso lo dà l'occhio a velocità reale.** `grave` = l'owner lo noterebbe al primo sguardo a velocità reale, o
   tradisce un tratto d'identità o la stella polare ("Ganondorf dentro Astro Bot"; per i VFX "starebbe in DK
   Bananza?"). `medio` = si nota guardando di nuovo. `lieve` = solo frame per frame.
5. **Il giudice risponde a ogni voce.** Accolta (diventa uno scarto) o respinta con il frame che la smentisce. Nessuna
   voce ignorata. Le voci respinte restano scritte in `review.md`.
6. **Chi arriva all'owner.** Solo lavoro con PASS del giudice **e** nessun difetto `grave` accolto rimasto aperto.
   I `medio`/`lieve` rimasti vanno all'owner in una riga, così sa cosa si è scelto di non correggere.
7. **Costo piccolo.** Una passata del critico per giro, stesse griglie del giudice (regola 4), nessuna cattura nuova.
   Non conta come giro di posa. Se trova solo difetti `gioco`, vanno alla coda del codice come sempre.

## Cosa resta uguale
Catena di cattura, regole 4, 10, 12 e 13, massimo 3 giri di posa, decisioni di design all'owner. Il critico non scrive
istruzioni per l'esecutore: le scrive il giudice, partendo dal difetto accolto più grave.
