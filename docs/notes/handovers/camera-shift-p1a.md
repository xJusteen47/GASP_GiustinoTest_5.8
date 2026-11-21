# Handover camera-shift-p1a (fisso, Opus medium, 2026-11-21)
Branch claude/runstart-generic-desktop (su eb2dd72f). Editor chiuso dall'owner 17:57Z, riaperto a fine thread. Lock _ue59 preso e liberato.
1. Tilt-shift senza blur sulla camera P1a. Merge di claude/project-thread-pg3fen (c0285047). Lo Shift (proiezione off-axis) lavora sulla vista P1a:
   porta in bolla una parte del pitch (StraightenVerticals 0,5) e sposta l'obiettivo, così il personaggio resta nello stesso punto dello schermo.
   Il blur (tilt) è SPENTO di default (gstn.Camera.TiltShift default 0, C++ bEnabled false). Telephoto saltato sotto P1a (distanza e FOV sono del
   profilo P1a); air hold ed ExtraDistance dell'anchor erano già esclusi da P1a. Niente duplicati.
   Controlli in GSTN Debug Options (lista automatica gstn.Camera.*): gstn.Camera.Shift (-1 DA, 0, 1), gstn.Camera.Shift.Straighten (0..1),
   gstn.Camera.TiltShift (0 default), gstn.Camera.FallPitch, gstn.Camera.FallPitchBlendSeconds.
   Prova: rows.csv pitch -18,00 (off) vs -9,00 (on) agli stessi frame (data/move_off|move_on/rows.csv, frame 20/100/240/340). Lo shift si vede
   nel viewport vero (HighResShot): il rischio "render ignora l'offset" è chiuso. Le SceneCapture non lo vedono: per questo lo script nuovo.
2. Landing del dropkick (owner 17:57Z). Causa: il pitch extra in caduta di P1a (FallPitchExtraDeg 10, proporzionale alla velocità) seguiva il
   tuffo del dropkick e tornava a 0 nel frame dell'atterraggio: scalino di 9,5° = camera giù di 112 cm in un frame, poi il saltello di rimbalzo
   la rialzava. Fix nel plugin GSTNCamera (Free mode): il pitch in caduta solo per una caduta vera (sotto l'ultimo terreno) e con entrata/uscita
   morbida FallPitchBlendSeconds 0,3 s (profilo). Numeri (Tools/GASNPPTest/camera_bob_check.py dk_base2 dk_fix, data/bob_check.txt):
   prima passo max 112,3 cm/frame, pitch 9,48°/frame, 3 inversioni; dopo 6,4 cm/frame, 0,44°/frame, 1 inversione.
   Resta: la camera sale durante il salto alto del dropkick (apice 300 cm sopra, oltre JumpDeadZoneUpCm 250): è il design P1a.
Build: Run-Timed Build OK (2 volte). Test GSTN.Camera+GSTNCamera 82/82 PASS (Saved/Logs/ci/test_camshift2.log, Check-TestLog.ps1).
Catture: Tools/GASNPPTest/camera_shift_timed.ps1 (+ camera_shift_capture.py, SHIFTCAP_PLAN move|dropkick): viewport vero, frame di gioco
(in background il mondo PIE avanza ogni ~7 tick editor, lo script conta il tempo di gioco). File in docs/notes/reviews/camera-shift/.
Da confermare (owner): shift acceso o spento di default (ora spento, come da misura su Astro Bot); valore Straighten 0,5.
COSTO: ~6 $ | 35 minuti desktop | 0 token review
