# pivot-overlord-1 (portatile, 2026-11-21, Opus medium) - pivot di corsa Overlord ritimato a 0,3 s + mirror, review PASS (tempi/dati)
Branch claude/project-thread-wc8ksn (da claude/project-thread-k5vbaf 126a8a6b), checkout GASP_Giustino2_n216qs. gstn.Loco.Base 1, gstn.Skid.Sim 0.

## Fatto
1. Assistant (regola 14, docs/notes/assistant/2026-11-21_pivot_overlord.md): ritimare = clip cotta con time warp, non play rate
   (Get_DynamicPlayRate allinea velocita); steeringtargettime va riscalata nei valori; lo stock NON specchia i pivot (L/R = 2 asset).
2. `AS_GSTN_RunPivot_Overlord_R` (Content/GSTN/Animation/Pivot) = M_Relaxed_Run_Turn_R_180_Lfoot, nucleo (velocita root < 0,9 corsa)
   f28-49 = 0,70 s -> 0,30 s, warp sin^2 1x ai bordi, picco 3,67x; curve rimappate, movedata_speed dalla root nuova, notify rimappate,
   root lock RefPose (Saved/Pivot/overlord_build.json, Tools/GAS/pivot/pivot_overlord_build.py).
3. `_L` = mirror cotto con coppie e asse X di MDT_UEFN_Mannequin, contact_l/r e foley L/R scambiati (pivot_mirror_bake.py,
   Saved/Pivot/mirror_AS_GSTN_RunPivot_Overlord_L.json, upright_check). r1 aveva l'asse sbagliato (enum "<AxisType.X: 1>" letto
   come Z: corpo capovolto sotto il pavimento in gioco, visto nella griglia); corretto + check indipendente che fallisce.
4. PivotSet: righe Run L/R -> _L/_R (lato dalla cella FutureFacingDelta), BlendTime 0,1 s (lo stock 0,3 = tutto il nucleo nuovo),
   riga LT ATTIVA (pivot_overlord_rows.py, Saved/Pivot/overlord_rows_set.txt). Divergenza 13 in gasp-divergenze.md.
5. Cattura (pie_capture_move.py mv_pivot_ovl/_l/_r, mv_pivot_base_r): riga in f118, peso 1 f124-130, capsula 586 -> min f124 ->
   420 f134 (~0,27 s). ASSERT OK f122-130 per lato (Saved/Pivot/cap_ovl2/assert.txt, cap_ovl1 per _r, cap_base2 base).
   Stick dritto all'indietro sceglie la L.
6. Review Fable r1 (docs/notes/reviews/pivot-overlord-v1/review-fable-r1.md): PASS tratti 1 (Toad) e 3 (due lati), nessun pop,
   niente pavimento, mirror pulito. Tratto 2 (bruto) = posa stock: legge come una giravolta leggera di jogging, non una frenata.
7. Fuori tema, chiesto dall'owner: GSTN.Presentation.Effects default 0 (VFX spenti in playtest), build reale OK
   (Saved/Logs/ci/build_wc8.log), commit 3a13fc01. Spegne anche suoni e shake delle cue; le scie delle mosse restano.

## Da sapere
- Video in review (var_ovl_l.mp4, var_ovl_r.mp4, base*.mp4) hanno VFX on (cvar esplicito nelle catture).
- Da controllare sul video: fase gambe diversa L/R a f122; probabile leggero scivolamento f136-146 dopo l'uscita.
- Tools/GAS/pivot/_probe_notify.py: sonda temporanea, non committata (cancellazione negata in auto mode), si puo cancellare.

## Prossimo
1. Passata di POSA in Cascadeur (fisso): lista nella review (frenata bassa f118-122, anche basse nel giro, braccia pesanti
   asimmetriche f124, spinta piu profonda f126-134, overshoot e assestamento f134-146), poi pivot_mirror_bake.py di nuovo.
2. Sprint: stessa build sulla clip sprint (PIVOT_SRC) e riga Sprint. 3. Coda loco owner: run start B/LL/RL, aria idle/crouch.

COSTO: ~14 $ | 0 min desktop | ~100k token review
