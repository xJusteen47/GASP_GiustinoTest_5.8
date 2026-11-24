# blender-preview-1 (2026-11-24, cloud, Sonnet medium budget 5 $) - anteprima pose in Blender, PROTO

Obiettivo (owner 2B): dato lo scheletro del manichino e la clip di una mossa, griglie e video brevi da dietro e di lato
in pochi secondi, stessi frame della cattura in gioco, per scartare giri sbagliati. Solo PROTO (regola 6), mai review.

Fatto (branch claude/project-thread-8esu46):
- `Tools/AnimPipeline/blender_preview/pose_preview.py` + README. Blender 5.3 come modulo Python (`pip install bpy`),
  nessuna installazione di Blender, nessun MCP, nessun editor UE; in cloud gira con GL software (lavapipe).
- Input = FBX della clip (ossa UEFN). Corpo proxy a capsule tra le giunture in posa, ricostruito a ogni frame
  (sinistra blu, destra arancio; `--silhouette` nero piatto). Viste come capture_ue/ingame_judge (az 180 dietro,
  az 90 lato, fov 55, 450 cm, el 12). Frame anim o `v<N>` con hitstop (riusa `animpipe/vframes.py`).
  `--base` mette base e variante sugli stessi frame; `--video` mp4 60 fps con il freeze. Ogni immagine marcata PROTO.
- Esempio: `docs/notes/reviews/blender-preview-proto/dash-r3-vs-r2/` (griglie az180, az90).

Prove (regola 11): tempi stampati dallo script, log /tmp (non in git), comandi nel README:
- dash r3, 9 frame x 2 viste: 10,0 s; dash r3 vs r2, 6 frame x 2 viste x 2 clip + 2 video: 9,9 s;
  video az90 = 29 frame (25 anim + 4 hitstop con --at 6 --hitstop 4), contati con ffprobe -count_frames.

Tentativi (regola 5): 2 problemi risolti al primo fix ciascuno:
1. crash di Blender dopo 14 render con ~160 oggetti in scena -> griglia del pavimento in una sola mesh;
2. proxy con modificatore Armature si smontava (ossa FBX importate con roll/lunghezze indovinate) -> capsule
   ricostruite dalle posizioni delle giunture, come leggono i check di clipcheck.

Limiti: proporzioni generiche, non la mesh di Overlord (niente contatti o compenetrazioni); niente root motion di
Mover, capsula, IK, blend, hitbox. Le clip fatte solo in editor vanno prima esportate (clipcheck/export_clips_ue.py, fisso).
Da fare (non in questo thread): voce in docs/INDICE-PROGETTO.md (branch claude/indice-progetto) "Anteprima pose
Blender PROTO: Tools/AnimPipeline/blender_preview/"; eventuale passo `animpipe render --blender` se l'owner lo vuole.

SUBAGENTI: 0
COSTO: ~2 $ | 0 minuti desktop | 0 token review
