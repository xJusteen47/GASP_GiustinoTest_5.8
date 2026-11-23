# Camera: altezza dal pavimento, mai dalla capsula (2026-11-23, cloud, Opus medium)
Branch `claude/project-thread-t1yn9x` da `claude/integ-2026-11-23` (e0fcf520). NON compilato, NON provato in editor.
Riuso: `UGSTN_CameraAnchorComponent` (camera-anchor.md), sorgente altezza esistente (`EGSTN_CameraVerticalSource`), camera P1a.
## Causa (lettura codice; numeri dal thread dropkick, Saved/DropkickK6/k6r2hit/dropkick_sweet_hit/frames.jsonl sul portatile, NON riletti qui)
1. La fix 11-20 (CapsuleBottom) usava la base della capsula A TERRA e la radice della mesh IN ARIA. Dedotto (da verificare con la trace): nel loop la radice della
   mesh sta ~55 cm sotto dove starebbe in piedi: in aria la camera la seguiva in giù (sotto la quota di stacco), all'atterraggio la capsula da 35 cm sul torso
   tocca terra e la sorgente passa a base capsula + 86: +57 cm in un frame. Una capsula alzata sul torso mentre Mover dice ancora "a terra" (a3) idem.
2. La camera P1a (quella in gioco, `gstn.Camera.P1a 1`) riceveva il pivot dal subsystem tickable, che gira DOPO la vista: un frame di ritardo
   (aperto dal 11-21). E non aveva l'ancora come prerequisito di tick.
## Fix
- Nuova sorgente `Floor` (default): a terra pavimento sotto la capsula (linea dal centro, canale e risposte della capsula) + mezza altezza in
  piedi; in aria il corpo disegnato, MAI sotto pavimento + mezza altezza. Nessuna capsula la tocca. Dati: `FloorProbeDistance` (300 cm),
  `bFloorClampInAir` (on). Cvar A/B `gstn.Camera.Anchor.Vertical 3/1/2` (floor / base capsula 11-20 / mesh).
- P1a: pivot consegnato da `OnAnchorUpdated` alla fine del tick dell'ancora (stesso frame); ogni camera del pawn ticka dopo l'ancora.
- Dropkick: `setup_dropkick_body_capsule.py` ha `DROPKICK_CAPSULE_AIR_PIN=torso|feet|head` (default torso, invariato). Regola 12: "feet"
  riporta l'atterraggio anticipato che il torso (owner 11-15) aveva tolto; con la camera sul pavimento la scelta è solo di collisione.
- Owner 19:14Z (chiarimento): la capsula si restringe VERSO L'ALTO, cioè sparisce il volume sotto i piedi e il bordo alto resta dov'è. Il default
  di AIR_PIN ora è `head`. Lo script scrive la distanza tra il bordo basso e il torso del loop (WARNING sopra 15 cm = calo o atterraggio anticipato).
  `DROPKICK_TIMELINE` serve a puntare su TL_Overlord_DropkickTBK6 (K6). Un bordo basso che sale non spinge mai contro il pavimento: nessun controllo di collisione in più.
- Test: `GSTN.Camera.Anchor.FloorSourceIgnoresEveryCapsule`, `.FloorIsTheDefault`. Doc: camera-anchor.md.
## Da fare in editor (fisso, dopo build)
1. Build + `GSTN.Camera.*` (atteso tutto verde). 2. `DA_CameraAnchor_Default`: Vertical Source deve leggere Floor (se salvato a mano su
CapsuleBottom, rimetterlo). 3. Cattura A/B dropkick sweet hit + whiff, camera di gioco, `gstn.Camera.Anchor.Vertical 1` vs `3`, con
`gstn.Camera.Trace 1` + `camera_trace_check.py`: niente salto all'atterraggio, niente discesa in aria. 4. Giro rapido: crouch spam, FAir/dair,
knockdown, salto su piattaforma, caduta da bordo, scale. 5. Rilanciare lo script (default head, DROPKICK_TIMELINE della K6), leggere la riga gap e catturare l'atterraggio.
INDICE-PROGETTO (branch indice-progetto) §5.9: aggiungere "altezza camera dal pavimento (Floor), P1a pivot stesso frame".
SUBAGENTI: 1, critica animazione dropkick K6 (griglie k6r2b), Opus
COSTO: ~5 $ | 0 min desktop | ~60k token review
