# Handover: brief VFX stile v2 "Astro Bot" (2026-11-17, cloud, Fable 5.2 high, solo documenti)
Richiesta owner 23:00Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCB4Qm5m3DmUm7CKzbkY8AG) + 23:00:56Z proxy a basso impatto (cmsg_01CRvUdrWm1dJrM4q54YDBLN3tQi4k2dkxXNS73KP9oMxy). Base: M2 r4 `claude/project-thread-vcepyw` e7757181.
## Fatto
- `docs/notes/stile/vfx-stile-v2-astrobot.md` (copia project files handoffs/): diagnosi (sprite Unlit piatti = "adesivi"), 9 cambi, tabella per effetto, perf, 3 decisioni owner, costo ~55 $ portatile, controlli Fable.
- `docs/notes/stile/stile-sheet-vfx-review.md`: fusa la proposta (voci [NUOVO]) + righe [V2]; checklist D ora 21 voci, etichetta "materiale" nuova.
- Copiati qui `vfx-clarity-ricerca.md` e la proposta (marcata FUSA).
## Prove
Diagnosi da griglie r4 `docs/notes/reviews/polish-m2/r4/hit_large_close_22-39.png` e `jump_side_80-103.png` (branch vcepyw), guardate una volta.
## Applicato (regola 6, da confermare 13d)
Testo Gemini usato come input, non fonte: escluse collisioni particella-particella, detriti persistenti di default, vetro traslucido. Rimbalzi = piano analitico CPU (no GPU depth buffer: toaster CPU-only). Ombre degli effetti = disco mesh, non decal (decal blob invisibile in M1).
## Decisioni owner aperte
1 materiale Lit (A) / Unlit finto (B); 2 VFX in hitstop 0,15 (A) / 0 (B) / 1 (C); 3 detriti brevi + persistenza spenta (A) / persistenti in avventura (B) / nessuno (C).
## Prossimo
Thread portatile v2-r1 (Opus medium, ~30 $ + Fable 8 $) dal brief; verificare in 5.9 l'API per il tempo del componente Niagara nell'hitstop.
Costo stimato thread: ~4 $.
