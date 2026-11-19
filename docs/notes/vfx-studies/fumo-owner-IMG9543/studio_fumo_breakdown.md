# Studio: polvere di neve all'atterraggio (riferimento dell'owner, IMG_9543)

Fonte: `riferimento_fumo_owner_IMG_9543.mov` (project files), ripresa col telefono di uno schermo: 3,3 s, 30 fps, 100 frame,
HDR (HLG) convertito in SDR per lo studio. Due atterraggi a terra su neve, visti dall'alto a 3/4:
**A** = f47 (contatto) .. f60, **B** = f77 (contatto) .. f91, più una scia di rotolata f92-f99.
Immagini in questa cartella: `screen_f30-f100.jpg` (tutto lo schermo, un frame ogni 3), `zoom_A_f44-f67.jpg` e `zoom_B_f74-f100.jpg`
(frame consecutivi attorno al personaggio), `keyframes_annotati.jpg` (6 pose chiave di B con note).
Limiti: è un video di uno schermo (moiré, mosso, compressione), quindi la grana fine non si legge; tempi in frame a 30 fps
(1 frame = 33 ms; in un gioco a 60 fps = 2 frame).

## Come leggere un effetto (metodo riusabile)
Per ogni effetto rispondi in quest'ordine, con il frame che lo prova:
1. **Origine e forma di emissione**: da dove nasce (punto, anello, contorno del corpo) e in che direzione parte.
2. **Quanti e quanto grandi**: numero di lobi distinguibili, taglia rispetto a una misura del personaggio.
3. **Velocità e frenata**: quanto si sposta tra i frame 1-2-3 e quando si ferma (curva ease-out).
4. **Vita della taglia**: cresce, resta, si restringe? (regola dell'owner per la polvere: cresce sempre, mai si restringe).
5. **Vita dell'opacità**: quando inizia a sparire e in quanti frame.
6. **Colore e luce**: lato in luce, lato in ombra, bordo, rispetto al pavimento.
7. **Bordi e materia**: morbido o netto, a lobi o a fili, grana.
8. **Sovrapposizione**: lobi separati o una massa sola, e quando si fondono.
9. **Durata totale** in frame e in secondi.

## Breakdown (atterraggio B, il più leggibile; A conferma)
| # | Aspetto | Cosa si vede | Frame |
|---|---|---|---|
| 1 | Origine | Non un punto: nasce **sul contorno di ciò che tocca il suolo** (mani, piedi, bacino). Al contatto c'è già uno spruzzo basso e sottile lungo il bordo della sagoma. La direzione è **radiale dal centro del corpo**, piatta sul pavimento | f78, A f48 |
| 2 | Quantità e taglia | 6-8 lobi distinguibili, ognuno grande circa 1/3-1/2 del busto; l'insieme a fine corsa è largo ~1,4 volte l'apertura delle braccia. Bassissimo: resta sotto il ginocchio, quasi steso | f80-f83, A f53-f55 |
| 3 | Velocità | Scatto molto veloce nei primi 2 frame (gran parte della corsa), poi frenata forte: dal 4°-5° frame i lobi sono quasi fermi. Ease-out netto, niente scivolamento a velocità costante | f78 -> f80 -> f83 |
| 4 | Taglia nel tempo | I lobi crescono ~1,5x dal frame +1 al +5, poi crescono piano mentre spariscono. **Mai si restringono** | f79-f87 |
| 5 | Opacità | Pieni (quasi opachi al centro) fino a ~+6/+8, poi svaniscono in ~4-5 frame. Prima va via il velo chiaro esterno, poi i nuclei | f84-f91 |
| 6 | Colore e luce | Il colore è **la neve stessa in ombra**: la parte alta è chiara quasi come il pavimento, il lato in ombra e la base sono **blu-grigio più scuri del pavimento**. È il contrasto ombra/luce, non un colore diverso, a far leggere la polvere su un suolo chiaro | f80-f83 |
| 7 | Bordi e materia | Bordi morbidi ma il corpo è **a grumi** (lobi tondi, cavolfiore), non a fili né a peli. Il velo finale è più liscio | f82, f87 |
| 8 | Sovrapposizione | Nascono come lobi separati vicini al corpo; in 3-4 frame si allargano, si toccano e diventano **un anello unico** attorno al personaggio; alla fine un velo chiaro che li fonde | f80 -> f83 -> f87 |
| 9 | Durata | Visibile ~12-14 frame a 30 fps = **0,4-0,45 s** (A: f48-f58, ~10 frame). Nella scia di rotolata i lobi restano fermi più a lungo (~8 frame) e svaniscono senza muoversi | B f78-f91 |

Extra: qualche grumo bianco di neve schizza in alto e indietro e resta sospeso (f46-f56): è un elemento separato dalla polvere.

## Principi (perché funziona)
- **Un colpo, un centro**: tutto parte nello stesso istante dal corpo e va verso fuori; il cervello legge un impatto unico.
- **Veloce poi fermo**: lo spostamento è quasi tutto nei primi 2 frame; il resto della vita è crescita lenta e dissolvenza. Dà peso.
- **Si cresce, non si rimpicciolisce**: la polvere si espande nell'aria e diventa più rada; sparisce per trasparenza.
- **La forma la danno le ombre**: su neve chiara la polvere chiara si vede solo grazie al lato in ombra più scuro e freddo.
- **Grumi, non fumo filato**: lobi tondi e pieni, contorno morbido; niente texture a capelli.

## Uso per altri effetti
Stessa tabella per dash, scivolata, partenza in corsa: cambiano origine e direzione (preset `DUST_SHAPES` in
`Tools/GAS/setup_presentation_vfx.py`), il "look" (colore, materia, crescita e dissolvenza) resta quello della polvere condivisa.
Strumento per i fogli di frame: `Tools/GASNPPTest/vfx_ref_sheet.py`. Verifica "mai restringersi": `Tools/GASNPPTest/vfx_size_monotonic.py`.

## Confronto con la nostra polvere (A2 r5, 2026-11-19)
Misure del revisore (Fable) in `docs/notes/reviews/vfx-v3/a2/r5/REVIEW-A2r5.md`; il gioco gira a 60 fps.
| # | Riferimento | r4 (prima) | r5 finale (r5d) |
|---|---|---|---|
| 1 Origine | contorno del corpo, radiale e piatta | colonna tra le gambe a 60-70° | anello di 12 cm ai piedi, ~7-9° sul pavimento |
| 2 Quantità | 6-8 lobi grandi | 14 piccoli | 18 lobi che si sovrappongono |
| 3 Velocità | quasi ferma a 0,15 s | lenta e costante | 70 % della corsa a 0,067 s, 94 % a 0,15 s |
| 4 Taglia | cresce, mai si restringe | si restringeva (erosione + curva del template) | cresce sempre; tolte erosione e curva del template |
| 5 Opacità | piena, poi via in fretta | sfumava da subito | piena fino al 40 %, poi 1 - t^2,5 |
| 6 Colore | lato in ombra più scuro e freddo | valori r3 | valori r3 (decisioni dell'owner aperte) |
| 7 Materia | grumi morbidi | pieghe scure, aspetto di roccia sul pavimento scuro | morbida, senza pieghe |
| 8 Fusione | lobi che diventano un anello | lobi separati | copertura dell'anello 0,89-0,97, poi 0,81-0,86 |
| 9 Durata | 0,4-0,45 s | ~0,45 s | 0,45 s |

Lezione tecnica: se una particella "si restringe", in Niagara controlla prima i moduli di taglia ereditati dal template (ScaleSpriteSize
in Particle Update), poi il materiale e il flipbook. I numeri del materiale da soli non bastano a vederlo.
