# tempi-import-cattura-1 (cloud, 2026-11-23, Opus medium, sola lettura)
Domanda owner: unlit / import più veloce. Proposta completa: docs/notes/tempi-import-cattura-2026-11-23.md (copia in handoffs/).
## Fatto
- Letti (branch claude/project-thread-6blrlz): k8/run_r2_editor.sh, capture_k8_r2.ps1, runs_r2.json, pie_capture_timed.ps1,
  pie_capture_move.py (300 frame per run dropkick_sweet, 3 viste PNG per frame, t.OverrideFPS 60), editor_session.py,
  handover dropkick-k8-1/-2, dropkick-k7-skill-test, docs/notes/speed-up.md.
- Minuti dai handover: K8 r2 animpipe 6 + cattura 9 (5 run); K8 r1 12 + 9,6 (3 run); K7 15 + 11.
- Cattura: 1500 frame x 3 PNG; 2 run su 5 sono K7 bloccata, 1 solo hitbox. Unlit: guadagno piccolo (stima), perde profondità.
- Log del portatile (Saved/) NON letti: la ripartizione dentro i 9 min è stima.
## Proposta
A cattura snella (cache base K7, hitbox solo all'ultimo giro, PNG solo nella finestra, luce semplice su az90/az180): 9 -> 3-4 min, ~2-3 $.
B un editor per import + cattura (job server di speed-up.md): -1/2 min, ~4-6 $. C anteprima `animpipe render --standalone` prima della cattura.
Consigliata 1C (A + C). Decide l'owner.
SUBAGENTI: 0
COSTO: ~1 $ | 0 minuti desktop | 0 token review
