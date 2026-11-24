# Perché import e cattura durano tanto, e come accorciarli (2026-11-23, cloud, sola lettura)

Domanda owner: "animare in unlit o simile e ridurre il tempo di import?". Fonti: branch claude/project-thread-6blrlz.

## Dove vanno i minuti (giro 2 di K8)
| Fase | Minuti | Fonte |
|---|---|---|
| animpipe + import (carta, build/check offline, avvio editor, import clip, montage, hitbox, dump giunti) | 6 (giro 1: 12, K7: 15) | docs/notes/handovers/dropkick-k8-2.md, -k8-1.md, dropkick-k7-skill-test.md |
| cattura, 5 run in una sessione | 9 (giro 1: 9,6 con 3 run) | dropkick-k8-2.md |
| resto (pacchetto, attese) | ~10 | stesso handover, 25 min totali |

Dentro la cattura (stime, i log sono sul portatile e non li ho letti):
- 5 run x 300 frame (pie_capture_move.py:133) = 1500 frame; ogni frame scrive 3 PNG (az90, az180, game) = 4500 PNG.
- Non è "tempo reale": il motore avanza 1/60 s per frame renderizzato (t.OverrideFPS 60). È lento perché ogni frame rende 3 viste in più
  e le legge dalla GPU per salvarle. Misurato: 7 viste = 3,5-4 fps (docs/notes/speed-up.md §5); con 3 viste stima ~7 fps, cioè ~3,5 min solo di scatti.
- Il resto (~5 min, stima): avvio editor (19-24 s a caldo, speed-up.md §1; minuti se deve ricompilare shader dopo un cambio branch),
  caricamento livello, 5 avvii/stop di Play, salita del salto, ASSERT.
- 2 run su 5 sono K7, già approvata e bloccata: si ricattura ogni giro una cosa che non cambia. 1 run (hitbox) serve solo all'owner.
- L'import della clip in sé dura secondi (stima): i 6 min sono avvio editor + giri di build/check animpipe + setup montage/hitbox.

## Unlit?
Si può, ma rende poco: a 480 px il costo è soprattutto lettura GPU + scrittura PNG, non l'illuminazione (stima: meno del 20 %).
E toglie la profondità: braccia davanti al petto, gomiti, conserte diventano una macchia piatta, proprio quello che la review giudica (regola 10).
Meglio "luce semplice": Lumen, ombre, AO e motion blur spenti solo nelle viste di review; le 3 luci di riempimento ci sono già.

## Opzioni
- **A (consigliata) - Cattura snella.** Base K7 presa dalla cache (si ricattura solo se cambia la catena di cattura), run hitbox solo all'ultimo giro,
  PNG solo nella finestra della mossa (20 frame prima della pressione fino a FAF+30, non tutta la salita), luce semplice su az90/az180 e vista game piena.
  Stima: cattura 9 -> 3-4 min a giro. Costo ~2-3 $ (codice cloud), prova vera al prossimo giro sul portatile. Rischio: base e variante devono avere
  gli stessi frame (regola 10) -> la cache salva anche la versione della catena e la finestra.
- **B - Un solo editor per import e cattura** (il "job server" già previsto in speed-up.md TODO). Stima -1/2 min a giro (un avvio e un caricamento in meno,
  niente shader del secondo avvio). Costo ~4-6 $, tocca script condivisi. Da fare dopo A.
- **C - Anteprima prima della cattura.** `animpipe render --standalone` esiste già (animpipe/verbs.py:9): un'immagine della posa prima di catturare,
  si scarta un giro sbagliato senza 15 min di editor. Costo ~0-1 $ (regola nella skill). Non sostituisce il video in-game.
  Cascadeur/Live Link come anteprima = cambio di strumento (regola 6): solo se lo decidi tu.

## Cosa serve dall'owner
Una risposta: 1A (solo A), 1B (A poi B), 1C (A + C). Consigliata 1C: A taglia il tempo, C evita giri inutili, B dopo se serve ancora.
Fermo intanto: nulla, K8 giro 2 continua col metodo attuale.
