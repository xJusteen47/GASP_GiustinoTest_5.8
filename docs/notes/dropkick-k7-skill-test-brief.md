# Brief: dropkick giro 3 (K7) come test della skill di animazione (fisso, 2026-11-23)
Macchina: FISSO, editor + _ue59 (lock IN-USO.txt). Modello: Opus medium per le pose; build, import, catture a subagenti
Sonnet/Haiku (regola 15d). Budget 6 $ (+30% max). Non aggiorna _ue59 per il playtest. Max 2 tentativi per obiettivo.
Decisione owner 19:14Z: critica Opus chiusa, 1B 2A 3A (giro 3 = braccia + startup col corpo + carica stretta; atterraggio
= thread a parte DOPO; VFX scia/lampo NON ora, solo hook). Hitbox provvisorie decise dal thread (owner: "basta che funzionino").

## Branch
Parti dal ramo del dropkick (claude/project-thread-2d0gnu bccbae9c) e unisci claude/project-thread-k9d2l7 (integ 11-23 +
MCP v2 + follow-through + T0 + hitbox). Conflitti noti (provati nel cloud):
- CHT_GSTN_StockMover_PoseMatch.uasset (binario, cambiato da entrambi): tieni la versione di k9d2l7 (integ, righe run start
  approvate 20/21/30/31); le righe PivotSet di pivot-overlord-1 si rifanno con il suo script se servono. Scrivilo nell'handover.
- Tools/GASNPPTest/pie_capture_move.py: tieni ENTRAMBI i blocchi (PIE_CAPTURE_RUNS e PIE_CAPTURE_FRAMES); TIMEOUT = quello dei RUNS.
- docs/notes/gasp-divergenze.md: tieni entrambe le righe.

## Passi (ordine anti-spreco: niente cattura finche' un passo prima fallisce)
1. `manifest.needs_build()`: build (Run-Timed -Preset Build) solo se il C++ e' cambiato (qui si': RemoveAttackHitboxTracks).
   Poi test `GSTNCombat.Core.Timeline.RemoveAttackHitboxTracks` + unit MCP (`test_t0_prep` ecc., anche il test pwsh).
2. Pose K7 con make_k6_cards.py (copia come k7): review k6r2b punti 1-3 (ali tenute a1-a2, pugno a lato della testa, rilascio
   a8-a9 gomiti >= 6 cm fuori dalla sagoma su az180) + critica punti 4-5 (bacino che guida il tuck da a0, breakdown 70% a a2,
   palla stretta gia' a a5 e massima compressione 1 frame prima dello scatto). NON toccare: snap a13-a15, settle, smug,
   gambe unite e dritte, linea, clearance >= 0.4 cm offline.
3. setup_dropkick_kick_variant.py con DROPKICK_KICK_ROUND=K7 (settle K6, loop come K6).
4. Hitbox: `DROPKICK_TL=TL_Overlord_DropkickTBK7 SETUP_FORCE=TL_Overlord_DropkickTBK7` Tools/GAS/setup_dropkick_hitboxes.py
   (prima DROPKICK_HITBOX_DRY=1 e guarda le 4 righe f<N> nel log). Scrive Saved/AnimR2/setup_dropkick_hitboxes.json.
5. Dump giunti K6 e K7 (clip_joints_dump_ue.py, mai provato in UE: se fallisce e' "strumento", non giro), poi OFFLINE:
   anim_silhouette_check e anim_motion_overlay K7 vs K6 su startup a0-a14, viste az90/az180/az315/game. Il punto 1 della
   review si misura qui (gomiti fuori sagoma su az180). Se la silhouette dice "illeggibile" dove la review chiede leggibile:
   correggi PRIMA di catturare.
6. anim_capture_move dry_run (gate: report OK, dry run, hitbox sync, leggibilita' >= 2 cm/3 gradi, tentativi), poi run vero:
   base K6 (riusata dalla cache se c'e') + K7 in UNA sessione, run hit e whiff, viste az90, az180, game, camere a 350 cm;
   una run con `gstn.Combat.Debug.Hitboxes 1` per far vedere le hitbox. Serve ASSERT: OK.
7. Commit in docs/notes/reviews/dropkick-kick-k6/k7/: mp4 real + slow4x, griglie review_pack base sopra variante, PNG di
   silhouette e motion, README con comandi. manifest.costi_line -> handoffs/costi.csv.

## PASS della skill (non della mossa)
Ciclo guidato con i tool MCP senza script di quaternioni a mano; nessuna cattura sprecata (ogni cattura ha passato il gate);
hitbox visibili e attive nella run hit; ASSERT OK; A/B omogeneo K6 vs K7. La mossa la giudica poi un thread cloud
(Opus high + critico cieco Opus high, regola 10) che riceve SOLO le griglie, i PNG di silhouette/motion e i criteri.
Problemi degli strumenti: etichetta "strumento", fix in coda, non contano come giro di posa.
