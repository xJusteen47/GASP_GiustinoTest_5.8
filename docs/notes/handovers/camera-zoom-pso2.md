# Handover camera-zoom-pso2 (cloud, Opus high, 2026-11-25)
Richiesta owner 25/11: "se non c'è un'opzione per regolare lo zoom della camera serve metterla; levare il fatto che si giri dietro di default (manda a quel paese la precisione); zoom base = Phantasy Star Online 2". Branch `claude/project-thread-dome4c` da `main` 152d25e6.

Cosa c'era: camera P1a (plugin GSTNCamera, attiva in gioco con `gstn.Camera.P1a 1`) con preset di distanza Near/Medium/Far (0.8/1/1.25) e `DistanceAction`, ma nessuna azione di input assegnata, e sul pawn di playtest (LookInputSource ControlRotation) i tasti del plugin non venivano proprio letti. Auto follow dietro al personaggio ACCESO di default (decisione 2A del 19/11, ora sostituita dall'owner).

Fatto:
- Zoom libero: `ZoomAction` nel profilo (asse 1D per frame come il mouse), `AddZoomInput`, `FGSTN_CameraMath::StepZoomScale` (moltiplicativo, range Custom 0.6-1.6, nessun ease mentre si zooma; i preset restano con ease 0.45 s). Cvar `gstn.Camera.Zoom` (scala, -1 = scelta del giocatore) visibile nel pannello GSTN Debug Options (gstn.Camera.* è già elencato).
- I tasti del plugin (recenter, distanza, zoom, assist) ora si leggono qualunque sia LookInputSource; solo il look dipende da esso.
- Distanza base 650 -> 580 cm: dagli screenshot PSO2 dell'owner (`riferimenti/camera-pso2/` nei project files) il personaggio occupa 25-35% dell'altezza schermo; con FOV orizzontale 85 a 16:9 un personaggio di 180 cm fa 30% a 580 cm (a 650 faceva 27%). Pitch e inquadratura (PSO2 tiene il personaggio più in basso, camera meno inclinata) NON toccati: da valutare a video.
- Auto follow spento di default: `FGSTN_CameraUserOptions::bAutoFollow = false`, cvar `gstn.Camera.AutoFollow` default 0 (1 lo riaccende). Lo switch designer del profilo resta acceso, quindi l'opzione del giocatore basta a riattivarlo.
- Input come dati: `Tools/GASNPPTest/camera_zoom_setup.py` crea `IA_GSTN_CameraZoom` (Axis1D) e `IMC_GSTN_Camera`: rotella (-0.12 per scatto, su = più vicino), d-pad su/giù tenuti (0.9/s con Scale by Delta Time). Il game module li assegna al profilo P1a solo se esistono (controllo una volta per sessione editor: riavviare l'editor dopo il primo run).
- Test: Core `ZoomStepIsMultiplicativeAndClamped` (nuovo), `DistancePresetsCycleAndClamp` (range Custom); Runtime `ZoomInputMovesTheDistanceDirectlyWithinItsRange` (nuovo), `AutoFollowTurnsBehindOnlyInASustainedRun` (prima verifica che di default non giri, poi lo accende).
- Nessuna parte GASP stock toccata (niente in gasp-divergenze.md); nessun Blueprint modificato.

Rischi: API Python 5.9 di InputModifierScalar/ScaleByDeltaTime e scrittura `default_key_mappings` (lo script stampa il controllo dei mapping); il d-pad potrebbe avere eventi tasto grezzi nel Blueprint GASP (come L3 il 16/11): se lo zoom col d-pad fa anche altro, va consumato come IA_GASNPP_RunBoost. Uno slot salvato `GSTNCamera_Options_0` con bAutoFollow=1 vincerebbe sul default (oggi nessuno chiama SaveUserOptions).

Desktop (owner a casa): build `Run-Timed.ps1 -Preset Build`, test `GSTNCamera.*`, nell'editor `import camera_zoom_setup as z; print(z.run())`, riavvio editor, PIE: rotella e d-pad zoomano, correndo a lungo la camera non si gira più da sola.

ESITO: PARZIALE codice, test e script scritti; non compilato e non provato (macchine offline)
SUBAGENTI: 1, lettura pagine web opzioni PSO2, Opus
COSTO: ~4 $ | 0 minuti desktop | 0 token review
