# Run start v3: sheet delle curve (regola 13c, 2026-11-20)

Clip 30 fps, 46 frame (1.5 s), contati da 0 (f60 = 2 x f30). Base: AS_GSTN_RunStart_Frantic f0-8 + run loop stock GASP
time-warpato (runstart_compose_casc_v3.py); stile a gruppi rigidi sopra (runstart_restyle_casc_v3.py). Identita':
docs/notes/briefs/runstart-identity.md; richieste: docs/notes/reviews/runstart-stockbase/review-blind.md "What a v3 needs".

| Parte | f0 | Movimento principale | Overshoot | Follow-through | Assestato |
|---|---|---|---|---|---|
| Piede dx (stomp) | idle, dietro | carico f1-2 (tallone su, punta giu' 15-25 gradi), ginocchio su f3-5 (caviglia 46-47 cm a f5-6 = sospensione pesante 2 f), SLAM f6-8 (17 + 20 cm/f, ease-IN), contatto f8 = 0.267 s, 28 cm avanti al piede sx | - | piatto e piantato f8-12 (squash) | spinta f12-13 |
| Piede sx | piantato | appoggio fermo f0-10 (tolto il mini-stamp Frantic f3-5) | - | stacco f11-13 in cross-fade verso la corsa | - |
| Bacino | 88.3 cm | leggero affondo f1-2 (84.5), SALE sul sollevamento f4-5 (86), cade f6-8; fondo f9 (circa -6.5 cm sul contatto), squash f9-11 | spinta f12 +1.5 cm | - | f15 |
| Busto (spine_02) | 0 | contro-inclinazione indietro f2-5 (-6 gradi) mentre il ginocchio sale, frustata avanti f7-9 (+17), corsa a +12 f14-34 | f9 17 -> f10 15 -> f12 11 | - | f42 = lean stock (consegna al loop) |
| Testa (neck_01) | 0 | segue il busto con 1 f di ritardo sulla frustata, poi contro-ruota (-8) per tenere lo sguardo avanti | f10 rimbalzo +2 | 1 f dietro al busto | f42 |
| Braccia carica | lungo i fianchi | si aprono in fuori f2-6 (fino a 28 gradi, minacciose), indietro -12 | f8-9 SLAM avanti-giu' +15/+20 | dx 1 f in ritardo e ampiezza 0.8 (no twinning) | f12-13 |
| Braccia corsa | swing stock | pompaggio: guadagno 1.7 sul swing stock (f14-34), gomiti +18 gradi piu' chiusi, flail laterale 7 +/- 6 gradi alla cadenza del passo (6.4 f), dx sfasata | - | mano dopo l'avambraccio (rig) | f42 = swing stock |
| Gambe corsa | - | cadenza 1.45x stock (passo 6.4 f = 0.21 s), falcata 0.72 (piedi verso il bacino, nessuno scivolamento: viaggio = 0.72 x 1.45 x stock), piedi 4 cm piu' larghi | - | - | rate e falcata 1.0 a f42 |
| Uscita | - | f42-45 = run loop stock a velocita' 1.0, fase sorgente 56.7/75 (0.756, contatto sx) | - | - | consegna a BS_Run_Loop |

Viaggio: 612 cm in 1.5 s (fine a 500 cm/s come il loop stock); prima dello stomp il bacino avanza 17 cm (passo d'impegno).
Cosa NON e' nella clip: polvere sul contatto f8 (gioco), riga chooser e uscita senza SS_180_Turn (dati montage, UE).

## UE (runstart-v3-1)
Import AS_GSTN_RunStart_v3 (runstart_v3_ue.py): 46 f, 1.5 s; round trip v3base vs Frantic f0-7 median 0.0 deg (only calf/leg of the compose
edit differ), pelvis f0 4.2 cm: no 8-11 deg offset, no diff fix. Root travel 613 cm on root -> root motion on + force root lock (as the loop).
Curves from the clip's own bones (curves_author kind pivot): contact_l/r + phase; contact_r onset f8.5 (stomp). NO movedata_speed (decision:
ABP dynamic play rate stalls our clips near speed 0, skid a5) and no enable_warping (stride warping would shrink the stomp at low capsule speed).
Hand-off: Run Start -> "Run Start Exit Run" at 1.4 s = M_Neutral_Run_Loop_F from 1.789 s (composer phase; feet match 1.800 s, same cost).
