# Review dropkick K6, pack k6r2b (cloud, 2026-11-21) - VERDETTO: FAIL posa (giro 2 di 3), solo sulle braccia in startup

Build `claude/project-thread-l1dcng` 84b7c354. Base r1 = K6 giro 1. Guardate 8 immagini, tutte fogli miei dai mp4 a
velocita' reale (un png per frame, 384-480 px a frame): press az315 e az180 r1 sopra r2 (a0-a9), zoom press az315 r2
(a1-a12), alta az90 e az315 r2 (a12-a31), zoom alta az90 r2 (a14-a25), settle whiff az90 r1 sopra r2 (a13-a22),
press az90 + settle az180 r2. Tre sono allegati qui (`review_*.jpg`). ASSERT OK sulle tre run (assert_lines.txt).
Non guardata: la run hit (stessa clip; resta da guardare al giro 3).

## 1. Identita' (prima): PASS
- Gambe unite e dritte da a15 (az90, az315): PASS.
- Braccia conserte: in piedi da a3-a4, sdraiate da a14 con avambracci sul petto: PASS.
- Smug in caduta (testa sollevata, sguardo lungo il corpo), tenuto per tutta la caduta alta: PASS.

## 2. Vita
- LINEA Big Band (bacino punto piu' basso, gambe mai sotto il piano del corpo, busto curvo indietro): PASS, tenuta
  anche nel rimbalzo (a19-a25 gambe quasi orizzontali ma ancora sopra la linea del busto).
- Snap a13 -> a15 in 2 frame: PASS, intatto.
- SETTLE (richiesta 2): PASS. Su az90 a 250 cm, senza zoom: lock a15, gambe che salgono ancora a16-a17 (r1 e' fermo
  negli stessi frame, foglio settle), ricaduta a18-a19. Nell'alta la testa passa da rovesciata (a15-a17) a sollevata
  (a18-a21) e il pugno alto si stacca dal petto a18-a21 e ricade a22-a25: piccoli ma si vedono. Non toccare.
- PRESS delle braccia (richiesta 1): FAIL posa, ma per il TEMPO, non per l'ampiezza.
  Il contrasto adesso c'e' ed e' grande (a2 ali aperte, a3 gia' piegate, a4 strette), anche da dietro: il rischio
  segnalato dal costruttore non c'e', su az180 a2 il braccio sinistro e' teso di lato e il destro ha il pugno sopra
  la spalla. Il difetto e' la sequenza a schermo: a0 braccia larghe (posa di salto) -> a1 guardia raccolta -> a2 ali
  aperte -> a3 chiuse. L'apertura vive UN solo frame ed e' preceduta da una chiusura: in 3 frame le braccia fanno
  dentro-fuori-dentro, a velocita' reale e' uno scatto/tremolio, non un caricamento seguito dalla stretta.
  (Nota: a schermo l'apertura e' a a2 e la stretta a a4, un frame prima di quanto dice il README.)
- RILASCIO a8: FAIL posa per leggibilita'. Da a6 le ginocchia coprono le braccia su az315, da dietro i gomiti non
  escono dalla sagoma del busto (a5-a9 identici a r1), su az90 tra a5 e a7 non cambia niente di visibile.
- Dettaglio: a2 su az315 il pugno vicino sta DAVANTI al viso.

## 3. Tecnico
- Clearance PASS (dal README, non ricontrollata). Nessuna compenetrazione visibile nei fogli.
- Whiff basso: a20 gli avambracci scattano in verticale un frame all'atterraggio (c'era gia' in r1 a19 e in K5):
  etichetta GIOCO (blend nella clip di atterraggio), gia' in coda al codice, non conta come giro.
- Coda FAF: non applicabile (attiva fino all'atterraggio).

## Cosa correggere (giro 3, ultimo; etichetta: posa; solo chiavi delle braccia a0-a9)
1. Apertura TENUTA: la chiave 0 non deve raccogliere le braccia in guardia. Dalla posa larga di a0 andare diretti
   alle ali e tenerle almeno 2 frame a schermo (a1-a2), poi stretta in 2 frame (a3-a4) come adesso. Limite di
   silhouette: su az180 e az315 i gomiti non rientrano mai verso il busto prima della stretta.
2. Pugno vicino a lato della testa, non davanti al viso (az315 a1-a2).
3. Rilascio visibile FUORI dalle ginocchia: a8-a9 gomiti che escono di lato di almeno 6 cm oltre la sagoma del busto
   su az180, spalle giu'; in alternativa dichiarare nel README che il rilascio e' coperto dal tuck e lasciare
   decidere l'owner. Non stringere di piu' il press (clearance al limite).
4. Non toccare: settle, linea a15, snap a13-a15, smug, timeline, hitbox.

## Cattura (non conta come giro)
- Alta a 350 cm: testa e piedi in quadro, bene, ma il personaggio e' ~140 px: aggiungere una vista az90 a 250 cm che
  segue il bacino per il settle.
- La differenza luma tra frame non e' usabile qui (camera che segue e sfondi diversi tra r1 e r2): giudizio a occhio.

## Prove
Fogli: `ffmpeg -i <run>_realspeed.mp4 %03d.png` (file N = a(N-21), etichette a<N> bruciate nei frame), poi
`ffmpeg ... -filter_complex concat,tile`. Frame citati leggibili nei tre `review_*.jpg` allegati.
