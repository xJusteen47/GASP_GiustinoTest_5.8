# loco-runstop-diagnosis (2026-11-20, cloud, Opus high, regola 5) - perche' il restyle Cascadeur del run stop gira testa e busto
Letto: loco-runstop-1.md, loco-runstop-2.md, sprintstop_restyle_casc_v2.py, sprintstop_restyle_ue.py (branch claude/loco-runstop-1 @021780b0).
ESITO: causa trovata nel metodo di edit (non nel rig, non nel transfer UE, non in Cascadeur). Nessun desktop usato.

CAUSA COMUNE. Nel rig Cascadeur ogni box ha un frame da 3 punti: origine (MainPoint), mira (punto successivo o DirectionPoint)
e LATO (AdditionalPoint) (Tools/AnimPipeline/pose_solver.py, SPEC). restyle_v2 TRASLA solo un sottoinsieme dei punti:
sposta Main/Direction di petto, collo e testa ma NON i loro AdditionalPoint (spine_02/04, neck_01, head, clavicle, hand) ne'
i LimbDir delle braccia. Il vettore lato = Additional - Main cambia di -offset; quando l'offset supera la distanza Main-Additional
il lato si ribalta e il box rolla di ~180 gradi. Distanze nel rig UEFN (cards/overlord/jab/fk/owner_keys_2026-11-15b.json):
head Main-Additional 5.1 cm, spine_04 7.4, neck_01 3.4, spine_02 4.9, hand 9.9. Offset v2: head 13 cm (f41), petto 10 cm (f39).
Torna con i test di loco-runstop-2: solo pelvis (frame pelvis-spine_02-thigh, tutti mossi insieme) PULITO; petto +7 > 7.4*
-> spine_04 68 gradi; petto+testa (13 > 5.1) -> head 177. Non si vede di lato perche' e' un roll attorno all'osso.
Secondo difetto, minore: traslare petto e testa con pelvis fermo cambia le lunghezze (neck mosso a meta') -> il solver
ricostruisce angoli a caso. Previsione da verificare (0 $): anche lowerarm/hand di v2 hanno roll (Additional/LimbDir fermi);
in ue_build.json guardare top_delta_deg di hand_*/lowerarm_*.
Stessa lezione gia' imparata in ftilt v3.1 ("il collo si stacca"): ganon_ftilt_v3_motion.py body() ruota GRUPPI COMPLETI di punti
(Main+Additional+Direction) attorno al giunto del PADRE (petto attorno a spine_02, testa attorno a neck_01). Il run stop non l'ha riusata.
L'opzione B dell'handover (autoposing) non tocca la causa: non consigliata. pose.set rotation dei box (opzione A dell'handover)
usa API non verificata; la rotazione dei gruppi di punti usa solo set_data_value su global_position, gia' verificata.

OPZIONI
A (CONSIGLIATA) Restyle a ROTAZIONI GERARCHICHE in Cascadeur. Pelvis: come v2 (traslazione + piedi pinnati, pulito).
  Busto/testa/braccia: angoli (lean petto attorno a spine_02, nod testa attorno a neck_01, swing braccio attorno a clavicola e
  spalla) applicati come rotazione rigida a tutti i punti del gruppo, schema di ganon_ftilt_v3_motion.py body() non-LEGACY.
  Stessi tempi delle chiavi v2 (P_DY, C_DZ, H_DZ, A_*), convertiti in gradi (C_DZ 10 cm su leva spine_02->neck ~45 cm = ~13 gradi).
  Script generico: curve d'angolo per fase (curve contact/movedata_speed), quindi riusabile su ogni stop (principio owner:
  niente hack per clip). Gate prima di UE: top_delta_deg < ~25 su ogni osso, nessun salto > 90 gradi/frame (metrics),
  poi sprintstop_restyle_ue.py RS_VER=v3, clipcheck, righe chooser, cattura f200-259, review. Costo: thread fisso ~12-15 $,
  ~60-90 min (Cascadeur aperto, script server ON). Rischio: basso, l'unico ignoto e' l'estetica.
B Restyle direttamente in UE come rotazioni locali (estendere sprintstop_restyle_ue.py, o additiva Control Rig). Non puo' rollare
  (rotazioni locali esplicite), niente round trip. Ma cambia strumento (regola 6: decide l'owner), perde i tool fisici di
  Cascadeur; se additiva a runtime e' una divergenza GASP (regola 14). Costo ~10-14 $ + decisione owner. Da tenere se A fallisce 2 volte.
C Spedire lo sprint stop STOCK com'e' (gia' vivo: stop_sync_share 0.01 vs vecchio 0.25, loco-runstop-1) e passare alla fase 3;
  restyle in un'ondata di polish. Costo ~0 $ (servono comunque righe + cattura). Non va verso "Ganondorf in Astro Bot".
Raccomandazione: A, con C come cancello: se la review blind non giudica v3 piu' viva/Ganon dello stock, si spedisce lo stock.
Divergenze: nessuna. Lock _ue59 non toccato.
COSTO: ~2 $ | 0 min desktop | 0 token review
