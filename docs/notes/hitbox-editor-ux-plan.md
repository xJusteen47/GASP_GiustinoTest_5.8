# Editor hitbox più comodo: piano (cloud, 2026-11-15)

Base letta: `claude/integ-cp3` 069287f7, `Plugins/GSTNCombatEditor/README.md` (E4-E13 + outliner), `Tools/GAS/setup_overlord_jab_kit.py`.
Solo piano, nessun codice. Uso principale: l'owner che piazza le hitbox dei jab.

## Cosa c'è già (molto)
Timeline a frame con finestre e chiavi trascinabili; viewport 3D con la posa al frame; gizmo W/E/R sulla hitbox con auto-key, Shift = sposta
tutto il percorso, Mirror; tab Hitboxes (albero gruppi/hitbox, click = seleziona + playhead); hit data con quadrante dell'angolo e anteprima
knockback; bake dei socket (Ctrl+B) con stato "stale"; validatori. Qualsiasi scheletro (anche nemici non umani).

## Attriti (dedotti dal codice: nessuno l'ha ancora usato da designer)
1. **Le modifiche a mano si perdono.** `setup_overlord_jab_kit.py` (riga ~179) riscrive tutto `data` di TL_Overlord_Jab1/2: rilanciarlo
   cancella le hitbox piazzate dall'owner. Vale anche per dair/dropkick. Rischio più grave.
2. **Non si vede se arriva al bersaglio.** Nel viewport non c'è un manichino vittima né le sue hurtbox: la portata si giudica solo in gioco.
3. **Non si vede l'arco intero.** Si vede la hitbox di UN frame; la scia sui frame attivi ("copre gran parte dell'arco?") non è costruita.
4. **Seguire la mano costa passi.** I jab usano una sfera in spazio root con 2 chiavi; farla seguire la mano chiede spazio BakedSocket,
   nome del socket nel Details, poi Ctrl+B; bake vecchio = errore; "bake al salvataggio" è spento.
5. **Finestra attiva separata dalla posa.** Inizio/fine si trascinano sulla timeline; manca "inizia/finisci qui" (al playhead) mentre si scorre.
6. **Frame diversi.** La timeline conta frame di simulazione (62,5 Hz); l'owner ragiona in frame dell'animazione (es. "hitbox dal 15").
7. **Forma, socket e dati solo nel Details**, senza preset né "copia hitbox da un'altra mossa".
8. **Mai provato in una finestra vera**: outliner e gizmo sono passati solo dai test headless.

## Piano a checkpoint
| CP | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| H0 | Prova guidata su TL_Overlord_Jab1 via MCP: apri, scorri, seleziona, gizmo, bake, salva (senza salvare mappe). Griglia di screenshot + lista di ciò che si rompe | fisso, Sonnet medium | 4-6 $ |
| H1 | Protezione: gli script setup non toccano le tracce Hitbox di un timeline esistente (solo con `force=True`); bake al salvataggio acceso di default | cloud, Opus medium | 6-8 $ |
| H2 | Vedere la portata: manichino vittima a distanza regolabile con le sue hurtbox, contatto evidenziato per frame; scia della hitbox su tutta la finestra; righello con frame anim + sim | cloud, Opus high | 12-15 $ |
| H3 | Piazzare in fretta: "Attacca all'osso" (click sull'osso nel viewport → socket + bake automatico); `[` `]` = inizio/fine attiva al playhead; copia hitbox da un'altra mossa | cloud, Opus high | 10-12 $ |
| H4 | Build _ue59, test headless, video del flusso completo; righe nuove nella guida designer | fisso, Haiku/Sonnet | 5-7 $ |

Totale 37-48 $. H0 e H1 non dipendono dalle risposte; H2/H3 sì.

## Scelte applicate (regola 6)
Frame anim e sim mostrati entrambi (come impostazione); la hitbox può restare su percorso o seguire l'osso, come oggi (entrambe esistono).

## Domande per l'owner (risposta tipo "1A 2A 3A")
1. **Prima di costruire, provi tu 10 minuti l'editor su TL_Overlord_Jab1?**
   A (consigliata) sì: parte solo H0+H1 (~12 $), H2/H3 aspettano cosa ti blocca davvero. B no, parto con H0-H3 (~40 $). Fermo con A: H2/H3.
2. **Vittima nell'anteprima:** A (consigliata) manichino in piedi con hurtbox, distanza regolabile (in H2). B il manichino di L_Test_Hub (+3 $,
   dipende dalla mappa). C nessuno. Fermo con C: la portata si vede solo in gioco.
3. **Preset:** A (consigliata) "copia hitbox da un'altra mossa" (in H3, ~0 $ extra). B anche libreria di preset con nome (+5 $). C niente.
