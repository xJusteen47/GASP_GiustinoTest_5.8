**Esito: PASS** (con riserve dichiarate sotto: polso/pugno e compenetrazione non verificabili a fondo dalle prove)

# Review Fable r5: Overlord run start v4r4 (vs v4r3, v4r2)

Prove guardate: 8 griglie (az0/az90/az45/game, a+b) e 8 frame di `owner/runstart_v4r4.mp4` (v116, 120, 128, 136, 148, 156,
164, 172, foglio 6 viste in `_tmp/sheet.png`). Nient'altro.

## Criteri owner (feedback su v4r3)
- **Gomiti all'altezza di v4r2: SI.** az0 v112-v136: in v4r3 i gomiti sono larghi e alti ("ali di pollo", omero quasi orizzontale);
  in v4r4 scendono e si chiudono verso il busto, silhouette frontale vicina a quella di v4r2 (v120, v124, v132 i casi più chiari).
  az45 v120-v124: gomito arretrato sotto la linea della spalla, in v4r3 era sopra.
- **Niente "zombie": SI.** L'omero ora cambia angolo tra un frame e l'altro (az90 v116 -> v128 -> v136, az45 v140 -> v152):
  oscilla tutto il braccio dalla spalla, non solo l'avambraccio.
- **Mani dove erano in v4r3: SI a occhio.** Nelle coppie v4r3/v4r4 allineate i pugni cadono nello stesso punto dello schermo in
  tutte e tre le viste; non ho trovato una cella in cui la mano si sposti.
- **Braccia fuori dal corpo: nessuna compenetrazione visibile**, vedi limiti.

## Tratti d'identità
1. Stomp tenuto: presente (v100-v108 carica, polvere da v112), identico a v4r3.
2. Rev-up: presente, passi corti v112-v136 che si allungano v140-v176; cadenza uguale a v4r3.
3. Stile bruto: busto inclinato e pugno avanti al mento conservati (az90 v156, v168-v176). Il pompaggio resta ampio; con i
   gomiti più bassi la sagoma frontale è un po' meno larga di v4r3, ma più pesante e credibile: per me è un guadagno.
4. Compenetrazione: nessuna visibile da az0/az45/az90 e dalle viste posteriori del video.
5. Gomiti come v4r2 e omero che oscilla: rispettato.

## Vita (a occhio)
- Peso e anticipazione invariati rispetto a v4r3: carica, pestone, spinta. Il braccio ora legge come un arco unico
  spalla-gomito-pugno, con il gomito che guida il rientro: più fluido di v4r3.
- Follow-through del braccio arretrato buono (az90 v120, v156; az45 v168-v172 mano aperta dietro).
- Riserva minore: az0 v116 e v136 i gomiti restano un po' aperti di lato (più bassi di v4r3, non alti). Si legge come
  postura da bruto, non come zombie. Non blocca.
- Vista di gioco: personaggio troppo piccolo per giudicare le braccia; lettura complessiva uguale a v4r3.

## Tecnico
- Numeri del brief coerenti con quello che si vede: gomito v4r4 vs v4r2 media -0,1 cm, picco +1,9 (v4r3: +2,7 / +10).
- Busto, gambe, polvere e avanzamento coincidono cella per cella con v4r3 nelle griglie.
- Assert di cattura OK (peso >= 0,8 sul 100% di f100-f180, min 0,94).
- Nessuno scatto evidente di rotazione del pugno o avambraccio spezzato nei campioni a passo 4.

## Fix (nessuno bloccante)
- [posa, facoltativo] az0 v116 e v136: se l'owner vede ancora i gomiti larghi, ridurre l'apertura laterale (non l'altezza) di
  pochi gradi in quei due passaggi.
- [gioco, fuori ambito] frame congelati f109/f112: bug noto, non valutato.
- [dati montage] niente da segnalare.

## Limiti delle prove
- **Polso e rotazione del pugno: non verificati a fondo.** Celle da ~250 px e passo 4 frame: uno scatto di 1-2 frame o una
  torsione del polso di 20-30 gradi non si vedrebbero. Vale "nessun difetto visibile", non "confermato naturale". Va visto il
  video a 60 fps (o slomo) sulle mani, soprattutto f12-58 della clip dove la rotazione arriva a ~40 gradi.
- **Compenetrazione:** il margine minimo è +1,8 cm; in az45 v128/v136 e az90 v116 la mano passa davanti a coscia/ginocchio
  e dal fermo immagine non distinguo sovrapposizione prospettica da contatto. Mi appoggio al numero del brief, non all'occhio.
- **Posizione mani invariata (0,001 cm):** confermata solo a occhio sulle griglie; il dato è del brief.
- v4r2 ha cadenza diversa: il confronto dell'altezza gomiti è per fase simile, non frame per frame.
- Ho estratto 8 frame come consentito; la risoluzione del foglio (6 viste a 1440x720) non aggiunge dettaglio alle griglie.
