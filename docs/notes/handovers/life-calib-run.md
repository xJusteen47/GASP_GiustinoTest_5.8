# life-calib-run (fisso, 2026-11-24, Sonnet medium) - taratura life + audit stop: FERMO al punto stabile (regola 6)
Brief: coordinatore 23:41Z (dump + life_calibrate.py) e 23:42Z (audit-stop da claude/project-thread-3ufz10). Nessun dump prodotto.
## Esito
- CALIB: non eseguibile. `python Tools/AnimPipeline/life_calibrate.py` con i soli dump esistenti (joints_..._w2/w3 copiati da
  GASP_Giustino2_ue59/Saved/AnimPipeline/runstart) crasha: `KeyError: 'pelvis'` in life._rel (life.py:127). Causa: quei dump li
  scrive runstart_clip_joints_ue.py (solo root + rig point, niente pelvis); il dump giusto e' clip_joints_dump_ue.py
  (ha pelvis/spine/head, CJD_ASSETS) che sta SOLO su claude/project-thread-wwnerm / 3ufz10, non su 1uvcac ne su ue59.
  Comando che lo mostra: `python Tools/AnimPipeline/life_calibrate.py` nel worktree claude/life-calib-run.
  (Nota: life_calibrate dovrebbe dare MISSING/errore chiaro se manca pelvis, non traceback: da correggere nel thread di taratura.)
- Dump UE: bloccato. Save-Assets.ps1 gira SOLO nel checkout GASP_Giustino2_ue59 e solo con uno script tracciato e pulito; ue59 e'
  sul branch claude/runstart-generic-desktop con modifiche locali dell'owner (DA_Locomotion_Default, w2t180Ls, CHT_..._PoseMatch,
  pie_capture_move.py). Non committo li' ne' aggiro il guard. Un worktree nuovo non ha Binaries/DDC (build 45 min, fuori budget).
- Audit stop (3ufz10): stesso blocco (audit_stop_dump_ue.py richiede Save-Assets). Non eseguito, nessun numero.
- Asset: AS_OverlordDropkick_StartupK7/K8 NON esistono in Content (c'e' solo AS_OverlordDropkick_Startup); settle K6 senza nome
  (manifest: SET_ME). ftilt/jab/dair sono in Content/AnimPipeline/Rig/{Overlord/FTilt,Pilot/Jab,Overlord/DAir}, non in RunStart.
## Serve per sbloccare (un solo passo, ~10 min, fisso, editor chiuso)
1. L'owner (o chi ha il permesso) mette su ue59 un commit con clip_joints_dump_ue.py + audit_stop_dump_ue.py (cherry-pick da
   3ufz10) SENZA toccare i suoi file locali (commit solo di Tools/AnimPipeline/gstn_anim_mcp/*), oppure autorizza esplicitamente
   un guard alternativo.
2. `Save-Assets.ps1 -Script Tools\AnimPipeline\gstn_anim_mcp\clip_joints_dump_ue.py` con CJD_ASSETS = path completi /Game/...
   (w3, w2 in /Game/GSTN/Animation/RunStart; ftilt, jab, dair sopra; K7/K8 = dump gia' fatti? cercare i nomi reali), poi
   audit_stop_dump_ue.py (-Tag audit_stop), poi life_calibrate.py e audit_stop_table.py.
3. Da decidere (owner): quale dair e' approvata (anim-approvate.md non ha la riga) e il nome degli asset K7/K8/K6.
SUBAGENTI: 0
COSTO: ~0,6 $ | 6 minuti desktop (nessuna build, nessun editor) | 0 token review
