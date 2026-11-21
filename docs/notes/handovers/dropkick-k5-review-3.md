# Handover: review dropkick K5 giro 3 (cloud, Fable high, 2026-11-21)

Branch claude/project-thread-98wtdn (da claude/project-thread-cqad05 82d09c64). Regola 10, ultimo giro di posa.

## Esito: PASS (posa). Va all'owner.
Review completa: docs/notes/reviews/dropkick-kick-k5/k5r3/review.md (stessa cartella del pack).
- Identità: gambe unite e dritte PASS; braccia conserte in piedi (a4-a13) PASS; braccia conserte sdraiato (a14 -> terra) PASS,
  blocco sotto la testa su az90 e testa tutta visibile su az315; compiaciuto in caduta PASS (testa sollevata, mento su, girata,
  guarda il bersaglio lungo il corpo). Limite: manichino senza volto, giudice finale l'owner.
- Vita: anticipazione PASS, scatto a13 -> a15 in 2 frame PASS, caduta tenuta PASS. Accento al blocco debole (dati montage).
- Tecnico: timing = sheet, hit = whiff, nessuna riduzione di ampiezza o root motion.

## Non posa, per le code
- gioco / clip di atterraggio: whiff da 480 cm, a19, le braccia scattano in alto un frame prima di terra. Nuovo: sulla pressione
  alta (a28) e sull'hit (a19 ancora in aria) NON succede, quindi è il blend verso la clip di atterraggio quando la terra arriva
  nei primi frame del loop.
- gioco: niente hitstop né reazione del manichino (hitbox dell'owner).
- cattura: hit e high senza spec (riga ASSERT FAIL "empty spec", vale la riga offline); nell'hit il manichino esce
  dall'inquadratura a a15; vista game a 15-25 px.

## Da portare all'owner (regola 13d)
1. Approvare K5 giro 3 come calcio del dropkick? Se sì: tag approved/dropkick-k5r3 + riga in docs/notes/anim-approvate.md.
2. Segmento one-shot di overshoot dopo il colpo (cambio di montage): consigliato sì.
3. Hitstop e reazione arrivano con le sue hitbox.

## Scelte applicate (regola 6)
Ho costruito 5 griglie dai frame degli mp4 del pack (review_*.jpg) perché le griglie del pack hanno riquadri da 120 px; e un
video A/B base | K5 per l'owner (ab_base_vs_k5r3_whiff_slow4x.mp4), copiato anche nei project files reviews/dropkick-kick-k5/k5r3/.

Prove: etichette "a<N> v<frame>" impresse negli mp4 (ffmpeg, un png per frame, 70 frame whiff e hit, 64 high; ffprobe
-count_frames); finestre ASSERT in k5r3/assert_lines.txt ricontrollate sulle etichette (whiff f140-f157 = a2-a19, high
f131-f157 = a2-a28, ultimo frame in aria in entrambi). Immagini guardate: 8 (limite 8).
COSTO: 3 $ stimati | 0 minuti desktop | ~95k token review
