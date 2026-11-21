# Brief: avvio del tech bounce del dropkick (attaccante che atterra), animazione dedicata (2026-11-17)

**Richiesta owner (11-17)**: il tech bounce del dropkick, o almeno il suo inizio, ha bisogno di un'animazione sua: non ha senso che le braccia
siano aperte all'istante. Ambito: SOLO il rimbalzo dell'UTENTE del dropkick all'atterraggio (Defence track `Self Landing Bounce`, 25 cm,
`claude/project-thread-ipt5wf` 10ac5989). Non il tech bounce generico delle vittime, non gli altri knockdown (regola 12).

**Macchina**: fisso (editor + agy). **Modello**: Fable 5.2 high come regista (giudica e decide le pose), Antigravity anima, catture/import/build a
subagenti Haiku/Sonnet (regola 3). **_ue59**: sì, con lock IN-USO.txt. **Budget**: ~18-25 $ (stima: un thread anim simile alla dair v9, meno pose).

## Tratti d'identità (regola 13a: la review li controlla PER PRIMI; cambiarli = decisione owner prima della build)
1. **Gambe unite e dritte** (rif.: posa del dropkick di Antigravity, la base dell'owner; vedi `owner-base-is-the-base`).
2. **Braccia conserte** (rif. Bison / Overlord): restano incrociate al contatto col suolo; si aprono SOLO dopo, come follow-through.
3. Il rimbalzo è **una conseguenza dell'impatto**, non un salto: il corpo arriva rigido, schiaccia, poi rimbalza.

## Cosa deve fare la clip (proposta per il regista, non pose finali)
- f0 (owner 11-17 15:11Z, ambito: SOLO questo tech bounce del dropkick): il primo frame **cambia posa in modo chiaro ma non eccessivo**,
  misurato sul caso d'uso: si deve leggere che l'impatto è arrivato (es. bacino giù e ginocchia che iniziano a cedere), non uno scatto e non una
  posa identica al loop in aria. Gambe unite e braccia conserte restano. La review lo giudica per primo dopo i tratti d'identità.
- Impatto 1-3 f: schiacciamento (bacino giù, ginocchia che cedono insieme, testa per ultima). Braccia ancora incrociate.
- Salita del rimbalzo: le braccia si sciolgono in ritardo (overlap 2-4 f), si aprono in arco come bilanciere, mai di scatto in 1 frame.
- Picco e ricaduta: si raccorda alla posa a terra/giacente attuale del knockdown (o alla prossima get-up quando esiste), stessa posa d'arrivo.
- Altezza e durata dal gioco: hop 25 cm (track), timing reale da misurare nella cattura base. I vincoli di gioco non piegano la posa (13e).

## Passi (regola 10)
1. **Prima di animare, cattura economica della BASE**: dropkick da salto alto e basso, a vuoto e a segno, lato + dietro + camera di gioco.
   Verifica quale clip parte oggi al rimbalzo e da dove (DA_ReactionAnims? clip generica di tech bounce?). **DA_ReactionAnims ha modifiche
   dell'owner: non toccarlo.** Se oggi non esiste un modo di dare alla mossa una clip di rimbalzo propria (override per mossa sulla track o sul
   profilo), prima un piccolo thread di codice (Opus medium) che lo aggiunga come dato, poi l'animazione.
2. Blocking di **agy** (scelta consigliata, applicata 11-17, regola 6): niente pose chiave dell'owner per questa clip. Fable regista decide le pose.
3. Blocking agy con **sheet delle curve** accanto alle pose (13c): timing, easing, overshoot, follow-through delle braccia.
4. Cattura in gioco, mp4 + griglie (base e variante, stessi frame) in `docs/notes/reviews/dropkick-tech-bounce-v1/`; review cloud Fable,
   frame per frame attorno a contatto col suolo e picco. Max 3 giri di posa. Solo PASS all'owner.
5. Handover ≤30 righe in `docs/notes/handovers/`.

## STACCO tra fase attiva e atterraggio (owner 11-17 15:18Z: è il motivo del riferimento Kazuya)
Il dair di Kazuya serve a mostrare lo **stacco netto** tra l'animazione attiva e quella di atterraggio: al contatto col suolo parte una clip
di atterraggio DISTINTA, che si separa a vista dalla posa attiva (non la stessa posa che scivola nel rimbalzo). Per il dropkick: il primo
frame del rimbalzo cambia posa in modo chiaro ma non eccessivo (vedi sopra), gambe unite e braccia conserte restano; la review controlla che
lo stacco si legga in camera di gioco, subito dopo i tratti d'identità.

## Riferimento Kazuya: SOLO animazione (owner 15:18Z "non per gameplay"; la posa NON si copia)
Guardare il suo down air per lo stacco tra posa attiva e clip d'atterraggio (sezione sopra). Nessun numero di gameplay viene da lì.

## Decisioni (11-17)
a) Blocking di agy (consigliata, applicata). b) Braccia conserte fino all'impatto, si sciolgono solo DOPO, durante la salita del rimbalzo
(consigliata, applicata). c) Primo frame: cambio di posa chiaro ma non eccessivo, adatto al caso d'uso (owner 15:11Z). Nessun punto aperto.
d) Gameplay (owner 15:15Z): niente landing lag in più per il dropkick (il tech bounce dedicato è già un lungo periodo vulnerabile);
hitbox d'atterraggio 1-2 f solo opzione nei dati, spenta di default. Lo stomp ha i suoi dati in `stomp-atterraggio.md`.
