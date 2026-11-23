# Dropkick K8 giro 2 (review 1: gambe a11-a12, cattura, a1) - pacchetto, 2026-11-23, portatile
Base = K7 approvata (KIT_Test_PlaytestTBK7), variante = K8 (KIT_Test_PlaytestTBK8), UNA sessione, viste az90 (lato), az180 (dietro), game.
ASSERT OK su k7, k8, k8_hitbox. Colpo sul manichino in entrambe. Allineamento per montage (alignment.json): whiff k8 +1 frame.
## Cosa cambia (solo a1-a12)
Porta da calcio tenuta a1-a10 (tenuta che respira), chiusura a10-a12, conserte alte con gomiti fuori dal busto, spalle su attorno alla
testa centrata, suole piatte, palla +16 gradi indietro. Snap a14-a15, settle, loop, gambe unite, hitbox: come K7.
## File
- startup/grid_dropkick_sweet_whiff_{az180,az90,game}.png: f140-152 passo 2 (lo startup, il pezzo cambiato). Griglie standard 149-156.
- ab_{whiff,hit}_{real,slow4x}.mp4: K7 sopra, K8 sotto, 3 viste. Singole: k7_*/k8_*. hitbox/: con le forme (solo per l owner).
- silhouette_*.png + silhouette.json: K8 vs K7, clip f0-15. gate_report.json: cancello della skill (FAIL ereditato, non forzato).
## Note per il revisore
