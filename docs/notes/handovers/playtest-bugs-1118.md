# Playtest bugs 2026-11-18 (fisso, Opus medium, _ue59) — handover
Base 3eecfcea + merge VFX v2 (aait2t, pulito, compilato). Diagnosi a righe per frame, niente video: `Tools/GASNPPTest/diag_playtest_1118.py`
(Run-Timed Capture, ~70 s; righe in Saved/Diag1118/{hold,dropkick,stomp}.jsonl; input scriptati, NON il pad reale).

1. **Crouch tenuto aria<->terra**: la simulazione è giusta. Righe hold.jsonl: crouch a terra f21, salto f90 -> tag AirCrouch f93, atterraggio f113 ->
   tag Crouch e capsula 55 a f115 (2 frame). Quindi il difetto che vede l'owner è di PRESENTAZIONE (ABP GASP: land/jump in piedi fra air crouch
   montage e crouch locomotion) o del pad reale (LT analogico). Da verificare con video + pad. Air crouch: oggi NON cambia la capsula (solo tag/anim).
2. **Dodge tenuto**: NON riprodotto. Dodge tenuto 3 s, con e senza stick: una sola richiesta (req 1438 e 1676), EndLag 8 frame, nessuna seconda.
   Serve sapere dall'owner: stick tenuto? quale tasto (shield/dodge condivisi?) e se succede con il pad. Possibile causa lato input reale.
3. **Camera**: il GSTN_CameraAnchorComponent c'è ma in MoveCameraComponent NON ferma il crouch: camera -30 cm (cz 202 -> 172) quando la capsula
   passa 86 -> 55. Il rig GASP non legge la posizione del GameplayCameraComponent. Tentativo 1: Mode ShiftView -> a regime la camera resta ferma
   (202 -> 203) ma c'è un sobbalzo +30 cm per ~0,5 s (la DampenPosition di GASP rincorre la root mentre noi correggiamo subito). DA rimesso com'era.
   Il GASP rig alza anche la camera in aria (salto). Serve decisione: correggere dentro il rig GASP (asset camera) o passare alla camera nostra (piano Astro C1).
4. **VFX v2**: unito e compilato. Da vedere in playtest.
5. **Root motion**: tabella `playtest-1118-root-audit.tsv` (comando: audit_root_motion_1118.py via UnrealEditor-Cmd -run=pythonscript).
   Ported57/Skid/SS_180_Turn: root yaw 0 (la root non gira: sbagliata). SS_180_Turn1/Turn2 e SkidOwner/SS_180_Turn2_Src: root 47.8 cm, yaw 180 (ok,
   EnableRootMotion off nei Ported57). SkidOwner/SS_180_Turn2 (RTG riapplicato): yaw -188 e bacino che deriva 2.7 cm (doppia correzione).
   Skid_Revised: root 98 cm, yaw 0 (è uno skid, ok). WallSlideIdle: in place. Nuova richiesta owner (07:36Z) su RTG script: NON fatta qui.
6. **Atterraggio stomp**: difetto del GIOCO trovato e corretto: a terra i passi Body-anchor fra le tracce capsula del dair facevano crescere la
   capsula dentro il pavimento (fondo -27 cm), il pawn restava affondato 28 cm per sempre (mz -28). Fix `FGSTN_BodyCapsule::KeepBottomOnFloor`
   (a terra il fondo della capsula resta fermo, in aria nulla cambia), chiamato dal modifier. Tentativo 1 (solo verso il basso) lasciava 10 cm
   sopra il pavimento; tentativo 2 ok: stomp.jsonl fondo 2 cm costante f69-f259, mesh finale 0. Resta il salto della mesh f69-77 (-80 -> 0 cm):
   dipende da clip + offset della traccia capsula -> thread animazione (clip landing con le regole nuove).
   Dropkick: la discesa è la caduta (vz -616 -> -1500 terminale), non la capsula (mesh continua al resize f62).
Test: `GSTNCombat.Core+GSTN.Camera` 248/248 (Saved/Logs/ci/test_1118.log.ue.log, conta "Result={Success}").
AM_OverlordDAirV9.uasset modificata dall'owner: non committata, non toccata.
**Seguito (stesso thread, decisioni coordinatore 1A 2A):**
- Camera: la causa del -30 cm col crouch era GASP: CameraRig_CrouchOffset (OffsetCameraNode TranslationOffset X40 Z-30), l'ancora già compensava la
  capsula. Messo a 0 (Saved/Diag1118/crouchoffset.txt prima/dopo). hold.jsonl: crouch f21 -> cz 202 costante (dip 8 cm per ~5 frame). Vale anche in ADS
  (l'offset è un rig unico): da separare se l'owner lo vuole in mira. Nuovo helper UGSTN_PropertyTextLibrary (proprietà non visibili a Python).
- Dodge tenuto + stick: GSTN_InputSimulator ora non conta come pressione un tap di un tasto già giù e ancora giù (Started di Enhanced Input
  rilanciato su tasto tenuto). Verifica dodge.jsonl: tasto+stick tenuti, tap spurio a f110 -> nessuna seconda richiesta (req 1036 unica). Col pad
  reale non verificato. Il comando viene consumato (Resolution.ConsumeFrom): il secondo dodge veniva da una pressione nuova finta.
- Test GSTNInput+GSTNCombat.Core+GSTN.Camera 387/387 (Saved/Logs/ci/test_1118b.log.ue.log). Skid/RTG: thread nuovo (2A).
Costo stimato ~18 $ (oltre i 15: camera e dodge aggiunti dopo).
