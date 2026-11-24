# dropkick-k5-3 (portatile, 2026-11-21, Opus) - giro di posa 3 di 3 del calcio: braccia sdraiate basse + testa smug. Pack pronto, review cloud da fare
Branch claude/project-thread-cqad05 (da uvzm3k 79631bd6), checkout GASP_Giustino2_n216qs. Timeline/hitbox di gioco NON toccate.

## Fatto
1. k5/fold3.py: fit delle braccia per le chiavi sdraiate (startup 14-15, loop 0/6/11) con la review k5r2b "Round 3": gomiti ai fianchi
   (18-22 cm), avambracci sul petto sx sopra dx, blocco braccia <= sterno + 11,3 cm (giro 2: 18,7). Prova: `python k5/fold3.py eval <card> <png>`.
   Chiavi 1-13 (piega in piedi, PASS) invariate. k5/render3.py: sagome offline del corpo intero (blocco braccia 28 cm vs testa 33 sopra il bacino; giro 2: 41 vs 31).
2. Testa da a15: collo sollevato, mento su, girata ~25 gradi verso az90 e inclinata ~25; faccia 35 gradi sopra l'orizzonte (giro 2: 54).
   Loop: solo +3 gradi mento/inclinazione a f6 (il loop e' 12 f = 0,2 s: un ciclo vero sarebbe un tremolio a 5 Hz).
3. Build: il build tiene SOLO i frame "poses" della card (0,4,8,11,15) e il rig interpola il resto, percio' la chiave 14 dell'author era
   ignorata e il f14 compenetrava (-0,34 cm). Aggiunta chiave di build f14 (smear, fold3.fit_path sui blend 13/15) in make_k5_cards.write.
   Check: clearance PASS min 0,43 cm (Saved/AnimPipeline/animpipe/overlord_dropkick_startup_k5_check/clearance.txt), loop min 0,73.
   Altri FAIL del check uguali al giro 2 (matplotlib mancante, lock a f15, snap 14->15, braccia incollate: voluti).
4. Catture (k5/capture_r3.ps1): whiff ASSERT OK (Saved/DropkickK5/k5r3w/assert.txt), hit e alta ASSERT OK offline (assert_offline.txt).
   Alta con camere a 160 cm. Pack docs/notes/reviews/dropkick-kick-k5/k5r3/ (README, mp4 tutti da a-20 con a-numero impresso,
   closeup_loop.jpg, assert_lines.txt) con k5/review_pack_r3.py.
5. PIE a 3 giocatori sul portatile: n216qs aveva PlayNumberOfClients=3 (ListenServer) -> rimesso 1 / Standalone (backup in %TEMP%/k5r3).
   Cherry-pick di ba09e939 (Run-Timed ripristina le chiavi PIE dopo ogni run). padmenu ha ancora PlayNumberOfClients=2 (non toccato).

## Da guardare / aperto
- Il mio sguardo (non e' la review): braccia basse sul petto, niente tenda; da az315 la testa si vede ma e' girata via (verso az90).
- a19 del whiff 480: avambracci su un frame prima del suolo = blend nel clip di atterraggio (altro clip, coda codice).
- Overshoot a15-a17 non fatto: serve un segmento one-shot dopo lo strike (montage) = decisione owner.
Prove: numeri offline da `fold3.py eval`/`render3.py`, build dai file citati.
COSTO: ~5 $ (stima) | 0 min desktop | 0 token review
