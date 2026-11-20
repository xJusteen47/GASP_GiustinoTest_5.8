# loco-runstop-2 (2026-11-20, fisso, Opus high) - restyle Cascadeur dello sprint stop GASP
Branch claude/loco-runstop-1 (base d893501b). ESITO: PARZIALE, fermo al limite di contesto (regola 2) su un difetto del metodo.
Righe chooser, catture, review: NON fatte. Niente all'owner.

Fatto (verificato):
1. Export headless di M_Relaxed_Sprint_Stop_F_{L,R}foot (export_clips_ue.py, json {"clips":{...}}): 115 f a 30 fps,
   curve contact_l/r + movedata_speed, 8 notify. Fasi (curve): sprint f0-21, frenata f21-45, piede avanti piantato da f33,
   assestamento f45-55, fermo f55-114.
2. Import in Cascadeur: SOLO con `FbxSettings.up_axis = Z` (default = personaggio sdraiato che vola). Scene in
   Saved/AnimPipeline/runstop/: SprintStop_{L,R}_v0_gaspZ.casc (base), _v2.casc (restyle), v0/v2 .fbx esportati up Z.
3. Tools/AnimPipeline/cards/runstop/sprintstop_restyle_ue.py (headless): duplica la clip GASP in
   /Game/GSTN/Locomotion/StockBase/RunStop/AS_GSTN_SprintStop_Ganon_{L,R} e trasferisce SOLO la differenza v2-v0 di Cascadeur
   (rotazioni locali, traslazione solo pelvis). Verificato in ue_build.json: root_diff 0.0 cm, curve e 8 notify mantenuti,
   pelvis delta max 6.5 cm a f42. Il round trip Cascadeur da solo sbaglia 8-11 gradi e 14-16 cm di pelvis: il metodo a
   differenza serve. Assets salvati ma ROTTI (vedi sotto): non collegati a niente, il prossimo thread li riscrive.

Difetto (causa del fermo): restyle fatto spostando i MainPoint (pelvis, chest, neck, head, braccia; piedi ri-pinnati, deriva
max 0.4 cm). Con chest/head spostati il rig gira i giunti: head 177-179 gradi e spine_04 24-123 gradi rispetto alla base
(misura: global_rotation in Cascadeur vs v0, f37-46; uguale in UE top_delta_deg). Test su f40 (2 tentativi, regola 5):
- solo pelvis -5 cm + piedi pinnati: PULITO (head 13.5, spine_04 9.7 gradi, coerenti con la discesa);
- chest +7 cm avanti: head ok ma spine_04 68 gradi; chest+head: head 177. Cioe' il twist dei box non segue i punti.
Le griglie v2 (v2_side_right.png, v2_34.png) sembravano buone di lato: il flip e' un twist, non si vede bene sul manichino.

Prossimo thread (fisso, Cascadeur aperto, script server ON; 5 tab aperti, nessuno da salvare):
A (consigliata) pelvis con MainPoint + piedi pinnati (pulito), busto/testa/braccia come ROTAZIONI dei box (pose.set rotation,
  space local, spine_03/04/05, neck_01, head, clavicle, upperarm) con gli stessi tempi di restyle_v2 (sprintstop_restyle_casc_v2.py:
  chiavi P_DY/C_DZ/H_DZ/A_*). Poi rilanciare sprintstop_restyle_ue.py (RS_VER=v3) e controllare top_delta_deg < ~25.
B provare pose.set con autoposing=true (non testato).
Poi: domanda all'Unreal Assistant sulle righe Stand Stops (regola 14), righe, isolamento, kb_motion, cattura f200-259, review.
Divergenze: nessuna nuova. Lock _ue59 liberato.
COSTO: ~16 $ (stima) | 0 min desktop (solo Cascadeur/headless) | 0 token review
