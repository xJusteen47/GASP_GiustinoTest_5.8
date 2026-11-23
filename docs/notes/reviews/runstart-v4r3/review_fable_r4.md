# Review Fable r4: Overlord run start v4r3 vs v4r2 (giro di posa 3 di 3)

Prove guardate: le tre griglie (ritagliate a piena risoluzione), `metrics_summary.json`, 8 frame del video
(v088, v090, v092, v094, v112, v118, v124, v130; 6 viste). Numeri "v" = frame video a 60 fps, come nelle griglie.

## Criteri dell'owner
1. **Braccia nel corpo: risolto a occhio.** In az0 i gomiti di v4r3 sono chiaramente più larghi da v112 a v168
   (v4r2 li tiene incollati ai fianchi). Nei frame video v112, v118, v124, v130 il pugno basso passa FUORI dalla coscia
   in az0, az180 e az315; il pugno alto resta davanti al mento senza entrare nella testa. Nessuna compenetrazione
   vista in nessuna delle 6 viste. Ampiezza del pompaggio non ridotta (az90: stesso arco anca-mento di v4r2).
   Punti più stretti, da ricontrollare al rallentatore: v118 e v136 (ginocchio alto vicino all'avambraccio avanti,
   az90/az270) e v144-v148 az0 (pugni vicini allo sterno: da davanti la profondità non si legge, az90 li dà davanti).
2. **Primi passi più rapidi: visibile, misura non verificabile.** In az90 v4r3 cambia fase prima di v4r2 già da v120,
   e v128-v136 mostrano falcate compatte dove v4r2 a v128 ha una falcata lunga e distesa. Poi da v152 le falcate si
   riallungano: il "motore che sale di giri" si legge. Con passo griglia 4 (un passo = 10 frame video) non posso
   confermare il valore "5 frame" né contare i 3 passi: è coerente, non misurato da me.
3. **Scatto del busto: NON chiuso.** v4r3 non cambia nulla qui e il brief lo attribuisce ai frame congelati. Non ho
   potuto verificarli (servono coppie di frame consecutivi f108-f110, non le avevo). Vedi sotto, sezione tecnica.

## Tratti d'identità
1. Stomp tenuto: sì. v084-v108 in az90 e az0 sono indistinguibili tra le due righe (carica v092-v100, gamba su v104,
   pestone e polvere v108-v112).
2. Rev-up: sì a occhio (vedi punto 2).
3. Stile bruto: sì. Busto a 40-45 gradi da v108 a v176, gomiti fuori, pugno al mento. I gomiti più larghi in az0
   v148-v164 rafforzano il carattere; al limite dell'"ala di pollo" a v152-v160 ma ancora in personaggio.
4. Compenetrazioni: nessuna vista (limiti sotto).
5. Scatto del busto dovuto alla clip: non escluso, vedi tecnico.

## Vita
Anticipazione e peso dello stomp intatti. Archi delle braccia larghi e leggibili da davanti, silhouette più chiara di
v4r2 (in v4r2 az0 le braccia si fondono col torso a v120-v140). Il busto oscilla un po' di più nei passi corti
(v124-v132 az90: testa su/giù), accettabile come frenesia. Root coerente con l'ombra/capsula in tutti i frame visti.

## Tecnico
- Peso clip: f100-f175 a 1,0, uscita 0,94 / 0,78 / 0,50 a f180/185/190, 0 a f195. Come v4r2.
- **Busto in uscita (dati montage / gioco):** in az90 il busto passa da circa 40 gradi (v176) a circa 15-20 (v188-v192)
  in entrambe le versioni, proprio mentre il peso della clip scende. È un secondo candidato per "scatta quando si
  rialza" che il brief non considera: se l'owner parlava dell'uscita, i frame congelati f109/f112 non c'entrano.
- **Busto v104 -> v108 (posa, invariata):** da quasi eretto a circa 45 gradi in 4 frame video. Fa parte del pestone e
  a me legge come voluto, ma cade sulla stessa finestra dei frame congelati: le due cause non sono separabili da qui.
- **Discrepanza numeri (dati):** il brief dà travel in gioco f80-200 = 794 cm (v4r2 785); il json dà
  `actor_x_f200` = 746,6 (v4r2 784,5), cioè -4,8 %. Sotto la soglia del 15 %, ma i due numeri non tornano: chiarire.
- **Piedi (posa o dati, da verificare):** scivolamento piedi piantati f80-240: v4r3 sx 35,6 / dx 73,6 cm contro
  v4r2 49,2 / 52,1. Il destro peggiora del 41 % e diventa asimmetrico. A occhio dalle griglie non si vede, ma la
  falcata accorciata a pari velocità di root è proprio il caso in cui il piede pattina.

## Fix consigliati
1. gioco: confermare i frame congelati con frame consecutivi v106-v116 e chiudere la voce in coda codice.
2. dati montage: chiedere all'owner QUALE risalita scatta; se è l'uscita, allungare/ammorbidire il blend out f180-f195
   o allineare l'inclinazione finale della clip a quella del loop.
3. posa (minore): controllare lo scivolamento del piede destro nei primi 3 passi corti; non blocca.
4. dati: riallineare il travel del brief con il json.

## Limiti delle prove
- `grid_game.png`: il personaggio è alto circa 15 pixel, inutile per la posa; dice solo che la traiettoria è dritta.
- Compenetrazioni giudicate su 4 frame video a 6 viste più due viste di griglia a passo 4: i frame intermedi non visti.
- Il valore +1,8 cm del brief (capsule) non è verificabile a occhio; a quella distanza un contatto di mesh è possibile.
- Frame congelati f109/f112 e cadenza "5 frame": non verificati.

## Esito
**PASS** per il giro di posa (punti 1 e 2 dell'owner risolti a vista, stomp e stile bruto intatti, nessuna riduzione
oltre soglia). Restano aperti, fuori dalla posa: scatto del busto (gioco, più sospetto dati montage sull'uscita),
discrepanza travel (dati), scivolamento piede destro (da verificare).
