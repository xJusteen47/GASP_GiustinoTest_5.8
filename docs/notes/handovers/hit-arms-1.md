# Handover HIT-ARMS-1: braccia e mani della posa di colpo (fisso, _ue59, 2026-11-18)
Branch `claude/hit-arms-1` (base `claude/project-thread-98dml5` 5b22e56e). Opus + review Fable (subagente, 3 giri di posa). Lock _ue59 20:16-20:33.
## Fatto
- Nota di ricerca `docs/notes/hit-pose-arms-hands.md` (griglia Sakurai 9 tipi, trascrizione riassunta, 6 fotogrammi, regole pratiche).
- Libreria mani `Tools/GAS/hand_poses.py` + `/Game/GSTN/Animation/Hands/AS_GSTN_HandPoses` e `PA_GSTN_HandPoses` (Splayed, SplayedTense,
  Relaxed, Fist, Claw, Flat); doc `docs/notes/hand-poses.md`. Build: `Tools/GAS/build_hand_poses_and_hit_arms.py` (fa anche ArmsV1).
- Varianti HitStand (copie di ArmLow, originali intatti): ArmsV1 (4 key), ArmsV2 e ArmsV3 (`Tools/GAS/build_hit_arms_v2.py`,
  HITARMS_VERSION=V3): clip allungata a 0,9 s, picco oltre la tenuta fino al frame 4 (hitstop), assestamento ~0,2 s, deriva 5,5°.
- Set variante `DA_ReactionAnims_HitArms` (copia di DA_ReactionAnims_HitAdd, HitStand = ArmsV3). Il gioco NON lo usa: il BP non è toccato.
- Cattura ravvicinata: `Tools/GASNPPTest/react_close_boot_vd2.py` + `pie_capture_react_close.py` (190 cm, 640 px).
## Review Fable (`docs/notes/reviews/hitarms-1/r1..r3/REVIEW-fable.md`, griglie e mp4 accanto)
- r1 FAIL (braccio guida = gesto di "alt", braccio dietro sul fianco, posa congelata); r2 FAIL (mano guida sul fianco, posa "cowboy");
  r3: colpo frontale difendibile, V3 la migliore delle tre; colpo da sinistra FAIL "gioco": una posa sola non regge i colpi laterali.
- Parità Smash/AAA: no (Fable, r3). Video non guardati dal revisore: timing e blend con le additive non giudicati.
## Per l'owner (3 giri di posa esauriti, regola 10)
1. Approvare ArmsV3 come posa frontale (set HitArms nel BP)? 2. Pose per lato (specchio o variante laterale, codice) e/o 9 pose alto/medio/basso x S/M/L?
3. Ritocco gomito dietro 60->68° e +5° abduzione (un giro corto)?
## Coda codice ("gioco")
Scegliere o specchiare la posa HitStand per lato del colpo, così il braccio guida è quello dal lato dell'attaccante.
## Prove
Log build `Saved/Reaction/hit_arms.log`, `hit_arms_v2.log` (righe "frame N: ... hand from shoulder"); catture `Saved/AnimReact/hitarms_v1..v3(_close)/capture.log`
(righe `done:` con `'hit': True`); esiti `Saved/Logs/ci/hitarms_*.esito` (una cattura close v2 FAIL exit 3, ripetuta OK).
## Note
- La clip HitStand originale ha solo 4 key: la "vita" nei frame tenuti richiede una clip più lunga (regola HoldLastPose, sicuro).
- DA_ReactionAnims_HitArms caricato non si cancella headless: lo script lo riusa.
## Costo stimato: 18-25 $ (Opus ~140k contesto, 3 review Fable ~110k, 3 build, 7 catture).
