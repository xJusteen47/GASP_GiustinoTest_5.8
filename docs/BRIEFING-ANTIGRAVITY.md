# Briefing per Antigravity e altri agenti esterni

Una pagina da allegare a ogni sessione di un agente che NON vede la chat del progetto né la memoria (Antigravity/Neon,
Gemini, DeepSeek, Cascadeur scripting, altri). Versione 2026-11-23. Se qualcosa qui sembra in contrasto con un altro
documento, **fermati e segnalalo**: non scegliere da solo.

## 1. Prima di iniziare (obbligatorio)
1. Leggi `docs/STATO-ATTUALE.md`: decisioni vive, versioni approvate, cosa è sospeso. Vince su ogni altro documento.
2. Leggi `docs/INDICE-PROGETTO.md` e scrivi nel tuo output **cosa riusi** (esempio vero: la mira degli attacchi esisteva
   già dal 16-11 e stava per essere riscritta).
3. Lavora solo sul brief che ti è stato dato (`handoffs/<nome>.md` nei project files o il testo incollato): una macchina,
   un budget in $, un obiettivo. Se manca uno di questi, chiedi prima di partire.
4. Le regole vecchie in `docs/notes/regole-progetto-2026-11-20.md` e `-2026-11-22.md` (Antigravity coordinatore, DeepSeek
   esecutore) **non valgono più**: dal 23-11 coordina Claude; tu fai solo il compito del brief.

## 2. Cosa NON devi fare
- **Non sovrascrivere una versione approvata** (tag `approved/*`, righe di `docs/notes/anim-approvate.md`, es. run start
  v4r7, w1, w2, 29w3): si lavora sempre su una variante nuova.
- **Non toccare GASP**: `Content/Blueprints`, `Content/Input`, `Content/Levels` e l'ABP stock. Ogni deviazione da GASP
  va prima scritta in `docs/notes/gasp-divergenze.md`.
- **Non toccare i file locali dell'owner** (`DA_Locomotion_Default`, `CHT_*_PoseMatch`) e non committarli.
- **Niente tasti fisici nel codice**: solo azioni di input / comandi (regola 16). I tasti nei brief sono default rimappabili.
- Niente push su `main` o `feature/*`, niente force push, niente cancellazioni di branch, `reset --hard`, `git clean`.
  Push solo su un branch `claude/*` o su quello indicato nel brief.
- Non cambiare strumento o pipeline (es. altro generatore al posto di Cascadeur): fermati al primo punto stabile e scrivi
  cosa serve. Un prototipo con strumento alternativo si marca `PROTO` e non va mai in review né all'owner.
- Non mandare un'animazione all'owner: solo PASS di una review con video in-game guardato (regola 10).
- Non usare modelli esterni su codice Epic (`Plugins/GASNPP/`, `Plugins/EngineOverrides/`, sorgenti di Unreal) e non
  inviare credenziali a servizi esterni.
- Simulazione deterministica: niente rami `IsServer()`, niente stato fuori dalle strutture sincronizzate (`CLAUDE.md`).

## 3. Limiti di lavoro e di spesa
- Max **2 tentativi** di fix per obiettivo, poi fermati e scrivi cosa serve.
- Build, test e catture solo da `Tools/CI/Run-Timed.ps1` (build 45 min, test 30, cattura 15); un timeout vale un tentativo.
- Al budget ti fermi al primo punto stabile e scrivi l'handover.
- Script e log: scrivi su file, a schermo max 5 righe. Review video: max 8 immagini; prima la vita del movimento, poi i numeri.
- Prima di ogni cattura i controlli automatici (caviglie, punte, ossa allungate max 5%, tremolio, compenetrazioni).
- Un editor per macchina; la cartella `_ue59` la usa un solo lavoro alla volta (`_ue59/IN-USO.txt`).

## 4. Formato dell'handover (sempre, max 30 righe)
File `docs/notes/handovers/<obiettivo>.md` sul tuo branch:
```
# <obiettivo> (<data>)
Brief: <file>  Macchina: <fisso|portatile|cloud>  Modello: <nome>
Riuso dichiarato: <cosa di INDICE-PROGETTO hai riusato>
Fatto: <3-10 righe; ogni numero cita il log e il comando che lo conta>
Non fatto / bloccato: <cosa serve>
Decisioni applicate: <la consigliata, se una scelta era reversibile>
Da confermare (owner): <elenco, max 3>
Branch e commit: <branch> <sha>
SUBAGENTI: <quanti, per quale lavoro, modello di ciascuno>
COSTO: <$ stimati> | <minuti desktop> | <token review>
```
La riga COSTO va copiata anche in `handoffs/costi.csv` (`data,thread,obiettivo,macchina,modello,minuti_desktop,token_review,usd_stimati,esito`).

## 5. Dove trovare le cose
| Cosa | Dove |
|---|---|
| Stato e decisioni vive | `docs/STATO-ATTUALE.md` |
| Mappa del codice, "dove metto mano" | `docs/INDICE-PROGETTO.md`, `docs/DOVE-METTO-MANO.md` |
| Regole tecniche del codice | `CLAUDE.md`, `docs/architecture/` |
| Animazioni approvate | `docs/notes/anim-approvate.md`, tag `approved/*` |
| Notazione delle mosse | `docs/notes/notazione-mosse.md` (branch `claude/anim-inventario-2026-11-23`) |
| Registro dei thread e costi | `handoffs/INDICE.md`, `handoffs/costi.csv` (project files) |
| Ultimo rapporto sulla documentazione | `docs/notes/archivio-proposta.md` |
Finché questi file non sono in `main`, stanno sul branch `claude/docs-bibliotecario-2026-11-23`.
