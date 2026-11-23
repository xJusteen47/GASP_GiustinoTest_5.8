# loco-runstop-1 (2026-11-20, fisso, Opus high) - fase 2 rebuild: guard (Step A) + base del run stop (Step B, inizio)
Branch claude/loco-runstop-1 = loco-base-1 90d31bf9 + merge guard gz06ss 7eb0af44 (conflitto solo in gasp-divergenze.md, unito).
ESITO: Step A FATTO. Step B PARZIALE: catture base fatte, variante non ancora (checkpoint al limite di contesto, regola 2).

Step A (checklist di guard-scripts-1):
1. `Run-Timed.ps1 -SelfTest`: 5/5 PASS (casi 1-4 + 5/6 restore-data e assert-file).
2. Build `-Preset Build` -> Result: Succeeded (Saved/Logs/ci/build_runstop1.log), C++ montage con peso compilato.
3. jab1 KIT_Test_Overlord: `ASSERT: OK ... min 0.84` (Saved/GuardTest/ok/assert.txt); clip sbagliata -> `ASSERT: FAIL ... owned by
   AM_OverlordJab1V3 1.00` (bad/assert.txt). Bug trovato e corretto: pie_capture_timed.ps1 perdeva l'exit 3 di Run-Timed (602d096a);
   verificato dopo il fix: la cattura del run stop con finestra sbagliata esce 3.
4. fix_clips_ue.py dry run: fix_report.json ha "guard" per step; rifiuta lo splice di Skid_180 (calf_r 595->485 deg/s, -19%).
   Dash r3 NON rifiutato: l'asset dash in _ue59 e' modificato non committato (altro thread), non e' il r3 dell'handover.
5. vfx_spec_check --all con setup+flipbook di 65g9ca (git archive in scratch): VFX_SPEC: PASS (1 effetto).
4 suite offline: ESITO OK (python <test>).

Step B, catture base (mv_run_stop, KIT_Test_Overlord, 1 player, cvar gstn.Loco.Base 0/1), in docs/notes/reviews/runstop-stockbase/:
- base0 (vecchio ABP, RunStop.Mode off): M_Relaxed_Run_Stop_F_Rfoot entra a t=0.95 s: stop dritto da jogging, poco peso (= lamentela).
- base1 (stock pulito): M_Relaxed_Sprint_Stop_F_Lfoot entra a t=0.92 s: piede avanti piantato v196-200, discesa profonda v204-228,
  poi risalita. La nostra velocita' di corsa cade nelle righe Sprint della tabella stock. Gia' molto piu' vivo del vecchio.
- Lo stop prende il controllo ~15 frame dopo il rilascio (sinceRelease 16 a f198). ASSERT: OK *Stop* f200-f259 su entrambe
  (base*_assert_window200.txt; la finestra 192-215 del file assert_stop.json era troppo presto: FAIL 71%, errore mio).
- kb_motion compare (FBX Saved/ClipCheck/export -> BVH con Blender 5.2): GASP Run_Stop_F_L stop_sync_share 0.01, chain_zero_lag 0.09;
  vecchia V5 0.25 / 0.64 (kb_cmp_gaspRunStopL_vs_oldV5.json).
Prossimo thread (Step B seguito, fisso, Cascadeur gia' avviato dall'owner, script server ON):
1. Base clip = M_Relaxed_Sprint_Stop_F_{L,R}foot (quella che lo stock sceglie). Chiedere all'Unreal Assistant (regola 14) come si
   aggiunge una riga stop in Stand Stops (CHT_GSTN_StockMover_PoseMatch) e se disabilitare le mocap F Sprint o usare ModifyCost.
2. Restyle in Cascadeur (skill gstn-move-animation) solo sul corpo: pelvis piu' basso dopo il plant, overshoot del busto 4-12 f,
   braccia per ultime; root invariato. Duplicato in /Game/GSTN/Locomotion/StockBase/RunStop/ con curve e notify della mocap.
3. Righe: copiare celle OUT/filtri delle righe Sprint Stop F; cattura con finestra f200-259; kb_motion; review Fable + blind critic.
Divergenze: nessuna nuova (solo catture). Lock _ue59 liberato; checkout lasciato su claude/loco-runstop-1.
COSTO: ~12 $ | 20 min desktop | 0 token review
