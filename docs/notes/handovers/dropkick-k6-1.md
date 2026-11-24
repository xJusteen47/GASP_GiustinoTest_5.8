# dropkick-k6-1 (portatile, 2026-11-21, Opus) - variante K6 del calcio: peso, braccia che stringono, overshoot dopo il colpo, linea Big Band
Branch claude/project-thread-jx1fv2 (da cqad05 82d09c64), checkout GASP_Giustino2_n216qs. K5 intatto. Timeline/hitbox NON toccate.

## Fatto
1. Antigravity non c'e' sul portatile: proposta nostra; note di Gemini arrivate dal fisso (gemini-k6.md, cherry-pick ff486dfa) e integrate
   dove combaciano (picco 1-2 f dopo il colpo, punte oltre e rilascio, mento +6).
2. k6/make_k6_cards.py: startup K5 ri-chiavato sui frame di build (0/4/8/11/14/15) + strati K6 (press a4, relax a8, gather a11, clamp a15;
   testa in ritardo a4 e oltre a8); carta NUOVA settle_k6 (12 f, picco corpo f2, testa f4, rimbalzo f7, = loop f11); loop_k6.
   Owner 18:39Z (ambito colpo+settle+caduta): busto indietro e piegato, gambe avanti sopra il piano del corpo: bacino +8, anche +12, curl -8.
3. Build + check: clearance PASS sulle tre clip (startup min 0,07 cm f4). Primo build -0,10 cm su settle/loop (curl): braccia +2 cm dal petto.
4. setup_dropkick_kick_variant.py: env DROPKICK_KICK_SETTLE / DROPKICK_KICK_LOOP_ROUND. AM_OverlordDropkickK6 = 90 f (= originale):
   startup, settle x1, loop K6 x3, land. KIT_Test_PlaytestTBK6 (+WPN copy, GA/TL copie di TBR4).
5. Catture k6/capture_k6.ps1: k6w, k6hit, k6high, ASSERT OK su tutte con spec propria (Saved/DropkickK6/*/assert.txt). PIE resta 1 giocatore.
6. Pacchetto docs/notes/reviews/dropkick-kick-k6/k6a/ (README, K5 r3 come base, k6/review_pack_k6.py).

## Il mio sguardo (non e' la review): tiles az90 e grid_high_settle
Gambe a15-a17 sopra il bacino (V bassa), unite e dritte; braccia conserte. Press a4 lieve a 250 cm (3-4 cm).

## Aperto
- Review cloud (regola 10) su k6a: tratti PRIMA, poi vita (press/relax, settle, peso), poi tecnico. Una mossa = una review.
- Hitbox (owner): piedi ~34 cm piu' alti rispetto al bacino al colpo.
- Atterraggio: braccia che scattano 1 frame prima del suolo (clip land, coda codice).
Prove: clearance dai *_k6_check/clearance.txt; numeri delle gambe da plank_try (Poser offline) citati nel curves sheet.
COSTO: ~7 $ (stima) | 0 min desktop | 0 token review
