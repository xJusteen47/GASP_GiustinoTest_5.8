# dropkick-k8-4-piano-r3 (portatile, offline, 2026-11-24, Opus medium) - piano e carte del giro 3 di K8 (variante K8R3, K7 bloccata)
Fonti: dropkick-k8-3-diagnosi.md (stop su ogni chiave), dropkick-k7-k8-sguardo-slowmo.md (qsgab4). Nessun editor, nessuna cattura.
## Carte pronte
- k8/make_k8r3_cards.py -> overlord_dropkick_startup_k8r3.json (asset AS_OverlordDropkick_StartupK8R3, take Pilot ...K8R3).
  Pose Doom invariate (porta da calcio, conserte larghe e alte, spalle su e testa centrata, suole piatte, gambe r2, snap K7):
  1 decollo su 3 frame (a1 35 %, a2 75 % dalle braccia del salto, a3 arrivo); 2 tenuta VIVA: 4 chiavi diverse (a3 arrivo, a5 si gonfia,
  a8 tensione, a9 carica indietro), braccia 8-19 cm/frame; 3 chiusura a9-a12 su 3 frame: 42 / 38 / 1 cm (offline, report
  %TEMP%/k8/k8r3_report.txt) con piccolo overshoot ad a11; 4 part_delays testa/avambraccio/mano +1 frame; 5 tutte le chiavi fino ad a11
  `limb_led_recovery` (tangenti automatiche, senza fermata), a12 charge_long nello snap.
- Clearance offline minima 0,47 cm (a11, overshoot): il check UE decide; se fallisce, K8R3_OVER=0.
- k8/no_stop_gate.py: cancello anti-fermata sul dump (fermata = tutto il corpo < 3,5 cm/frame; pop = passo > 4x la mediana).
  Taratura: K7 FAIL (fermate 7-8, pop mani 40 cm a f2), K8 r2 FAIL (fermate 2-3 e 11, pop 56-59 cm a f9). Da usare anche su settle/loop K6.
## Ordine quando l'editor e' libero e il build campiona ogni frame (thread del build)
1 duplicare AS_Pilot_OverlordDropkick_StartupK7 -> ...K8R3; 2 animpipe author/build/check sulla carta; 3 dump UE di StartupK8R3 +
SettleK6 + LoopK6 (CJD_FACING=y), no_stop_gate su tutti: se FAIL, niente cattura; 4 variante DROPKICK_KICK_ROUND=K8R3 SETTLE=K6
LOOP_ROUND=K6 (mai REFRESH=1), hitbox TL_Overlord_DropkickTBK8R3; 5 cattura come capture_k8_r2.ps1 (run a vuoto prima), base K7.
Settle K6 fermo ~6 frame a terra (v158-v163): e' clip condivisa con la K7 approvata: serve una sua variante (decisione: thread clip atterraggio).
## Per il motore di cattura (altro thread)
Le camere az90/az180 saltano a v156-v158 (game no): sembra un pop del corpo, e' la camera.
SUBAGENTI: 0
COSTO: ~1,2 $ (stima, offline) | 0 minuti desktop | 0 token review
