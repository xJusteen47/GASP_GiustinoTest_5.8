# phase3-batch2-1 (2026-11-20, fisso, Opus high) - jump squat + land: base stock, brief, land v1 (pareggio -> stock)
Branch claude/loco-jumpland-1 (base d74b9a16). ESITO: PARZIALE. Land v1 fatto e misurato ma la review blind da' PAREGGIO con lo
stock -> cancello C: righe rimesse a stock. Squat: e' un difetto di GIOCO, non di posa (G1). Niente all'owner da qui.

1. Base stock (gstn.Loco.Base 1, 1 player): Saved/JumpLand1/base1 (KIT_Test_Overlord), base1_pt (KIT_Test_Playtest), sequenze
   nuove mv_jump_rest, mv_run_jump (pie_capture_move.py). Clip: stacco M_Relaxed_Jump_F_Start_Stand_Rfoot entrata a 0.667 s (salta
   il caricamento), blend 0.2 s da idle; atterraggio M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot (Stand Mode Transitions riga 4) entra a
   1.033 s, blend 0.2 s; di corsa Jump_F_Start_Sprint_Rfoot + Land_Run_Heavy_Rfoot poi il nostro SprintStop_Ganon_L.
   Conti: script inline su frames.jsonl (anim, loc, body.pelvis_z), stessi numeri nei due kit.
2. G1 (GIOCO): nessun frame di squat a terra in entrambi i kit: pelvis sale il frame dopo la pressione (f41, jump.phase 0->1),
   griglia takeoff_az90.png = posa idle che vola. L'owner aveva scelto squat 3 frame (1A, 2026-11-17) ma JumpSquatFrames=0 e i kit
   non lo sovrascrivono. Anche con 3 frame l'ABP resta Grounded durante lo squat (In Air entra al decollo): serve uno stato/riga
   "squat" (gasp-structure 10.2) = lavoro di codice + divergenza da registrare. -> coda del codice, poi la posa.
3. G2 (GIOCO/dati): blend di atterraggio 0.2 s FastFeet_FastRoot (stock) contro "quasi taglio netto" dell'owner: cambiare la cella
   BlendTime della nostra riga = divergenza (regola 14), chiedere prima all'Assistant.
4. Brief identita' (13a): docs/notes/briefs/jumpland-identity.md. Scoperta: la clip stock RI-PIANTA gia' un piede alla volta
   (misura Cascadeur 30 fps: contatto f29, pelvis min 54 cm f34, piede L su f40-50, piede R f54-66, fermo ~f80): tratto 3 gia' ok.
5. Land v1: Tools/AnimPipeline/cards/jumpland/land_restyle_casc_v1.py (metodo run stop v3), GATE OK top 18.5 neck_01 f37, salto
   32.8 gradi/f. Scene Saved/AnimPipeline/jumpland/LandStand_v0_gaspZ.casc, LandStand_v1.casc, fbx LandStand_L_{v0,v1}.
   UE: sprintstop_restyle_ue.py reso generico (RS_WORK/RS_SIDES/RS_STEM/RS_DST_NAME), Save-Assets tag landstand_v1: asset
   /Game/GSTN/Locomotion/StockBase/JumpLand/AS_GSTN_LandStand_Ganon_L, root_diff 0.0, top_delta upperarm 13 (ue_build.json).
   Righe: stops_rows_ue.py riga 5 di Stand Mode Transitions (RS_ROWS=new), cattura Saved/JumpLand1/v1cap: ASSERT: OK f114-180
   min 0.76 (docs/notes/reviews/landstand-v1/v1_assert.txt); isolamento: clip uguali allo stock tranne idle break casuali prima del
   salto e 3 frame di incrocio blend (51, 106, 112); pelvis min 54.3 vs 58.7 stock, a f140 81.8 vs 77.5.
6. Review blind Fable high (A=v1, B=stock, 4 griglie): PAREGGIO, entrambi impatto PASS, recupero FAIL (gobbo v132-150, >1 s),
   follow-through debole; la polvere copre f108-120 dove sta la differenza. -> RS_ROWS=stock (salvato, riga 5 disabilitata).
PROSSIMO (fisso, Cascadeur): land v2 = recupero RITIMATO (non offset): petto dritto entro ~f50 a 30 fps, overshoot indietro, mani
via dalle ginocchia; ampiezza impatto x2; cattura review SENZA VFX (o vista az90 con polvere spenta) + mp4 side|back|game;
controllare i piedi a piena risoluzione. Prima pero' G1 nella coda del codice. mv_run_jump: land di corsa non ancora guardata.
Da portare all'owner (coordinatore): G1 (squat 3 frame deciso ma non attivo) e G2 (taglio netto = divergenza).
Divergenze: nessuna. Lock _ue59 liberato. Asset salvati: AS_GSTN_LandStand_Ganon_L, CHT_GSTN_StockMover_PoseMatch (riga 5 off).
COSTO: ~16 $ (stima) | 9 min desktop (3 catture PIE + 3 headless) | ~85k token review (1 subagente)
