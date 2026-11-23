# Review hitadd-v3 (additivi di colpo direzionali) — reviewer Fable, round 2

Viste 6 immagini (front/left/right, az45 e az90), confronto con v2 a memoria della review precedente. Miniature da 256 px: angoli stimati a occhio. Back e Air ancora non catturate: **non giudicate**. Le voci "gioco" non contano come FAIL dell'animazione.

## Front — PASS
- Molto meglio di v2. Petto indietro da v024, testa che frusta v026-v028, picco tenuto v028-v030 con testa al massimo già dentro l'hitstop (in v2 era al 67%).
- Ritorno v031-v036, overshoot in avanti ben leggibile v036-v041 (petto e testa), seconda oscillazione piccola v042-v046, fermo da v047. Nessun pop.
- Braccia ora vive: il braccio vicino sale e si apre v033-v036 (az90), apertura asimmetrica a az45 v031-v035.
- **posa** (nota, non FAIL): a v028-v030 l'arco indietro è al limite (base + 22°, testa quasi orizzontale a az90). Se a peso 1.0 sembra "schiena rotta" in gioco: chest back 22 → 18, head back 34 → 32.
- Domanda 1 (qualità action stilizzato alla Smash): **sì**. Domanda 2 (pari a Smash / tripla A): **sì per il layer torso**; il risultato complessivo in gioco resta sotto finché mancano bacino e vibrazione in hitstop (gioco, in coda).

## Left — FAIL (lieve)
- Migliorato: durante il picco il braccio colpito è piegato (az45 v029-v040), non più teso; il ritorno ora si vede (az45 v041-v044, az90 v041-v049); il busto non è più "accasciato" come in v2.
- **posa** — overshoot laterale non leggibile: dopo il ritorno (v041-v044) il busto si ferma subito (az45 v045-v061 fermo). Con chest side 12 e twist -8 il 33% di overshoot vale circa 4°: troppo poco.
- **posa** — testa: il "ritardo verso il colpo" continua a non leggersi (az45 v029-v040 la testa resta bassa dal lato opposto all'anello).
- **dati montage** — dopo il ritorno la base lascia il braccio teso orizzontale verso l'attaccante (az45 v044-v061): è la posa base, non l'additivo.
- Domanda 1: **sì, di misura**. Domanda 2: **no** → FAIL.

## Right — PASS (con riserva)
- Identità leggibile: busto piegato via dal colpo v039-v047 (az90), testa inclinata verso l'attaccante rispetto al petto v043-v047, ritorno v048-v052 con il braccio che scende, piccolo overshoot v050-v053.
- **dati montage** — v053-v058 il braccio risale alla posa base orizzontale e resta puntato sull'attaccante fino a v070 (az90): silhouette debole, ma è la base.
- **posa** (da verificare) — az45 v053-v062: avambraccio sinistro piegato con la mano all'altezza della spalla, poi si ridistende a v063. Se viene dall'additivo, è il gomito che oscilla troppo.
- **gioco** — v046→v047 scatto di inclinazione e piedi (az90): spinta/passo della base, non dell'additivo.
- Domanda 1: **sì**. Domanda 2: **sì per il layer torso, con la riserva del braccio base**.

## Fix
1. Left: chest (4,12,-8) → (4,14,-14); head (8,-6,-4) → (8,-12,-4). Il twist è l'asse che si legge da entrambe le viste.
2. Left e Right: gomiti, fattore damping `k * 0.75` → `k * 1.0` sulla riga `lowerarm` (meno rimbalzo del gomito dopo v053).
3. Base (dati montage): in AS_React_HitStand e nella base laterale abbassare di circa 25° il braccio teso verso l'attaccante, oppure piegare il gomito di 30°. È il difetto più visibile rimasto in Left e Right.
4. Front, solo se in gioco l'arco risulta eccessivo: chest back 22 → 18, head back 34 → 32.
5. Catturare Back e Air prima di chiudere: 4 degli 8 clip non sono mai stati visti.
