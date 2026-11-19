# Studio: impatti in DK Bananza (clip dell'owner owner_clip_2059.mp4) e perché la nostra polvere sembra finta

Fonte: `owner_clip_2059.mp4` (project files), 4,2 s, 30 fps, 126 frame, 1466x842, grotta rossa scura, DK che colpisce una cassa.
Immagini: `overview_every3.jpg` (tutta la clip, un frame ogni 3), `zoom_hit_f40-f57.jpg` (colpi sulla cassa, frame consecutivi),
`zoom_cloud_f58-f81.jpg` e `zoom_cloud2_f92-f109.jpg` (la parete in alto a destra). Metodo: lo stesso dello studio
`../fumo-owner-IMG9543/studio_fumo_breakdown.md`.
Nota: la "nuvola" chiara in alto a destra (f61, f64, f72-f73, f96-f100) NON è fumo: è la parete di roccia illuminata da un lampo di luce
dell'impatto. Picco in 1 frame, si spegne in 3-4 frame. Da qui la lezione 1.

## Breakdown per strati (un impatto = più strati brevi sovrapposti)
| Strato | Cosa si vede | Tempo (30 fps) | Frame |
|---|---|---|---|
| Luce | Lampo caldo che illumina parete, pavimento e personaggio attorno all'impatto | picco in 1 frame, via in 3-4 | f61, f72, f96 |
| Anello d'urto | Anello azzurro sottile, stilizzato, si allarga attorno al pugno | 2-3 frame | f40-f42, f48-f51 |
| Scintille | Linee sottili ciano, velocissime, dritte, diverse lunghezze | 2-4 frame | f47-f50 |
| Getto d'aria/polvere | Pennacchio bianco traslucido e **stirato nella direzione del colpo** (sembra mosso), poi si apre | 2-3 frame stirato, poi si dissolve | f45-f46 |
| Schegge | Pezzetti scuri/gialli piccoli che volano e cadono | 3-6 frame | f41 |
| Pulviscolo | Puntini luminosi sospesi nell'aria che restano a lungo | continui | f58-f81 |
Ogni strato da solo è semplice; il realismo viene dalla **somma** di strati diversi per forma, velocità e durata, e da **tutti illuminati
dalla stessa luce della scena**. Il personaggio e la cassa hanno luce calda forte e bordo di luce; gli effetti la prendono anche loro.

## Perché la nostra polvere (A2 r5) sembra finta, in ordine di peso
1. **Non è illuminata dalla scena.** Il nostro sprite è "unlit": l'ombreggiatura è disegnata nel flipbook con una luce fissa nello spazio
   dello sprite, e il colore lo decidiamo noi. Non prende la direzione né il colore della luce vera, non fa ombra, non riceve ombra.
   In DK la polvere ha lo stesso lato in luce e lo stesso lato in ombra del personaggio vicino. È la differenza più grande.
2. **Un solo strato.** Noi: anello di polvere + 5 grumi minuscoli. DK: luce, anello, scintille, getto, schegge e pulviscolo, ognuno con il suo
   tempo. Un solo strato sembra un effetto "di sistema", non un evento fisico.
3. **Troppo uniforme e simmetrico.** 18 puff simili su un anello perfetto, tutti nati nello stesso frame, stessa velocità e vita. In natura
   la polvere ha grumi grandi e piccoli, un lato più forte (direzione dell'impatto), nascite sfalsate di 1-2 frame.
4. **Nessun mosso all'inizio.** In DK i primi 2-3 frame sono stirati nella direzione del moto (velocità altissima), poi diventano nuvola. I
   nostri puff sono tondi dal primo frame: sembrano oggetti, non aria spinta.
5. **Materia troppo pulita.** Il flipbook è rumore procedurale morbido e regolare; manca il dettaglio interno (pieghe piccole, densità
   variabile) di un fumo renderizzato/simulato.
6. Tempi: già vicini (0,45 s, frenata forte): non è la causa principale.

## Correzioni proposte (per impatto stimato / costo)
| # | Correzione | Impatto | Costo |
|---|---|---|---|
| 1 | Polvere **lit translucent** (o luce della scena passata al materiale: direzione e colore della directional + ambient dello skylight) e normal map del flipbook usata per la luce vera; ombra di contatto già c'è | alto | medio (materiale + cattura su entrambi i pavimenti) |
| 2 | **Lampo di luce** all'impatto: point light 1-3 frame, raggio ~2 m, colore del pavimento/caldo, decadimento 3-4 frame (Niagara light renderer o luce nel cue) | alto, economico | basso |
| 3 | **Strato schegge**: 6-10 granelli scuri del colore del pavimento, balistici con gravità, rimbalzo/spegnimento a terra, 0,3-0,5 s | medio-alto | basso |
| 4 | **Strato streak**: 6-8 sprite allineati alla velocità, molto stirati, vita 0,05-0,08 s, radiali sul pavimento, nascono con l'anello | medio | basso |
| 5 | **Variazione**: taglie 15-60 cm (2-3 grumi grandi), velocità ±40 %, nascita sfalsata 0-2 frame, raggio su disco, leggero sbilanciamento nella direzione del movimento | medio | basso (solo preset) |
| 6 | Flipbook più ricco (fumo renderizzato o pacchetto gratuito Niagara/flipbook, la coda del coordinatore ne ha già) | medio | medio-alto |
Ordine consigliato: 2 + 3 + 5 in un giro (economici, cambiano molto la lettura), poi 1 (il vero salto), poi 4 e 6.
Regole che restano: polvere che cresce e svanisce per opacità, mai si restringe (vale anche per schegge? no: le schegge sono solidi e
possono sparire a terra; solo la polvere segue la regola, da confermare con l'owner).
