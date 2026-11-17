# Style sheet VFX e look: regole per chi costruisce (M2) e per chi fa la review (Fable)
2026-11-17. Ambito: ogni VFX e ogni lavoro estetico del piano polish. Motivi e fonti: `handoffs/stile-astrobot-nintendo.md`. Si aggiunge alla checklist VFX approvata l'11-15 (silhouette-vfx-ricerca), non la sostituisce. I numeri sono punti di partenza: il verdetto è a occhio sul video in-game (regola 10); i controlli numerici sono solo avvisi.
Frame contati da 1, a 60 fps (1 frame = 16,7 ms).

## A. Regole di costruzione
| Voce | Regola |
|---|---|
| Forme | Solo: pallina, anello, stella 4-5 punte, goccia, striscia affusolata. Tondo = innocuo, appuntito = colpo |
| Numero | Poche particelle grandi: sbuffo 3-7, colpo 4-8 schegge + 1 lampo + 1 anello. Tetto 12 per burst |
| Materiale | `M_GSTN_VFX_Master`, Unlit, Masked. Texture RGB impacchettata (R forma, G luce finta, B erosione). 2 toni netti |
| Nascita | Parte già al 60% della taglia, 100% entro il frame 3. Il frame 1 del colpo è il più grande e il più chiaro |
| Morte | Rimpicciolisce o si erode dal bordo. Mai dissolvenza in trasparenza |
| Moto | Partenza veloce, drag alto (4-8), fermo prima di sparire. Mai velocità costante |
| Durata | Polvere 250-450 ms. Colpo 200-330 ms. Lampo 1-2 frame. Anello atterraggio 250-350 ms |
| Colore | Nucleo bianco → una tinta satura. Mai nero, mai grigio sporco. Primaria + secondaria (colore del kit dalla tabella M0) |
| Palette base | Polvere bianco caldo #FFF6E8 con ombra #D9C7B0. Colpo #FFFFFF → #FFD23C → #FF8A1E. Colpo forte: secondaria del kit |
| Contorni | Nessun outline. Bordo per stacco di valore |
| Scala | La taglia segue l'importanza: polvere < salto < atterraggio pesante < colpo forte. Scala dalla magnitudine dell'evento |
| Bloom | Solo il lampo del colpo supera 1 in emissivo. La polvere non brilla |

## B. Tabella effetti M2
| Effetto | Forma | Note |
|---|---|---|
| Partenza corsa | 3-4 palline dietro i piedi, spinte all'indietro | Frame 1 visibile. Riferimento: .mov dell'owner (Toad) |
| Skid | Palline in fila, 1 ogni 3-4 frame finché dura | Unico effetto con spawn continuo |
| Salto | 2-3 palline piccole + anello sottile a terra | Più piccolo dell'atterraggio |
| Atterraggio | Anello mesh che si allarga + 4-6 palline radiali | Scala dalla velocità d'impatto; sotto soglia niente |
| Sprint 2 | 3-5 strisce affusolate ai lati, bianche | Mai davanti al personaggio |
| Colpo S/M/L | Lampo + anello + 4/6/8 schegge o stelle | Orientato lungo la direzione del colpo |
| Muro/soffitto/terra | Anello sul piano della normale + palline | Stessa ricetta dell'atterraggio, ruotata |
| Scia tumble | 1 ribbon corto + pallina ogni 4-5 frame | Si spegne a fine hitstun |

## C. Perf toaster e online
CPU sim, Sprite o Mesh renderer, niente GPU sim, luci, collisioni, traslucido illuminato. ≤3 emitter per sistema, atlas unico 1024, Fixed Bounds, pooling, Effect Type condiviso con cull per distanza e tetto di istanze. Solo cosmetici, generati in locale dagli eventi di presentazione, mai replicati.

## D. Checklist di review (PASS solo se tutte sì; ogni NO etichettato forma / timing / colore / gioco)
1. A VFX spenti e accesi il personaggio resta sempre leggibile? L'effetto non copre mai il corpo né l'arto che colpisce.
2. Le forme sono solo quelle del vocabolario? Nessun fumo realistico, nessun rumore.
3. Si contano le particelle a colpo d'occhio (poche e grandi)?
4. Frame 1 dell'evento: l'effetto c'è già ed è leggibile?
5. Colpo: il frame 1 è il più grande e il più chiaro, e tutto è finito entro 20 frame?
6. La morte è per rimpicciolimento o erosione, senza alone trasparente che resta?
7. Il moto frena (ease-out) e non scorre lineare?
8. Colori: nucleo chiaro, una tinta satura, niente nero o grigio? Si distinguono polvere (neutra) e colpo (caldo)?
9. Le tre taglie del colpo si distinguono senza guardare i numeri?
10. Fermo immagine a metà vita: sembra un disegno pulito (2 toni, bordo netto) e non una nuvola sfocata?
11. Confronto col riferimento (.mov Toad per partenza e skid): stessa famiglia di forma e durata?
12. Sembra "Unreal generico"? Segnali: traslucido morbido, tante particelle piccole, scintille puntiformi, bagliore diffuso, colori smorti.

## E. Look generale (per il futuro checkpoint "Look v1", non per M2)
PBR morbido senza toon né outline; colori saturi a valore medio, roughness 0,25-0,4 (plastica) o 0,6-0,8 (opaco); rim Fresnel in emissivo (`MF_GSTN_Rim`); luce calda + cielo freddo forte, ombre blu mai nere; esposizione manuale; Tone Curve Amount 0,6-0,8, Toe 0,4, Saturation 1,1-1,15; bloom 0,2-0,4; lens flare, aberrazione, grana, motion blur spenti; DOF solo fuori dal gioco. Review: cattura prima/dopo dalla stessa camera, stessi frame.
