# skid-walkturn-3 (portatile, 2026-11-20, Opus medium) - PivotSet costruito, variante walk turn FAIL in review
Branch claude/project-thread-ucs9jl (da claude/skid-walkturn-2 1681355e), checkout GASP_Giustino2_n216qs.

## Fatto
1. Motore: UE 5.9 portatile ora CL 60738873 = fisso (Engine/Build/Build.version); rebuild 1176 s (Saved/Logs/ci/build_wt3.log);
   asset del fisso si aprono (pivot_rows_dump.py, Saved/Logs/ci/pivot_dump3.log).
2. Curve (richiesta owner): la clip pivot stock M_Relaxed_Run_Turn_L/R_180 ha root motion + ForceRootLock RefPose e curve contact_l/r,
   phase, movedata_speed, enable_warping, steeringtargettime; la nostra AS_GSTN_RunPivot_WalkTurn180 aveva solo contact/phase e niente
   root lock (Saved/Pivot/pivot_clips_dump.txt, pivot_curves_sample.txt). Tools/GAS/pivot/pivot_clip_setup.py scrive le tre curve dalla
   root (warping 0 nella rotazione, 1 da 0,64 s; steering = secondi alla fine svolta + 0,2, clamp 1) e il root lock. Assistant: Q1
   in docs/notes/assistant/2026-11-20_pivot3.md. NOTA regola 12: contraddice la scelta 11-19 "niente movedata_speed" (skid a5), che
   valeva per clip nostre a tempo nei nostri stati; qui è una riga del percorso stock.
3. PivotSet: Tools/GAS/pivot/pivotset_build.py (engine ChooserTableEditorLibrary.create_nested_chooser / copy_column / copy_rows).
   Riga 0 di Locomotion Transitions -> nested PivotSet (Gait + colonne di Run F Loco, pose match sul DB condiviso PSD Locomotion
   Transitions come lo stock), righe Run L/R. Verifica pivotset_verify.py. Divergenza 11 in gasp-divergenze.md. Riga ora DISABILITATA.
4. Cattura (base 1, gstn.Skid.Sim 0, sequenze mv_pivot / mv_pivot_base in pie_capture_move.py): la riga entra all'inversione (f117),
   peso >= 0,8 f130-f136, poi GASP ripesca M_Relaxed_Run_Reface_Start_090_R: si vedono ~0,3 s di clip (cap_var1 frames.jsonl;
   ASSERT OK rivalutato con capture_assert.py sulla stessa cattura, finestra iniziale f135-150 sbagliata). Base: Turn_L_180 stock fino
   alla corsa, ASSERT OK.
5. Review Fable r1 (docs/notes/reviews/pivot-walkturn-v1/): FAIL. Posa: si raddrizza a piedi uniti f120-124, anche alte, pausa in
   piedi f124-138; dati: pop in entrata e al passaggio f138-142; gira solo a destra (manca mirror). Gioco: base e variante ~0,55 s
   dall'inversione alla corsa contro ~0,3 s di Toad: lo decide il freno/riaccelerazione della capsula Mover stock, non la clip.
6. Toad (riferimenti/corsa-partenza-skid/mario3dw-toad-start-skid-owner-1117-small.mp4): skid ~0,3 s a occhio (polvere 8-12 frame a 30 fps, +-0,1 s).

## Prossimo (in ordine, dalla review)
1. Thread di CODICE (voce "gioco"): tempo di inversione della capsula Mover a 0,3 s (freno + riaccelerazione in inversione), misurato
   da frames.jsonl (loc). Chiedere all'Assistant come lo fa GASP stock prima (regola 14).
2. Sorgente pivot di CORSA (o posa di frenata: busto indietro, anche basse) invece della walk turn; mirror per il lato sinistro.
3. Sprint: copia della skid attuale con pivot_clip_setup.py, riga Sprint (PIVOT_SPRINT_CLIP). Colonna tag kit/skin ancora da fare.
Nessun video all'owner (solo PASS, regola 10). Mp4 base/variante nella cartella review.

COSTO: ~14 $ | 0 min desktop | ~85k token review
