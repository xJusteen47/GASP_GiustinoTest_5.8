# Libreria delle pose di mano (GSTN)

Pose di mano riutilizzabili per lo scheletro del mannequin UE (anche il rig Cascadeur dell'owner, che ha gli stessi ossi).
Fonte unica: `Tools/GAS/hand_poses.py` (dizionario `POSES`). Gli asset si rigenerano con
`Tools/GAS/build_hand_poses_and_hit_arms.py` (commandlet headless, idempotente).

## Asset
| Asset | Cosa contiene |
|---|---|
| `/Game/GSTN/Animation/Hands/AS_GSTN_HandPoses` | Sequenza a 60 fps, un frame per posa: 0 Reference, 1 Splayed, 2 SplayedTense, 3 Relaxed, 4 Fist, 5 Claw, 6 Flat. Ogni altro osso alla posa di riferimento. |
| `/Game/GSTN/Animation/Hands/PA_GSTN_HandPoses` | Pose asset fatto da quella sequenza, pose con gli stessi nomi (per Pose Blender / Pose By Name nell'AnimBP). |

## Le pose
| Posa | Aspetto | Quando |
|---|---|---|
| Splayed | dita larghe e piegate un po' all'indietro, pollice fuori | danno (shock), hitstun, lanci; è la mano "Smash" |
| SplayedTense | come Splayed ma più tesa e aperta | primi frame di un colpo, colpi forti, hitstop |
| Relaxed | curva a cascata (più chiusa verso il mignolo), pollice dentro | idle, fine dell'hitstun, ritorno alla locomozione |
| Fist | pugno pieno, pollice sopra le falangi | pugni, mosse a pugno (dropkick: braccia a pugno) |
| Claw | nocche dritte, punte uncinate, dita aperte | prese, tensione, dolore forte |
| Flat | lama dritta, dita unite | schiaffi, chop, guardie (i jab dell'owner sono a mano aperta) |

## Come sono fatte
- Ogni posa è un insieme di delta dalla posa di riferimento, in gradi, per dito: (curl_01, curl_02, curl_03, spread).
  Curl positivo = verso il palmo, negativo = iperestensione; spread = lontano dal medio, nel piano del palmo.
  Pollice: (curl_01, curl_02, curl_03, out), out = apertura lontano dall'indice.
- Gli assi si misurano sulla posa di riferimento (normale del palmo dal verso in cui le punte già si piegano), quindi
  funziona per mano sinistra e destra senza dipendere dagli assi locali degli ossi. Il log stampa "tip offset > 0"
  come controllo del verso.
- `finger_locals(ref_cs, ref_local, side, pose)` restituisce la rotazione locale di ogni osso delle dita; una clip
  la usa riscrivendo le tracce delle dita (le nostre clip non animano le dita in altro modo).
- Fondere due pose: `qslerp` sulle rotazioni locali, osso per osso (rotazioni pure, niente interpolazione lineare
  sugli angoli di Eulero).

## Aggiungere una posa
1. Aggiungere la voce in `POSES` e il nome in `ORDER` (la sequenza deve avere un numero pari di intervalli: con il
   frame Reference, il numero di pose deve restare pari; se diventa dispari, aggiungere una posa o un frame).
2. Rilanciare lo script con `HITARMS_SKIP_LIBRARY` non impostato.
3. Guardare le mani in una cattura ravvicinata prima di usarle in una mossa (regola 10).
