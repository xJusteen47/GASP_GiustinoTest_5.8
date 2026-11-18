# Handover camera-tiltshift-1 (cloud, Opus medium, 2026-11-18)
Branch claude/project-thread-pg3fen da claude/project-thread-98dml5 (ae6e00be). Codice NON compilato. Note complete: docs/notes/camera-tiltshift-1.md.
Fatto: tilt-shift (Cinematic DOF default + banda economica), camera più vicina (+400 invece di +550, ora dato nel DA), air hold stile Mario (la camera non segue il salto), riallineamento morbido all'atterraggio, look-ahead (spento). Tutto in FGSTN_CameraAnchorSettings = DA_CameraAnchor_Default, tutte le kit.
File: Public/Private Camera/GSTN_CameraAnchor.*, GASNPPTest/GSTN_PlaytestCamera.*, Camera/Tests/GSTN_CameraAnchorTests.cpp (+3 test), Config/DefaultEngine.ini (tolte righe ExtraDistance/Height), Tools/GASNPPTest/camera_tiltshift_setup.py (nuovo).
Per il fisso (Haiku/Sonnet, ~3 $):
1. Merge in _ue59, build (Run-Timed, 45 min).
2. Test GSTN.Camera.* (3 nuovi + 8 esistenti).
3. Lancia camera_tiltshift_setup.py (crea /Game/Blueprints/Camera/M_GSTN_TiltShift); se il nodo Custom non compila: 2 tentativi max, poi resta il DOF (il codice ricade da solo solo se il materiale manca: in caso di errore cancella l'asset).
4. Cattura PIE su L_Test_Hub: salto sul posto, salto su gradino, corsa; gstn.Camera.TiltShift 0/1/2 (stessi frame) per la review.
Decisioni applicate (regola 6, da confermare): ExtraDistance 400 / ExtraHeight 45; AirRiseBand 250 (l'apice del salto 350 cm sposta la camera di 100 cm); DOF Fstop 2, sensore 300 mm; look-ahead spento.
Rischi: AddCachedPPBlend dal modifier (se il DOF non si vede, controllare che GASP non sovrascriva il DOF nel suo rig); la scalabilità Low spegne il DOF (usare la banda); arrampicata conta come "in aria".
Costo stimato thread: ~6 $.
Aggiornamento 14:30Z: tilt e shift separati (note §8). Nuovi: modo TiltedPlane (M_GSTN_TiltPlane dallo stesso script), struct Shift (off-axis, spento), +2 test. Per il fisso, in più: `gstn.Camera.Shift 1` e `gstn.Camera.TiltShift 3` nella cattura A/B; Fable rivede prima dell'owner. Valori leggeri da scrivere nel DA: Fstop 4, SensorWidth 120.
