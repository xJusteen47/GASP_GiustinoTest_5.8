# fast-capture-1: motore di cattura rapida (1A + 1C + 2A), 2026-11-24, cloud, Opus medium

OBIETTIVO: giro di cattura da ~9 min a ~1 min (~30 s a editor aperto), sempre in-game (regola 10). Codice NON compilato (fisso e portatile occupati).
Riuso: pie_capture_move.py (driver, RUNS, assert), gstn_anim_mcp/capture_move.py (gate, cache, review pack), pie_capture_grid.py, animpipe author.Poser.
- 2A plugin nuovo `Plugins/GSTNCapture` (abilitato nel .uproject, niente Shipping): UGSTN_FastCapture, readback GPU asincrono, PNG su task
  (stessi file f<NNN>.png: assert, griglie, review pack invariati), anello di pre-roll, luce semplice sulle viste az* (non unlit), vista game piena.
- Driver `PIE_FASTCAP=1`: tutte le viste in un passaggio, solo press-20..press+90 (+ frame di hitstop) su disco, fine anticipata della sequenza,
  t.MaxFPS 0 e niente throttle in background. Senza plugin compilato torna al vecchio percorso ("FASTCAP: unavailable" nel log).
- Camere (nota coordinatore, K7/K8 v156-v158): seguono la MESH, non l'attore (l'attore scatta, la mesh è smussata); `capture_move.camera_jumps`
  avvisa dei salti (warnings nel risultato). Da verificare al primo giro che il salto sparisca.
- 1A: chiave cache sul contenuto C++ (non mtime) + CAPTURE_CHAIN: K7 si cattura UNA volta col nuovo motore, poi dalla cache; run hitbox solo con
  final_round=True; hitstop per mossa `hitstop_pairs=[[whiff, hit]]`: video hit allungato dell'hitstop + video affiancato whiff|hit.
- 1C: `Tools/AnimPipeline/quick_preview.py` (offline, ~1 s, PROTO solo per scartare): base grigia sotto variante, viste 90/180/135, avvisa sotto 2 cm.
  2B (Blender) fatto da un altro thread (blender-preview-1).
- Template: `Tools/AnimPipeline/cards/overlord/dropkick/k8/capture_k8_fast.py` (dry run di default; --real; --final per la run hitbox).
Test offline: `python -m pytest -q Tools/AnimPipeline/tests/test_t0_prep.py tests/test_skill_gaps.py tests/test_fast_capture.py tests/test_quick_preview.py`
= 48 passed (cloud). Fallimento già presente prima: test_gstn_anim_mcp::test_ps1_gate_fail_propagation_and_assets_validation (serve pwsh).

COMPILA E MISURA (fisso, editor chiuso, branch claude/project-thread-bazqb1):
1. `Tools/CI/Run-Timed.ps1 -Preset Build -WaitMutex -NoHotReload` (se fallisce GSTNCapture: max 2 tentativi, regola 5; firme RHI da
   controllare per prime: FRHIGPUTextureReadback::Lock, FImageView(FColor*), FImageUtils::SaveImageByExtension).
2. `python Tools/AnimPipeline/quick_preview.py Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k8.json --base=Tools/AnimPipeline/cards/overlord/dropkick/overlord_dropkick_startup_k7.json`
3. `python Tools/AnimPipeline/cards/overlord/dropkick/k8/capture_k8_fast.py --real` due volte: la 1a cattura K7+K8 (catena nuova), la 2a
   solo K8 (K7 "reused"). Tempo: riga "TEMPO giro" + righe `fastcap <seq>: ... wall_s` in Saved/AnimPipeline/captures/dropkick-kick-k6/k8fast/capture.log.
   Confronto: vecchio giro 9 min (dropkick-k8-2.md). Primo giro col nuovo motore: base ricatturata e guardata accanto alla vecchia (luce diversa).
Rischi: luce semplice diversa dalle review passate; fine anticipata se l'assert non ha finestre (allora gira tutta la sequenza).
INDICE-PROGETTO (da aggiungere quando claude/indice-progetto è in main): "Cattura review rapida: Plugins/GSTNCapture + PIE_FASTCAP in pie_capture_move.py; anteprima offline Tools/AnimPipeline/quick_preview.py".
SUBAGENTI: 0
COSTO: ~7 $ | 0 min desktop | 0 token review
