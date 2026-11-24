# dropkick-k7-k8-sguardo-slowmo (cloud, sola lettura, 2026-11-23, Opus high) - cosa si VEDE nello slowmo di K7 e K8
Fonte: k8r2/k7_whiff_*_slow4x.mp4, k8_whiff_*_slow4x.mp4 (senza hitbox; 15 fps = 1 tick per fotogramma, montage a0 = v138).
Griglie guardate una volta: k8r2/sguardo-cloud/{k7_az90_tile,k8_az90_tile,ab_az180_startup,k7_land_az90,k7_land_180_game}.jpg (5 immagini).
## Difetti visti (v = video, a = frame montage = v-138)
1 [posa/dati] STACCO SUL DECOLLO, K7 e K8: v138->v139 (a0->a1) le braccia saltano in UN frame da aperte/indietro a davanti (K7) o a
  pugni sopra la testa (K8, molto peggio, dietro az180). Poi K8 resta FERMO v139-v143 (a1-a5, 5 frame identici a pugni alzati).
2 [posa/dati] PALLA FERMA K7: v144-v147 (a6-a9) raccolta quasi identica da dietro e di lato: 4 frame congelati (dati: f7-f8, si vede di piu').
3 [posa/dati] CHIUSURA K8 IN UN FRAME: v147->v148 (a9->a10) braccia da larghe/aperte a conserte e corpo a palla di colpo; poi
  palla ferma v148-v150 (a10-a12). Tra v144 e v147 K8 si apre lentamente: deriva lenta + scatto = stop-and-go visibile.
4 [posa] Tutte le parti arrivano sulla chiave nello STESSO frame (braccia, testa, gambe insieme): nessuna sovrapposizione.
5 [dati montage, settle K6 condiviso] A terra dopo il whiff il corpo resta piatto e immobile v158-v163 (~6 frame), poi gambe e braccia
  si alzano a v164-v168: un'altra fermata, non coperta dai dump.
6 [cattura] Le camere az90/az180 saltano a v156-v158 (inquadratura cambia di colpo, la vista game no): sembra un pop del corpo ma e'
  la camera. Non conta come difetto di posa; da sistemare nella catena (camera che segue senza scatti) perche' inganna chi guarda.
OK: estensione gambe v151-v153 (2 frame) si legge come scatto voluto; non visti frame doppi o saltati.
## Diagnosi dei dati: CONFERMATA (stop su chiave in K7 a6-a9, K8 a1-a5 e a10-a12, chiusura K8 in un frame). In piu' il video mostra
lo stacco a0->a1 (decollo) e la fermata a terra del settle K6, che i dati non coprivano.
## La correzione A basta? NO da sola. Serve A + 3 aggiunte:
- a) spalmare anche a0->a2 (decollo su 2-3 frame partendo dalla posa del run-in), non solo la chiusura a9-a12;
- b) le tenute "che respirano" devono avere pose davvero diverse (tangenti automatiche tra chiavi quasi uguali danno ancora ~0 cm):
  il cancello < 3,5 cm le trova, ma la correzione e' di posa (K8 a1-a5 da ridisegnare come tenuta in movimento);
- c) offset 1-2 frame di braccia e testa (opzione B leggera) dopo A, per togliere l'arrivo simultaneo;
- d) stesso cancello anti-stop sul settle/loop K6 (v158-v163) prima di catturare; camera az90/az180 senza salti (voce gioco/cattura).
Costo stimato A + a-d: ~3 $ (1 build + 1 cattura), invariato l'ordine: prima K7 variante, poi K8.
SUBAGENTI: 0
COSTO: ~1,5 $ (stima) | 0 minuti desktop | ~45k token review
