# Revisione del nuovo processo di animazione (modello leggero su Antigravity) — 2026-11-06

**Premessa (owner, 06/11 15:07Z):** il nuovo processo gli sembra migliore del vecchio. Questa revisione dice cosa del vecchio vale la pena portarsi dietro, non lo difende.

**Cosa ho potuto leggere (solo lettura, 15:10Z):** in `GASP_Giustino2_feature` non c'è nessun file del nuovo processo: nessun `AGENTS.md`/`GEMINI.md`/`.agent/`, nessun file in `Tools/`, `docs/`, `Content/` o `Saved/AnimPipeline/` modificato dopo il 04/11. L'editor è aperto da ~01:47Z con il bridge `unreal-mcp` attivo: il modello lavora quindi direttamente nell'editor via MCP e le sue istruzioni stanno nel workspace di Antigravity, fuori dalla cartella. Le osservazioni sotto sono quindi requisiti, da confrontare con quelle istruzioni.

**Trovato in cartella:** la variante M (11-04 13:53) è completa in locale (carta, req, diagnosi, 3 mp4 + 6 griglie in `docs/notes/reviews/dair-varM/`) ma **non è mai stata pushata** (su GitHub `0pafeo` finisce al brief) e non ha avuto la revisione video.

## Cosa va bene
- Un modello leggero e veloce per iterare sulle pose è la scelta giusta: ogni variante dair su Claude è costata $4-12, il grosso in rendering e giri ripetuti, non in ragionamento.
- La pipeline `animpipe` è deterministica e scritta per essere usata da qualsiasi agente: `author → build → check → verify --req → render --standalone`. Basta che il modello leggero la chiami, non serve reinventarla.

## Rischi concreti (in ordine di gravità)
1. **Fix della cattura non in feature.** `3f3d01e` (ogni immagine mostrava la posa del frame precedente) e la PR #28 (`render --standalone`, `verify`, etichette `v<N> (f<n>)`) stanno solo su `0pafeo`, 35 commit sopra feature. Se `_feature` è su `feature/gas-npp-5.8`, **i video del nuovo processo sono sfasati di un frame** e lo stomp appare a f16. Prima cosa da verificare.
2. **Stato vivo solo nell'editor.** Lavorando via MCP, pose e sequenze cambiano negli asset aperti: se non passano da una carta JSON in git, un crash dell'editor (in `Saved/Crashes` ce n'è uno delle 03:27Z di oggi) o un `build` di `animpipe` (cancella e ricrea `LS_<nome>`) le perde. Requisito: ogni giro buono finisce in una carta variante o nel layer "Manuale" salvato.
3. **Gate visivo (regola 10).** Il modello leggero può dire "PASS" leggendo numeri. Il verify numerico non ha mai sostituito il video (varianti I e K passavano i numeri e fallivano a vista). Ora che l'owner guarda il video da solo il gate è rispettato per i giri locali, ma una variante non va unita alla carta principale senza mp4 committati in `docs/notes/reviews/<variante>/`.
4. **Chiavi dell'owner.** Le chiavi 15/31/37/42 della dair e i blocchi `approved` sono protetti da `load()` e `fidelity` solo se il processo passa da `animpipe`. Se il modello muove i controlli nel Control Rig via MCP o edita JSON a mano, nessun lock scatta. Requisito: mai `--unlock`, mai `--force` tranne il caso `hand_l|spine_03` −0,57 cm già noto.
5. **Numerazione frame.** Frame = frame di ANIMAZIONE, 0-based; nel video in-game ci sono 8 frame di hitstop dopo lo stomp con etichetta ripetuta. Un modello nuovo tende a contare i frame del video: va scritto nelle sue istruzioni.
6. **Varianti non distruttive.** Le varianti vivono in `overlord_dair_v4_var<X>.json`; la carta principale cambia solo dopo "applica". Un agente che itera veloce tende a sovrascrivere la carta principale.
7. **Divergenza dei rami.** Se il processo lavora su un ramo nuovo partito da feature, perde `0pafeo` (varianti K/L/M, req json, brief M). Meglio partire da `0pafeo` o unire prima `0pafeo` in feature.
8. **Editor e mappe.** L'editor principale dell'owner ha `DefaultLevel` non salvato; il render standalone apre `L_PilotCapture` in background. Il modello non deve salvare mappe né toccare il checkout principale.

## Divisione del lavoro proposta
| Chi | Cosa |
|---|---|
| **Modello leggero (Antigravity, locale)** | Giri di posa sulle varianti: modifica carta variante → `author/build/check/verify/render` → mp4 + griglie in `docs/notes/reviews/<variante>/`, push. Una modifica dichiarata per giro, diagnosi di 3 righe prima di ogni build, max 2 tentativi. |
| **Owner** | Guarda il video, dice "applica"/"scarta". Unico che sposta chiavi proprie o sblocca parti. |
| **Claude cloud (revisione)** | Solo su richiesta o prima di "applica": estrae i frame degli mp4 pushati e dà PASS/FAIL contro regole owner e base. Costo ~$1-2. |
| **Claude (Opus) — logica Unreal** | GAS/NPP/C++: M7-15 desync al 2%, M8 minion, M10 held/thrown, harness e test. Più manutenzione di `animpipe` stesso (bug della pipeline, nuovi check) quando il modello leggero li segnala. |

Per rendere il passaggio sicuro serve un solo file di istruzioni nella root di `_feature` (es. `AGENTS.md`, letto da Antigravity) con: comandi ammessi, regole 2-5 sopra, numerazione frame, cartella di output. Posso scriverlo io (~$1) come proposta separata (`AGENTS-proposta.md`), senza toccare il processo, partendo da `cards/README.md` e dal brief M.

## Domande per l'owner (un solo blocco)
0. Dove stanno le istruzioni/regole che hai dato al modello in Antigravity (file o testo)? Senza quelle non posso fare il confronto riga per riga. Mi basta che le copi in `docs/notes/` o in project files.
1. Su che ramo gira `_feature` adesso? Se è feature, unisco prima `0pafeo` (fix cattura + PR #28 + varianti) in feature? **Consiglio: sì.**
2. Il modello leggero usa `animpipe` o muove i controlli in editor via MCP? Nel secondo caso i lock dell'owner non scattano e il lavoro va salvato in una carta a fine giro.
2b. Pusho la variante M che è ferma in locale dal 04/11 (solo file in `docs/notes/reviews/dair-varM/` e la carta, nessun asset)? **Consiglio: sì**, così non si perde; lo faccio solo dopo il tuo ok.
3. Vuoi che scriva l'`AGENTS.md` per Antigravity con le regole sopra? **Consiglio: sì.**
4. La revisione cloud la vuoi su ogni variante o solo prima di "applica"? **Consiglio: solo prima di "applica"**, così non rallenta l'iterazione.
