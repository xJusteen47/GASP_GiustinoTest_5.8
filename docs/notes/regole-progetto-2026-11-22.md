# REGOLE DI LAVORO E DI SPESA (2026-11-22)

> Sostituisce `docs/notes/regole-progetto-2026-11-20.md`. Unifica le regole 1–15 (2026-11-15..2026-11-21), `docs/notes/audit_setup_agentico.md` e l'accordo multi-modello del 2026-11-22 (Claude Pro + Antigravity + DeepSeek V4 + Inkling locale). Ogni thread e ogni worker le seguono.

## 1. BRIEF
Si parte solo da un brief (max 1 pagina) in `handoffs/`, o dalla nota del coordinatore se corta. Dichiara **UNA** macchina (`fisso`, `portatile`, `cloud`), budget in `$` (o quota), tier/modello (`M0`–`M3`), e se aggiorna `_ue59`. Mai rileggere thread o chat vecchi: solo brief, piano del checkpoint, file toccati.

## 2. THREAD, HANDOVER E COSTI
Un thread = un checkpoint. Finito, handover di max **30 righe** in `docs/notes/handovers/` sul suo branch e chiude; il seguito è un thread NUOVO. Contesto: a **120k token** chiudi, mai oltre 160k. L'handover finisce con la riga:
`COSTO: <$ stimati> | <minuti desktop> | <token review> | provider=<provider> | quota_pro=<%>`
La riga va anche in `handoffs/costi.csv` (11 colonne: `data,thread,obiettivo,macchina,provider,modello,minuti_desktop,token_review,usd_stimati,quota_pro_pct,esito`). Il coordinatore tiene `handoffs/INDICE.md` (con campo `obiettivo:` per contare i tentativi) e ogni venerdì confronta `costi.csv` con l'uso reale. Copie degli artifact solo su `claude/artifacts`.

## 3. MODELLI E ROUTING (Tier M0–M3, aggiornamento 2026-11-22)
Un modello principale per thread (i worker/subagenti esterni su tier inferiori non consumano quota Pro).

| Tier | Ruoli | Modello primario | Fallback / Limiti |
|---|---|---|---|
| **COORD** | Coordinamento Prova B (2–3 gg): coda su git (`handoffs/queue.json`), `INDICE.md`, `costi.csv`, raccolta domande | **Antigravity** (`Orchestrator (Neon)` / `Hydrogen High`) coordina e avvia sui PC via `Run-Queue.ps1`; **Claude Pro cloud** (`Sonnet 5.5`) tiene i thread cloud e accoda proposte su git | Mai codice/log/video nel coordinatore; Claude cloud non tocca i PC locali |
| **M0** | Build (`Run-Timed.ps1`), test (`Check-TestLog.ps1`), catture GPU, `clipcheck`, grep, log, rinomine, montaggio video | **`deepseek-flash`** (via `Invoke-Worker.ps1` / `agent-bridge`) | `Gemini 4 Hydrogen (Low)` oppure **Inkling 2 Mini** locale su RTX 4080 **solo a `UnrealEditor*` chiuso** e contesto < 28k |
| **M1** | Codice routine, script `Tools/`, **`gstn-anim-executor`** (1 `istruzione.json` + gate + `esito.json`) | **Codice:** `deepseek-v4-pro` (fuori da fork Epic) · **Anim executor:** Antigravity `Gemini 4 Hydrogen (High)` | Mai usare DeepSeek su path Epic EULA; mai saltare cancelli 15b / `ASSERT` |
| **M2** | Design C++ / GAS-NPP / Mover, modifiche GASP ABP, fork Epic (`Plugins/GASNPP/`, `Plugins/EngineOverrides/`), **Diagnosi di Sistema R5-bis**, pose chiave Cascadeur + sheet curve, review diff | **Claude Pro** (`Sonnet`/`Opus`, thread brevi ≤ 60k token) | Antigravity `Orchestrator (Neon)` come secondario; implementazione delegata a M1 |
| **M3** | **`gstn-anim-reviewer`** indipendente (Regole 10 e 15a: video 1x + 25% slow + griglie + `ASSERT: OK`, VITA/ROOT prima dei numeri) | **2C:** `Gemini 4 Neon (High)` solo **dopo** calibrazione cieca sulle 4 review (≥3/4 esatte e 0 falsi PASS) + controllo Claude su 1 PASS su 3 | Se calibrazione <3/4 o falso PASS → torna a **2B** (Claude Pro obbligatorio sul PASS). Reviewer sempre indipendente dall'executor |

### Sicurezza EULA Epic, credenziali e VRAM
- **EULA Epic (hard block):** sorgente Unreal Engine (`C:\Program Files\Epic Games\UE_5.9\`, `Plugins/EngineOverrides/`, `Plugins/GASNPP/`) non va **mai** inviato a DeepSeek o altri provider esterni; resta solo su **Claude Pro** o **Inkling locale offline**.
- **Credenziali:** `DEEPSEEK_API_KEY` vive **solo** nella variabile d'ambiente utente di Windows (`os.environ`), mai nel repo, mai in file JSON/YAML tracciati e mai nelle chat.
- **Modello locale (VRAM):** `call_local` / `Invoke-Worker -UseLocal` è bloccato in codice se un processo `UnrealEditor*` è in esecuzione sul PC.

## 4. OUTPUT
Gli script scrivono su file e stampano **max 5 righe**; niente log, report o diff interi nel contesto. Un subagente di review riceve solo griglie, criteri e brief (niente handover vecchi, codice o log), target max **80k token**. Immagini: una griglia per vista, guardata una volta; review video max 8 immagini.

## 5. TENTATIVI E R5-bis
Max **2 tentativi** di fix, poi fermati e scrivi cosa serve. Build, test e catture solo tramite `Tools/CI/Run-Timed.ps1` (build 45 min, test 30, cattura 15) che chiude l'intero job tree e scrive `ESITO: TIMEOUT`; un timeout vale un tentativo. I tentativi si contano anche **per obiettivo** in `INDICE.md` / `queue.json`: dopo 2 thread falliti o parziali sullo stesso obiettivo, `Run-Queue.ps1` blocca in `needs_diagnosis` e si apre un thread di **DIAGNOSI DI SISTEMA** (cloud, M2, budget ≤ 8 $) che legge solo gli handover di quell'obiettivo e propone causa comune + 2–3 opzioni.

## 6. DECISIONI E R6-bis
Non aspettare l'owner: applica la consigliata e scrivila nell'handover. **NON vale** per cambiare strumento o pipeline: se manca lo strumento previsto da skill o brief (Cascadeur, editor, salvataggio asset, macchina) il thread si ferma al primo punto stabile e scrive cosa serve; un generatore alternativo produce solo prototipi marcati `PROTO`, mai in review né all'owner. All'owner solo architettura, feature nuove, design delle mosse, azioni irreversibili, costi oltre soglia: **UN messaggio** per sessione, max 3 decisioni, con opzioni a lettera, consigliata, costo e cosa resta fermo; risposta `"1A 2B 3A"`. Intanto si lavora la voce successiva della coda.

## 7. POSTI E LOCK `_ue59`
1 thread con editor per macchina (`fisso`, `portatile`) + **max 1 thread cloud Claude Pro attivo alla volta**. Un secondo thread sulla stessa macchina solo senza editor e in un worktree suo. `_ue59`: un thread alla volta (`_ue59/IN-USO.txt` gestito in `try/finally` da `Invoke-Worker.ps1` / `Run-Timed.ps1`); solo l'ultimo della sessione la aggiorna e la compila per il playtest. Niente lavoro speculativo.

## 8. COSTO
Al budget il thread si ferma al primo punto stabile e scrive handover e riga in `costi.csv`. Il coordinatore può dare **UNA** estensione fino a +30%; oltre, o per più di 15 $ non previsti, decide l'owner. Un piano approvato copre fino a +20%. Le stime partono dai costi reali in `INDICE.md`.

## 9. OWNER
Scrive al coordinatore, non nei thread (eccezione: approvazioni di macchina/dispositivo). A inizio sessione il coordinatore manda **UN** messaggio con ciò che servirà e la frase da incollare. Un thread non chiede mai l'altra macchina: si passa via branch git.

## 10. ANIMAZIONI E R10-ter
Nessuna variante arriva all'owner senza che qualcuno abbia **GUARDATO** il video in-game. Prima di una mossa: cattura economica della clip BASE; se mostra difetti del gioco, prima un thread di codice. Il thread sul fisso committa mp4 (1x + slow 4x) e griglie (variante e base, stessi frame) in `docs/notes/reviews/<variante>/`; il reviewer guarda i frame attorno a hit e FAF, confronta con base e regole dell'owner, dà PASS/FAIL. Una review = una mossa. Voci di FAIL etichettate `posa` / `dati montage` / `gioco`; `"gioco"` va alla coda del codice e non conta come giro. Max 3 giri di posa, poi decide l'owner. Solo PASS va all'owner.
- **Catena di cattura:** nessuna review parte senza `ASSERT: OK` da `capture_assert.py` (clip attesa con peso ≥ 0,8 nella finestra attesa).
- **Ordine obbligatorio:** giudica **PRIMA la VITA** (anticipazione, arco, peso, follow-through) e **ROOT** (root motion coerente con capsula entro ±20%) sul video a velocità reale e 25% senza numeri, **POI** i check tecnici.
- **Guard di conservazione:** un fix automatico che riduce ampiezza, velocità di picco o root motion oltre il 15% è **FAIL**; splice max 0,1 s e max 10% della clip, sempre con griglia prima/dopo. Se l'owner dice che il video non torna, prima si controlla la catena di cattura.

## 11. PROVE
Ogni numero in un handover cita file di log e comando che lo conta. Chi parte da un handover ne ricontrolla uno in sola lettura. Un report arrivato troppo presto si verifica sempre.

## 12. GUARDIA SULL'OWNER (2026-11-15)
Prima di eseguire una richiesta dell'owner, coordinatore e thread controllano se: contraddice una regola/decisione esistente; generalizza oltre il suo ambito una regola locale; porta a lavoro agentico subottimale. In quei casi NON eseguire alla cieca: avvisa subito citando regola o rischio e proponi 2–3 opzioni strutturate. Ogni regola dell'owner si registra con il suo **AMBITO**.

## 13. IDENTITÀ E BASI DELLE MOSSE (2026-11-15; ambito: tutte le animazioni)
a) Ogni brief elenca 2–3 **TRATTI D'IDENTITÀ** con riferimento; la review li controlla PER PRIMI e cambiarli richiede decisione dell'owner.
b) Versioni approvate dall'owner sono **BLOCCATE** (`approved/<mossa>-<versione>` e `docs/notes/anim-approvate.md`); nessun thread le sovrascrive (bloccato dal pre-commit hook `Tools/CI/Test-GitGuard.ps1`).
c) Lo **SHEET DELLE CURVE** è condiviso tra chi anima e chi fa review.
d) Nessun "da confermare" resta solo in un handover: il coordinatore li raccoglie per l'owner.
e) I vincoli di gioco non piegano la posa: si spostano manichino o hitbox, non si accorcia l'arco, salvo decisione dell'owner.

## 14. GASP STOCK (2026-11-20; ambito: locomotion e ABP)
Il default è il comportamento stock di GASP Mover (pose matching, root motion dalla clip, uscite a evento). Ogni deviazione ha una riga in `docs/notes/gasp-divergenze.md` (cosa, perché, alternativa stock, costo per tornare indietro) PRIMA del commit; prima di aggiungerla si chiede all'Unreal Assistant (`Tools/UE59/ask_unreal_assistant.py`) come lo fa GASP stock.

## 15. EFFICIENZA ANIMAZIONI, CODA E GUARDIE (2026-11-21; aggiornata 2026-11-22)
a) La regola 10 vale anche per la locomotion: nessuna variante va all'owner senza review indipendente.
b) **Controlli automatici pre-cattura (`-PreCheck` in `Run-Timed.ps1`):** caviglie mai sotto 10 cm, angolo punte nel range stock, ossa mai allungate oltre 5%, picchi di tremolio frame per frame su gomiti/polsi, compenetrazioni; se uno fallisce non si apre l'editor per catturare né si avvia la review.
c) **Ritocchi come parametri:** per "un po' di più/meno" si espone una cvar nel menu GSTN Debug Options (un controllo per opzione) invece di una clip nuova.
d) **Lavoro meccanico a tier economici:** build, import, catture e montaggio video a `M0`/`M1`.
e) **Catture:** tutte le varianti e viste in una sessione, video real-time GPU invece di PNG per frame, sempre dietro E di lato, finestre sul secondo monitor; prima UNA direzione (es. 180 L), le altre dopo l'OK.
f) **Coda deterministica (ex thread di turno LLM):** `Tools/CI/Run-Queue.ps1` legge `handoffs/queue.json` e lancia worker freschi via `Tools/CI/Invoke-Worker.ps1` (0 token di contesto residuo). Le voci bloccate in attesa owner vengono saltate.
g) **Editor & Git Guard:** se un editor l'ha aperto un thread e l'owner non ha modificato niente, lo chiude senza chiedere (`Tools/UE59/Close-OwnerEditor.ps1`); **vietato** committare file locali dell'owner (`DA_Locomotion_Default.uasset`, `CHT_*_PoseMatch.uasset`, `Mixamo/`, `EditorPerProjectUserSettings.ini`), bloccato meccanicamente da `.githooks/pre-commit` → `Tools/CI/Test-GitGuard.ps1`.
h) **Integrazione:** il codice prodotto nei thread confluisce in un branch di integrazione compilato sul PC almeno una volta al giorno.
