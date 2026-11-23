# Mesh senza Blender: opzioni (ricerca 2026-11-23)

Scopo: 4 personaggi/skin sullo scheletro unico (limiti in `skin-proporzioni-analisi.md`) e props, senza che l'owner usi Blender.
Stato fonti: UE 5.9 è **Preview 2**, tutto ciò che riguarda Canvas è **Experimental**. Sotto, "NON CONFERMATO" = nessuna fonte letta lo dice.

## 1. Unreal 5.9: cosa c'è davvero

**Unreal Canvas** (è questo che intende l'owner; non è il Canvas Panel UMG). Spazio di lavoro condiviso persone+agenti, apre da *Tools > Unreal Canvas*; plugin: Unreal Canvas Bridge, Spatial Annotations, Model Context Protocol. Fonti: [overview](https://dev.epicgames.com/documentation/unreal-engine/unreal-canvas-overview-in-unreal-engine?lang=en-US), [getting started](https://dev.epicgames.com/documentation/unreal-engine/getting-started-with-unreal-canvas-in-unreal-engine?lang=en-US), [release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-release-notes?lang=en-US), [forum Preview 2](https://forums.unrealengine.com/t/unreal-engine-5-9-megajam-preview/2852840).
- Categoria **Mesh**: genera 3D da *una immagine o più viste* (multiview), alcuni modelli espongono topologia/numero facce ("non garantiscono geometria pronta per produzione"). Flusso: isola oggetto, scegli modello mesh, Open Viewer, *Import to Content Browser*.
- Import: mesh senza rig → Static Mesh; file con skin → Skeletal Mesh. **Auto-rig** locale con SkinTokens (richiede GPU NVIDIA ≥8 GB); "la qualità del rig dipende dall'input".
- Provider: fal.ai, Scenario, Luma, World Labs (+ modelli locali SAM2/Depth Anything, che segmentano soltanto). **Chiavi API tue, ogni run è a pagamento** dal provider; l'uso dell'agente ha costo a parte.
- NON CONFERMATO: quale provider/modello fa le mesh, poligoni, formati, texture/UV, retopo quad, se il rig è compatibile con lo scheletro UEFN/mannequin, prezzo per mesh. Va provato (vedi Passo 0).
- Non è pronto per produzione: lavorare su copia del progetto.

**Modeling Mode** (sculpt, PolyEdit, Boolean, UV, Remesh, Retopo): esiste da UE 5.x. Le note 5.9 citano solo un ritocco al brush Move; **non ho trovato fonti 5.9 su novità di retopo, skin weights, Skeletal Mesh editor o MetaHuman** (verificare nel Modeling Mode del 5.9 aperto). Dalle versioni precedenti (conoscenza mia, non riverificata in 5.9): sculpt, PolyEdit/PolyGroup, Boolean, UV, simplify e remesh, auto-retopo "AutoRemesher/Retopology"; pesi pelle con Skin Weight Paint nel Skeletal Mesh Editor; morph target. È usabile a mano ma è un editor "vero": non è più facile di Blender per organico.
**Utile per noi:** IK Retargeter 5.9 ha *Proximity IK, Pelvis Volume, Limb Twist* (pensati per proporzioni diverse) → aiuta solo se si ricorre al retarget per skin (escape hatch, cambia le pose approvate).
**MCP/Assistant 5.9:** Unreal MCP con toolset (anche "geometry", contenuto non descritto), Python API "modeling", Fab image/mesh generation, Claude Code integrato. Un agente può quindi pilotare l'editor; quanto sia profondo il toolset di modeling è NON CONFERMATO.

## 2. Generatori AI esterni (tutte le fonti sono di vendor o blog di vendor: claim, non verificati)
| Servizio | Rig | Note |
|---|---|---|
| Meshy | auto-rig + preset animazione; ha MCP e plugin UE | richiede piano Pro per API; remesh |
| Tripo | auto-rig, "clean topology" | |
| Rodin/Hyper3D | rigging aggiunto 23-10-2026 (dettagli NON CONFERMATI); mesh dense, serve remesh; stili Low Poly/Stylized; 1 immagine, viste extra opzionali | ha plugin UE e MCP |
| Hunyuan3D | solo mesh | |
Nessuna pagina dice se il rig combacia con lo scheletro UEFN mannequin: **assumere di no**. Strada realistica: generare mesh, poi **ri-skinnare** sullo scheletro di gioco (copia pesi da mesh base / auto-skin) invece di usare il rig del servizio. Prezzi: non trovati in fonti lette; di solito crediti a piano, ordine pochi $ per personaggio (stima mia).

## 3. Editor più facili di Blender e agenti
- **Blender via MCP** ([ahujasid/blender-mcp](https://github.com/ahujasid/blender-mcp), poi Blender Lab MCP ufficiale 5.1+): Blender va installato, ma l'owner non deve usarlo: un agente scrive Python (`execute_blender_code`) e controlla con viste (`look`); `generate_3d` chiama Tripo/Hunyuan/Rodin/Meshy. Limiti dichiarati: scultura organica debole (assemblaggi di primitive), niente retopo quad scriptato confermato. Un thread cloud non lo può fare: serve il PC con Blender aperto.
- Nomad Sculpt, Blockbench, ZBrush, Mixamo/AccuRIG, Character Creator: **non ho trovato fonti** su MCP/scripting; assumere "pilotabili solo a mano". Nomad (tablet/iPad) è il più semplice da imparare ma resta manuale.
- Mixamo/AccuRIG: auto-rig umanoide gratuito, ma scheletro diverso dal nostro → serve comunque ri-skin.

## 4. Pipeline consigliata (classifica)
1. **A. Canvas/AI → mesh → ri-skin sullo scheletro di gioco, ritocchi da agente via Blender-MCP nascosto** (consigliata). Costo: chiavi API ~5-15 $ per i 4 personaggi (stima), 2-3 thread. Rischio: qualità stilizzata e proporzioni nei limiti da controllare con script (altezza, gambe, braccia).
2. **B. Mesh AI esterna (Meshy/Tripo/Rodin) + stessi passi.** Più scelta di modelli e rig/retopo pronti, costo a crediti.
3. **C. Base mesh Mannequin + Modeling Mode a mano.** Gratis, controllo totale, ma lavoro manuale lungo dell'owner. Sconsigliata.
Nota coerenza: le mesh sono solo pelle; hitbox/hurtbox restano del kit (regola skin A).

## 5. Flusso da disegni (richiesto dall'owner)
1. **Disegni ortogonali** su manichino base (fronte, lato, retro) stampato, foto in project files. Owner. Solo foto dritte, sfondo bianco, A/T-pose come il mannequin.
2. **Pulizia e isolamento**: agente ritaglia/raddrizza le foto (script) o Canvas SAM2 isola. Agente da solo.
3. **Generazione low poly**: Canvas Mesh multiview (se il modello lo permette), oppure Rodin/Tripo/Hunyuan multi-view con stile Low Poly; alternativa: agente costruisce base low poly dalla base mesh del mannequin con Blender-MCP. Agente da solo, owner approva da immagini/turntable.
4. **Rifinitura a passi**: l'agente applica una modifica per volta (Python in Blender-MCP o Modeling tools via MCP), renderizza 3 viste, l'owner dice "ok/più X". Alternativa senza strumenti agente: istruzioni numerate per Modeling Mode (PolyEdit, brush Move). Agente + owner solo a decisioni.
5. **Controllo proporzioni** contro i limiti (altezza -5/+7%, gambe ±4%, braccia ±5%...) con script automatico. Agente da solo.
6. **Retopo quad alla fine**: Modeling Mode "Retopology/AutoRemesher" in UE (qualità NON CONFERMATA in 5.9), oppure Quad Remesh di servizi cloud (Meshy Remesh, 3D AI Studio: vendor), oppure addon Quad Remesher/Instant Meshes lanciati dall'agente in Blender. Senza Blender: UE o servizio cloud.
7. **Skin sullo scheletro di gioco + pesi** (copia pesi dalla mesh base, Skin Weight Paint in UE per correzioni), import, test con pose approvate. Agente per import/test; l'owner solo guarda video.

## 6. Cosa fa l'owner a mano (numerato)
0. Aprire UE 5.9 Preview 2 su una **copia** del progetto, abilitare plugin Unreal Canvas Bridge, Spatial Annotations, Model Context Protocol, riavviare.
1. Tools > Unreal Canvas, accettare il download iniziale (alcuni GB), incollare una chiave API (es. fal.ai) in Impostazioni provider.
2. Dare a un thread una foto/disegno di prova (un solo personaggio) e dire "ok costo massimo X $".
3. Se si sceglie Blender-MCP: installare Blender una volta (solo installare), lanciare lo script di setup indicato dal thread, premere "Start MCP Server" (tasto N > scheda MCP).
Tutto il resto lo fanno thread; test su PC desktop (Canvas e Blender non girano da cloud).

## 7. Prossimo passo proposto
Passo 0 di prova sul desktop (~2-3 $, 1 thread): una mesh da 3 viste con Canvas, vedere topologia, import e se il rig combacia. Esito decide A o B.
