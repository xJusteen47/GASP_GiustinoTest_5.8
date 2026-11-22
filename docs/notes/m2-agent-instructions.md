# Istruzioni per l'agente di M2 (bozza, 2026-10-19, non ancora inviate)

Il progetto è cambiato due volte il 19/10. Rileggi le note di revisione datate 2026-10-19 in combat.md, combat-m1-core-input.md, combat-m2-sim-input.md e il nuovo docs/architecture/input.md.

1. **Abilità di combattimento** (combat.md §11): per M2 cambiano solo questi punti.
   - I comandi senza binding non sono candidati.
   - I lock tag `Input.Lock.Command[.<radice>]` sostituiscono `BlockingTags` (M2 §6b).
   - Nuovo campo `RequestButton` nella sync state e API del contesto di attivazione (M2 §7a).
   - L'evento di buffer `MoveActivated` diventa `Input.BufferEvent.AbilityActivated`.
2. **Plugin GSTNInput separato** (input.md).

Ordine di lavoro (concordato con l'utente il 2026-10-19):

0. Aspetta che l'agente del fork NPP abbia chiuso il suo lavoro: modifiche al fork committate o annullate, harness pulito. S1 e S1b non usano l'harness. S2 invece aspetta anche lo spike ForwardPredict (combat.md §12.7), oppure procede solo nei momenti in cui lo spike non usa l'editor.

1. Se build e harness di M2.2 sono verdi, chiedi all'utente di committare lo stato attuale come checkpoint. Altrimenti mettilo da parte prima di spostare i file.
2. **S1** (input.md §8): crea il plugin `GSTNInput` e il modulo `GSTNInputCore`, sposta con `git mv` il codice generico di M1 e applica le rinomine di input.md §4.
   - Restano in `GSTNCombatCore`: tag dei bottoni e dei comandi combat, `UGSTN_StandardSlots` e i loro test.
   - Aggiungi i valori combat in `Config/DefaultGame.ini`.
   - Criterio di accettazione: i 58 test passano, divisi tra `GSTNInput.Core.*` e `GSTNCombat.Core.*`.
   - Un commit.
3. **S1b** (input.md §4.1 e §8): l'ordine di priorità diventa una lista configurabile di criteri `FGSTN_PriorityCriterion` in `UGSTN_InputSettings::PriorityCriteria`, con default Tier, Specificity, ManualOffset. `FGSTN_CommandPriority` diventa un vettore di punteggi. Con la lista di default tutti i test esistenti restano verdi; aggiungi i nuovi test elencati in S1b. Un commit.
4. **S2**: prosegui M2 secondo la specifica aggiornata, con il codice NP di input in `GSTNInputNP`. Checkpoint proposti:
   - S2a: il modulo `GSTNInputNP`, con dentro `FGSTN_InputCmd`, `FGSTN_InputSimState` e il tracker (aggiorna i redirect in DefaultEngine.ini). Nessun cambio di comportamento; `scenario_combat_spike` deve restare pulito.
   - S2b: simulatore, `IGSTN_InputSimHost`, `UGSTN_CommandBindingSet`, il componente combat come host, sorgenti e context provider Mover. Scenari M2 §8.
   - S2c: lock, comandi senza binding, contesto di attivazione e `scenario_combat_lock`.
   - S2d: `UGSTN_SimEventSubsystem` e `Input.Event.CommandExecuted`.
   - S2e: `UGSTN_InputSimComponent` autonomo, con un test minimo.

Correzione all'harness (segnalata dall'agente del fork il 19/10):
- `scenario_combat_spike` fallisce il confronto quando `report()` viene letto più di ~4 s dopo l'ultimo tasto: `combat history '6461:5'!='?:5'`. Non è una divergenza: non ci sono riconcili.
- Causa: sul client gli eventi più vecchi della finestra serializzata (255 frame) perdono il frame assoluto (`AncientFrame`, stampato come `?`), mentre il server tiene quello vero.
- Correzione: nel confronto `_diff` della history in `gasnpp_test.py`, considera un frame `?` uguale a qualunque frame vecchio dell'altro lato, oppure confronta solo gli eventi degli ultimi 255 frame.

Regole:
- Avvisa l'utente prima di modificare `BP_GASNPP_TestCharacter`: `MoveAction`/`ButtonActions` passano all'asset `UGSTN_InputConfig`.
- Committa solo i tuoi file e solo dopo l'ok dell'utente.
- Non fare mai push.
- Le esecuzioni dell'harness vanno a un subagente Sonnet con effort medio.
