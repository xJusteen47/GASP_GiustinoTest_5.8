# Handover: skill e libreria di riferimenti per le animazioni (2026-11-15, cloud, solo doc)

Branch `claude/project-thread-ybl6a8` (da `claude/project-thread-0tmywc-docs`, mai su feature). Nessun codice toccato.
Documento: `Artifacts/Animazione/skill-e-riferimenti.html` = https://claude.ai/artifact/TXAt4tWjuWW5CfnjXH6o63

## Raccomandazioni (in attesa del sì dell'owner)
Modello (owner 11-15 00:49Z): REVISORE (Claude) giudica e scrive UNA istruzione esatta alla volta; ESECUTORE (Antigravity o modello
economico) la applica e mostra prima/dopo. Antigravity non crea pose buone da testo né da pose minime. Pose di partenza mai da zero.
1. Skill del revisore: video regola 10 + domanda di qualità + istruzioni a 5 campi (frame, parte, modifica, principio, verifica). ~$8-12.
2. `gstn-move-animation` (35 KB) ridotta a skill dell'esecutore. ~$5-8. Da decidere: 2 tentativi per istruzione, tetto 6 per mossa.
3. Libreria riferimenti "solo numeri" in `Tools/AnimPipeline/reflib/` (indice.csv, <id>_timing.csv, classi.csv); immagini/GIF/video
   solo in `Saved/AnimPipeline/refs/` sul desktop (già escluso da git). Pilota 10 mosse ~$10-15, poi ~$1 a mossa.
4. Confronto nella review: tempi contro la classe, curva di estensione, affiancato di lato, sei domande. +$0,5-1 a review.
5. No: animazioni/modelli estratti dai giochi; skill per teoria, regole di feel, debug lato gioco, processo Antigravity.

## Nuova soglia di review (owner 11-15 00:24 e 00:26Z)
"Questa animazione è alla pari di una della qualità di Smash Bros o di altri giochi tripla A simili? Se no, si rivede."
Proposta operativa: sei domande sì/no (posa d'impatto, contrasto di tempo, primo frame, peso, recupero, pulizia) + affiancato con due
riferimenti della stessa classe; se no, max tre scarti con frame e correzione; resta il limite di due giri.

## Fatti verificati
- Esistono già: `ufd_fetch.py`, `ufd_measure.py`, `ref_keypoints.py`, `ref_keyposes.py`, `motion_ref.py`, `ref_sidebyside.py`,
  `mario_compare.py` (branch anim4x). Misurate: jab Mario, ftilt Terry e Ganondorf, fsmash Ryu e Little Mac.
- Skill nel repo: `.claude/skills/animpipe-move` (SKILL 5 KB + rubric, review, rules). `gstn-visual-review` è confluita lì.
- Principi oggi: i 2 doc anim-theory sono su feature, il terzo su ujyw42; NESSUNA skill li cita, letti solo se il brief li nomina.
- I riferimenti Smash sono solo di lato: per la vista da dietro non c'è confronto (principi + Kill la Kill IF).
- Il passo "affiancato" richiede che il revisore veda il riferimento: review sul desktop o griglia mandata al cloud e mai committata.

## Aperto
- Sì dell'owner ai punti 1-3. Domanda: registra lui 5-10 clip da dietro, o da dietro si giudica solo con i principi?
- Licenze delle fonti non verificate sul web (conoscenza generale; non è un parere legale).

Costo stimato del thread: ~$8-10 (Fable, ~260k token in ingresso con cache, ~12k in uscita).
