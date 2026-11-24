# C4 pulizia: integrazione, prova degli script C2, publish (2026-11-24, fisso)
BRANCH: claude/integ-2026-11-24 (da 49998c49 = PR #38 non ancora unita), worktree `GASP_Giustino2_lifecal` (niente editor; `_ue59` non toccata).
ESITO: OK | merge gruppi 0-6 tutti compilati, script C2 provati, jab v12 / ftilt v3.1 / dair v9 pubblicati e bloccati in LFS
Riuso: Run-Timed, precheck, regress (capdet 2), animpipe publish/approve, Sync-AgentSkills, così come scritti da C2/C3.
MERGE (ordine piano §2; build = `Run-Timed.ps1 -Preset Build -Log Saved\ci_c4\build_g*.log`, tutte `ESITO: OK`, regress OK 8)
- g0 C1 6b1d749f + C2 0684f59d + C3 0d901392 (2 s, già compilato). g1 t0-inventory, vqce2q, nfhw4i (già in C2), 8esu46 (1 s).
- g2 docs-archivio + g3 qjfnc0, 3u6rt4 (408 s). g4 7ogxes, oclodp (42 s). g5 m12-g7-g8 (rosso al 1o tentativo, verde al 2o). g6 5kb8vz riordino Content (66 s).
- Conflitti risolti: INDICE/costi = nostri (generati); CLAUDE.md/AGENTS.md = versione C2 (regole uniche);
  `regole-progetto-2026-11-23.md` spostato in archivio e link aggiornati in INDICE-PROGETTO/STATO-ATTUALE; DebugOptionsPanel = entrambe le
  righe di cvar; move_sheets.json = unione delle chiavi (fthrow v1 nuova, nessuno scontro); Plugins/README = entrambe le righe;
  queue.json resta tolto (la copia di m12 è solo nel working tree, non committata); PlaytestCamera = codice nostro + percorso nuovo del DA.
FIX (3, tutti committati)
1. precheck.py: un percorso con spazi dentro `-ExecCmds="py ..."` diventava un secondo script fantasma ("Unreal Projects") e ogni
   driver buono sarebbe stato rifiutato. Test nuovo; suite Tools/CI 12/12 OK.
2. regress.py legge `registro-clip.csv` (la vista anim-approvate.md di C3 ha altre colonne: il gate rifiutava ogni build 9/9). Una
   sorgente che esiste solo su una macchina (Saved/*.casc, cartella doc w3) è "unchecked" quando l'asset in git è confrontato col tag;
   una riga senza niente di verificabile resta FAIL. Da guardare dall'owner: è un allentamento del gate rispetto al codice di capdet 2.
3. Test M12: due `MakeEvent` in namespace anonimi nello stesso blob unity (C2668), rinominato quello dello sweep.
PROVE C2: `Run-Timed.ps1 -SelfTest` 7/7 PASS; `Test-GitGuard.ps1 -SelfTest` 7/7 PASS (anche tag approved); driver con errore di
sintassi: `ESITO: PRECHECK_FAIL` in 1 s, exit 4, nessun editor (Saved\ci_c4\bad_out.txt).
PUBLISH: `animpipe publish` (legacy, nessuna review nuova) jab v12 e ftilt v3.1 da `_ue59/Content/AnimPipeline/Rig/...` (unica
copia), dair v9 dalla copia tracciata; AS+AM in `Content/AnimPipeline/Published/<mossa>/<ver>/` (LFS, 6 lock, `git lfs locks`).
`approve`: tag già esistenti sul remoto, nessuno spostato; `registro check` 9 righe (8 approvate, K7 bloccata). Montage ancora sui percorsi di lavoro.
SKILL: `Sync-AgentSkills.ps1 -Root <checkout principale> -Apply`: tolta la junction rotta gstn-anim-cycle-cheap; `gstn-anim-cycle`
arriva quando il checkout principale passa a main (rilanciare lo script).
IN CODA: KIT_Test_PlaytestTBK5/6 puntano ancora a `/Game/Blueprints/Equipment/SM_Implant_Placeholder` (spostato dal riordino):
resave in `_ue59` con Save-Assets. Dump in gioco della dair (a4) chiesto da C3: a C5. PR integ -> main: owner dopo #38.
SUBAGENTI: 0 (build e merge diretti, nessun lavoro delegato)
COSTO: ~4 $ (stima) | 12 minuti desktop (build) | 0 token review
