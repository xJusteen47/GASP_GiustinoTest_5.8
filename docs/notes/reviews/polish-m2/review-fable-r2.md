# Review visiva VFX kit v1 (polish M2), round 2
2026-11-17. Reviewer: Fable (regola 10). Catture: ON `r2c/vfx1`, OFF `r2a/vfx0`. Guardati 8 montaggi fatti dai frame grezzi ON (crop e zoom 2x-7x fuori dal repo), viste side, game e close; in più conteggi e letture di colore fatte per script sulla differenza ON/OFF. Misure in cm dal manichino (~180 cm = ~132 px nella vista side), colori letti a schermo. Checklist: sezione D dello style sheet.

## Verdetto complessivo: FAIL
Grande passo avanti: 6 fix su 11 chiusi, l'artefatto a croce è sparito ovunque, l'anello d'atterraggio esiste, le tre taglie del colpo si distinguono a occhio, le linee sprint non toccano più il corpo. Restano però dei NO netti su colpo (schegge), partenza corsa, anello di atterraggio/knockdown e colore della polvere. Non va all'owner.

## Per effetto
| Effetto | Verdetto | Cosa non passa (frame guardati) |
|---|---|---|
| Partenza corsa | FAIL | **forma** (3): ancora 1 solo grumo allungato di ~16x20 cm (2-3 palline sovrapposte), nessun ventaglio di 4 palline (run_skid side f29-f38). **timing/moto** (7): il grumo scorre all'indietro a velocità costante, ~5,3 px/frame per 12 frames (f29-f41, ~90 cm), nessuna frenata. **colore**: vedi polvere. Primo frame visibile f29 |
| Skid | FAIL (lieve) | OK f139-f147: 1, 2, 3 palline da ~16 cm in fila, contabili. **forma** (10): a f151-f155, quando il personaggio è quasi fermo, 3-4 palline nascono nello stesso punto e fanno un grumo sul piede; una pallina è tagliata dal pavimento e resta una lunetta bianca a terra (f147-f155) |
| Salto | PASS | Anello bianco concentrico al marker e a terra (f21-f27), 2 palline, morte pulita (f31-f34) |
| Atterraggio | FAIL | **timing**: l'anello si vede solo a f84-f87 e a f89 non c'è più: ~5 frame (~85 ms) contro 250-350 ms. **gioco** (1): a f84-f87 due palline da ~35 cm stanno davanti a piedi e stinchi. OK: anello sottile che si allarga (~110 → ~175 cm), 5-6 palline radiali, f84 già pieno, morte per rimpicciolimento pulita (f95-f101) |
| Linee sprint | PASS | Vista close e game f190, f230, f250, f268: 3-4 strisce bianche affusolate solo ai lati, mai sul corpo. Prima linea a f179, spente a ~f286 |
| Colpo S | FAIL | **forma** (3): le schegge restano un grumo di stelle sovrapposte largo ~30 cm sul fianco della vittima per tutto f27-f34 (zoom f31-f34: le gialle si fondono in una macchia verticale); non si contano. **colore**: 2 stelle rosse #F82808 già nel colpo S (f31-f33), mentre il rosso doveva essere solo del colpo L. OK: lampo bianco #F7F7F7 di 1 frame (~37 cm), anello giallo #E8D828 da f27 poi #F8B818, tutto chiuso a f34 |
| Colpo M | FAIL | Stessi NO di S (schegge a grumo, rosso presente). OK **punto 9**: lampo ~44 cm, anello ~88 cm a f31, più grande di S (~68 cm) a colpo d'occhio |
| Colpo L | FAIL | **forma**: schegge a grumo fino a f33, si separano solo dopo il lancio (f34, 5-6 stelle sul torso della vittima). **gioco** (1): dalla camera del giocatore (close f26-f33) lampo e schegge sono nascosti dal corpo dell'attaccante, il colpo forte si legge solo come anello giallo. **colore**: il rosso non distingue L perché c'è anche in S e M; a f26 resta una stella crema #E1D6B7 sul bordo del lampo. OK: anello ~102 cm a f31, taglia distinguibile da M |
| Scia tumble | PASS (con nota) | Niente più nuvola né macchia in hitstop (hit_large f26-f34); 1 pallina ogni 4-5 frame da ~18 cm, disco pulito fino alla fine (hit_medium f38-f54, zoom 7x). Nota **forma**: alla velocità di lancio le palline distano ~2 m, si legge 1 pallina sola (f38, f47, f55), non una scia; la pallina è a tinta piatta, senza secondo tono |
| Anello knockdown | FAIL | **gioco/timing**: anello e palline compaiono a terra a ~f198-f200 mentre il corpo è ancora in aria (~50-70 cm, f200-f203) e tocca terra a ~f207-f212, quando l'anello sta già sparendo. La taglia va bene (~170 cm, 5-6 palline) |
| Splat (Impact) | PASS (con nota) | Anello sottile sul piano del muro che si allarga e si erode in 12 frame (f48-f60), 4 palline da ~40 cm che frenano e muoiono pulite (f63-f65). Nota **forma**: a f48-f51 le palline sono sovrapposte e coprono le cosce |
| Rimbalzo muro/soffitto/terra | NON GIUDICABILE | Il gioco non ha generato gli eventi (README) |

Punto 11 (.mov di Toad) ancora non verificato: il riferimento non è nel pacchetto.

## Stato dei fix del round 1
| # | Fix | Stato |
|---|---|---|
| 1 | Artefatto a fine vita | CHIUSO: nessuna croce o T in salto, atterraggio, scia, splat |
| 2 | Anello atterraggio, 4-6 palline, knockdown | PARZIALE: anello e palline ci sono; anello troppo breve; knockdown in anticipo sul corpo |
| 3 | Taglie del colpo | CHIUSO per lampo e anello; accento rosso non esclusivo di L |
| 4 | Colore del colpo | QUASI: anello giallo da f27, niente beige; resta la stella crema a f26 e il rosso al posto dell'arancio #FF8A1E |
| 5 | Schegge a ventaglio | APERTO: non si allontanano, grumo fino a fine hitstop |
| 6 | Skid | QUASI: fila contabile; grumo a bassa velocità e lunetta a terra |
| 7 | Partenza corsa | APERTO: 1 grumo, moto lineare |
| 8 | Salto a terra | CHIUSO |
| 9 | Linee sprint ai lati | CHIUSO |
| 10 | Scia tumble | CHIUSO (ribbon tolto: aggiornare la tabella B) |
| 11 | Colore polvere | APERTO: #DEDCD7 (atterraggio) e #D7D7D7 (scia), ombra #C8C8B8 su ~8% della pallina; target almeno #F0E8DA con ombra #D0BEA8 sul 25-30% |

## Fix per il round 3, in ordine
1. Schegge del colpo: devono uscire dal grumo entro il frame 3 (a f29 almeno 40-60 cm dal punto d'impatto, distanza tra stelle ≥ una stella). Verificare che velocità e drag agiscano davvero (oggi ~30 cm in 6 frame) e che non siano ferme in hitstop.
2. Rosso #FF3B2F solo da intensità 1.0; S e M finiscono in arancio #FF8A1E. Togliere la stella crema dal frame del lampo (schegge invisibili al frame 1).
3. Partenza corsa: 4 palline separate (20/16/12 cm) con velocità iniziali diverse nel ventaglio di 60°, drag 6 reale (ferme entro ~40 cm), mai un grumo.
4. Anello atterraggio: vita 15-18 frame, spessore che cala fino a sparire (come l'anello dello splat, che va bene). Palline che nascono su un raggio di 25-30 cm, non sui piedi; stessa cosa per lo splat.
5. Knockdown: far partire l'anello quando il corpo tocca terra (oggi ~8-10 frame prima a schermo); controllare se l'evento è in anticipo o se è la mesh in ritardo.
6. Polvere: più chiara e calda a schermo (vedi fix 11), secondo tono su 25-30% anche nella pallina della scia.
7. Skid: spawn per distanza percorsa (1 ogni 35-40 cm) o stop sotto ~150 cm/s; alzare le palline di mezzo raggio perché il pavimento non le tagli.
8. Colpo dalla camera del giocatore: provare una cattura con attaccante non in asse con la camera; se lampo e schegge restano nascosti, è il fix 1 che deve portarle fuori dalle sagome.
9. Scia tumble (facoltativo): spawn per distanza (1 ogni ~60 cm) così in volo veloce si leggono 3-4 palline in fila.

## Per il round 3
Stesse sequenze. In più: un rimbalzo vero su muro e a terra (serve un colpo che generi WallBounce / GroundBounce), e il .mov di Toad nel pacchetto per il punto 11.
