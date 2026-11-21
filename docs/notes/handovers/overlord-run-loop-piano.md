# Handover: piano loop di corsa Overlord (corsa base + sprint 1) dai riferimenti Ganon
Thread cloud, 2026-11-21. Solo piano, nessun codice o asset.
OBIETTIVO: spec di posa e tempi per stadio 1 (corsa base, via di mezzo tra camminata e corsa di Ganon) e stadio 2
(sprint 1 = facsimile run di Smash), da usare in un thread sul fisso con Cascadeur.
Fatto:
- Analisi di IMG_9598.mov (30 fps): camminata ciclo ~0,75-0,77 s, corsa ciclo ~0,53 s (appoggi contati a mano sulle
  griglie walk15.jpg e run30.jpg, ritaglio che segue Ganon con tracking del rosso: script track.py/grid.py/labeled.py
  nello scratchpad del thread, non committati).
- Griglie etichettate: docs/notes/reference-dossiers/overlord-run-loop-ganon/ (A walk, B run, + sequenze intere).
- Piano: docs/notes/overlord-run-loop-piano.md (copia in project files handoffs/overlord-run-loop-piano.md):
  tabelle di posa, tratti d'identità 2x3, sheet curve, 10 passi di authoring, pendenze via BS + delta stock.
Scelte applicate (regola 6): corsa base 0,64 s, lean 8-12°, gomiti bassi; sprint 1 0,53 s, lean 20-25°;
phase nella convenzione stock (triangolo) come da nota coordinatore; nomi AS_GSTN_Overlord_RunLoop_F_v1 / _Sprint1Loop_F_v1.
Da confermare (13d): variante run start v4r9 con busto eretto e gomiti bassi (commento owner sulla variante 0; v4r7 bloccata).
Prossimo: thread sul fisso (Opus high + Fable sulle pose), ~20-25 $.
COSTO: ~3 $ | 0 min desktop | 0 token review
