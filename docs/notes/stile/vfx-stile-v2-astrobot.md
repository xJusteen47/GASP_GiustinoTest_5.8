# Brief VFX stile v2 "Astro Bot": da adesivi 2D a giocattoli 3D (2026-11-17)
Thread cloud, Fable 5.2 high, solo brief. Richiesta owner 23:00Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCB4Qm5m3DmUm7CKzbkY8AG: "più alla Astro Bot, meno cartoon evidente", testo Gemini allegato) e 23:00:56Z (cmsg_01CRvUdrWm1dJrM4q54YDBLN3tQi4k2dkxXNS73KP9oMxy: dove basta un proxy a basso impatto, usare il proxy). Base: kit M2 r4 (branch `claude/project-thread-vcepyw` e7757181, `polish-m2-r4.md`). Implementazione: PORTATILE, Opus medium, review Fable 5.2 high (regola 10).
**Diagnosi** (griglie r4 `hit_large_close_22-39`, `jump_side_80-103`, guardate una volta): palline, stelle e anelli sono sprite Unlit piatti, bianco/giallo uniforme, senza luce né riflesso: sembrano adesivi 2D sopra la scena. È questo il "cartoon evidente", non il timing (che ha passato r4).
**Testo Gemini (regola 12):** non è una fonte, è un'analisi plausibile. Vale per noi: mesh tonde, bordo netto, materiale lucido, colpo al massimo al frame 1, burst radiali puliti, segni a terra (coincide con sheet A e vfx-clarity). NON vale così com'è: detriti che restano finché li spazzi (costo e rumore in PvP, diventa impostazione), collisioni rigide fra particelle (no), fumo "di cotone" sempre (solo polvere), vetro traslucido (lo finge un opaco con Fresnel).

## Cambi al kit (valgono per tutti i kit, regola ALL TEST KITS)
1. **Mesh, non sprite.** Mesh Renderer CPU con 4 mesh low-poly del kit: sfera smussata (~80 tri), toro (~96), scheggia tonda "a gemma gonfia" (~60, sostituisce la stella piatta), capsula affusolata (~48). Generate da script (Geometry Script in editor), niente asset a pagamento. Lo sprite resta solo per il lampo del frame 1.
2. **Materiale `M_GSTN_VFX_Toy`: Default Lit, opaco Masked**, Roughness 0,2-0,35 (plastica lucida) o 0,6 (polvere "cotone"), Specular 0,5-0,7, rim Fresnel in emissivo (riusa `MF_GSTN_Rim` del Look v1). Riflessi veri dalla luce e dallo SkyLight: ancora l'effetto alla scena. Erosione dal bordo (canale B) e rimpicciolimento restano. Le 2 bande finte (canale G) spariscono sulle mesh: la luce vera fa il lavoro.
3. **Emissivo piatto e saturo solo per energia:** nucleo del colpo frame 1-2, anello del colpo, kit con elemento (fuoco, elettrico). Valore uniforme, niente gradiente interno, sopra 1 solo il lampo.
4. **"Vetro al neon"** (anello colpo, onde d'urto): toro opaco, emissivo saturo + Fresnel bianco sul bordo. Nessun Translucent.
5. **Timing:** colpo e impatti nascono al 100% al frame 1 (già regola per il colpo); polvere resta 60%→100% in 2 frame. Morte: rimpicciolimento a 0 con ease-in (curva "pop out"), erosione solo per la polvere.
6. **Hitstop dei VFX:** durante l'hitstop gli effetti del colpo rallentano a `HitstopVfxTimeScale` (impostazione per giocatore, 0 = congelati, 1 = liberi), poi ripartono. Il lampo frame 1-2 si vede comunque. Codice piccolo in GSTNPresentation (tempo del componente Niagara dall'evento hitstop; API da verificare in 5.9). Decisione 2.
7. **Burst radiali:** schegge su ventaglio/anello regolare con jitter ≤10°, niente vortici né curl noise (già quasi così: sfasare gli angoli, nota Fable r4).
8. **Rimbalzi a terra (proxy, owner 23:00:56Z):** niente collisione Niagara (trace) né GPU depth buffer (il toaster resta CPU). Piano analitico all'altezza dei piedi dell'evento: un modulo scratch riflette la velocità con restituzione 0,5-0,6 e attrito, max 2 rimbalzi. Detriti persistenti: pool fisso condiviso (24 pezzi per arena, il più vecchio si ricicla), vita configurabile; **spenti di default**, attivabili per game mode o preset grafico. Decisione 3.
9. **Ombra sotto gli effetti in aria:** disco piatto scuro-blu (non nero, non decal: il decal blob è risultato invisibile in M1) proiettato lungo `GSTN_DownAxis`, solo per effetti che durano > 15 frame o volano > 50 cm (schegge L, detriti, scia tumble). Bersaglio in aria colpito: anello a terra sotto l'impatto (sheet A, "Segno a terra").

## Tabella per effetto
| Effetto (sistema r4) | Oggi | v2 |
|---|---|---|
| Partenza corsa, skid, salto | sprite bianchi piatti | 3-5 sfere "cotone" Lit roughness 0,6, bianco caldo, ombra di contatto dalla luce vera; morte pop-out |
| Atterraggio, rimbalzo terra | anello sprite + palline | toro Lit lucido che si allarga + 4-6 sfere radiali che rimbalzano 1 volta sul piano |
| Colpo S/M/L | lampo + anello giallo + stelle 2D | lampo sprite frame 1-2 + toro "neon" emissivo + 4/6/8 schegge a gemma lucide (gialle/arancio, secondaria del kit), 1-2 rimbalzi, ombre per L |
| Muro / soffitto / bump | anello sul piano + palline | come atterraggio ruotato sulla normale; schegge che cadono e rimbalzano a terra |
| Linee sprint 2 | strisce sprite | capsule affusolate bianche lucide ai lati (stesse regole di posizione r2) |
| Scia tumble | ribbon + palline | catena di sfere lucide che rimpiccioliscono (via il ribbon), ombra a terra |
| Knockdown | anello ×1,2 | come atterraggio pesante + 2-3 detriti se l'impostazione è accesa |

## Perf toaster (invariati i tetti: ≤12 particelle per burst, ≤3 emitter, CPU, bound fissi, pooling, solo locali, mai replicati)
Mesh Lit opache costano poco nel deferred: ~12 × 80 tri per burst. Rischio: draw call (una per emitter mesh) → max 2 emitter mesh per sistema, mesh nel pool. Preset "basso": schegge 4 invece di 8, niente ombre, niente detriti, mesh LOD a ~30 tri.

## Decisioni owner (consigliata in grassetto)
1. Materiale: **A) mesh Lit lucide (plastica/neon), come sopra**; B) mesh ma Unlit con luce finta (più economico, meno "tangibile"). Ferma: tutto il giro.
2. VFX nell'hitstop: **A) quasi fermi, `HitstopVfxTimeScale` = 0,15 di default**; B) congelati (0); C) liberi (1). È un'impostazione in ogni caso; si sceglie il default.
3. Detriti: **A) rimbalzano e spariscono in 0,6-1 s; persistenza = impostazione spenta di default (pool 24)**; B) persistenti di default in modalità avventura/spedizioni, spenti in PvP; C) niente detriti. Ferma: punto 8.

## Costo (portatile, da INDICE: M2 r1 ~25-30 $ + 8, r4 ~14 + 7)
Giro v2-r1 (mesh da script, materiale, 9 sistemi, tabella, codice hitstop VFX, piano rimbalzi, cattura): ~28-32 $ Opus + ~8 $ Fable. Un giro di ritocco previsto: ~12 $ + ~6 $. **Totale ~55 $ (48-60).** Detriti persistenti/pool solo se decisione 3B: +6 $.

## Cosa controlla Fable (in quest'ordine, poi checklist D 1-20 di `stile-sheet-vfx-review.md`)
a) Fermo immagine a metà vita: sembra un oggetto 3D lucido con riflesso e ombra, non un adesivo? b) L'effetto prende la luce della scena da due punti di L_Test_Hub (chiaro e scuro) senza diventare grigio o nero? c) Lampo frame 1 più grande e chiaro, nessun alone traslucido. d) Hitstop: gli effetti rallentano col personaggio e ripartono insieme. e) Rimbalzi: max 2, si fermano sul piano, niente compenetrazione visibile, niente pezzi che "scivolano" all'infinito. f) Ombre degli effetti in aria leggibili da dietro e 3/4. g) Lo stesso effetto ha la stessa forma su tutti i kit. h) Confronto prima/dopo stessi frame con r4. Voci di FAIL etichettate forma / timing / colore / materiale / gioco.

## Coerenza con i documenti esistenti
`stile-astrobot-nintendo.md` §2 diceva Sprite + Unlit + luce finta a 2 toni: v2 lo sostituisce SOLO per renderer e materiale (forme tonde, poche particelle grandi, Masked, niente outline, colore e timing restano). `vfx-clarity-ricerca.md`: compatibile (additivo solo nel nucleo, gerarchia, segno a terra). Proposta checklist fusa ora in `stile-sheet-vfx-review.md`, con le righe A aggiornate a v2 (marcate [V2]).
