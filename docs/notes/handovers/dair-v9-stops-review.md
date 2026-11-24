# dair-v9-stops-review (fisso, 2026-11-24, Opus; nota coordinatore, owner scelta B, budget ~4 $) - branch claude/dair-v9-stops-review
FATTO
- Cattura Play della dair v9 approvata in _ue59 (asset = 916338ad), ASSERT: OK, 1 tentativo 4,6 min; lock preso e liberato, nessun
  commit ne' salvataggio in _ue59. Pacchetto livello 2 completo in docs/notes/reviews/dair-v9-stops/ (review.md = esito e prove).
- Critico cieco Opus high: FAIL vita e fermate. a15-17 statua + pop a18 (stop-and-go), a23-26 quasi statua, affondo 17-26 invisibile
  (~1 cm/frame), corpo sospeso all'apice in gioco, gambe a X da dietro f48-58.
- Metrica stop_sync: a17 e a25 fermate VERE; a4 tenuta viva da dichiarare (carica a4-a12); a35 falso allarme (mai a schermo: il
  montage in gioco finisce ~a34). Controllo sul dump: bacino a15-18 1,0/1,0/0,2/0,6 cm, a24-27 0,6/0,2/0,2/0,8 (review.md).
- Owner ("limbo a mezz'aria") dato al critico dopo il verdetto: coincide (busto indietro, ginocchia avanti, sospeso).
- 8 proposte per una variante (review.md), 2 marcate DA DECIDERE OWNER (palla aperta 1A, spinta in basso vs "niente stallo" 3A).
PER L'OWNER / COORDINATORE
- La v9 resta bloccata e approvata; la riga dair di life_calibration resta un caso da correggere nella metrica, non nella clip:
  contare eventi (8 parti sullo stesso frame = 1) e ignorare i frame fuori dal montage in gioco, poi ritarare (regola 17).
- stop_sync non diventa bloccante finche' quella correzione non passa la taratura.
- Il pacchetto finale vuole base + variante (INV-06): per la review di una base approvata serve un modo "solo base" nel tool.
- Cattura: export PNG = 96 s su 99 per sequenza, GPU ferma. GSTNCapture (plugin sul branch skill) non e' compilato in _ue59.
- Gioco: montage dair finisce a ~a34 (FAF 42 mai mostrato); in dair_hit_rise la vittima resta a terra (cattura colpo da rifare).
SUBAGENTI: 2 (cattura + pacchetto: Sonnet; critico cieco fasi 1-4: Opus high)
COSTO: ~4,5 $ (stima: coordinamento ~1,2, Sonnet ~0,6, critico Opus ~2,7) | 8 minuti desktop (cattura 4,6 + video/pacchetto) | ~200k token review
