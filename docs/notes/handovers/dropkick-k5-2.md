# dropkick-k5-2 (portatile, 2026-11-21, Opus) - giro di posa 2 del calcio: braccia conserte "a piega". Build + catture fatte, review cloud da fare
Branch claude/project-thread-uvzm3k (da 72dtra ac6dacee + review bo3bag e332fb64), checkout GASP_Giustino2_n216qs. Timeline/hitbox di gioco NON toccate.

## Fatto
1. Fit nuovo delle braccia (k5/fold.py): ricerca nel frame anatomico del petto con le STESSE forme del check di compenetrazione
   (physbodies.json). Calibrato sul giro 1: offline -5,0/-4,7/-2,8 cm contro UE -5,05/-4,49/-2,86 (clearance.txt del giro 1).
   Piega: spalle in avanti (clavicole ~25 gradi), gomiti avanti e dentro (sx 16/29/-7, dx -7/20/-17 cm da spine_05), avambracci
   orizzontali sx sopra dx, pugno dx piegato indietro sotto il gomito sx, nessun pugno davanti ai gomiti. Risultato in k5/fold_r2.json,
   applicato da make_k5_cards.py su tutte le chiavi (startup e loop); gambe, tronco, durate e scatto a13->a15 del giro 1 invariati.
   Chiave 1 = breakdown (braccio dx in anticipo e davanti al petto), braccia piegate dalla chiave 2.
2. Build startup/loop K5 + check: clearance PASS, minimo 0,43 cm (giro 1: -5,41) - Saved/AnimPipeline/animpipe/overlord_dropkick_startup_k5_check/clearance.txt.
   legibility/contact_faf FAIL = matplotlib mancante sul portatile + lock a f15 voluto; hitstop_snap/co_motion come giro 1 (voluti).
   setup_dropkick_kick_variant.py run(refresh=True): AS/AM K5 aggiornati (Saved/DropkickK5/setup_dropkick_kick_variant.txt).
3. Catture (k5/capture_r2.ps1; camere laterali a 250 cm, manichino a 190 cm invece di 150): K5 whiff ASSERT OK (Saved/DropkickK5/k5r2w/assert.txt),
   hit e pressione alta 700 cm ASSERT OK offline (capture_assert.py, assert_offline.json in k5r2hit e k5r2high), base whiff ASSERT OK (baser2).
   Whiff e hit in sessioni separate: nella stessa sessione il manichino dell'hit resta davanti alla camera laterale del whiff.
4. Pacchetto review: docs/notes/reviews/dropkick-kick-k5/k5r2b/ (griglie az90/az315/game base vs K5, mp4 az90/az315/game 1x e 4x,
   tiles 480 px di a3/a15/a18/a22, caduta alta k5high_* + grid_k5high_fall.jpg). La cartella k5r2/ (non versionata) e' la prima cattura, da ignorare.

## Da guardare / aperto
- a3: la piega NON si legge ancora. Causa: il build ritima lo startup (spacing "charge_long"): nel clip f1-f2 restano la posa f0, f3 a meta',
  piega da f4 (clearance.json per frame: 15,3/15,1/14,5/3,9/0,7...). Anticiparla = spacing o part_delays delle braccia: prossimo giro, se serve.
- Vista az90 a15-a18: coi gomiti avanti e il corpo sdraiato, avambraccio/gomito si alzano sopra il petto (verso il cielo). Da giudicare nel video:
  i gomiti avanti li chiedeva la review; se a lato si legge ancora come "esce dal petto", l'alternativa e' gomiti piu' bassi ai fianchi con
  avambracci sul ventre (meno Bison, piu' piatta).
- Caduta alta: anche a 700 cm restano 14 frame di loop prima dell'atterraggio (cade a ~24 cm/frame): un solo ciclo visibile.
- Overshoot dopo a15 NON fatto: il loop cicla ogni 12 frame, un overshoot nel loop pulserebbe a ogni ciclo; serve un segmento one-shot = montage.
- Chiudere l'editor di sessione dopo un build crasha su un assert della cache di compressione (AnimSequence.cpp:5291), dopo i salvataggi: innocuo qui.
- Gioco (noti): manichino mai colpito, niente hitstop.
Prove: numeri di clearance dai file citati; posizioni da `python cards/overlord/dropkick/k5/fold.py eval <card> <png>` (Tools/AnimPipeline).
COSTO: ~7 $ (stima) | 0 min desktop | 0 token review
