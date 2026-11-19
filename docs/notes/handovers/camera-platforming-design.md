# Handover camera-platforming-design (cloud, Opus high, 2026-11-19)
Richiesta owner 17:27Z: camera alla Astro Bot/Bananza/Odyssey, distanza regolabile, punti di autofocus del designer; ADS separata. Solo studio e design, nessun codice.
Branch claude/project-thread-mu9mh4 (da feature/gas-npp-5.8). Documento: docs/notes/camera-platforming/design.md (copia project files handoffs/camera-platforming-design.md).
Scoperta: il plugin GSTNCamera (aree, binari, Free alla Astro, DistanceScale, 44 test) esiste ma non è mai stato collegato a un personaggio; le sue impostazioni sono in Project Settings (contro la regola owner). Il vecchio piano C0-C3 non lo citava.
Proposta: plugin GSTNCamera come cervello, UGSTN_CameraAnchorComponent come soggetto (pivot pulito da capsula/crouch); nuovi: distanza regolabile (preset 0,75/1/1,3 + slider + limite d'area), AGSTN_CameraFocusPoint (Frame/Look/Reveal, Soft/Firm/Lock), retino + silhouette. Fasi P0-P4 ~40-54 $ (+P5 editor 6-8 $).
Decisioni owner (§8): 1 base (A plugin nostro), 2 rotazione automatica default (A come Astro, spenta), 3 piano/tempi (A sostituisce C0-C3, P0 dopo la locomozione).
Applicate da confermare (§7): settings → Data Asset, un solo air hold, preset distanza, croce su/giù per la distanza, retino default.
Gemini: prompt di ricerca alla cieca (agy-camera-research-prompt.md) e di review (agy-camera-review-prompt.md); esiti da integrare in §9 prima di portare le decisioni all'owner.
Prove: §2 dal codice (git show origin/claude/ue59-sync-1119:Plugins/GSTNCamera/README.md e GSTN_CameraTypes.h); misure Astro da docs/notes/reviews/ref-astrobot-camera/README.md (branch pg3fen). Numeri §4 non misurati = [A].
Costo stimato thread: ~5 $ finora.
