# Dair variante L: revisione visiva (2026-11-03, thread cloud)
Fonte: i 6 mp4 di questa cartella (L e base, az 90/0/45, 51 frame, 384 px, b0b84c3) + mp4 K di `dair-varK/`.
Frame estratti con ffmpeg; etichette lette sul video (v27 = f19, mappa v<15 f=v, v15-22 = f15, v>=23 f=v-8).
Guardate una griglia base/K/L per vista: az0 f17-27 (zoom 2x), az90 f17-27, az45 f18-27. Diff pixel L vs K su tutti
i 51 frame delle tre viste.

**VERDETTO: PASS** (le ginocchia a rana di K sono sparite, nient'altro e' cambiato).
1. Vista frontale f19-26: PASS. In az0 f20-25 le ginocchia puntano avanti (rotula verso camera) e le gambe restano
   parallele sotto i fianchi; a f22 cosce 22 px contro fianchi 25 px (K: 34/29, base 24/27). Piedi uniti come in K,
   nessuno scatto a f19/f26 (ingresso e uscita del peso 0,4 morbidi), fianchi invariati. In az45 le gambe restano
   unite con il ginocchio avanti, nessuna apertura; in az90 la piega resta in avanti, un po' piu' marcata di K.
2. Tutto il resto come K: PASS. Diff pixel L vs K = zero su v0-26 e v35-50 (f0-18 e f27-42) in tutte le viste;
   su v27-34 (f19-26) le differenze stanno solo sotto il bacino (parte alta <=12 px di rumore). Quindi impatto f15
   con hitstop v15-22, startup f0-17 (f12-13 braccia un po' piu' alte, gia' noto), follow-through (piedi giu' a
   f19, testa e pugni che scendono fino a f23, visibile in az90), FAF 37-42 (busto appena avanti, pugni chiusi,
   braccia incrociate) sono quelli di K, gia' PASS nella review K.
3. Pugno sinistro / petto: come K, nessuna compenetrazione visibile (-0,57 cm e' sotto mezzo pixel a 384 px).
   Resta la decisione numerica dell'owner sull'esenzione hand_l|spine_03 22-25.
4. Regole owner (cards/README.md): PASS. Ginocchia davanti, piedi affiancati, niente frog knees, pugni chiusi,
   nessuna chiave owner toccata a video, nulla scatta dopo l'attivo.

Da portare all'owner insieme al video (decisioni sue, non difetti):
- accettare il contatto hand_l|spine_03 f22-25 come per la mano destra (gate clearance renderizzato con --force);
- FAF f39-42 con braccia incrociate (eredita da G) contro la base che apre le braccia e inclina di piu';
- poi "applica" o "scarta" (applica = copia a mano sulla card principale, le key 37/42 sono dell'owner).
Nessuna correzione richiesta.
Costo revisione: ~15 chiamate, contesto ~45k, circa $1,5.
