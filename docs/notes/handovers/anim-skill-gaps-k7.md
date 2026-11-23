# anim-skill-gaps-k7 (cloud, 2026-11-23, Opus medium) - i 6 buchi della skill trovati dalla review K7 (owner 2A)
Branch claude/project-thread-jcugce = j1twqo (test K7) + questo lavoro. Riuso: capture_move/manifest/review_pack/silhouette di T0,
pie_capture_move PIE_CAPTURE_RUNS, cvar gstn.Combat.Debug.Hitboxes e il debug draw esistente. Niente editor preso.
## Fatto
1. Cancello e pose da carta: `capture_move.card_report` legge il JSON di `animpipe check` (PASS, piu' recente di carta e build, la carta
   nomina la clip) e lo registra nel manifest con l'hash della carta; variante con `"card"` al posto di `intent_hash`. Sempre
   `gate_report.json` nella cartella review, anche se FAIL (motivo per voce: check FAIL con blocchi, stale, clip sbagliata).
2. Piu' kit per sessione: `"kit"` per run in runs.json; pie_capture_move da' il kit prima del reset della run. Stessa sessione = stesso spawn
   (lo sfondo diverso base/K7 sparisce).
3. Hitbox nella cattura: le scene capture non disegnano le linee di debug. C++ `UGSTN_CombatSimComponent::GetDebugHitboxShapes` (JSON
   delle forme disegnate, stesso offset mesh, linger compreso; debug, non stato); pie_capture_move logga per frame `hb` + camere per vista
   (`cams`); `capture_overlay.py` le proietta e le disegna in griglie (review_pack) e video/sheet (pie_capture_grid, `--hitboxes=0` per
   toglierle). Etichetta `hb<N>` sulle celle con hitbox.
4. Silhouette: parametro `out` (anche nel tool MCP): PNG + `silhouette.json` nella cartella review, da committare.
5. Allineamento A/B: offset per run = inizio montage (frames.jsonl) - inizio base; griglie, video e `alignment.json` allineati; inizio
   sconosciuto = offset 0 e run elencata in `unaligned`, mai indovinato.
6. Silhouette "illeggibile" = palla informe: nessuna estremita' >= 12 cm fuori dal torso E circolarita' del contorno >= 0,65 (root e
   ossa IK escluse). Riga FINDING con sporgenza massima e circolarita'. Le estremita' vicine al torso restano solo elencate.
## Test
- Offline: tests/test_skill_gaps.py 10/10 (+3 subtest), test_t0_prep 29/29 (2 test silhouette riscritti per la nuova regola),
  gstn_anim_mcp/cheap/capture_assert ok. Comando: `python -m pytest -q tests/test_skill_gaps.py tests/test_t0_prep.py` (cwd Tools/AnimPipeline).
- Falliscono anche prima di questo lavoro: test_ps1_gate_fail_propagation (pwsh assente in cloud), test_capture (fixture dair_v4 mancante).
## NON provato (coda portatile)
- Build C++ (GetDebugHitboxShapes) mai compilato. Poi UNA cattura K7 via anim_capture_move con card + kit per run + run hitbox: verifica
  hitbox visibili nelle griglie, sfondo uguale, offset whiff = 1.
- Silhouette K6/K7 rifatta sui dump (`out=docs/notes/reviews/dropkick-kick-k6/k7`) e json committato: rimisura i gomiti (10,9/8,5 cm)
  e mette il numero della palla da dietro. Soglie 12 cm / 0,65 tarate su pose sintetiche, da confermare sui dump veri.
SUBAGENTI: 0
COSTO: ~3,5 $ (stima) | 0 minuti desktop | 0 token review
