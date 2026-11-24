# pipeline-spline-1 (cloud, 2026-11-24, Opus medium) - build senza stop-and-go per tutte le mosse da carta
Owner "vai" alla proposta C della diagnosi dropkick-k8-3 (branch claude/project-thread-6blrlz). Base: claude/integ-2026-11-23.
Riuso (regola 1): ue_rig.build (stesse modifiche alle chiavi), card.load/segments, checks.run; nuovo solo animpipe/curves.py.
## Cosa cambia
- Carta `"curve": "spline"`: dopo drop/edits/hitstop_blend/pin/part_delays il build scrive una chiave su OGNI frame, da una
  cubica per canale che PASSA per le chiavi (pendenza a tre punti, monotona: nessun osso supera una posa; Euler per la via corta).
  Piatte solo le chiavi `hold: true` / `impact: true` o in `holds`. I profili di spacing non la modellano piu'.
- Default invariato ("tangents"): le carte approvate (K7, regola 13b) si ricostruiscono identiche. `animpipe variant new`
  mette "spline" sulle varianti nuove: la variante K7/K8 si fa da li'.
- `animpipe check`: blocco `stop_and_go` (bloccante sulle carte spline, misura sulle altre). Startup: nessun passo con tutto il
  corpo < 3,5 cm/frame fuori dalle tenute, salto max <= 4x mediana. Clip intera: nessuna serie > 3 passi fermi (decollo,
  atterraggio; nota coordinatore 23:10Z). Avvisi (misura): chiavi consecutive quasi uguali (< 3,5 cm), braccia+testa+gambe che
  arrivano nello stesso frame. Soglie nel blocco carta `stop_and_go`; esenzioni `exemptions` check "stop_and_go".
## Prove
- `cd Tools/AnimPipeline && python tests/test_curves.py` -> 13 ok (tra cui: build vecchio con tangenti piatte sulle chiavi K7 =
  FAIL, spline sulle stesse chiavi = PASS; serie bacino K7 3 2 0 0 5 ... = FAIL).
- `python -m pytest -q tests --continue-on-collection-errors` -> 9 failed, 67 passed, 1 error; senza le mie modifiche
  9 failed, 54 passed, 1 error (stessi 9 + test_casc_roundtrip: mancano dump locali, preesistenti).
- NON verificato: build reale in UE (nessun editor). Le soglie vengono dalla diagnosi, non tarate su clip approvate.
## Per compilare/testare sul PC (portatile, editor aperto, script a tempo)
1. merge di questo branch nel branch di lavoro della dropkick; `animpipe variant new <startup_k7> --why="spline"`;
   aggiungere `impact: true` sulla chiave 14 (strike) e `hold: true` solo sulle vere tenute.
2. `Tools/CI/Run-Timed.ps1` con `python -m animpipe build <variante>` poi `python -m animpipe check <variante>`:
   atteso `densified` > 0 in <move>_build_ue.json e `stop_and_go` PASS. Il build e' piu' lento (una add_key per frame e canale).
3. Taratura: `check` sulle carte approvate (jab v12, dair v4, ftilt v3.1) deve dare stop_and_go come misura; se FAIL li', alzare
   min_cm/max_ratio prima di usarlo come cancello generale.
Aperti: voce in docs/INDICE-PROGETTO.md (branch claude/indice-progetto) da aggiungere al merge.
SUBAGENTI: 0
COSTO: ~2,5 $ (stima) | 0 minuti desktop | 0 token review
