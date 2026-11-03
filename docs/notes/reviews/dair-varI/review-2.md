# Dair variante I: revisione visiva 2 (render corretto, 2026-11-03, thread cloud)
Fonte: docs/notes/reviews/dair-offset/overlord_dair_v4_varI_az90_ingame.mp4 (51 frame, etichette = frame anim 0-based,
hitstop v15-v22 = f15) + grid_f12-18_side_base_vs_varI.png. Frame estratti con ffmpeg, griglie 13-26 e 35-42 guardate,
quote in pixel misurate sulla sagoma (az 90, 384 px, personaggio ~140 px).

**VERDETTO: FAIL** (criterio 2).
1. Impatto a f15: PASS. f14 ancora raccolto, f15 corpo disteso e piedi giù (bordo basso 228 -> 285 px); hitstop congela f15.
2. Follow-through dopo lo stomp: FAIL. Tutta la sagoma scende come un blocco: cima testa 150 -> 158 px e piedi 285 -> 293 px
   negli stessi frame f18-19 (stessa corsa, 8 px). Minimo piedi/corpo f19, minimo testa f19-20 (pari), cima dei pugni 179 @f19 /
   178 @f20: ritardo 0-1 frame e 1-4 px relativi, invisibile a scala schermo. f21-22 si vede solo la piega delle ginocchia.
3. FAF 37-42: PASS con nota. Tronco appena in avanti e stabile 37-42, pugni chiusi, avambracci lontani dal petto, nessuna
   compenetrazione visibile. Nessun render base 35-42 disponibile: il confronto con la base qui non e' visivo.
4. Prima di f17 invariato: PASS con nota. f14-17 identiche alla base (diff 0 px); f12-13 differiscono di poco (braccia
   leggermente piu' alte, testa piu' coperta), coerente con la modifica key 12 upperarm della variante G gia' nota all'owner.
   f18 variante ~3 px piu' bassa = inizio del dip, atteso.

**Correzione unica consigliata**: rallentare la risalita del bacino (bacino fermo in basso f19-22 invece di risalire
~9 cm f19->21), cosi' testa e pugni, che toccano il minimo a f20-21, si vedono proseguire oltre il bacino fermo. Non tocca
le braccia incrociate (nessun problema di clearance come in J), ma cambia il timing del bacino dopo lo stomp: serve il
permesso dell'owner. Alternativa (b): staccare gli avambracci dal petto di qualche cm a f20-23 (tocca la posa braccia dell'owner).
Costo revisione: ~20 chiamate, contesto ~45k, circa $1,5.
