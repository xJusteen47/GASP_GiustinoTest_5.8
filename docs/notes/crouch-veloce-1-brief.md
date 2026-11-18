# Brief: crouch veloce con follow-through (terra + aria), tutti i kit

Owner 2026-11-18 23:30Z/23:39Z: "La posa di crouch lo voglio bello veloce il passaggio. Da e fuori. In aria una posa misto tra air time e crouch,
più ristretta/piegata". Riferimento: Ganondorf SSBU, `handoffs/crouch-ref/` (foto, 3 video, griglie). Ambito: crouch a terra + air crouch,
TUTTI i kit (profilo condiviso, memoria ALL KITS SAME). Salto/atterraggio: solo riferimento, vedi Proposte.
Macchina: FISSO (_ue59) quando libero; analisi e piano in cloud (questo thread). Modelli: Opus esegue, Fable 5.2 high giudica e decide le pose,
catture/build a subagenti Haiku/Sonnet. Budget stimato: 45-70 $ per 3 giri (owner: budget aperto questa settimana). Aggiorna _ue59: sì (lock).

## Tratti d'identità (regola 13a; la review li controlla PER PRIMI)
Terra (rif. `grid_9542_in_out_30fps_t1.75.jpg` f8-11, `ganon_crouch_photo.jpg`):
1. MOLTO basso e raccolto: ginocchio anteriore quasi a terra (quasi in ginocchio), posteriore basso; busto piegato sopra la coscia, testa avanti
   ma sguardo in avanti. Il bacino scende molto più del crouch GASP.
2. Braccia raccolte davanti alle ginocchia, avambracci quasi incrociati, pugni chiusi; gomiti fuori dalla sagoma del busto (leggibile da dietro).
3. Passaggio a SCATTO: in giù in ~4 frame di gioco, su in ~4-5; braccia e testa arrivano 1-2 frame dopo il corpo (follow-through), piccolo
   overshoot e assestamento entro ~8 frame.
Aria (rif. `grid_9540_air_landing_t1.2-2.4s.jpg`, `grid_9540_crouch_jump_t3.9-5.5s.jpg` t0.8-1.0):
1. Raccolta a "palla aperta": cosce su verso il petto (anca > 90°), tibie piegate sotto, ginocchia asimmetriche (una più alta), MAI gambe tese.
2. Busto incurvato avanti sopra le ginocchia, braccia strette in guardia davanti al petto, gomiti in fuori dalla sagoma.
3. Entrata a scatto con overshoot (piega in più), uscita che si apre partendo dalle gambe; più compatta della posa di caduta, meno del flip.

## Sheet delle curve (frame di gioco a 60 fps, contati da 1; regola 13c)
| Clip | f1 | Movimento principale | Overshoot | Follow-through | Assestato |
|---|---|---|---|---|---|
| Crouch Enter (terra) | idle in piedi | bacino f1-f4, ease-IN (accelera come cadendo), 85% del tratto in f3-f4 | f4-5: bacino 3 cm sotto la posa, busto +4° avanti | braccia arrivano f5-6 con 6-8° oltre, mani f7-8; testa f5 (contro-rotazione per tenere lo sguardo) | f8-10 |
| Crouch Idle (loop) | posa crouch | respiro lento: bacino ±0.5 cm, spalle ±1°, 2 s | - | mani 4 frame in ritardo sul busto | - |
| Crouch Exit (terra) | posa crouch | spinta f1-f4, ease-OUT (parte esplosiva), 90% entro f4 | f5: bacino +1.5 cm, petto +3° indietro | avambracci 1-2 frame dopo, superano idle di 5° | f9-10 = idle in piedi |
| Air Crouch Enter | qualunque posa d'aria | piega f1-f3, ease-in | f4: anca +8°, schiena +4° di curva | braccia f4-5, piedi (caviglie) f5 | f8 |
| Air Crouch Loop | posa aria | quasi ferma: bacino ±1 cm, 1 s; nessun respiro visibile | - | - | - |
| Air Crouch Exit | posa aria | gambe si aprono prima f1-f3, busto f2-f4 | f4 anca -4° | braccia f4-6 | f6 -> posa di caduta |
Misure dal riferimento (telefono 30 fps = 2 frame di gioco a step): giù 2 step, assestato 4 step; su 2-3 step, braccia assestate 4 step.

## Integrazione (decisioni applicate, regola 6)
- Terra: clip nostre come righe "raw" nel chooser + transizioni nella state machine di `ABP_GSTN_Locomotion` / `CHT_GSTN_LocomotionAnimations`
  (regola owner GASP ANIM), blend d'ingresso 0-2 frame; warping lasciato acceso. Entrambi i comportamenti come impostazione:
  `DA_Locomotion_Default` > Crouch > `bFastCrouch` (nostro, default ON) / OFF = crouch GASP com'è oggi. Tempi di blend nel profilo, non nel kit.
- Crouch walk: oggi clip GASP (bCrouchWalk ON). Con la posa nuova il passo GASP stona: giro 1 lo lascia, la review lo segnala come "gioco".
- Aria: si sostituisce il CONTENUTO di `AS_AirCrouch_Loop_PLACEHOLDER` (stesso percorso) e si aggiunge `AS_AirCrouch_Enter`. In `DA_ReactionAnims`
  (owner 23:48: nessuna modifica a mano, si può toccare; prima git status) vanno Enter e BlendOutProfile della regola AirCrouch.
  L'uscita oggi è un blend di 3 frame: per l'Exit con follow-through serve un campo ExitAnim nella regola (codice piccolo, GSTNCombatGAS).
- Atterraggio con LT tenuto: air crouch -> crouch a terra quasi a taglio netto (regola LANDING), senza passare dal land GASP in piedi.

## Strumento e passi (fisso)
1. Cattura economica della BASE (crouch GASP in/out, air crouch placeholder, atterraggio con LT) prima di toccare nulla (regola 10).
2. Esporta pose sorgente (idle in piedi, crouch idle GASP, fall loop) in json bone; pose nuove come offset locali FK con
   `casc_fk_take.py`-style (slerp locale = archi naturali, principio INTERPOLATION SPACE), piedi piantati via foot_paths a terra.
3. Publish headless, cattura in-game camera di gioco a velocità reale + rallentata, Fable review (identità prima). Max 3 giri di posa.

## Proposte (non nel lavoro, per l'owner)
- Atterraggio Ganon (griglia 9540 t0.53-0.8): squat profondo 2-3 frame con braccia larghe, poi risalita in ~0.3 s. Confrontare col nostro land.
- Salto: anticipo basso (jumpsquat) e ginocchia raccolte in salita; il doppio salto è un flip a palla.
