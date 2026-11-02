# Handover anim pipeline: fase B, jab v12 e Ganondorf v3.1 sul rig (2026-11-02, branch claude/project-thread-s125cq)

Esito: **le due mosse sono bakate dal motore unico** (`python -m animpipe build cards/<mossa>.json`, ~10 s ciascuna):
`/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build` (11 pose, 10659 chiavi) e `AS_JabV12_Build` (9 pose, 8685 chiavi).
Checkpoint 2 è nella storia git di questo file. Non esistono file Step dell'owner per queste mosse: le pose sono i frame
chiave delle clip v3.1/v12 (Ganondorf: i 5 frame dell'owner a 0/7/10/27/37 più 6 intermedi).

- Checkpoint pose (una griglia per mossa, `Tools/AnimPipeline/sheets/`, non in git): `ganon_ftilt_v31_poses.png` (riga in
  alto = i 5 frame dell'owner), `jab_v12_poses.png`; build sopra sorgente: `ganon_ftilt_v31_poses_ab.png`.
- Ganondorf, difetti risolti con `edits` nella scheda (rotazioni FK risolte dallo Jacobiano del rig, `fk_solve.py`):
  gomito dietro fuori profilo da 31 a 13 frame (gomito ~20 cm dietro il petto invece di 33-35, chiavi 20-32);
  avambraccio sinistro nel fianco allo scatto: clearance al frame 10 da -4,0 a +4,1 cm (mano 28 cm di lato, era 13).
  Bloccanti 5/6 come la sorgente: legibility FAIL ereditato (controlli da pugno su un calcio), ma migliore della sorgente.
- Jab v12: bloccanti 6/6 PASS (come la sorgente). **Pugno dietro non risolto** dopo 2 tentativi mirati (togliere la
  torsione della mano 1-3: 70°/frame, peggio; mano + braccio: 36,5°, uguale). I 36° del frame 0->1 vengono dallo swing
  del braccio di guardia nel contatto di 1 frame, non dalla torsione: serve una decisione di posa (domanda 1).
- Motore: ultimo frame ora bakato giusto (la sezione tornava al frame 0 a t = durata; play rate 0,99999); `lock_lengths`
  (stretch spento sul bake, traslazioni locali fisse; tolto fino a 1,56 cm sul collo) -> bone_lengths PASS su entrambe;
  scheda: `edits`, `drop`, `spacing` per posa, `sheet_extra`; `check --src` (stessi controlli sulla sorgente); planted
  solo sui piedi della scheda; `probe <take>` (canali del rig); `poses` (griglia, `--over`, `--blind`).
- Da guardare: ginocchio dietro del jab 9 cm più avanti della sorgente in guardia (IK gamba del rig, frame 0 e 18-37);
  spaziatura Ganon 3-7 a 0,17 (sorgente 0,78); motion_check FAIL identico alla sorgente su entrambe.
- A/B cieco (Sonnet, A = sorgente, B = build): jab B di poco (pugno più pulito dal busto al contatto); Ganondorf B
  (recupero più basso e piantato, più vicino al riferimento), ma segnala al frame 10 vista frontale il braccio sinistro
  che si chiude a triangolo verso il busto: effetto dell'abduzione, da guardare in gioco.
- Non fatto: giudizio in gioco (montage + hitstop + camera), per budget. Editor chiuso col kill (asset salvati dal build).
  Scratch in `/Game/AnimPipeline/Rig`: `AS_/LS_Jac1_Build`, `Jac2_Build`, `GanonAxisProbe_Build` (cancellabili).

Domande per l'owner:
1. Jab: il braccio di guardia scatta indietro col busto nel frame di contatto e il pugno gira. Lo tengo fermo al mento
   (la guardia segue il petto, cambio di posa al frame 1) o accetti lo scatto stile Mario?
2. Ganondorf: braccio dietro ora vicino al corpo nel recupero e braccio sinistro aperto allo scatto (griglia `_ab`): ok?

Costo stimato: Opus ~85 chiamate, contesto finale ~130k, circa $9-11; subagente Sonnet (A/B cieco) ~75k token, < $0,5.
