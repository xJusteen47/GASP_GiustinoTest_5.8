# Dair variante K: revisione visiva (2026-11-03, thread cloud)
Fonte: i 6 mp4 di questa cartella (K e base, az 90/0/45, 51 frame, 384 px, cdd7ca1). Frame estratti con ffmpeg,
mappa v->f: v<15 f=v, v15-22 = f15 (hitstop), v>=23 f=v-8. Guardate griglie base/K f12-27 per vista, f33-42 tutte le
viste, zoom 4x petto f17-25 e gambe az0 f17-26. Quote in px sulla sagoma (personaggio ~145 px).

**VERDETTO: FAIL** (punto 5: ginocchia a rana nella vista frontale). Il follow-through chiesto ora si vede.
1. Impatto f15 / startup: PASS. f15 = posa d'impatto, congelata v15-v22. f0-9 e f14-17 uguali alla base (diff solo
   etichetta/rumore), f10-13 braccia un po' piu' alte (key 12 di G, nota). Tra K e I: f0-17 identici (diff <40 px).
2. Follow-through: PASS. Base ferma f15-27 (testa 150-151 px, piedi 285-286). In K i piedi toccano il minimo a f19
   (294 px) poi risalgono con la piega delle ginocchia, mentre la testa continua a scendere 158->163 px f19->23 e il
   petto si incurva, pugni in avanti e in giu' f21-24; risalita f25-27. Si legge come sovrapposizione degli arti,
   non come blocco unico. Ampiezza modesta (5 px di testa) ma visibile in sequenza.
3. Pugno sinistro / petto: nessuna compenetrazione visibile in nessuna delle tre viste f20-25. A 384 px -0,57 cm
   e' meno di mezzo pixel: il video non puo' ne' confermarla ne' smentirla. Resta un fatto numerico; l'esenzione
   hand_l|spine_03 22-25 la decide l'owner.
4. FAF 37-42: PASS con nota. K uguale a I (diff <130 px); busto appena avanti, braccia incrociate chiuse, avambracci
   non nel petto. Ma rispetto alla base la differenza e' grande: nella base a f39-42 il busto e' piu' inclinato e le
   braccia si aprono (gomiti larghi, mani ai fianchi), in K restano incrociate. Viene dalle modifiche FAF di G,
   non da K. L'owner deve vederlo quando decide applica/scarta.
5. Regole owner: FAIL. Vista frontale (az0) f21-24: con i piedi uniti le ginocchia si aprono verso l'esterno, le
   gambe fanno "( )" (larghezza cosce 34 px contro fianchi 30 px a f22; base 23/27). E' la "frog knees" che la
   README chiede di controllare. In laterale (az90) la piega e' corretta, in avanti. Nessun'altra violazione vista
   (pugni chiusi, nessuna chiave owner toccata a video, nulla scatta dopo l'attivo).

**Correzione unica consigliata**: tenere la piega delle ginocchia f20-25 com'e' in ampiezza e timing, ma ruotare le
cosce verso l'interno (o la direzione del ginocchio) cosi' che le ginocchia puntino avanti e restino entro la
larghezza dei fianchi in az0 (cosce <= fianchi a f22). Non toccare bacino, testa, braccia ne' frame 0-17.
Se l'owner vuole tenere K com'e' ora, deve accettare anche le ginocchia aperte e l'esenzione hand_l.
Costo revisione: ~20 chiamate, contesto ~60k, circa $2.
