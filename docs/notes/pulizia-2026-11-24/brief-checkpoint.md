# Pulizia repo e harness: brief dei checkpoint (2026-11-24)

Piano approvato: `handoffs/pulizia-repo-harness-piano-2026-11-24.md` (owner 1A 2A 3A + i tre blocchi di fix, "Va benissimo" 01:56 UTC).
Base comune: `main` dopo la PR #38 (= `claude/dair-v9-stops-review` 49998c49 = t3 + review dair). Finché #38 non è unita, partire da 49998c49.
Ogni thread: legge solo questo brief, il piano e i file che tocca; dichiara cosa riusa (`docs/INDICE-PROGETTO.md` sul branch `claude/docs-archivio-2026-11-23`); handover ≤30 righe con SUBAGENTI e COSTO. Ordine: C1 e C2 e C3 in parallelo (C1 fisso, C2/C3 cloud), poi C4, poi C5.

## C1 — fisso, Sonnet medium (build/cattura a subagente Haiku), budget 2 $, ~30 min desktop
Obiettivo: cattura video in tempo reale invece dei PNG per fotogramma (oggi 96 dei 99 s per sequenza).
1. Segna `_ue59/IN-USO.txt`; porta `_ue59` su 49998c49 (o `main` dopo #38) senza toccare i file sporchi dell'owner.
2. `Run-Timed.ps1 -Preset Build` con GSTNCapture (già abilitato nel .uproject).
3. Una cattura della clip base dair v9 con GSTNCapture, 3 viste; deve stampare `ASSERT: OK`. Misura secondi per sequenza prima/dopo (log citati).
4. Libera `IN-USO.txt`. Non toccare skill, registri, branch. Se la build fallisce 2 volte: fermati e scrivi cosa serve.

## C2 — cloud, Sonnet medium, budget 7 $ (P2 + blocco di fix 1)
1. Unisci prima `claude/project-thread-nfhw4i` (capdet 2, tocca `Run-Timed.ps1`).
2. Regole: un solo `docs/notes/regole-progetto.md` (testo = istruzioni di progetto attuali, 17 regole); CLAUDE.md e AGENTS.md rimandano lì senza copiare; CLAUDE.md: tronco `main` (non `feature/gas-npp-5.8`), punta a `docs/INDICE-PROGETTO.md`, `takeover-state.md` in archivio. AGENTS.md per Antigravity come al §5 del piano.
3. `Tools/CI/Build-Indice.py`: genera `INDICE.md` e `costi.csv` dalle righe `ESITO:` / `SUBAGENTI:` / `COSTO:` degli handover, con controllo dello schema (2A). Togli `handoffs/queue.json` dal repo.
4. `Run-Timed.ps1`, PreCheck di default: `py_compile` dei .py passati; l'asset/scheletro chiesto esiste nel progetto del comando; rifiuto se un LLM locale usa la VRAM (nvidia-smi). Chiusura dell'editor appena il driver scrive `ESITO:`; un TIMEOUT dopo `ESITO: OK` non conta come tentativo. Test offline dove possibile.
5. Guard: `.githooks/pre-push` e `Test-GitGuard.ps1` accettano `refs/tags/approved/*` (solo creazione, mai cancellazione/spostamento).
6. `Cleanup-Branches.ps1` su `main`, legge `docs/notes/pulizia-2026-11-24/branch-elenco.csv`, tag `archivio/<nome>` prima di cancellare C/D/E; elimina `Remove-MergedRemoteBranches.ps1`. Ricalcola il CSV sullo stato del giorno (dry run).
7. `Tools/CI/Sync-AgentSkills.ps1`: junction `~/.gemini/config/skills/<nome>` -> `<repo>\.claude\skills\<nome>` per ogni skill, toglie le junction rotte (`gstn-anim-cycle-cheap`). Dry run di default.
8. Docs-sweep: i documenti dei branch di categoria D in un commit.

## C3 — cloud, Opus medium, budget 5 $ (P4 + correzione stop_sync)
1. `Tools/AnimPipeline/registro-clip.csv` (colonne al §4 del piano), riempito dalle righe di `anim-approvate.md`, tag `approved/*` e branch `approved-*`; `anim-approvate.md` diventa vista generata.
2. `animpipe publish <mossa> <ver>` (rifiuta senza `ASSERT: OK` e review livello 2 PASS; copia in `Content/AnimPipeline/Published/<mossa>/<ver>/`, LFS con lock, regole in `.gitattributes`) e `animpipe approve ... --owner "<parole>"` (riga, tag, vista). Estendi `locks.py` e il comando `approve` esistenti. Test offline.
3. stop_sync (review dair v9, branch `claude/dair-v9-stops-review`): conta EVENTI (fermate di più parti sullo stesso frame = 1) e ignora i frame che il montage in gioco non mostra (dair finisce ~a34). Resta MISURA: diventa bloccante solo con la prova di taratura (regola 17) sui dump committati in `docs/notes/life-calib-run/dumps/`: passa sulle approvate e sullo stock GASP, boccia K7/K8 r2. Se i dump non bastano, scrivi quali servono a C4.

## C4 — fisso, Haiku/Sonnet, budget 4 $, ~90 min desktop (dopo C2 e C3 unite)
1. Merge dei branch E nell'ordine del piano §2 (gruppi 1-6, `5kb8vz` con il riordino Content per ultimo, editor chiuso), build dopo ogni gruppo; gruppo rosso fuori e in coda.
2. Prova reale dei PreCheck di C2 (un driver con errore di sintassi deve fermarsi in secondi).
3. `animpipe publish` + `approve` di jab v12, ftilt v3.1, dair v9 (asset dal fisso, oggi non in git); K7 senza tag (owner). `Sync-AgentSkills.ps1 -Apply`. Dump mancanti per C3 se richiesti.

## C5 — fisso, Opus medium, budget 4 $ + 30 min desktop (dopo C4; blocco di fix 2)
Una sola sessione editor per giro via MCP (import + montage + cattura) con la cache capdet: la base e le clip approvate non si ricatturano se l'hash non cambia. Misura il giro K8 prima/dopo (oggi 25 min).

## Owner (raccolti dal coordinatore in un messaggio)
Unire la PR #38; accendere "Automatically delete head branches" nelle impostazioni del repo; dopo C2 lanciare `Cleanup-Branches.ps1 -Apply`; chiudere le PR #20 #21 #22 #25 #27; togliere la skill di account `gstn-move-animation`; push dei tag se il guard non è ancora aggiornato.
