# Stile Astro Bot / Nintendo (Mario 3D World, Odyssey, Wonder): cosa lo fa e come rifarlo in UE 5.9
Thread cloud, Fable 5.2 high, 2026-11-17. Richiesta owner 14:42Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNPHx6iiXBoZNJy1DkBc4VPc). Solo ricerca, nessun codice. Regole operative per M2 e revisori: `handoffs/stile-sheet-vfx-review.md`.
Etichette: **[F]** = detto da una fonte (elenco in fondo); **[A]** = analisi mia da osservazione dei giochi, non confermata da uno studio. I numeri UE sono PUNTI DI PARTENZA da regolare a occhio in editor; i nomi dei campi vanno verificati in 5.9.

## 1. Perché Unreal sembra "generico" e questi giochi no
Il look generico nasce dai default, non dal motore [A, in parte F7]: tonemapper filmico ACES che ingrigisce i colori saturi e chiude le ombre nel nero; esposizione automatica; bloom, lens flare, aberrazione, grana, motion blur accesi; texture fotografiche rumorose; ombre nere; fumo traslucido realistico. Nintendo e Team Asobi fanno l'opposto su ogni voce:
1. **PBR morbido, non toon.** Odyssey usa PBR con Fresnel vero, riflessi da cubemap e speculari evidenti che danno un aspetto lucido "di plastica" [F1]. Astro Bot cerca una sensazione "super tangibile": ogni oggetto è un giocattolo di plastica, metallo o gomma con un suo "DNA digitale" [F2]. Niente contorni a inchiostro, niente cel shading sui personaggi [A].
2. **Forme grandi, dettaglio piccolo quasi assente.** Superfici lisce, smussi larghi, pochi materiali per oggetto. Il dettaglio sta nella forma e nel riflesso, non nella texture [A].
3. **Colore come informazione.** Il percorso calpestabile è il colore dominante e chiaro, il pericolo ha una tinta satura in contrasto, il grigio spento segnala "non interattivo" [F3]. Le ombre sono colorate (blu/viola), mai nere [A].
4. **Effetti come "colla".** In Odyssey gli effetti "sono come colla" fra le interazioni; sbuffo bianco sotto i piedi in corsa, stelline quando un colpo va a segno, e il realismo cresce con la distanza dal personaggio: vicino a Mario tutto è cartoon [F4].
5. **Tutto reagisce.** In Astro quasi ogni oggetto reagisce a tocco e pugno [F2]. La "qualità" percepita viene dalla risposta del mondo più che dal rendering.
6. **Animazione che comunica lo stato.** Wonder ha ripreso le pose classiche (salto a braccia aperte, stop dopo la corsa) perché si leggano subito; il corpo guarda la direzione di marcia mentre il viso resta visibile [F5]. In Astro il timing cartoon è stato rifinito in più giri [F2].
7. **Ombra a picco.** Mario ha un'ombra proiettata dritta dall'alto, separata dalla luce principale, per leggere la profondità nei salti [F6]. Coincide col nostro depth marker.

## 2. VFX (ambito: M2 e ogni VFX futuro)
**Forme [A, F4].** Vocabolario chiuso: pallina, anello, stella a 4-5 punte, goccia, striscia affusolata. Niente rumore, niente fumo volumetrico, niente scintille puntiformi realistiche. Tondo = innocuo (polvere, salto), appuntito = colpo.
**Sbuffo di polvere.** 3-7 palline GRANDI, non 40 piccole. Quasi bianche con un'ombra tinta (2 toni netti), opache. Nascono già al 60% della taglia, arrivano al 100% in 2-3 frame (50 ms), poi scivolano rallentando e spariscono RIMPICCIOLENDO o erodendosi dal bordo, mai sfumando in trasparenza. Vita 250-450 ms.
**Scintilla di colpo.** Frame 1 = l'immagine più grande e più chiara (lampo bianco 1-2 frame), poi anello che si allarga e 4-8 schegge/stelle che partono veloci e frenano. Tutto finito entro 200-330 ms. 3 taglie = stessa ricetta, scala e numero di schegge diversi.
**Timing.** Attacco istantaneo, coda breve: pop, tenuta, sparizione. Curve ease-out forti sulla velocità (drag alto), mai moto lineare. Su schermo meno tempo possibile [F8].
**Colore.** Nucleo bianco, poi una tinta satura, mai nero né grigio sporco. Una tinta primaria e una secondaria per effetto [F8]. Polvere bianco caldo; colpo giallo/arancio; colore del kit come secondaria (dato della tabella M0).
**Contorni.** Nessun outline. Il bordo si legge per stacco di valore: sbuffo chiaro con ombra interna più scura della stessa tinta.
**Ricetta Niagara (toaster, online).**
- Un master material `M_GSTN_VFX_Master`: Unlit, **Masked** (non Translucent: niente overdraw e bordo netto; Rivals 2 usa Masked per quasi tutto [F9]). Texture a canali impacchettati [F9]: R = forma, G = gradiente di luce finta dall'alto, B = rumore di erosione. Opacity Mask = R × step(soglia, B), soglia da Dynamic Parameter guidato dall'età. Colore = lerp(ColoreOmbra, ColoreLuce, G a 2 bande con smoothstep stretto) × Particle Color.
- Emitter: CPU sim, Sprite Renderer, burst unico (nessuno spawn rate salvo scie), Fixed Bounds, Local Space off. Moduli: Scale Sprite Size by curve (60→100→0), Drag 4-8, gravità 0 o leggermente negativa per la polvere, rotazione iniziale casuale, nessuna collisione, nessuna luce, nessun ribbon tranne la scia tumble (1 ribbon).
- Budget: ≤12 particelle per burst, ≤3 emitter per sistema, texture ≤512 (atlas unico 1024 per tutto il kit). Effect Type condiviso con cull per distanza e tetto di istanze; pooling attivo.
- Online: effetti solo cosmetici, generati in locale dagli eventi di presentazione M0, mai replicati; il random non deve essere uguale fra client.
- Anello di atterraggio e onde d'urto: mesh ad anello con scala da curva e la stessa erosione, non sprite stirati.

## 3. Materiali e shading (ambito: personaggi, oggetti di scena, livello di test)
- **Personaggi e oggetti:** Default Lit, nessun toon. Base Color satura a valore medio (sRGB circa 0,35-0,85; niente neri sotto 0,05, niente bianco puro). Roughness a due famiglie: plastica lucida 0,25-0,4, opaco/gomma 0,6-0,8. Metallic 0 salvo metallo vero. Normal map solo per forme grandi. Niente texture fotografiche (Megascans = look generico).
- **Rim light nel materiale:** Material Function `MF_GSTN_Rim`: Fresnel (esponente 3-4) × maschera verso l'alto e verso la luce, sommata in Emissive a 0,2-0,5 col colore del cielo. Stacca il personaggio dallo sfondo da ogni angolo di camera; costo quasi nullo.
- **Occlusione:** AO morbida e larga a bassa intensità (0,4-0,5), mai nera: la tinta d'ombra viene dallo SkyLight.
- **Saturazione:** personaggi e oggetti interattivi più saturi dello sfondo; lo sfondo si schiarisce e si raffredda con la distanza (nebbia esponenziale chiara azzurra) [A, coerente con F3].

## 4. Luce e post-process
Un Post Process Volume Unbound per livello (o preset in un data asset), niente Project Settings.
- **Luce:** una Directional calda (leggermente gialla) + SkyLight forte e fredda (intensità 1-1,5) così l'ombra resta blu/viola e leggibile. Ombre a bordo morbido. Ombra o blob a picco sotto ogni personaggio (già: depth marker).
- **Esposizione:** manuale (Min EV = Max EV). L'auto-esposizione cambia la luminosità dei colori a ogni giro di camera.
- **Tonemapper:** non toccare gli shader del motore (la sostituzione ACES→AgX [F7] modifica il motore e non è adatta a un plugin pubblico). Nel PPV: Tone Curve Amount 0,6-0,8, Toe 0,4 (ombre aperte; default Slope 0,88 / Toe 0,55 / Shoulder 0,26 [F10]), Expand Gamut 0,5-1 da provare, Saturation globale 1,1-1,15, Contrast 1,0-1,05, ombre tinte verso il blu in Color Grading > Shadows.
- **Bloom:** basso (0,2-0,4), acceso solo da emissivi sopra 1 (lampo del colpo). **Spenti:** lens flare, aberrazione cromatica, grana, vignetta (max 0,2), motion blur, Lumen reflections costose.
- **DOF:** spento in gioco; solo nelle camere non giocate (vittoria, menu), dove dà l'effetto "diorama di giocattoli".
- **Toaster:** tutto questo costa MENO dei default (meno passate di post). GI: SkyLight + luce diretta bastano per lo stile; Lumen è un'opzione grafica, non un requisito.

## 5. Camera e animazione (solo promemoria, già deciso altrove)
- Camera: studio e piano completi in `handoffs/camera-astrobot-piano.md` (owner 14:44Z). In breve [A]: obiettivo lungo e lontano (FOV 45-55), telecamera che non segue il salto in verticale, aree autoriali a dati. **Da confermare dall'owner** (cambia il feel).
- Marker di profondità (owner 14:44Z: "più olografico e meno solido"; ambito: depth marker, lavoro del suo thread): proposta di stile = anello sottile additivo/traslucido con bordo chiaro e interno quasi vuoto, tinta ciano del kit, leggera pulsazione e righe di scansione, niente disco pieno; orientato secondo l'asse "giù" comune (`GSTN_DownAxis`, vedi piano camera §3).
- Animazione: resta il piano polish §5 (smooth/cartoon, overshoot, follow-through). Aggiunte dallo stile: squash/stretch della mesh su salto e atterraggio (±10-15% per 2-4 frame, volume conservato), pose classiche leggibili [F5], oggetti di scena che reagiscono al tocco con un rimbalzo in vertex shader o molla (costo basso) [F2].

## 6. Applicato e da confermare (regola 13d)
Applicate come consigliate (regola 6), reversibili: (a) nessun outline, né sui personaggi né sui VFX; (b) sbuffi opachi Masked a 2 toni che rimpiccioliscono; (c) PBR morbido, niente toon shader; (d) tonemapper ammorbidito via PPV, motore intatto. Da confermare dall'owner: FOV (§5), e (a) se preferisce provare un contorno.
**Rischio regola 12:** M2 copre solo i VFX. Materiali, luce e post (§3-4) sono lavoro nuovo: propongo un checkpoint "Look v1" sul portatile dopo M2 (PPV + rig luci su L_Test_Hub + MF_GSTN_Rim + materiali base, cattura prima/dopo, review Fable), stima 10-14 $.

**RISPOSTA OWNER 2026-11-17 14:51Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNRKWcVYMcjNU9F14B9PQP7B): 1A 2A 3A.** 1A telecamera solo per spedizioni/livelli (le arene PvP tengono l'attuale). 2A telecamera dopo l'ondata 1 del polish, partendo da C0. 3A checkpoint "Look v1" approvato, dopo M2. Brief: handoffs/look-v1-brief.md, handoffs/camera-c0-brief.md. Restano da confermare: FOV 45-55 e valori §4 del piano camera, nessun outline.

## Limiti
Nessuna fonte di studio su bloom/tonemapper/DOF di Mario o Astro, né sui parametri degli sbuffi: quelle voci sono analisi mia [A]. Digital Foundry su Astro letto solo in riassunto (risoluzione dinamica 1440p-2160p, 60 fps). Nessun video guardato in questo thread: il riferimento visivo resta il .mov dell'owner (Toad, partenza e skid).

## Fonti
F1 slaent.com/thread/1283214 (analisi tecnica Odyssey) · F2 blog.playstation.com/2019/01/24/astro-bot-rescue-mission-inside-the-art-and-animation-of-japan-studios-ps-vr-hit · F3 gamedeveloper.com "What Mario Learned from Mickey Mouse, Part 1: Colour" · F4 nintendolife.com/news/2022/06/super-mario-odysseys-vfx-artist-on-combining-mario-with-the-real-world · F5 nintendo.com/sg/interview/aqmx/02.html (Ask the Developer, Wonder) · F6 threadreaderapp.com/thread/924061000919285761 (C. Wade, dettagli Odyssey) · F7 video "The Unreal Engine look explained" (riassunto readray.ai) · F8 vfxapprentice.com/blog/five-artistic-principles-gaming-vfx · F9 rivals2.com/workshop/?p=11 · F10 documentazione Epic "Color Grading and the Filmic Tonemapper"
