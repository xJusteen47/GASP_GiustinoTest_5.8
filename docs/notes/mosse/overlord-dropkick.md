# Scheda mossa: Overlord dropkick = Fair = Big Band j.HK (cards `cards/overlord/dropkick/overlord_dropkick_{startup,loop,land}.json`)
Stato 2026-11-15: la versione attuale (r1, gambe aperte) è PEGGIO della base; ripristino della base Antigravity in corso. Nessuna approvata.

## 1. Identità
| # | Tratto | Riferimento | Chi |
|---|---|---|---|
| 1 | GAMBE UNITE E DRITTE (piedi vicini, ~10 cm) | base Antigravity, owner 11-15 | [OWNER] |
| 2 | BRACCIA CONSERTE sul petto (posa di potere: l'ingresso è uno snap durante il caricamento, parte da guardia) | base Antigravity; `docs/notes/anim-process.md` Pilastro D; refs Bison | [OWNER] |
| 3 | **Deve finire in stato GROUNDED** (a terra): il roll da lì serve per le combo extension; l'owner ne parlerà più avanti. La rialzata non deve togliere questo stato | owner 15-11 15:31Z | [OWNER] (dettagli [DA CONFERMARE]) |
| 4 | Tenuta in aria fino all'atterraggio, poi caduta pesante con un rimbalzo e stesa; poi rialzata | `anim-process.md` §land; owner 11-15 (hitbox attiva fino a terra = corretto) | [OWNER] hitbox; hitbox [OWNER] |
Riferimento: i file `*.r0.backup.json` e `_design_r0` in `cards/overlord/dropkick/` sono lo stato prima di r1 (la r0 aveva braccia conserte, atterraggio che non arriva a terra: difetto da correggere SENZA toccare i tratti 1-2). Regola: `memory owner-base-is-the-base`: cambiare un tratto = decisione dell'owner prima della build.

## 2. Fasi
| Fase | Frame | Note |
|---|---|---|
| Startup | 0-15 | caduta→raccolta (chiavi 0,4,8,11,15), colpo f15 = primo frame attivo |
| Loop | 0-11 | periodo 12, respiro ±1,5 cm (da aria) |
| Hold-until-landed | [16,27) | resta in aria finché non tocca terra; land a 60 (`handovers/dropkick-land-1.md`) |
| Land | 0-31 | slam f1-f2, rimbalzo f6, secondo tocco f10, a terra f16 = `reaction_lieup`, tenuta a f31 |
| Rialzata | non esiste | oggi: da steso a accovacciato in 4 frame (cp5b) |

## 3. Curve [DATI] (card r1; la base r0 non ha curve scritte)
| Segmento | Timing | Easing | Overshoot | Follow-through |
|---|---|---|---|---|
| Startup | 16 f, rilascio 6/10/26/58% su f12-15 | pose dense, passo massimo f14→15 (45,8 cm) | n/d | busto/testa/braccia dopo 1-2 frame |
| Loop | 12 f | respiro lento | | 1-2 f di ritardo su busto/testa |
| Land | 32 f | slam + 1 rimbalzo (+13,5 cm) | rimbalzo unico | testa frusta indietro f1-2 |
Valori della base (r0): [DA CONFERMARE] (da rileggere da `r0.backup.json`).

## 4. Hitbox: OWNER
Attiva dal f15 (CSV `OverlordDropkick_timing.csv` frame 16) **fino all'atterraggio**: confermato corretto dall'owner 11-15. Forma e posizione: dati kit, [DA CONFERMARE]. FAF: non fissato: [DA CONFERMARE].

## 5. Blocchi
Approvata: nessuna. Base da bloccare come `good/dropkick-1`: la versione Antigravity precedente al commit 26733f24 (r1). Commit esatto: [DA CONFERMARE] (non nel clone corto, `git log 26733f24^` sul fisso). Comando: `git tag -a good/dropkick-1 26733f24^ -m "base Antigravity: gambe unite, braccia conserte"`. Tag: DA CREARE (cloud 403).
| n | Tag | Commit | Card | Data | Buono | Manca |
|---|---|---|---|---|---|---|
| 1 | good/dropkick-1 (DA CREARE) | 26733f24^ [DA CONFERMARE] | base Antigravity (= r0) | 2026-11-15 | gambe unite, braccia conserte, struttura | atterraggio che non arriva a terra, nessuna rialzata |

## 6. Domande aperte
1. Rialzata: ora vincolata dal grounded state + roll (combo extension, dettagli dell'owner in arrivo); rialzata veloce o animata (domanda 4)? 2. Caduta capsula 53 cm: traccia capsula (consigliata, ~$10-15) o clip che parte all'altezza del loop (domanda 5).
3. Dettagli del grounded state/roll quando l'owner li dà.

## 7. Storia
Antigravity base → 26733f24 r1 (gambe aperte, rollio 35°, braccia lanciate: peggio, owner 11-15) → diagnosi `claude/project-thread-hgy214` → ripristino in corso. Codice: hold-until-landed (`dropkick-land-1.md`), cp5b FAIL gioco (no rialzata).
