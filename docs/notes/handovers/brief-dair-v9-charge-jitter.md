# Brief: DAir V9, scatto della testa nella carica (diagnosi 2026-11-17, thread playtest-fixes-1117c)
Ambito (owner): solo la carica della dair V9, la testa deve restare circa alla stessa altezza. NON approvata, nessuna variante all'owner senza review regola 10.
Macchina: fisso (_ue59). Modello consigliato: Fable high per decidere le pose, Sonnet per catture/build. Budget stimato 8-12 $.
Tratti d'identità da non toccare (dalla v9): palla stretta in carica, colpo in 2 frame, rimbalzo dei piedi, recovery morbida sulla 37.

## Misura (prova: `docs/notes/reviews/overlord-dair-v9-jitter/numbers.md`, da `Saved/PF1117c/diag/dair.jsonl`, script `Saved/PF1117c/diag_dair.py`)
Salto, poi giù + X in aria (Overlord: prima entra l'air crouch, poi la dair). Altezze dei socket rispetto all'origine dell'attore, per frame di gioco:
- f16→f17→f18 (air crouch → dair frame 0 → 1): bacino -50 → -45 → -9, testa 4 → 10 → **44 (+34 cm in un frame)**.
- f18→f20 (dair 1 → 3): testa 44 → 39 → **10 (-29 cm in due frame)**, poi 16-18 fino alla 7, di nuovo 10 alla 8.
- Il root della mesh rispetto alla capsula salta: -115, -105, -105, -117, -111, -111, -110, -118 (±12 cm, f17-f25); negli stessi frame la capsula
  sale a scatti (+13,7 cm a f20 e +10,4 a f25 con vz 370 e 290, cioè +6 attesi): è il cambio di capsula della mossa (traccia capsule, review
  `capsule-moves-dair`) che sposta l'attore e la mesh compensa male o in ritardo.
- vz del Mover è liscia (418 → 214): il movimento in sé non scatta.

## Diagnosi
1. **Blend d'ingresso (animazione/montage, causa principale)**: la dair parte dall'air crouch (bacino -50) ma il suo frame 0-1 è quasi in piedi (bacino -9)
   e la carica torna giù entro la 3. Su-giù-su di 30+ cm in 3 frame = lo scatto che si vede. Il blend-in del montage è di fatto 0-1 frame (regola owner
   "primo frame netto"), quindi niente lo copre.
2. **Capsula della mossa (gioco/dati montage)**: i cambi di altezza della capsula nella carica spostano l'attore di 5-8 cm in un frame e il root della
   mesh oscilla di ±12 cm: secondo contributo, visibile come tremolio della testa anche dove la posa è ferma (f21-f24).
Non è root motion (il root è costante dentro il clip, varia solo per l'offset mesh/capsula) né le curve (non lette qui: da controllare nel thread).

## Cosa fare (thread animazione, una mossa)
- Posa (Fable decide): frame 0-3 della carica che partono dalla posa dell'air crouch (bacino ~-45/-50) e scendono nella palla senza risalire;
  testa entro ±5 cm per frame nella carica. Oppure blend-in del montage di 3-4 frame SOLO per l'ingresso da air crouch (serve decisione owner:
  va contro la regola del primo frame netto, ambito: tutte le mosse).
- Gioco (coda codice, non conta come giro di posa): capsula della dair costante nella carica, o mesh offset che compensa nello stesso frame
  (`GSTN_BodyCapsuleModifier`, mesh smoothing). Rimisurare con `Saved/PF1117c/diag_dair.py` (root costante ±1 cm).
- Review regola 10 sul video in-game, base = v9 attuale (`AM_OverlordDAirV9.uasset` locale non committato, sporco da prima di questi thread).
