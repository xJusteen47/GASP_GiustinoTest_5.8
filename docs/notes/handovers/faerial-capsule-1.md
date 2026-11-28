# F.Aerial greybox e tuffo della schivata (cloud + fisso, Opus, 2026-11-28). Branch `claude/project-thread-aycsgv` da `main` 82dfd88d
Owner 00:02Z: "Il F.Aerial ha l'indicatore dei piedi ancora avanti, e quando la capsula va a terra resta distesa dove era l'indicatore". 00:03Z: "il tuffo
non mantiene il momentum scivolando ... la capsula resta distesa a mezz'aria". Kit KIT_Test_Playtest, F.Aerial = dropkick (knockdown all'atterraggio, voluto dal 11-17).

## Diagnosi (cattura su _ue59, main, frame per frame: Saved/FAerialDiag)
- F.Aerial: la collisione (Body Capsule, ancora Body) e l'indicatore stanno sul corpo vero, ~65 cm avanti alla radice. La clip capsula del dropkick
  (pose chiave, make_capsule_clips.py) ha il bacino vicino alla radice: la capsula era disegnata dietro l'indicatore; all'atterraggio la radice va sotto
  la collisione (GroundedRestore, giusto per il manichino) e la capsula scattava avanti di 65 cm. Disteso a terra il bacino greybox restava a z ~92.
- Tuffo: rotazione attorno al centro della capsula (85 cm), quindi sulla pancia ~50 cm sopra il pavimento. Scivolata: LandingSlide 4000 cm/s^2, 16 frame, 131 cm.
- Primo tentativo scartato prima della build: cambiare GroundedRestore per tenere la radice ferma in orizzontale (avrebbe rotto il manichino vero).

## Fatto
1. `FGSTN_GreyboxShape::OverridePelvisFollow` / `ShapeSettings.bMoveClipFollowsDriverPelvis` (default on): la clip capsula mette il bacino sopra quello
   della guida in X/Y (altezza e rotazioni restano della clip).
2. `FloorRest` (tuffo: quanto è disteso, |sin| dell'angolo) e `bFloorRestByTilt` (clip capsula a terra: 0 fino a 30° di inclinazione, 1 da 70°):
   la capsula scende sul pavimento oltre al floor lift.
3. `FGSTN_DodgeLandingKitExtension::LandingSlide.Along.Deceleration` 4000 -> 1500.
Test: Greybox.Dive (FloorRest), GSTNAnimation Greybox.SquashScale (TiltFloorRest), DodgeLanding.BellyUntilStopped aggiornato.

## Prove sul fisso (_ue59, e57b487d)
Build OK, GSTNCombat+GSTNAnimation 760/760. Catture Saved/FAerialFix2 (video in Saved/FAerialFix2/video e _scratch/faerial):
- F.Aerial: bacino greybox = bacino guida in X/Y ogni frame, nessun salto orizzontale all'atterraggio (prima +65 cm in un frame); disteso a terra z 30-38.
- Tuffo: scivolata 39-41 frame, 335 cm; capsula sulla pancia z ~34 (appoggiata).
## Aperto (già presenti prima, non toccati)
- F.Aerial: a v157 la capsula sale 28 cm e a v158 scende a terra in un frame (salto del montage alla sezione land); a fine montage (v185) il bacino
  scende 28 cm per un frame e risale. In aria la capsula sta ancora 25-35 cm dietro l'indicatore nei primi frame dopo l'accorciamento (è il bacino
  della guida rispetto al centro del torso su cui è fittata la collisione).

ESITO: PASS indicatore/capsula allineati, nessun salto all'atterraggio, disteso e tuffo appoggiati a terra, scivolata 335 cm; 2 scatti verticali di un frame aperti
SUBAGENTI: 0
COSTO: ~12 $ | ~60 min desktop | 0
