# Proposta: Claude al posto dell'interpolazione IA di Cascadeur (2026-11-02)

Solo proposta, niente codice finché l'owner non approva.

## Com'è oggi
- La scheda (`cards/<kit>/<move>.json`) è la fonte di verità. Gli intermedi tra le chiavi li fa la pipeline in modo
  meccanico: `author.frames()` interpola i campi `author` con una smoothstep, poi la build applica i profili di `spacing`
  e i `part_delays`. Nessuno "pensa" agli intermedi: archi, overlap e anticipo dei singoli arti escono solo se qualcuno
  aggiunge chiavi a mano.
- Il giro con Cascadeur (`casc-export/import/revert/diff`) porta in Cascadeur solo le chiavi della scheda. Le curve che
  Cascadeur mostra tra le chiavi sono sue e servono solo da anteprima.

## Cosa cambia
L'owner mette in Cascadeur solo le **sue** chiavi. Claude scrive le **pose intermedie** (breakdown) tra una chiave e
l'altra, seguendo le regole di `cards/README.md` e i tempi dati dall'owner. Le pose intermedie tornano in Cascadeur come
chiavi sparse che l'owner può modificare o cancellare. Le chiavi dell'owner sono bloccate e Claude non le tocca mai.

## Percorso consigliato
1. **Segnare le chiavi (in Cascadeur).** Un comando nel menu Scripts, "GSTN → Segna chiavi mie", aggiunto a `casc_prepare.py`
   con l'API `csc`. Salva la scena, esporta le ossa (`export_joints`) e scrive accanto al `.casc` un file
   `<move>_owner_keys.json` con i frame delle chiavi dell'owner e, se l'owner lo indica, i tempi (fasi, hitbox).
2. **Import.** `casc-import --owner-keys` legge quelle chiavi come oggi. Ogni chiave dell'owner diventa un blocco
   `approved` (`by: owner`) su tutte le parti tranne dita e polso villain, che restano della pipeline. Si usa il formato di
   PR #24, quindi la build si ferma se una posa bloccata cambia.
3. **Interpolazione (una sessione di Claude).** Claude riceve la scheda, le chiavi dell'owner, i tempi e le regole del
   README. Restituisce:
   - pose intermedie nuove in `author`, ognuna segnata `origin: claude` con una riga di motivo (per esempio "arco della
     mano", "ginocchio prima del busto");
   - se servono, `spacing` e `part_delays` per segmento.

   Claude non scrive mai una chiave su un frame dell'owner. Il comando lo controlla: se una posa intermedia cade su una
   chiave bloccata, la scheda non viene scritta.
4. **Controlli.** Prima di mandare il risultato all'owner girano `animpipe build` e `check`. Devono passare co_motion,
   hitstop_snap (≤10 cm), clearance (con le esenzioni già accettate), lunghezza delle ossa, la regola del frame 0 e, dopo
   PR #24, `rules` e `source_fidelity`. Se un controllo fallisce, Claude ha al massimo 2 tentativi (regola 5). Poi si
   ferma e scrive quale controllo fallisce e su quali frame.
5. **Ritorno in Cascadeur.** `casc-export` riscrive la scena con le chiavi dell'owner e quelle di Claude. Il file
   `<move>_owner_keys.json` dice quali sono quali, perché l'API non permette di colorare le chiavi. La scena va
   riaperta. L'owner guarda e, se una posa intermedia non gli va, la sposta (diventa sua e si blocca al prossimo
   import) oppure la cancella.
6. **Tornare indietro.** `casc-revert` rimette la scheda com'era prima dell'ultimo giro, usando lo snapshot che
   `cards/<kit>/history/` salva già a ogni import. `casc-diff` mostra quali parti ha cambiato Claude.

## Come si avvia
- **Fase 1 (subito, senza codice nuovo per l'avvio):** l'owner salva in Cascadeur e scrive al coordinatore "interpola
  la dair". Il coordinatore apre una sessione sul desktop con un brief fisso (`docs/notes/handovers/anim-pipeline.md`
  più il README).
- **Fase 2 (facoltativa):** il comando Scripts lancia da solo Claude Code in modalità senza interazione (`claude -p`)
  sul desktop, con lo stesso brief. Un clic per giro, ma la spesa parte a ogni clic.

## Costi
Ogni interpolazione è una sessione di Claude. La stima, fatta sui giri della dair di oggi (~$1 per un giro
mirato, $10-19 per un thread intero), è di circa $1-3 per giro con Opus effort medium, tra lettura della scheda, pose e
2 build+check. Sonnet costerebbe meno ma oggi sbaglia di più sulle pose. Il contesto resta piccolo: scheda + README +
risultati dei check su file (regola 4).

## Rischi
- `fit()` sa esprimere solo quello che `author.py` sa fare. Una posa intermedia troppo ricca viene approssimata, ma il
  residuo viene riportato, non nascosto.
- Va fatto dopo il merge di PR #24, perché blocchi, `rules` e `source_fidelity` vengono da lì e il codice tocca anche
  `__main__.py`.

## Decisioni per l'owner
1. **Avvio:** messaggio al coordinatore (consigliato per partire) oppure un pulsante in Cascadeur che lancia Claude
   da solo.
2. **Cosa fa Claude:** pose intermedie modificabili in Cascadeur (consigliato) oppure solo curve (`spacing` e ritardi
   degli arti), senza chiavi nuove.
3. **Blocco:** ogni chiave dell'owner si blocca da sola all'import (consigliato) oppure solo quelle che l'owner approva
   a parte.
4. **Modello:** Opus effort medium (consigliato) oppure Sonnet, più economico.

## Decisione (owner, 2026-11-02 23:28Z): "Ok. Come consigliato."
Avvio con un messaggio al coordinatore; Claude scrive pose intermedie modificabili; ogni chiave dell'owner si blocca da
sola all'import; Opus effort medium. Si implementa dopo il merge di PR #24 (brief in fondo a
`docs/notes/handovers/anim-pipeline.md`).
