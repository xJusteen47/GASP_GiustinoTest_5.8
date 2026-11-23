# Dropkick K7 (giro 3 di K6, decisione owner 1B) - pacchetto di review, 2026-11-23, portatile
Base = K6 r2 (KIT_Test_PlaytestTBK6), variante = K7 (KIT_Test_PlaytestTBK7). Stesse impostazioni: pressione a 480 cm, camere a 350 cm,
viste az90 (lato), az180 (dietro), game. Montage da v138 in entrambe, colpo sul manichino a v153 (danno 0 -> 10) in entrambe.
ASSERT: OK su tutte e 5 le run (base_whiff, base_hit, k7_whiff, k7_hit, k7_hit_hitbox).

## Cosa cambia (solo startup a1-a12; snap a14-a15, settle K6, loop K6, gambe unite, linea, smug NON toccati)
1. Ali tenute a1-a3 direttamente dalle braccia larghe del salto (niente guardia), stretta a4-a5.
2. Pugno sinistro a lato della testa (8 cm fuori, 7 indietro rispetto a K6).
3. Rilascio a8-a9 piu' largo: gomiti fuori dalla sagoma del busto da dietro 10,9 / 8,5 cm (a8), 10,4 / 8,4 (a9) (silhouette.json az180, pad 7 cm).
4. Il bacino guida il tuck da a1: 25 / 70 / 92 % verso la palla a a1 / a2 / a3 (breakdown 70 % a a2).
5. Palla stretta da a5 (talloni-glutei 19 cm, K6 43), compressione crescente fino ad a12 (ginocchia-petto 26 cm, talloni 8 cm), punte tese.
Clearance offline 0,41 cm (minimo, path a3-a5), UE PASS (animpipe check). co_motion PASS.

## File
- grid_startup_whiff_{az90,az180}.png: f139-149 fitti (startup), base sopra K7 sotto. Le griglie review_pack standard
  grid_dropkick_sweet_{whiff,hit}_{az90,az180,game}.png prendono 144-154 (attorno a rilascio e colpo).
- *_real.mp4 / *_slow4x.mp4: v130-200. k7_hit_hitbox_* = run con gstn.Combat.Debug.Hitboxes 1.
- silhouette_*.png, motion_*.png: anim_silhouette_check / anim_motion_overlay K7 vs K6, clip f0-14 (dump UE, CJD_FACING=y).
## Note per il revisore
- Lo sfondo cambia tra base e K7 (la sessione del kit K6 e quella del K7 partono da spawn diversi): solo la cornice, non la mossa.
- motion: 2 pop alle caviglie a f4 (arrivo della palla, anche K6 ne ha 2); silhouette "illeggibile" quasi ovunque e' taratura
  dello strumento sul tuck (testa e piedi sempre vicini al busto), non un giudizio: usare le distanze dei gomiti.
## Comandi
python Tools/AnimPipeline/cards/overlord/dropkick/k7/make_k7_cards.py (cwd Tools/AnimPipeline); animpipe author/build/check sulla
carta; setup_dropkick_kick_variant.py (DROPKICK_KICK_ROUND=K7 SETTLE=K6 LOOP_ROUND=K6); setup_dropkick_hitboxes.py
(DROPKICK_TL=TL_Overlord_DropkickTBK7); cattura k7/capture_k7.ps1 (runs_*.json, assert_*.json accanto).
