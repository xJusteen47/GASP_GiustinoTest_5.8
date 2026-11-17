# Handover: ricerca stile Astro Bot / Nintendo + piano telecamera (2026-11-17)
Thread cloud, Fable 5.2 high, solo ricerca e documenti, nessun codice, _ue59 non toccata.
Richieste owner: 14:42Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNPHx6iiXBoZNJy1DkBc4VPc) stile; 14:44Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCqFtaiorumQRbABLXoBEn7) telecamera, marker olografico, asse "giù".

## File (docs/notes/stile/, copie in project files handoffs/)
- stile-astrobot-nintendo.md: cosa caratterizza lo stile e ricette UE (VFX, materiali, luce, post).
- stile-sheet-vfx-review.md: regole di costruzione per M2 + checklist di review a 12 voci per Fable.
- camera-astrobot-piano.md: studio telecamera, modello a tipi + aree (Galaxy), piano C0-C3, ~34-42 $.

## Scelte applicate (regola 6), reversibili
Nessun outline; sbuffi Masked opachi a 2 toni che rimpiccioliscono; PBR morbido senza toon; tonemapper ammorbidito via Post Process Volume senza toccare il motore.

## Da confermare dall'owner (13d)
1. Nessun outline (o prova di un contorno).
2. FOV stretto 45-55 e valori telecamera §4.
3. Telecamera: ambito (solo spedizioni, consigliata) e quando (dopo ondata 1, consigliata).
4. Checkpoint nuovo "Look v1" (materiali, luce, post su L_Test_Hub), 10-14 $: M2 copre solo i VFX.

## Per altri thread
- M2 (portatile): usare lo style sheet sezioni A-C; la review usa la sezione D.
- Depth marker: stile olografico (anello sottile additivo, niente disco pieno) e asse comune `GSTN_DownAxis` (default gravità, sovrascrivibile anche di lato), letto da marker, ombra e telecamera. Non costruito.

## Limiti e prove
- Fonti di studio solo per: PBR/Fresnel/speculari di Odyssey, ombra a picco di Mario, effetti "colla" e stelline, tangibilità di Astro, formato telecamere di Galaxy. Parametri di sbuffi, post e telecamera di Astro = analisi mia, senza video guardato: C0 del piano camera li misura.
- Stato del Gameplay Camera System in 5.9 non verificato (Experimental fino a 5.7). Unico indizio nel repo: `gh api -X GET search/code -f q='GameplayCameras repo:xJusteen47/GASP_GiustinoTest_5.8'` dà 1 risultato, Config/DefaultEngine.ini (ramo di default).
- Nomi dei campi UE e valori numerici: punti di partenza, da regolare in editor.
Costo stimato: 6-9 $ (circa 30 ricerche e letture web, 3 documenti).
