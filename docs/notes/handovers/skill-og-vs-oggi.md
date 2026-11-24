# skill-og-vs-oggi (cloud, sola lettura, 2026-11-23, Opus high, budget 3 $) - buchi della skill animazione rispetto alla OG
Pagina: docs/notes/review/skill-og-vs-oggi-2026-11-23.md (copia in project files handoffs/). Nessun codice toccato.
Fonti: OG = Tools/AnimPipeline/contract/archive/gstn-move-animation_2026-11-15.md (+ copia account sincronizzata del 20/11);
oggi = btrk7b 7af96c4a (skills-public anim-reviewer/executor, project.md, rubric.md, gstn-anim-cycle-cheap), qsgab4 d7222a0a
(sguardo slowmo), 50f3a529 (diagnosi), 7f3f61d6 (review K8 r2), 3f950a23 (piano K8 r3 con no_stop_gate.py).
## Buchi principali
1 nessun cancello di FERMATA (animpipe checks.py solo co_motion; kb_motion della OG solo su m6s9y2, mai unito);
2 motion_overlay segnala solo pop > 3x e > 30 cm/s: una fermata e' invisibile; prodotto solo per K7, assente in K8 r1/r2;
3 scie dell'esecutore solo sulle chiavi toccate; 4 review_pack 8 immagini attorno ad anchor/FAF salta lo startup;
5 revisore senza mp4 (cheap r.30): K8 r2 PASS su ab_whiff_real, a1-a5 congelati letti come "porta da calcio leggibile";
6 carica_viva solo nota; 7 basi ereditate e settle K6 mai rigiudicati; 8 rubrica mai tarata sul video.
## Proposta: A consigliata (~3-4 $): no_stop_gate in animpipe check, fermate + striscia velocita' in motion_overlay,
pacchetto 8 immagini con 2 schede velocita' e 1 riquadro slowmo startup, carica_viva bloccante sul corpo intero.
B ~1,5 $ solo testo; C ~7-9 $ A + kb_motion + taratura + spline per frame.
## Aperto
- Owner: scelta A/B/C; togliere o aggiornare la skill account gstn-move-animation (cita strumenti non nel repo).
- Righe dei numeri: verificate leggendo i file citati (git show <branch>:<file>, grep -n); nessuna misura nuova.
SUBAGENTI: 0
COSTO: ~2 $ (stima) | 0 minuti desktop | ~0 token review (nessuna immagine guardata)
