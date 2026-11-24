# Anim MCP: costo e velocità con modelli deboli (DeepSeek Flash)

Data 2026-11-23. Ambito: `gstn-anim-mcp` v2 (branch `claude/anim-mcp-v2-turn4`), solo ricerca, nessun codice toccato.
Legenda: **[CONF]** = confermato da fonte citata; **[INF]** = mia deduzione, da verificare con una misura.

## 1. Risposta breve sulla cache (la tua domanda)

**La cache NON scade "tra una chiamata e l'altra".** Fonte ufficiale (https://api-docs.deepseek.com/guides/kv_cache):
- **[CONF]** Attiva di default, su disco, nessun parametro. Hit solo se il prefisso coincide per intero con un'unità di prefisso già salvata.
- **[CONF]** Nessun TTL fisso: "una volta che la cache non è più in uso viene cancellata automaticamente, di solito entro da poche ore a qualche giorno". Quindi tra due chiamate a distanza di minuti non scade.
- **[CONF]** È "best-effort": nessuna garanzia di hit al 100%. La costruzione richiede alcuni secondi, quindi la **prima** richiesta con un prefisso nuovo non fa hit, e due richieste lanciate quasi insieme in parallelo possono entrambe fare miss.
- **[CONF]** I campi di risposta sono `usage.prompt_cache_hit_tokens` e `usage.prompt_cache_miss_tokens`.
- **[CONF]** Granularità/minimo di token: la pagina non li dichiara. Non assumere un blocco o un minimo.
- Prezzi: la pagina ufficiale dei prezzi **non è stata leggibile** (il fetch restituiva la pagina Quick Start). Cifre da fonte terza (https://apidog.com/blog/deepseek-v4-1-flash-pricing/, "verificate 10 sett. 2026", quindi possibili di essere vecchie): off-peak hit $0.003 / miss $0.15 / output $0.60 per 1M; peak (lun-ven 01-04 e 06-10 UTC) il doppio. **[INF]** orientative, da ricontrollare. Conta il rapporto: un hit costa ~1/50 di un miss.
- Modello oggi: `deepseek-flash` (servito da V4.1-Flash-1017); endpoint formato Anthropic `https://api.deepseek.com/anthropic` (https://api-docs.deepseek.com/quick_start).

**Perché potresti vedere sempre prezzi da miss (ipotesi in ordine di probabilità, tutte [INF]):**
1. Testo variabile all'inizio: data/ora, cwd, stato git, id di sessione, elenco file nel system prompt. Un solo carattere diverso all'inizio azzera il prefisso. Claude Code con `claude -p` aggiunge contesto dinamico di suo.
2. Router/proxy (LiteLLM, OpenRouter, ecc.) che manda le chiamate a provider o istanze diverse: la cache è per provider, non condivisa.
3. Ordine o contenuto dei tool che cambia tra chiamate (lista tool rigenerata, ordine dei dict non stabile, descrizioni con valori dinamici).
4. Chiamate troppo ravvicinate o parallele sul prefisso nuovo (cache non ancora costruita), o sessioni sempre nuove a freddo.
5. Il prefisso fisso è corto: la parte che cambia domina e il risparmio si vede poco.
6. Lettura sbagliata: le fatture mescolano hit e miss; guarda i due campi `usage`, non il totale.
**Prova decisiva (2 chiamate, costo ~0):** stessa richiesta due volte a 10 s di distanza, direttamente su DeepSeek senza router, leggendo `prompt_cache_hit_tokens`. Se la seconda fa hit, il problema è nel nostro bridge/prefisso; se no, è nel provider/endpoint.

## 2. Come è fatto oggi il tool (letto `mcp_server.py`)
7 tool; le descrizioni sono lunghe (solo `anim_apply_intent` ~2.5 KB: timing, parametri) e **statiche**: ottimo come prefisso cacheabile, a patto che l'ordine resti identico. Le risposte sono `json.dumps(res, indent=2)` per intero: contro la regola 4 (max 5 righe) e il peggior spreco di token in uscita dal tool. Nota: i tool `anim_gate_check` e `anim_capture_ab` sono già "0 token LLM" (deterministici): bene.

## 3. Regole di layout del prompt (per bridge e MCP)
1. Ordine fisso: system prompt → definizioni tool (ordinate, serializzazione stabile) → regole/skill → solo alla fine i dati della chiamata. Mai data, cwd, id, percorsi temporanei in testa.
2. Storia append-only: non riscrivere né riordinare messaggi precedenti; i risultati nuovi vanno in coda.
3. Lo stesso modello e lo stesso endpoint per tutta una sessione; niente router che alterni provider.
4. Lancia **una** chiamata di riscaldamento prima del batch, poi i parallelismi (la cache richiede secondi).
5. Logga a ogni chiamata hit/miss/output da `usage` in un CSV: base per `handoffs/costi.csv`.

## 4. Piano d'azione ordinato (risparmio / sforzo / rischio)

| # | Azione | Risparmio | Sforzo | Rischio |
|---|---|---|---|---|
| 1 | Misurare: log di `prompt_cache_hit/miss_tokens` + test delle 2 chiamate (§1). Decide tutto il resto | abilita | 0,5 h | nullo |
| 2 | Risposte tool compatte: JSON senza indent, max ~5 righe di sintesi, il dettaglio in file (`path` + `sha`) | alto (output e contesto) | 2-3 h | basso se il file ha tutti i dati |
| 3 | Prefisso stabile: rimuovere ogni testo dinamico dall'inizio, ordine tool fisso, test automatico che due avvii producano lo stesso prefisso byte per byte | alto (se oggi si paga miss) | 2 h | basso |
| 4 | Batch: un tool `anim_apply_and_check(src,dst,intent)` che esegue snapshot + intent + gate + sintesi in una sola chiamata (oggi 3-4 giri) | medio-alto (meno giri = meno contesto riletto) | 3-4 h | basso, i tool singoli restano |
| 5 | Cache dei risultati per hash (asset sha + intent canonico + versione tool): stessa richiesta = risposta da file, 0 token | medio | 2-3 h | medio: invalidare bene su cambio clip/regole |
| 6 | Più lavoro negli script deterministici: validazione dell'intent (range, frame fuori clip) prima della chiamata, correzioni automatiche dei gate banali, riassunto numerico già fatto dallo script | medio | 3-5 h | basso |
| 7 | Parallelismo: catture A/B e gate di varianti indipendenti insieme, dopo il riscaldamento (punto 3 regole) | velocità alta, costo neutro | 2 h | medio: GPU/_ue59 è una risorsa singola (lock) |
| 8 | Routing: Flash per applicare intent e gate, un modello più forte solo per giudicare la posa (già regola 10/15d) | qualità protetta | 1 h | basso |
| 9 | Pre-compressione del contesto per i modelli deboli: un "cheat sheet" breve dei parametri più usati nel prefisso; il manuale lungo solo su richiesta | medio | 2 h | medio: tagliare troppo peggiora l'output |

Nessuna azione riduce la qualità del risultato se i punti 2 e 5 conservano i dati completi su file e i gate (regola 15b) restano identici. Il giudizio visivo (regola 10) non si tocca.

## 5. Implicazioni per le regole
- **Regola 3 (modelli):** DeepSeek Flash non è nella tabella; andrebbe aggiunto per lavoro meccanico (apply intent, gate, catture) accanto a Haiku/Sonnet, **mai** per giudicare pose (resta Fable/Opus). Decisione dell'owner, già pendente (riscrittura regola 3). Il prezzo da miss/hit rende il prefisso stabile più importante della scelta del modello.
- **Regola 15:** (b) i gate restano obbligatori e identici; (c) i ritocchi come parametri si sposano bene con `intent_json` e con la cache per hash; (d) conferma che il lavoro meccanico va ai modelli economici; (f) i thread "di turno" beneficiano della cache solo se riusano lo stesso prefisso nella stessa sessione, quindi meglio meno sessioni lunghe che tante brevi.
- **Regola 4 (output):** le risposte JSON a pieno indent la violano già oggi.
- Esclusioni dal progetto (sorgenti Unreal Engine, credenziali) restano: nessun dato del genere nei prefissi.

## 6. Da decidere dall'owner (se vuole procedere)
1. Autorizzare il test delle 2 chiamate (punto 1) e dire quale bridge/router usa oggi: senza questo la causa dei "sempre miss" resta ipotesi.
2. Ordine consigliato di esecuzione: 1 → 2 → 3 → 4, poi 5-9 secondo i numeri misurati.

COSTO: ~$1,5 stimati | 0 minuti desktop | 0 token review
