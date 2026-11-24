# Principi da considerare nella review finale (livello 2, INV-13)

Owner 2026-11-24 (ambito: review di ogni animazione): i principi di animazione e le note di Sakurai della skill OG vanno
controllati DAVVERO in review, con tutti i documenti di ricerca (corpo, picchiaduro, feel). Owner, stessa sera: "sono
principi, il reviewer dovrebbe considerarli, non applicarli come un automa".

Quindi questa NON e' una lista di pass/fail. Per ogni voce il revisore (e il critico cieco) scrive come l'ha considerata
per QUESTA mossa, col metodo in quattro passi di `docs/notes/anim-theory/principi-animazione-corporea.md` §13:
1. **intento**: cosa deve comunicare la mossa su questo punto (dalle chiavi, dal brief, dai tratti d'identita');
2. **previsione**: cosa predice il principio per questa mossa;
3. **osservato**: cosa mostrano i frame (frame `f`/`v`/`a` e vista), a video prima che nei numeri (INV-02);
4. **scostamento**: dove osservato e previsione divergono e PERCHE'; se lo scostamento e' voluto (stile, identita',
   regola dell'owner) si scrive il motivo e resta accettato; se non lo e', e' un difetto.

Una voce che non riguarda la mossa (es. atterraggio su una mossa a terra) si chiude con `non pertinente: <motivo>`.
I numeri dei documenti sono ordini di grandezza, mai soglie: le misure oggettive restano ai controlli deterministici
(livello 1, `animpipe check`, metriche di vita INV-11), che qui si citano come prova, non come verdetto.

Il pacchetto del livello 2 (`final_review_pack.py`) scrive `final/principi.md` gia' con tutte le voci da riempire;
`Tools/AnimPipeline/gstn_anim_mcp/principi_review.py check <file>` dice se ogni voce e' stata considerata e documentata
(non se la mossa "rientra"). Senza `PRINCIPI: OK` non c'e' PASS. Test: `tests/test_principi_review.py`.
Questa tabella e' la fonte degli identificativi; li cambia solo l'owner. Indice di tutti i documenti di ricerca, con
cosa contengono e chi li usa: `contract/indice-ricerca.md`.

Riferimenti: percorso nel repo; `pf:` = cartella condivisa del progetto (project files); `OG` =
`Tools/AnimPipeline/contract/archive/gstn-move-animation_2026-11-15.md` (sezione); `KB P-*` = knowledge base generale
(`skills-public/knowledge/animation/`, solo sul branch `claude/project-thread-m6s9y2`, non in main); `m6s9y2:` = file
solo su quel branch (note dei corsi, uso interno: `git show origin/claude/project-thread-m6s9y2:<percorso>`).

| ID | Principio | Domande da considerare (guida, non soglie) | Documenti |
|---|---|---|---|
| PR-01 | Intento e fasi leggibili | Cosa comunica la mossa (peso, velocita', chi la fa, dove va l'energia)? Standby, carica, colpo, follow-through, recovery si leggono a colpo d'occhio? | `docs/notes/anim-theory/principi-animazione-corporea.md` §13; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §1; `pf:moveset-template/ricerca.md` (Sakurai, 4 fasi); OG "Posing fundamentals" 11 |
| PR-02 | Anticipazione | C'e' una preparazione in verso opposto, proporzionata alla forza e allo startup disponibile, leggibile da chi difende? Il primo frame e' gia' un cambiamento visibile? | `docs/notes/anim-theory/principi-animazione-corporea.md` §5; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §10 Startup; `pf:handoffs/archivio/2026-11/vfx-clarity-ricerca.md` §1.4; OG "Style principles" 1; `pf:handoffs/archivio/2026-11/rivals2-animazione.md` (frame 1); `m6s9y2:docs/knowledge/study/stylized-attacks-part1.md` L03; KB P-ANTIC-01..05 |
| PR-03 | Timing, spacing e snap d'impatto | Contrasto tra lento e veloce? La parte che colpisce accelera e copre la maggior parte del percorso negli ultimi 1-2 frame (Sakurai)? Tratti a velocita' costante voluti? | `docs/notes/anim-theory/principi-animazione-corporea.md` §2; OG "How the owner judges" 2; OG "Style principles" 1; `m6s9y2:docs/knowledge/study/stylized-attacks-part2.md` L09-10; KB P-TIME, P-SPACE-03 |
| PR-04 | Peso, equilibrio e catena cinetica | Il peso carica prima di spingere e assorbe dopo? Il baricentro e' sopra l'appoggio quando tiene? La forza va dal suolo (o dal bacino in aria) verso l'estremita'? Contrappeso e contrapposto? | `docs/notes/anim-theory/principi-animazione-corporea.md` §4; OG "Posing fundamentals" 1-5 e 10; OG "How the owner judges" 1; KB P-WEIGHT |
| PR-05 | Posa d'impatto | Il frame che l'hitstop congela e' la posa piu' leggibile e curata della mossa, non un passaggio di overshoot o overlap? Si vede chi colpisce, con cosa, verso dove? | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §2.2 e §10 Impatto; KB P-GAME-07 |
| PR-06 | Hitstop (Sakurai) | La durata e' coerente con la forza? Durante la pausa: vittima che trema piu' dell'attaccante, a terra solo di lato, blend breve nella posa di danno, attaccante che avanza appena? Se manca un pezzo di presentazione e' `gioco`, non posa. | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §2.1; `pf:handoffs/archivio/2026-11/hitstop-sakurai-1.md` (8 tecniche); `pf:handoffs/archivio/2026-11/vfx-clarity-ricerca.md` §1.1 |
| PR-07 | Dopo l'hitstop: liberazione | Alla ripartenza il movimento e' subito ampio e poi decelera? Ogni oscillazione dopo l'impatto ha una causa visibile? | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §2.3, §8, §10 Dopo l'hitstop |
| PR-08 | Tenuta con vita, non blocco di cemento | Le tenute (contatto, carica) sono scelte e vive: rinculo piccolo che decade, respiro, nessun corpo fermo come una statua? Sui colpi forti l'arto resta esteso qualche frame (Sakurai)? Ogni tenuta ha il suo motivo (INV-12)? | `docs/notes/anim-theory/principi-animazione-corporea.md` §8; OG "How the owner judges" 3; OG "Style principles" 2; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §5; KB P-HOLD-01..03, P-LIFE-01..07 |
| PR-09 | Follow-through, overlap e secondario | Le parti partono e si fermano in sequenza (spalla, gomito, polso, mano a 1-2 frame)? Il secondario ha una causa, decade e finisce con la mossa? Nei picchiaduro: poco, corto, deciso. | `docs/notes/anim-theory/principi-animazione-corporea.md` §7; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §8; OG "How the owner judges" 4; OG "Style principles" 9; KB P-OVERLAP |
| PR-10 | Archi e traiettorie | Mani, piedi e testa viaggiano su archi lisci? Ogni cambio di direzione ha un motivo? Niente tremolio su archi e rotazioni. | `docs/notes/anim-theory/principi-animazione-corporea.md` §9; OG "How the owner judges" 5; KB P-ARC |
| PR-11 | Overshoot e settle | Gli overshoot sono proporzionali alla frenata, piccoli e non galleggianti? Il settle dura quanto la parte e' pesante? | `docs/notes/anim-theory/principi-animazione-corporea.md` §6; OG "Style principles" 10; KB P-SETTLE |
| PR-12 | Recovery e FAF | Il recovery parte dall'arto (muscolare, non "bambola spinta") e mostra un costo proporzionato alla forza? Al FAF il corpo e' pronto, in guardia compatta, pugni chiusi? | OG "Style principles" 4 e 8; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §10 Recovery e FAF; KB P-SETTLE-02, P-SETTLE-04 |
| PR-13 | Silhouette e linea d'azione, da dietro e di lato | Dalla camera di gioco (dietro) e di lato l'arto che colpisce esce dalla sagoma, il gomito dietro resta dentro, il pugno non e' coperto? Linea d'azione chiara, che si inverte tra carica e colpo e va col knockback? | `docs/notes/anim-theory/principi-animazione-corporea.md` §11; `pf:anim-theory/silhouette-multidirezionali.md`; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §4; OG "Style principles" 5; KB P-POSE |
| PR-14 | Esagerazione e stile | Le pose sono spinte oltre il realistico dove serve (Sakurai)? La mossa sta nella stella polare "Ganondorf dentro Astro Bot"? Lo stile a pose tenute si usa solo se l'owner l'ha scelto. | OG "Style principles" 3; `pf:handoffs/archivio/2026-11/vfx-clarity-ricerca.md` §1.5; `docs/notes/review/critico-estetico.md`; `docs/notes/anim-theory/principi-animazione-corporea.md` §10; `pf:handoffs/archivio/2026-11/stile-astrobot-nintendo.md` §5 |
| PR-15 | Articolazioni, simmetrie e piedi | Ossa naturali (polsi in linea, gomiti e ginocchia nel verso giusto, mani che ruotano con l'arto) salvo esagerazione nel verso della forza? Niente twinning? Piedi piantati senza scivolare? | OG "How the owner judges" 7; OG "Posing fundamentals" 6-8; `docs/notes/anim-theory/principi-animazione-corporea.md` §12; KB P-CONTACT, P-LIFE-06 |
| PR-16 | Colpito e a vuoto coerenti | La versione a vuoto e' la stessa mossa con l'energia non ceduta (piu' follow-through), coerente con quella che colpisce? | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §10 Dopo l'hitstop |
| PR-17 | Passaggio all'idle e alla mossa successiva | La mossa arriva alla guardia o all'idle con un settle, senza scatti al blend e senza posa congelata? Il passaggio alla mossa successiva e' pulito? | `pf:handoffs/archivio/2026-11/ritorno-idle-ricerca.md`; `docs/notes/anim-theory/animazione-picchiaduro-action.md` §10 Recovery e FAF; KB P-GAME-06 |
| PR-18 | Aerei e atterraggio | In aria il baricentro segue una traiettoria credibile? L'atterraggio assorbe e dura quanto il landing lag, con il taglio deciso dall'owner? | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §7; `pf:handoffs/archivio/2026-11/landing-hard-cut-regola.md`; `pf:handoffs/archivio/2026-11/stomp-atterraggio.md`; KB P-ARC-04 |
| PR-19 | Reazione della vittima | Se la cattura mostra il bersaglio: la reazione va nella direzione del colpo, la parte colpita guida, il primo frame mostra gia' il colpo, la forza separa i livelli di reazione? | `docs/notes/anim-theory/animazione-picchiaduro-action.md` §6; `pf:handoffs/archivio/2026-11/reazioni-ricerca.md`; `pf:handoffs/archivio/2026-11/rivals2-animazione.md` (hitstun) |
| PR-20 | Root motion e capsula | Il root si muove col corpo (dentro circa +-20 % del percorso della capsula), senza scivolare o anticipare il bacino? | `Tools/AnimPipeline/contract/rubric.md`; `.claude/skills/gstn-anim-reviewer/project.md` "Audit 2026-11-20"; KB P-GAME-02 |

## Formato di ogni voce in `final/principi.md`

```
### PR-03 Timing, spacing e snap d'impatto
- intento: <cosa deve comunicare la mossa su questo punto>
- previsione: <cosa predice il principio per questa mossa>
- osservato: <frame e vista: f12-15 az180 ...>
- scostamento: nessuno | <dove e perche'>
- voluto: si, <motivo> | no | -
- giudizio: coerente | difetto grave|medio|lieve posa|dati montage|gioco | non pertinente: <motivo>
```
`osservato` cita almeno un frame e una vista (salvo `non pertinente`). Uno scostamento voluto ha il motivo scritto e resta
`coerente`; uno non voluto e' un `difetto` con peso ed etichetta. Il critico cieco riempie lo stesso file in una copia sua.
