# Review r6 (Fable): Overlord run start v4r5 vs v4r4

**Esito: PASS** (con 3 note non bloccanti; nessun FAIL).

Prove guardate: brief, le 3 griglie (una volta ciascuna, a terzi), `speed_v4r5.png`, `metrics_summary.json`, 8 frame del video
(v70-v105 passo 5).

## Criteri dell'owner
- **Passo piantato tolto: sì.** In v4r4 il corpo resta accucciato sul posto da v92 a v104 e corre da v108; in v4r5 è accucciato solo
  a v95-v96 e a v100 è già in falcata con la polvere. Attore a f200: 832 cm contro 744.
- **Resto di v4r4 tenuto: sì**, per quanto si legge: gomiti larghi, pugni alti e busto inclinato sono gli stessi nelle due righe a
  parità di fase (v4r5 è ~10 frame di clip avanti).
- **Accelerazione graduale: sì in parte.** Parte dal primo frame (niente tratto fermo), ma non è una rampa unica: vedi nota 1.

## Tratti d'identità
1. **Rev-up frenetico: presente.** Da v100 a v124 ogni campione (4 frame) mostra una fase di gamba diversa con falcata corta; da v136
   a v152 la falcata è chiaramente più lunga. La cadenza esatta non si misura con passo 4.
2. **Stile bruto: presente** da v96 a v156: busto avanti, pugni al mento, pompaggio ampio (az0 v100-v132).
3. **Gomiti/omero: come v4r4.** Nessuna compenetrazione visibile nelle viste az0 e az90; a questa risoluzione (~200 px per riquadro)
   non posso escludere contatti piccoli avambraccio/petto.

## Vita
- **Anticipazione:** il carico accucciato dura circa 0,1 s (v92-v96). Si legge come un "giù e via", coerente con la richiesta; è
  breve ma il dislivello del bacino è netto, quindi basta.
- **Ingresso da idle:** blend leggibile, non uno scatto. v85 idle, v90 inizio flessione, v95 accucciato. A v92 (az90) una gamba è
  sollevata con il piede a penzoloni: è la posa intermedia del blend, dura un campione.
- **Peso e archi:** il bacino scende e poi sale in avanti con arco continuo; la polvere a v100 marca bene la spinta.
- **Follow-through / uscita:** tra v160 e v176 il busto si raddrizza e le braccia si stringono (run loop). Vedi nota 3.
- **Root/capsula:** nessun salto del corpo rispetto all'ombra nelle tre viste.

## Note (non bloccanti)
1. **dati montage — rampa a due tempi.** Il grafico sale ripido fino a ~450 cm/s a 0,6 s, poi resta piatto fra 450 e 490 fino a
   1,2 s, poi sale a gradino a 585. È un "scatto, pausa, scatto", non una crescita continua alla Toad. Fix: accorciare o inclinare il
   pianoro dello stage 2 così la velocità continua a salire fra 0,6 e 1,2 s.
2. **gioco — latenza e scivolo iniziale.** Il peso della clip è 0 fino a v90 e 0,68 a v95: per ~0,17-0,25 s dallo stick la capsula
   accelera già (10% a 0,13 s) mentre il corpo è ancora in idle. Lo scivolo è di pochi cm e a occhio non si nota, ma la partenza
   "subito" è limitata da questo ritardo di selezione, identico in v4r4. Fuori ambito per la clip.
3. **gioco — uscita verso il run loop.** La clip esce a v160-v175 (v4r4 a v180-v195) e il loop è più dritto e con braccia strette:
   il tratto bruto si perde 20 frame prima rispetto a v4r4. Non è un difetto della clip; è il loop che non porta lo stile.

## Tecnico
- Assert di cattura OK: peso >= 0,8 sul 93% di f95-f170 (min 0,56 in uscita). `ss180_max_w` = 0 in entrambe.
- Piedi piantati f80-240: L 49,5 / R 43,8 cm contro L 36,6 / R 73,8 di v4r4. Totale 93 contro 110 cm e più bilanciato, ma il sinistro
  peggiora di 13 cm. Lo scivolo residuo non si vede nelle griglie; il warping copre abbastanza.
- A regime la velocità oscilla fra 585 e 635 cm/s a dente di sega: è del run loop (gioco), presente anche nella curva "before".

## Limiti delle prove
- Il grafico confronta v4r5 con **v4r3**, non con v4r4: il confronto di rampa con v4r4 viene solo dai numeri del brief.
- Griglie a passo 4 e riquadri piccoli: cadenza dei primi passi, scivolo dei piedi e compenetrazioni fini non sono verificabili a
  occhio. La vista game è troppo lontana per giudicare la posa: l'ho usata solo per la traiettoria.
- Il frame congelato vicino a v99 (bug di gioco noto) non l'ho cercato.
- La metrica dei piedi è euristica e copre f80-240, quindi include il run loop oltre alla clip.
