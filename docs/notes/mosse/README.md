# Schede mosse e blocco "buona" (regola 13)

File: `_modello.md` (modello), `<kit>-<mossa>.md` (una per mossa). Chi anima legge la scheda prima di tutto; chi giudica la usa come metro.

## Due livelli di blocco
| Livello | Chi lo dà | Tag git | Dove si scrive | Cosa vieta |
|---|---|---|---|---|
| **approved** | l'owner, dopo aver visto il video | `approved/<mossa>-<versione>` | `docs/notes/anim-approvate.md` + scheda §5 | sovrascrivere la versione: si lavora su una variante |
| **good** (buona) | il revisore (PASS parziale) o l'owner ("questa va bene, ma serve un altro giro") | `good/<mossa>-<n>` | scheda §5, una riga | peggiorare senza tornare indietro: la prossima iterazione parte da qui |

Risposta all'owner: **sì**. Se un'iterazione è buona ma ne serve un'altra, la si blocca come `good` e si continua su una VARIANTE.

## Ciclo
1. **Blocca.** Chi ha il permesso di push (il thread sul fisso) crea il tag sul commit che contiene la card e i video di quel giro:
   `git tag -a good/<mossa>-<n> <commit> -m "<cosa è buono>"` poi `git push origin good/<mossa>-<n>`. Il cloud NON crea tag (GitHub risponde 403):
   scrive la riga nella scheda con `Tag: DA CREARE` e il comando pronto; il thread sul fisso lo esegue e cambia in `creato`.
   Se il tag non c'è, vale il commit scritto nella riga (è sufficiente per tornare indietro).
2. **Variante.** La card nuova si chiama `<mossa>_v<m>.json` (mai sovrascrivere la card buona), si parte da una copia della `good` più recente.
   `animpipe snapshot <mossa>` prima di ogni modifica (già previsto dall'esecutore). Le pose che l'owner ha già accettato si bloccano per frame
   e parte con `python -m animpipe approve <card> --frames=.. --parts=.. --why=..` (guardia di `locks.py`).
3. **Confronto.** Il revisore guarda variante e ultima `good` sugli stessi frame (come già fa con la base) e risponde: meglio, uguale, peggio.
4. **Se peggio** (anche solo "due peggio di fila" secondo il contratto): si torna all'ultima buona senza cancellare nulla:
   `git checkout good/<mossa>-<n> -- <path della card>` (nuovo commit sul branch di lavoro; i file buoni restano nella storia e nel tag).
   Le varianti scartate restano su disco, non referenziate.
5. **Se meglio e ancora imperfetta**: nuova riga `good` (n+1). **Se l'owner approva**: tag `approved/...` e riga in `anim-approvate.md`.
6. Limite: max 3 giri di posa per mossa (regola 10), poi decide l'owner. I giri `gioco` non contano.

## Cosa NON blocca
Un `good` non congela i tratti d'identità dell'owner più di quanto già facciano; non sostituisce `approved`. Cambiare i frame attivi/hitbox resta dell'owner.
