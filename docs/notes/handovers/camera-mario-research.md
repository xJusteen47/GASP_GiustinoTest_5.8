# Handover camera-mario-research (desktop senza editor, Opus high, 2026-11-21)
Richiesta (coordinatore, dall'owner): ricerca camera Mario 3D World/Odyssey/Bowser's Fury + Astro Bot, gap con la camera attuale, piano a fasi, max 3 decisioni. Solo piano.
Branch claude/camera-mario-research da claude/project-thread-hiq1zx (46f54dc7). Worktree wt-camera-agy. Nessun codice, editor e _ue59 non toccati.
Documento: docs/notes/camera-platforming/ricerca-mario-astro.md (rev. 3; copiati qui anche design.md e camera-p0-brief.md dal branch mu9mh4 per averli insieme).
Nuovo rispetto a design.md del 19/11:
- 3D Land/3D World: camera parallela a distanza fissa, voluta per non perdersi e per il multi su uno schermo (Iwata Asks, F1/F2). Manca nel plugin: modalita Parallel proposta (decisione 1).
- 3D World multi: zoom out per tenere tutti in quadro (F5/F6), criticata in parte (F7); Wonder a capofila e il controesempio (F8). Decisione 2 (co-op condivisa).
- Bowser's Fury: libera 360, segue dietro in corsa, R ricentra, si allarga su Plessie e nelle battaglie giganti: solo [N] (Neon), da vedere.
- Astro Bot rotazione automatica: Neon dice si, design.md diceva quasi mai: conflitto aperto, chiuso in P0 con 8 fotogrammi da video.
- Gap: P0 non e mai partito (nessun handover camera-p0 su nessun branch remoto: git ls-tree su tutti i branch). Pivot capsula-sicuro e tilt-shift fatti nell'anchor (jitter-3 non compilato).
Piano: P0 (fisso) | P1a nuovo, codice+test headless nel plugin, si puo fare subito in cloud | P1b | P2 ostacoli | P3 focus + Parallel | P4 livello di prova | P5 editor/co-op. P0-P4 ~39-52 $.
Decisioni owner (§5): 1 Parallel (A per area/livello, consigliata), 2 co-op stesso schermo (A no per ora, consigliata), 3 tempi (A P1a subito in cloud, consigliata).
Neon: 2 web_lookup (sessioni 27f3d174, b58fe0c6); diversi link sembravano generici o inventati, quindi i suoi punti restano [N] e non entrano nei valori. Verifiche web: 3 ricerche (Iwata Asks, 3D World multi, opzioni Fury).
Prossimo: se 3A, brief P1a (cloud, Opus high, 8-10 $) da §4 del documento.
COSTO: ~5 $ | 0 minuti desktop | 0 token review
