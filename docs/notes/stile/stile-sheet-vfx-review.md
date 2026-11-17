# Style sheet VFX e look: regole per chi costruisce e per chi fa la review (Fable)
2026-11-17, aggiornato 23:10Z: fusa la proposta di `stile-sheet-vfx-review-proposta.md` (voci [NUOVO], da `vfx-clarity-ricerca.md`) e righe aggiornate allo stile v2 (voci [V2], da `vfx-stile-v2-astrobot.md`; le [V2] marcate "decisione" valgono dopo la risposta dell'owner, fino ad allora vale la consigliata). Ambito: ogni VFX e ogni lavoro estetico del piano polish. Motivi e fonti: `handoffs/stile-astrobot-nintendo.md`. Si aggiunge alla checklist VFX approvata l'11-15 (silhouette-vfx-ricerca), non la sostituisce. I numeri sono punti di partenza: il verdetto è a occhio sul video in-game (regola 10); i controlli numerici sono solo avvisi.
Frame contati da 1, a 60 fps (1 frame = 16,7 ms).

## A. Regole di costruzione
| Voce | Regola |
|---|---|
| Forme [V2] | Solo: sfera smussata, toro, scheggia tonda "a gemma gonfia", capsula affusolata, goccia. Mesh 3D low-poly (Mesh Renderer), non sprite piatti; lo sprite resta solo per il lampo del frame 1. Tondo = innocuo, scheggia = colpo |
| Numero | Poche particelle grandi: sbuffo 3-7, colpo 4-8 schegge + 1 lampo + 1 anello. Tetto 12 per burst |
| Materiale [V2, decisione 1] | `M_GSTN_VFX_Toy`: Default Lit, Masked (mai Translucent). Plastica lucida roughness 0,2-0,35, polvere "cotone" 0,6; rim Fresnel in emissivo. Erosione dal bordo (canale B) e rimpicciolimento restano; le 2 bande di luce finta solo sullo sprite del lampo |
| Emissivo [V2] | Piatto e saturo, senza gradiente interno, solo per energia: nucleo del colpo, anello "neon" (toro opaco emissivo + Fresnel bianco), kit con elemento |
| Nascita [V2] | Colpo e impatti al 100% al frame 1 (il frame 1 del colpo è il più grande e il più chiaro). Polvere 60% → 100% entro il frame 3 |
| Morte [V2] | Rimpicciolisce a 0 con ease-in ("pop out"); la polvere può anche erodersi dal bordo. Mai dissolvenza in trasparenza |
| Moto | Partenza veloce, drag alto (4-8), fermo prima di sparire. Mai velocità costante. Burst su ventaglio/anello regolare, jitter ≤10°, niente vortici o curl noise |
| Rimbalzi [V2, decisione 3] | Solo su piano analitico ai piedi dell'evento (modulo scratch, restituzione 0,5-0,6, max 2 rimbalzi). Niente collisione Niagara a trace, niente GPU depth buffer. Detriti persistenti: pool fisso 24 per arena, impostazione spenta di default |
| Durata | Polvere 250-450 ms. Colpo 200-330 ms. Lampo 1-2 frame. Anello atterraggio 250-350 ms |
| Colore | Nucleo bianco → una tinta satura. Mai nero, mai grigio sporco. Primaria + secondaria (colore del kit dalla tabella M0) |
| Palette base | Polvere bianco caldo #FFF6E8 con ombra #D9C7B0. Colpo #FFFFFF → #FFD23C → #FF8A1E. Colpo forte: secondaria del kit |
| Contorni | Nessun outline. Bordo per stacco di valore e riflesso |
| Scala | La taglia segue l'importanza: polvere < salto < atterraggio pesante < colpo forte. Scala dalla magnitudine dell'evento |
| Bloom | Solo il lampo del colpo supera 1 in emissivo. La polvere non brilla |
| Gerarchia [NUOVO] | Ogni evento VFX ha un voto di importanza 1-5 nella scheda mossa (es. polvere 1, salto 2, atterraggio 2, colpo S 3, M 4, L 5). Taglia, luminosità e durata seguono il voto; mai due eventi di voto diverso con lo stesso aspetto. Due voti per evento: per chi lo fa e per chi lo subisce (owner 21:04Z); l'effetto che minaccia il giocatore locale usa il voto della vittima |
| Additivo [NUOVO] | Solo il nucleo del frame 1-2 del colpo è additivo/emissivo (> 1). Il resto opaco Masked. Niente raggi di luce o lampi a tutto schermo alla Smash |
| Stessa funzione, stessa forma [NUOVO] | Lo stesso effetto di gioco (colpo, lancio, scudo, stordimento, fuoco, elettrico) ha la stessa forma in tutti i kit. Il colore del kit cambia solo la secondaria, mai la forma. Una variante cosmetica non è mai meno leggibile della base |
| Portata onesta [NUOVO] | L'effetto sui frame attivi non è mai più piccolo dell'hitbox né molto più grande (tolleranza indicativa ±20%) |
| Segno a terra [NUOVO] | Colpo su bersaglio in aria o lancio in profondità: anello/ombra a terra sotto l'impatto, così da dietro e di lato si legge dove e verso dove |
| Ombra degli effetti [V2] | Effetti che durano > 15 frame o volano > 50 cm (schegge L, detriti, scia tumble): disco piatto blu scuro (mai nero, non decal) lungo `GSTN_DownAxis` |
| Taglia minima a schermo [NUOVO] | L'effetto del colpo ha una taglia minima in pixel a schermo: con la camera lontana si scala, come il tremito |
| Hitstop [NUOVO + V2, decisione 2] | Proiettili: hitstop ridotto. Con più di 2 giocatori: moltiplicatore di hitstop come impostazione PER MODALITÀ di gioco (dato del game mode, non project setting; owner 21:04Z). Tremito orizzontale a terra, verticale in aria, sempre rispetto allo SCHERMO. 4 frame di passaggio sussulto → dolore durante il fermo. Gli effetti del colpo rallentano a `HitstopVfxTimeScale` (impostazione giocatore, default 0,15) e ripartono col personaggio |

## B. Tabella effetti (v2: dettagli per effetto in `vfx-stile-v2-astrobot.md`)
| Effetto | Forma | Note |
|---|---|---|
| Partenza corsa | 3-4 sfere "cotone" dietro i piedi, spinte all'indietro | Frame 1 visibile. Riferimento: .mov dell'owner (Toad) |
| Skid | Sfere in fila, 1 ogni 35-40 cm finché dura | Unico effetto con spawn continuo |
| Salto | 2-3 sfere piccole + toro sottile a terra | Più piccolo dell'atterraggio |
| Atterraggio | Toro lucido che si allarga + 4-6 sfere radiali, 1 rimbalzo | Scala dalla velocità d'impatto; sotto soglia niente |
| Sprint 2 | 3-5 capsule affusolate ai lati, bianche lucide | Mai davanti al personaggio |
| Colpo S/M/L | Lampo + toro "neon" + 4/6/8 schegge a gemma | Orientato lungo la direzione del colpo; ombre per L |
| Muro/soffitto/terra | Toro sul piano della normale + sfere; schegge che cadono a terra | Stessa ricetta dell'atterraggio, ruotata |
| Scia tumble | Catena di sfere lucide che rimpiccioliscono | Si spegne a fine hitstun; ombra a terra |

## C. Perf toaster e online
CPU sim, Mesh renderer (sprite solo per il lampo), niente GPU sim, luci, collisioni a trace, traslucido. ≤3 emitter per sistema (max 2 mesh), mesh ≤ ~100 tri con LOD ~30, Fixed Bounds, pooling, Effect Type condiviso con cull per distanza e tetto di istanze. Preset basso: metà schegge, niente ombre né detriti. Solo cosmetici, generati in locale dagli eventi di presentazione, mai replicati.

## D. Checklist di review (PASS solo se tutte sì; ogni NO etichettato forma / timing / colore / materiale / gioco / gerarchia)
1. A VFX spenti e accesi il personaggio resta sempre leggibile? L'effetto non copre mai il corpo né l'arto che colpisce.
2. Le forme sono solo quelle del vocabolario? Nessun fumo realistico, nessun rumore.
3. Si contano le particelle a colpo d'occhio (poche e grandi)?
4. Frame 1 dell'evento: l'effetto c'è già ed è leggibile?
5. Colpo: il frame 1 è il più grande e il più chiaro, e tutto è finito entro 20 frame?
6. La morte è per rimpicciolimento o erosione, senza alone trasparente che resta?
7. Il moto frena (ease-out) e non scorre lineare?
8. Colori: nucleo chiaro, una tinta satura, niente nero o grigio? Si distinguono polvere (neutra) e colpo (caldo)?
9. Le tre taglie del colpo si distinguono senza guardare i numeri?
10. [V2] Fermo immagine a metà vita: sembra un oggetto 3D lucido (riflesso, bordo netto) e non un adesivo piatto né una nuvola sfocata?
11. Confronto col riferimento (.mov Toad per partenza e skid): stessa famiglia di forma e durata?
12. Sembra "Unreal generico"? Segnali: traslucido morbido, tante particelle piccole, scintille puntiformi, bagliore diffuso, colori smorti.
13. Gerarchia: guardando un fermo immagine con più effetti, l'occhio va prima al colpo più importante, poi al resto? La polvere non compete mai col colpo.
14. Contrasto su due sfondi: l'effetto si legge sia su fondo chiaro sia su fondo scuro o saturo, e prende la luce della scena senza diventare grigio? Cattura su due punti di L_Test_Hub.
15. Portata onesta: con il debug delle hitbox acceso, la parte visibile dell'effetto nei frame attivi corrisponde all'hitbox?
16. Profondità: dalla vista dietro e a 3/4, si capisce dove ha colpito e verso dove va il lancio (segno a terra, ombre degli effetti in aria)?
17. Camera lontana: alla distanza massima di camera prevista il colpo S si vede ancora?
18. Hitstop e tremito: tremito orizzontale a terra e verticale in aria rispetto allo schermo, calante, hurtbox ferma; [V2] gli effetti rallentano col personaggio e ripartono insieme?
19. Coerenza fra kit: lo stesso effetto di gioco ha la stessa forma nel kit nuovo e in quelli esistenti?
20. Avvio: dal frame 1 dopo l'input c'è un cambio visibile (posa o effetto)?
21. [V2] Rimbalzi: max 2, si fermano sul piano, nessuna compenetrazione visibile, nessun pezzo che scivola all'infinito?

## E. Processo
La scheda mossa (`docs/notes/mosse/`) riceve due righe: voto di importanza dei suoi eventi VFX e forma prevista. Il VFX si pensa con la mossa, non dopo.

## F. Look generale (checkpoint "Look v1")
PBR morbido senza toon né outline; colori saturi a valore medio, roughness 0,25-0,4 (plastica) o 0,6-0,8 (opaco); rim Fresnel in emissivo (`MF_GSTN_Rim`, condiviso con `M_GSTN_VFX_Toy`); luce calda + cielo freddo forte, ombre blu mai nere; esposizione manuale; Tone Curve Amount 0,6-0,8, Toe 0,4, Saturation 1,1-1,15; bloom 0,2-0,4; lens flare, aberrazione, grana, motion blur spenti; DOF solo fuori dal gioco. Review: cattura prima/dopo dalla stessa camera, stessi frame.
