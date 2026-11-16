# Handover: piano anim layer skid + wall slide (2026-11-16)
Thread cloud, Opus high, solo piano, nessun codice. Costo stimato ~3 $.
Piano: docs/notes/anim-layer-skid-wallslide-piano.md (copia in project files handoffs/).
Base: feature/gas-npp-5.8 d258c41c. Letti: anim-layers.md, wall-anim.md, corsa-mario-piano.md §5, commit d67011e4 (claude/stance-offset, tocca SandboxCharacter_Mover_ABP).
Verificato sul fisso (listato in sola lettura, find su GaspMoverProject/Content): asset skid in GiustinoMoves/Skid/, wall slide in GiustinoMoves/WallSlide/WallSlideAnims/, CR_WallIkControlRig in due copie, pose sullo scheletro SK_UEFN_Mannequin.
Scelte applicate: slot di override con Blend Poses by int; layer per feature (Wall, Skid); stato da sim in struct C++; CR mani dentro il wall layer; root motion off; distance matching skid; stance resta additivo busto; svolta 180 come opzione per kit.
Da confermare all'owner: 1 stance A/B (consigliata A).
Prossimo: L0 (fisso, copia asset) e L1 (cloud, C++) in parallelo; L3 dopo P2 della corsa.
