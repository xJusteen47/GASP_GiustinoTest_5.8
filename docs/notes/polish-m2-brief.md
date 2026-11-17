# Brief M2: kit VFX v1 (Niagara) per movimento e colpi
Macchina: PORTATILE (editor + MCP editor). Modello: Opus medium; review cloud Fable 5.2 high (regola 10, VFX off poi on; solo Fable firma il PASS, owner 14:26Z). Budget: 17 $. Dopo M0 (serve la tabella eventi; se M0 non è ancora entrato, preparare i sistemi e collegarli dopo).
Piano: polish-movimento-vfx-piano.md M2 + §5; checklist VFX approvata 11-15 (handoffs/silhouette-vfx-ricerca.md, artifact silhouette). Riferimento owner: riferimenti/mario3dw-toad-start-skid-owner-1117.mov (sbuffi a partenza e skid).
## Stile
Smooth / cartoon alla Fortnite / Astro Bot: forme morbide, sbuffi a pallina che si gonfiano e svaniscono, colori puliti; l'effetto non copre mai il personaggio, la taglia segue l'importanza.
## Sistemi (in /Game/GSTN/VFX/, nomi NS_GSTN_*)
Polvere partenza corsa, scia di polvere skid, sbuffo salto, anello atterraggio scalato dalla velocità, linee di velocità a sprint 2, scintille colpo 3 taglie, impatto muro/soffitto/terra, scia tumble. Parametri User esposti (scala, colore) per la tabella M0. Perf toaster: pochi sprite, niente luci/GPU sim, bound fissi.
## Fine
Clip in-game (stessi frame VFX off/on) committate in docs/notes/reviews/polish-m2/; review cloud PASS/FAIL; handover docs/notes/handovers/polish-m2.md + copia handoffs/.
