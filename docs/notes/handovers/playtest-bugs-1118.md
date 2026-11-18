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

**Seguito 2 (09:30-09:42):** ADS: `FGSTN_CameraAnchorSettings::bCrouchOffsetInAim` (on) + `AimCrouchOffset` (40,0,-30) + `AimCrouchHalfLife` 0.1 s;
letto da PlayerInputState.WantsToAim di GASP. NON verificato in PIE: Run-Timed ESITO BLOCCATO (diag1118i.log), editor fermo sul dialogo di
ripristino pacchetti (Saved/Autosaves/PackageRestoreData.json, non toccato). Merge vbnr9g (air momentum): rinominato `Dt` -> `AirMomentumDt` nel
test (C4459 in unity build). Test GSTNInput+GSTNCombat+GSTN.Camera 842/842, AirMomentum 5/5 (Saved/Logs/ci/test_1118c.log.ue.log).
Profili: tutti i 36 KIT_Test_* hanno DA_Locomotion_Default; i 4 KIT_Example* no (Saved/Diag1118/kits_loco.txt). Dummy non controllato.
Da fare: click owner sul dialogo, poi `DIAG_ONLY_AIM=1` Run-Timed Capture diag_playtest_1118.py e video ADS crouch.

**Seguito 3 (09:58):** merge drift DI (vbnr9g 565f8259; di nuovo `Dt` -> `AirMomentumDt`). Trovato: `ProduceInput` azzerava il MoveInput
della vittima con Lock.Reaction/Lock.Movement (bRooted) -> la drift non avrebbe mai visto lo stick. Ora in aria (Mover.IsInAir) il MoveInput resta,
la facing resta tenuta; a terra/downed invariato. Test 843/843, AirMomentum 6/6 (test_1118d.log.ue.log). PIE non verificato (dialogo restore).

**Seguito 4 (10:19):** merge vbnr9g dba37f89 + 642fbe12, compila. Test 846 ok, 1 FAIL: GSTNCombat.Core.JumpDesigner.ClosedForm (apex 0.485/0.483 s
vs 0.5 +-0.005; altezza ok) in test_1118e.log.ue.log. Non corretto qui (codice del thread vbnr9g). Gravità: L_Test_Hub senza override (global_gravity_set False),
PhysicsSettings default -980 -> WorldGravity 980 giusto (Saved/Diag1118/gravity.txt). Jump Designer sul kit rosa NON acceso: KIT_Test_Playtest usa
DA_Locomotion_Default condiviso da tutti i 36 kit (regola 12): serve decidere se accenderlo per tutti o fare un DA_Locomotion_Playtest.

**Seguito 5 (10:25):** merge 46224f43 (test con TimeToApex 0.8). Ancora FAIL, e peggio: modello 0 altezza 309.9 / apice 0.572 s, modello 1
304.5 / 0.541 s (attesi 350 / 0.8) -> non è condizionamento: closed form e FGSTN_JumpLogic::SimulateApex non concordano (test_1118f.log.ue.log).
Calcolo a mano del closed form (K .29, T .48, A .32): Fall 1469, V0 675, v a fine finestra 470 -> serve guardare cosa fa la logica dopo la finestra.
