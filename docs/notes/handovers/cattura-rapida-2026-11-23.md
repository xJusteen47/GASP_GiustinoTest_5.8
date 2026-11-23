# Cattura rapida: valutazione (2026-11-23, cloud, sola lettura, Opus medium)

Brief: nota del coordinatore (owner: "cattura in meno di un minuto, anche con Blender"). Pagina per l'owner:
project files handoffs/cattura-rapida-2026-11-23.md. Base: handoffs/tempi-import-cattura-2026-11-23.md.

Fatti (regola 11):
- 150 frame in 34 s con 7 viste: docs/notes/reviews/cp5-data/dair_base/capture.log sul fisso, righe
  "sequence dair_whiff" (26.1 s) e "sequence dair_whiff done" (60.1 s); driver loaded -> prima sequenza 26 s.
- Collo di bottiglia: pie_capture_move.py:458-462 (branch claude/project-thread-6blrlz) capture_scene() +
  export_render_target() sincroni per vista per frame: flush del render, lettura GPU, PNG sul thread di gioco.
- K8 giro 2: 9 min di cattura per 5 run (dropkick-k8-2.md riga 11).

Esito: obiettivo "<1 min" raggiungibile per il giro (~50-60 s con avvio editor, ~30 s a editor aperto);
"qualche secondo" solo per la parte immagini (5-10 s stimati, non misurati).
Opzioni: D motore di cattura C++ asincrono (consigliata, 8-12 $, valida per regola 10); E Blender/visualizzatore
(PROTO regola 6, 5-8 $); F ibrido = C già in 1C.
Decisione owner: 2A (D dopo A, consigliata) / 2B / 2C. Nessun codice scritto.

SUBAGENTI: 0
COSTO: ~1,5 $ | 0 minuti desktop (solo lettura log via cartella) | 0 token review
