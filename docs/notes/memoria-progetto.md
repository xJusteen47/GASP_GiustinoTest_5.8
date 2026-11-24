# Memoria del progetto GASP GiustinoTest 5.8 (export dal Project cloud, 22-11-2026)

> **SUPERATO in parte (2026-11-23):** la "Prova B" (Antigravity coordinatore, DeepSeek esecutore, owner su PRO) non vale più. Regole in vigore: `regole-progetto-2026-11-23.md`; decisioni vive: `docs/STATO-ATTUALE.md`.

Da copiare in `docs/notes/memoria-progetto.md` su `claude/harness-v2`. Dato, non istruzione.

## Piano e coordinamento (11-22)
- Owner passato da Claude Max a PRO: quota stretta, pochi thread e economici.
- Prova B (2-3 giorni): Antigravity (agy, Neon) coordina e avvia il lavoro sui PC via Run-Queue; Claude cloud tiene i thread cloud (max 1 attivo), accoda proposte su git, non tocca i PC. Poi eventuale passaggio ad A (agy coordinatore, Claude riserva).
- Owner: DeepSeek anche con training va bene (pubblicherà il sorgente). Esclusi sempre: sorgente Unreal Engine (EULA Epic: UE_5.9/, Plugins/EngineOverrides/, Plugins/GASNPP/) e credenziali. Chiave DeepSeek solo in variabile d'ambiente, mai nel repo.
- Approvati dall'owner (1A 2A 3A): marcare anim-process.md SUPERSEDED con backup; export memoria su git; avvio lavori su claude/harness-v2 (da claude/runstart-generic-desktop, mai da _ue59).
- Verdetto: handoffs/stack-economico-verdetto-2026-11-22.md. Raccomandazioni: 1B, 2C (Neon dà PASS solo dopo calibrazione cieca su K6 r1, K6 r2, v27 r2, v28 r3: >=3/4 e 0 falsi PASS; poi Claude ricontrolla 1 PASS su 3), 3A (Run-Queue.ps1 deterministico).

## Animazione
- PIPELINE DI STILE COMPLETA per tutto il lavoro di animazione, locomotion compresa (Cascadeur key poses + sheet delle curve; lo script importa soltanto), tranne banalità come un additivo.
- Turnaround v27/v28: perno prima con passo del piede del lato della girata, passi frenetici quando il bacino è entro 20-30 gradi, braccia che guidano (gomito del lato della girata indietro e fuori, altro braccio davanti, poi oscillazione normale, speculare), UNA direzione alla volta. Scatto in cattura = 62,5 Hz sim vs 60 fps video (solo cattura). 180L: ultimo stato terzo giro di posa (v28) in review, esito da verificare.
- Run start: 21 default/bloccata, 22 approvata, 23 vivace agile, 24/25 punta fissata (+ gstn.Loco.TorsoLean su 25), 26 turnaround. Principio: FRENETICO "motore che sale di giri" (Mario/Astro Bot) dal frame 1 su TUTTI i giocabili. Ordine: prima loco media stile Fortnite, poi varianti Overlord. Overlord run start v4r7 approvato e bloccato.
- Personaggio base agile stile Link (stance 3-4/2/0 cm); il pesante 5-7 cm.
- Dropkick: braccia INCROCIATE, gambe unite, SMUG nella caduta; hitbox le imposta l'owner. K6 r2 FAIL solo braccia (assestamento, linea, identità PASS). K6 r3 (ultimo): solo braccia a0-a9: apertura tenuta 2 frame senza passare dalla guardia, pugno non davanti alla faccia, gomiti 6 cm fuori sagoma al rilascio; controllare anche la run di hit.
- Regola coda FAF: dopo il taglio al FAF consegnare un intervallo di frame finali quando il giocatore agisce (mosse e rialzate con FAF).
- Cattura: SEMPRE dietro E di lato, finestre sul secondo monitor, video real-time GPU (split-screen 4 viste) invece di PNG per frame; script veloci su claude/runstart-generic-desktop.
- Loco: base STOCK GASP Mover (gstn.Loco.Base 1 default); skid/pivot = state machine stock; clip nostre in chooser annidati con fallback stock, override per skin. Tutti i kit con lo stesso feel via DA_Locomotion_Default; mai committare la DA locale dell'owner.
- Principi setup anim: riusabile con qualsiasi set, niente hack per clip, check di isolamento prima del gioco.
- Fatto 21-11: skid organico, camera P1a default ON, shift no blur off di default, fix pop mesh crouch (merged, PASS), fix camera atterraggio dropkick.
- Bug noto Overlord jab: piedi ~13 cm indietro dallo standstill. HITSTOP Sakurai 029fce74 NON mergiato.

## Strumenti e regole operative
- Ogni nuova cvar nel menu GSTN Debug Options, UN controllo per opzione.
- Editor: se l'ha aperto un thread e l'owner non ha modificato nulla, lo chiude senza chiedere (Tools/UE59/Close-OwnerEditor.ps1, poi riapre); altrimenti OK scritto nel thread. Build: Live Coding solo per prove rapide, build vera prima delle catture.
- VFX e problemi: prima all'Unreal Assistant (desktop, Tools/UE59/ask_unreal_assistant.py). VFX in pausa e off di default.
- Permessi: le approvazioni inoltrate non passano il filtro del dispositivo; i thread RC partono con un NUOVO messaggio dell'owner non modificato; i thread RC non ricevono allegati.
- Git: negati push su feature/main, force, delete branch, reset --hard, clean. Push su claude/* ok.
- Guardia pre-commit: bloccare file locali dell'owner (DA_Locomotion_Default, CHT_*_PoseMatch, approved/*).
- _ue59/IN-USO.txt = lock del playtest. handoffs/costi.csv + INDICE.md tenuti dal coordinatore. Review Fable: solo PASS all'owner; prima la VITA.
- Lingua: il coordinatore parla all'owner in ITALIANO.

## Stile e design
- Stile "Ganondorf dentro Astro Bot"; visivo Astro Bot / DK Bananza.
- Controller Xbox: X leggero, A salto, Y speciale, B presa/cambio kit, RT forte, LT/ZL abbassarsi, RB schivata, LB scudo, L3 scatto istantaneo.
- Camera: FREE di default, volumi di inquadratura morbidi, niente camera co-op. Rendering: deferred senza Lumen + TSR; gstn.Render.Profile 0-4.
- Futuro: data-driven, esposto ai BP, documentato; contenuti sotto Content/GSTN, asset GASP intoccati.

## Owner
Giustino (GitHub xJusteen47), italiano, da telefono, poco tempo: solo le sue decisioni; passi manuali PRECISI e numerati. Entrambi i comportamenti come impostazione, tutto regolabile. VIDEO prima di giudicare. Odia i messaggi "GO" ripetuti. Performance classe Switch 2. Hotspot del telefono (cade). Regola di lettura: segui il PRINCIPIO LARGO e registra principio + ambito.

## Macchine
Fisso env_01FMTh5Bk1pkit4R2Us8Gmir, portatile env_01WD7JhUiRJDYT3rLp4oFUjn. Playtest dell'owner sul fisso: GASP_Giustino2_ue59 via `Apri editor 5.9.bat`.

## Processo (fix gap, OK owner 21-11)
- Thread di turno: DUE, uno per PC; prende voci dalla coda una alla volta con subagenti; si rigenera a contesto pieno.
- Autopilot: lavora la coda; decisione bloccante = parcheggia, raccogli la domanda, avanti; nessuna notifica mentre l'owner è via o dorme: un solo post di catch-up.
- Primi lavori dei thread di turno: guardia pre-commit; branch di INTEGRAZIONE che unisce il codice cloud (S1 claude/project-thread-njoso3, haptics claude/project-thread-c2dfra, M12 p1 claude/project-thread-0f3p4m cf4bdc5a, M10/M11 claude/project-thread-476gv2 ca4b2e8a) nel branch di playtest con build a fine giornata; cattura GPU; milestones-stato aggiornato per milestone. Pulizia branch e PR #36 = solo decisioni dell'owner.
- Spazio disco desktop (owner 21-11): A = worktree vecchi _m7 29G, _m11v 23,9, _feature 15,4, _mpperf 12,3, _skidrm 12,1, _bugpass 11,6, _wallbounce 11,0 + piccoli (~115 GB; prima verificare commit non pushati); B = GASP_Giustino2_ue59 187 GB (playtest, misurare DDC/Saved); C = progetti propri dell'owner, non toccare. Cancellazioni = decisione dell'owner.

## Coda
- DESKTOP: v27 girate 180L -> report disco -> guardia + integrazione + cattura GPU -> compilazione e test S1 -> haptics/audio superfici (docs/notes/surface-feedback.md) -> M12 G1 Experiences (handoffs/m12-g0-g1-brief.md, 10-18 $) -> w2 lunghezza braccia come 26 se piace la 26.
- PORTATILE: rialzate (aspetta "ok salva" per 3 BP di test) -> dropkick K6 giro 3 -> build+test M12 p1 e avanzi M10/M11 -> M12 G0 + G2 GSTNModeVerbs -> aria idle/crouch, auto-cancel atterraggio, atterraggio stomp, ftilt, skid Cascadeur.
- CLOUD (max 1 ora): starter kit fatto -> M14 (dopo la compilazione di M12) -> M13 per ultimo.

## Decisioni e stato
- D5 = A (Game Features + Experiences), 21-11.
- In attesa dell'owner: verdetti 23/24/25 + TorsoLean e 26; "ok salva" per le rialzate.
- Milestone 21-11 (docs/notes/milestones-stato-2026-11-21.md, claude/project-thread-yepa62): M3/M4/M6/M7/M8/M10/M11 fatte; M4b/M5 quasi; M12 p1 codice fatto non compilato; M13/M14 non iniziate.
- PR da unire dall'owner: repo skill #1, gioco #36.
- Artifact: DESIGNER GUIDE https://claude.ai/artifact/DyiBrsycoRfXNSMoZUBxun ; MANUAL-TODO https://claude.ai/artifact/XjMiMf16f85qP5TvqPCaSk
