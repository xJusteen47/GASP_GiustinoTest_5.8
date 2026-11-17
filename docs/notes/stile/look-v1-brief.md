# Brief Look v1: materiali, luce e post in stile Astro Bot / Nintendo
Approvato dall'owner 2026-11-17 14:51Z (3A, cmsg_01CRvUdrWm1dJrM4q54YDBLNRKWcVYMcjNU9F14B9PQP7B). Parte DOPO M2.
Macchina: PORTATILE (editor + MCP editor). Modello: Opus medium; review cloud Fable 5.2 high (solo Fable firma il PASS). Budget: 12 $. Non aggiorna _ue59.
Fonti: handoffs/stile-astrobot-nintendo.md §3-4, handoffs/stile-sheet-vfx-review.md sezione E. Non rileggere altri thread.
## Cosa fare (solo su L_Test_Hub, tutto a dati, niente Project Settings, motore intatto)
1. Cattura PRIMA: 3 camere fisse (personaggio vicino, campo medio, campo largo), stessi frame per il DOPO.
2. Post Process Volume Unbound: esposizione manuale, Tone Curve Amount 0,6-0,8, Toe 0,4, Saturation 1,1-1,15, bloom 0,2-0,4, spenti lens flare, aberrazione, grana, motion blur; DOF spento. Verificare i nomi dei campi in 5.9.
3. Luci: Directional calda + SkyLight fredda forte (ombre blu, mai nere), nebbia chiara azzurra in distanza.
4. `MF_GSTN_Rim` (Fresnel in emissivo, mascherato verso l'alto e la luce) e 2 material instance base: plastica lucida (roughness 0,25-0,4) e opaco (0,6-0,8), applicati al manichino e a 3-4 oggetti del livello.
5. Misura perf prima/dopo (stat unit, stat gpu): il dopo non deve costare di più.
## Tratti d'identità (la review li controlla per primi)
Ombre colorate e leggibili; personaggio che stacca dallo sfondo da ogni angolo; colori saturi senza bianchi bruciati né neri chiusi.
## Vincoli
I numeri sono punti di partenza: decide l'occhio sul video. Nessun outline, nessun toon shader. Non toccare asset dell'owner né i materiali dei personaggi esistenti: si lavora su istanze nuove. Max 2 tentativi per voce.
## Fine
Griglie prima/dopo in docs/notes/reviews/look-v1/; review cloud PASS/FAIL; handover docs/notes/handovers/look-v1.md + copia in handoffs/.
