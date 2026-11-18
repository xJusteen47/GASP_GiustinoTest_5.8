# Review hitadd-v2 (additivi di colpo direzionali) — reviewer Fable

Viste 8 immagini: 6 fogli hitadd-v2 (front/left/right, az45 e az90) + 2 base (hitstand_off, side_hitstand_l_off). Miniature da 256 px: gli angoli sono stime a occhio. Back e varianti Air non catturate: **non giudicate**.

## Front — FAIL (identità OK, parità no)
- Identità: OK. Petto indietro da v024, testa che frusta indietro dopo il petto (v026-v028), picco tenuto v028-v033, ritorno v034-v041 con testa che supera in avanti (v038-v041). Nessun pop finale.
- **posa** — braccia: il "fling in avanti" non si distingue dalla base (hitstand_off ha già lo stesso braccio teso avanti); differenza visibile rispetto alla base = quasi solo la testa.
- **posa** — overshoot piccolo sul petto (circa 5°), si legge solo sulla testa; da v046 a v057 il corpo è fermo (12 frame morti).
- **dati montage/gioco** — il blocco a 0,07 s (frame 4,2) ferma la testa al 67% e il gomito destro al 36% del picco (ritardi 2,8 e 3,5 + 3 frame di salita): durante l'hitstop il picco non è completo.
- **gioco** — bacino e gambe immobili per 30 frame: è un additivo solo torso su posa tenuta, senza vibrazione in hitstop.
- Domanda 1 (qualità action stilizzato alla Smash): **no**, di poco. Domanda 2 (pari a Smash / tripla A): **no**.

## Left — FAIL
- **posa** — az45 v032-v058: busto piegato troppo (base laterale + 18° side + 14° twist), testa sotto la linea delle spalle: si legge "accasciato", non "colpito".
- **posa** — testa: va con il petto, lontano dal colpo; il "ritardo verso il colpo" non si legge (head side +9 ha lo stesso segno del petto).
- **posa** — braccio del lato colpito teso orizzontale verso l'attaccante per tutta la sequenza (az45 v031-v064); nella base era piegato sul petto.
- **posa** — az90 v036-v058: nessun ritorno/overshoot leggibile, sembra un fermo immagine.
- **gioco** — secondo colpo a v043 (Hitstun:12, flash bianco): v042 e v044 sono uguali, l'additivo non riparte. Da verificare se voluto.
- **gioco** — az90 v058→v059: salto di posa in un frame a fine hitstun (cambio della base, non dell'additivo).
- Domanda 1: **no**. Domanda 2: **no**.

## Right — FAIL (meglio di Left)
- Identità: parziale. az90 v035-v043 corpo inclinato via dall'attaccante, ritorno visibile v044-v050, poi fermo fino a v067. A az45 il busto sembra quasi dritto.
- **posa** — braccio teso orizzontale verso l'attaccante da v035 a v067 (az45 e az90): silhouette da T-pose, è il difetto più evidente.
- **posa** — testa centrata, nessun ritardo leggibile; overshoot appena visibile.
- **gioco** — v034→v035 rotazione di circa 90° in un frame (comportamento noto, non contato come difetto).
- Domanda 1: **no**. Domanda 2: **no**.

## Fix numerici (build_hit_additives.py)
1. `VARIANTS` Ground k 0.151 → 0.11 (overshoot petto 22% → 33%, testa 32% → 44%); Air 0.12 → 0.09.
2. `FRAMES` 29 → 41 (il taper oggi parte dal frame 23 e cancella la seconda oscillazione).
3. Front: clav_l (-14,0,0) → (-24,0,0), clav_r (-9,0,0) → (-14,0,0), elbow 18/10 → 30/14, arm_delay (1.5, 2.5) → (2.0, 3.5).
4. Left: chest (6,18,-14) → (4,12,-8); head (8,9,-10) → (8,-6,-4); clav_l (8,-16,0) → (16,-4,0); elbow_l 16 → 28.
5. Right: head (9,-10,11) → (9,6,4); clav_r (9,15,0) → (16,4,0); elbow_r 17 → 28; chest invariato.
6. Hitstop (gioco): blocco del tempo clip 0,07 s → 0,10 s e `HOLD_END` 4 → 6, così testa e braccia arrivano al picco prima del fermo. In alternativa: ritardo head ≤ 1,2.
7. Gioco: far ripartire l'additivo a ogni nuovo colpo in hitstun; sfumare l'uscita dalla base a fine hitstun (4-6 frame).
8. Per la parità (proposta, fuori dalle tabelle): traccia bacino (circa -4° back, 3 cm indietro, ritardo 0) e vibrazione in hitstop; finché il corpo sotto spine_01 resta fermo la risposta alla domanda 2 resta no.
