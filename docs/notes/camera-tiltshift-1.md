# Camera: tilt-shift "giocattolo", camera più vicina, studio Mario Odyssey / Astro Bot (camera-tiltshift-1)
Thread cloud, Opus medium, 2026-11-18. Richiesta owner 11:58Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNAwCQkvosBvcfniVSPjqiKd). Ambito: tutte le kit (un solo DA camera, DA_CameraAnchor_Default). Codice NON compilato: build e test sul fisso.
**[F]** fonte · **[A]** analisi/inferenza mia, da verificare su video.

## 1. Come funzionano le camere di Mario / Astro Bot (asse verticale)
- **Non seguono il salto.** Durante il salto la camera resta all'altezza del terreno da cui sei partito; si riallinea quando atterri su un'altra quota. È il "platform snapping" dei platform 2D (Super Mario World) [F2, F3], portato in 3D: Galaxy lo scrive nei dati di ogni camera con soglie `uplay` (quanto sali prima che la camera salga, default 300) e `lplay` (quanto scendi prima che scenda, default 800), ritardi `pushdelay`/`pushdelaylow` (120 frame) e `camint` (frame per arrivare in posizione) [F1]. Odyssey e Astro Bot si comportano allo stesso modo a occhio [A].
- **Cadute: segue giù** per farti vedere dove atterri (quando scendi sotto la quota di partenza) [A, coerente con F1 lplay].
- **Riallineamento all'atterraggio morbido** (~0,2-0,4 s), non uno scatto [A].
- **Anticipo (look-ahead)** leggero nella direzione di corsa, lento a girare [A]; FOV stretto (Galaxy 45° [F1]) e pitch 20-30° in giù [A, già in camera-astrobot-piano.md].
- **Effetto giocattolo (tilt-shift):** è la profondità di campo cortissima di un obiettivo macro: solo una fetta sottile attorno al personaggio è nitida, il terreno vicino (basso schermo) e lo sfondo (alto schermo) sfocano. Con camera alta e inclinata in giù, profondità e altezza sullo schermo coincidono, quindi un DOF fisico focalizzato sul personaggio dà proprio la banda del tilt-shift [A; tecnica standard, F4]. Team Asobi non ha pubblicato parametri: nessuna fonte primaria trovata.

## 2. Confronto con la nostra camera (anchor, camera-anchor-1)
| Cosa | Mario/Astro | Noi prima | Ora |
|---|---|---|---|
| Salto normale | camera ferma | seguiva il salto (molla 0,07 s) | **ferma** (AirRiseBand 250 cm) |
| Salto altissimo / arrampicata | sale per l'eccedenza | seguiva | segue oltre i 250 cm |
| Caduta sotto la quota | segue giù | seguiva | segue (AirFallBand 0) |
| Atterraggio su altra quota | riallinea morbido | 0,07 s | **LandingHalfLife 0,2 s** |
| Capsula (stomp, knockdown) | n/d | anchor dal mesh | invariato |
| Look-ahead | leggero | no | **c'è, spento** (LookAheadTime 0; prova 0,25) |
| Distanza | vicina, FOV stretto | +550 cm | **+400 cm**, ora dato nel DA |
| Tilt-shift | sì | no | **sì**, on/off + intensità |

## 3. Cosa è stato fatto (codice, branch claude/project-thread-pg3fen)
- `FGSTN_CameraAnchorSettings` (DA_CameraAnchor_Default): nuovi gruppi **Air** (bHoldGroundHeightInAir, AirRiseBand, AirFallBand, LandingHalfLife), **Horizontal** look-ahead (LookAheadTime, MaxLookAhead, LookAheadHalfLife), **Framing** (ExtraDistance 400, ExtraHeight 45: prima 550/60 nel DefaultEngine.ini, righe tolte), **TiltShift**.
- Tilt-shift (`FGSTN_CameraTiltShiftSettings`, applicato da UGSTN_PlaytestCameraModifier): **CinematicDOF** (default) = fuoco sulla distanza camera-personaggio, Fstop 2, sensore virtuale 300 mm (il trucco: sensore grande = obiettivo lungo = fuoco sottile); **ScreenBand** (economico, per toaster) = materiale post-process a banda centrata sull'altezza a schermo del personaggio, 12 letture per pixel sfocato, creato da `Tools/GASNPPTest/camera_tiltshift_setup.py`. Senza materiale ricade sul DOF. Nota: con scalabilità Low il motore spegne il DOF, lì serve ScreenBand.
- Console per A/B: `gstn.Camera.TiltShift` (-1 dati, 0 off, 1 DOF, 2 banda), `gstn.Camera.TiltShift.Intensity`, `gstn.Camera.PlaytestExtraDistance/Height` (-1 = dati).
- Test nuovi: GSTN.Camera.Anchor.AirHoldKeepsGroundHeight, .LandingOnHigherGroundCatchesUpSoftly, GSTN.Camera.TiltShift.Math.

## 4. Valori da toccare (DA_CameraAnchor_Default)
Distanza: Framing > ExtraDistance (400; più basso = più vicino). Toy look: TiltShift > Intensity (1; 0 = spento), SensorWidth (300 mm; 150 = più leggero, 500 = molto miniatura), Fstop. Salto: Air > AirRiseBand (250 cm; il salto designer è 350 cm, quindi l'apice sposta la camera di 100 cm; metti 350 per camera del tutto ferma), LandingHalfLife.

## 5. Proposte (non fatte)
1. FATTO (13:45Z): Framing > TelephotoFOV (0 = spento). FOV più stretto e camera arretrata della quantità che tiene il personaggio della stessa grandezza a schermo (tan(FOV/2)); contro un muro il FOV si riapre. Prova 55, poi 45. Globale nel DA; per kit/personaggio con un componente anchor proprio.
2. Ritardo prima del riallineamento in salita (pushdelay di Galaxy) se il riallineamento all'atterraggio risulta ancora nervoso.
3. Pareti/arrampicata: oggi contano come "in aria"; se la camera resta troppo in basso sui muri, escludere le modalità muro dall'hold.
4. Dissolvenza a retino degli ostacoli fra camera e personaggio (Odyssey) e modalità a volumi: già nel piano camera-astrobot (C1-C2).

## 6. Ricerca: io vs Gemini (risposta raccolta dall'owner, 13:42Z)
Gemini: nessuna fonte citata, ricetta generica (DOF + banda + luci) già coperta qui; numeri non verificati e per il gameplay estremi (FOV 20-35°, pitch 35-55°); non tocca la domanda sul comportamento verticale della camera. Punto utile: l'**ottica** (FOV stretto con camera più lontana) come metà dell'effetto miniatura, che qui era solo la proposta 1 ed è ora implementata (TelephotoFOV). Io: fonti per i parametri (Galaxy, platform snapping), risultato già in codice, ma ho sopravvalutato il blur (corretto in §7).
**Verdetto:** su questa domanda la ricerca propria ha reso di più per precisione e fonti; Gemini è utile come checklist veloce o seconda opinione, non come fonte di numeri. Per le prossime: Gemini per una lista ampia di idee, poi verifica con fonti.

## Fonti
F1 lumasworkshop.com/wiki/Cameras (parametri camere Super Mario Galaxy) · F2 fukuchi.org/en/works/jumplab/settings-camera-platformSnapping.html · F3 gamedeveloper.com "Scroll Back: the theory and practice of cameras in side-scrollers" (Itay Keren) · F4 dev.epicgames.com "Cinematic Depth of Field" / react-postprocessing tilt-shift (tecnica banda a schermo)

## 7. Correzione 2026-11-18 13:38Z (owner: "blur decisamente esagerato")
Default molto più leggeri: l'effetto deve restare sottile, appena percepibile ai bordi (Astro Bot). Il blur cresce circa come SensorWidth / Fstop: 300/2 = 150 → 120/4 = 30, cioè 1/5.
DOF: Fstop 2 → 4, SensorWidth 300 → 120 mm. Banda: BandMaxRadius 8 → 3 px, BandHalfHeight 0,12 → 0,25, BandFeather 0,3 → 0,35. Intensity resta 1.
Il DA_CameraAnchor_Default sul fisso ha già i valori vecchi salvati: vanno scritti a mano nel DA (le nuove default C++ valgono solo per asset nuovi). Anteprima subito senza build: console `gstn.Camera.TiltShift.Intensity 0.2` (stesso effetto dei nuovi default).

## 8. Tilt e shift sono due cose diverse (owner 14:17Z)
Un obiettivo tilt-shift ha due movimenti indipendenti [F5, F6]:
- **Tilt** (basculaggio): la lente si inclina rispetto al sensore. Il piano di fuoco non è più parallelo al sensore ma ruota (regola di Scheimpflug: piano del sensore, piano della lente e piano di fuoco si incontrano su una retta). Cambia SOLO cosa è a fuoco. Tilt "inverso" (piano più ripido del terreno) = fetta nitida sottilissima = effetto miniatura. Tilt "verso il terreno" = tutto il pavimento a fuoco (uso architettonico).
- **Shift** (decentramento): la lente scorre parallela al sensore. Cambia SOLO l'inquadratura: proiezione decentrata (off-axis). Uso classico: camera in bolla (verticali parallele, niente edifici che "cadono") e shift verso il basso per riprendere il terreno. Nessun effetto su fuoco o sfocatura.
Il "tilt-shift miniatura" delle foto è quasi sempre solo **tilt** (o la sua imitazione a banda sullo schermo, F7).

**Astro Bot usa lo shift?** Misura sui riferimenti (`docs/notes/reviews/ref-astrobot-camera/`, `vp_vertical_edges.py`): nella giungla (astro_ref_1) gli spigoli della torre convergono verso il basso (pendenze +0,05 / −0,08, fuga verticale a y ≈ 3500 su 786 px di altezza); nel voxel la fuga verticale è a y ≈ 1700. Le verticali convergono, come in una camera inclinata normale: **nessun segno di shift che raddrizza le verticali**. Uno shift piccolo non si può escludere (servirebbe un terzo punto di fuoco finito per trovare il centro ottico; qui la terza direzione è parallela allo schermo). Nessuna fonte di Team Asobi. Quindi: shift disponibile ma **spento** di default.

**Fatto (codice, da compilare):**
- Tilt (DA > "Tilt (focus)", nome C++ invariato `TiltShift`): nuovo modo **TiltedPlane** = tilt vero. Piano di fuoco per il personaggio ruotato di `TiltAngle` (default −20°, tilt inverso) attorno all'asse destro della camera; blur = distanza del pixel dal piano / distanza dalla camera × `PlaneBlurScale` (12), max `PlaneMaxRadius` (4 px). Materiale M_GSTN_TiltPlane dallo stesso script (posizione relativa alla camera, 12 letture). Gli altri modi restano: CinematicDOF (= tilt 0, piano parallelo al sensore), ScreenBand (imitazione a schermo).
- **Shift** (DA > Shift, indipendente): `bEnabled` (spento), `StraightenVerticals` 0..1 (porta la camera in bolla e compensa con lo shift: il personaggio resta nello stesso punto dello schermo, i muri smettono di pendere), `Offset` manuale (mezzi schermi). Implementato con `FMinimalViewInfo::OffCenterProjectionOffset` nel modifier, nessun asset.
- Console: `gstn.Camera.TiltShift 3` (piano inclinato), `gstn.Camera.Shift 0/1`.
- Test: GSTN.Camera.Shift.StraightenKeepsCharacterOnScreen, GSTN.Camera.TiltShift.TiltedPlaneNormal.
Rischi: se il render ignorasse l'offset off-axis passato dal modifier, lo shift non si vede (si verifica al primo PIE con `gstn.Camera.Shift 1`); la posizione mondo nel materiale post-process va verificata in compilazione (2 tentativi, poi resta il DOF).

Fonti aggiunte: F5 cambridgeincolour.com "Tilt shift lenses" (parte 1 shift/prospettiva, parte 2 tilt/profondità di campo) · F6 Scheimpflug principle · F7 SIGGRAPH 2013 poster, Nakane, "Simulating Depth of Field Effects Taken by a Camera with a Tilt Shift Lens"

## 9. Altri riferimenti (owner 14:23Z, astro_10..13 ritagliati)
astro_12 è il migliore (pavimento a piastrelle, porta e colonna verticali): pitch ≈ 15,5°, FOV orizzontale ≈ 86° (79-95° se ci fosse uno shift non misurabile); fuga verticale coerente con una camera inclinata semplice. Conferma: grandangolo, niente shift visibile, tilt/sfocatura quasi assenti. Dettagli e comandi in docs/notes/reviews/ref-astrobot-camera/README.md.
