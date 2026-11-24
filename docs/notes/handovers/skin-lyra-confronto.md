# Handover: Lyra vs skin (2026-11-23)
Obiettivo: confrontare il sistema cosmetico di Lyra con i piani skin (cosmetici modulari + proporzioni). Solo ricerca.
Esito: doc docs/notes/skin-lyra-confronto.md (copia in project files skin-cosmetici/skin-lyra-confronto.md).
- Lyra non è sul PC (C:\Users\jacop\Documents\Unreal Projects: solo GASP e worktree): confronto da codice Lyra noto + note pubbliche x157.
- Quasi tutto il pattern Lyra c'è già (loadout su PlayerState, UGSTN_SkinComponent asincrono, anim layer da tag in GSTNAnimation).
- Da prendere: parti temporanee con handle, tag dalle parti, collisione per parte nel validatore, forza skin/tema da GSTN Debug.
- Da non prendere: cambio del corpo invisibile per corporatura, ChildActorComponent per pezzo, riferimenti hard.
- Stima prova cosmetica: 16-25 $ (prima 16-26).
Decisioni CONFERMATE dall owner 2026-11-23: 1A corpo invisibile fisso, 2A parti con handle in F3, 3A idea riscritta, non copia.
Da verificare: nomi esatti delle classi Lyra e testo EULA della versione in uso.
SUBAGENTI: 0
COSTO: ~1,5 $ | 0 min desktop | 0 token review
