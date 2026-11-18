# Brief: partenza, arresto e skid della corsa (2026-11-18, cloud, Fable, solo design)

Ambito: locomozione di TUTTI i kit (profilo condiviso). Frame contati da 1, sim 62.5 Hz (1 frame video a 30 fps = ~2 frame sim).
Riferimenti dell'owner, griglie in `docs/notes/reviews/ref-smash-run/`:
- Smash, Ganondorf: `smash_run_cycle.jpg` (4.0-5.0 s), `smash_run_stop.jpg` (9.6-12.0 s), `smash_skid.jpg` (27.6-32.3 s).
- Toad: `toad_a_closeup.jpg` (0-1.9 s, partenza), `toad_b_far.jpg` (2.4-11.2 s, partenze/arresti/svolte; Toad e' piccolo: si leggono tempi e polvere, non le pose).

Limiti dichiarati: video ripresi col telefono, input non visibili. I tempi di Smash sono di un personaggio pesante: valgono le PROPORZIONI tra le fasi, non i numeri. I frame qui sotto sono valori di partenza, tutti dati nel profilo.

Principio comune (dai due riferimenti): la posa cambia SUBITO e si legge in silhouette; il peso lo raccontano il busto e il bacino, non la durata; cio' che e' morbido (mantello, testa, braccia) arriva dopo. La sim non cambia: tutto questo e' presentazione.

## 0. Posa di corsa (base per tutte e tre)
Da `smash_run_cycle.jpg`: busto inclinato in avanti ~40-45 gradi con la testa che guida, falcata lunga con fase di volo, gamba dietro distesa, braccia piegate e strette, mantello orizzontale, polvere dietro ai piedi. Per noi: il ciclo run GASP + lean additivo del profilo e' gia' questa famiglia; la differenza da chiudere e' la quantita' di inclinazione (oggi piu' dritto). Nessuna clip nuova.

## 1. Partenza (run start) - NON da Smash (niente dash iniziale)
Tratti d'identita' (rif. `toad_a_closeup.jpg`, frame video 24-34):
1. Posa di corsa dal frame 1: nessuna camminata di transizione, il busto e' gia' inclinato.
2. Arti velocissimi che "grattano" il terreno mentre la velocita' sale (Toad: gambe sfocate, linee di velocita').
3. Sbuffo di polvere alla partenza, dietro ai piedi.

Pose chiave e tempi:
| Frame | Cosa |
|---|---|
| 1-2 | Taglio quasi netto da idle alla posa di corsa (blend 2 frame). Lean = lean di corsa + 10-12 gradi. Sbuffo di polvere. |
| 3-N | "Scramble": ciclo di corsa a velocita' di riproduzione ~x1.9, che scende a x1 mentre la velocita' sim sale al bersaglio. N = quando la sim arriva al ~90% (oggi ~13 frame a stick pieno). Piccoli sbuffi a ogni passo. |
| N..N+6 | Il lean extra rientra a quello di corsa con un piccolo rimbalzo (2 gradi oltre, poi a posto). |

Curve: play rate = 1 + K x (1 - velocita'/bersaglio), K ~0.9, lineare sulla velocita' (non sul tempo, cosi' vale a ogni stadio e a stick parziale). Lean extra: stesso fattore, uscita ease-out. Follow-through: testa e braccia in ritardo di 2 frame sul busto al frame 1.
Come farlo: nessuna clip da disegnare. Ciclo run GASP + play rate + lean additivo gia' esistente. Lo stride warping va spento durante lo scramble (altrimenti accorcia il passo e annulla l'effetto): i piedi scivolano APPOSTA, e' il linguaggio di Mario/Toad. A stick poco inclinato (camminata) K scala con l'inclinazione: sotto ~40% niente scramble.
GASP: nessuna riga nuova nel chooser; si evita che il motion matching scelga i suoi "start" realistici (lenti) quando lo scramble e' attivo: una condizione sullo stato di partenza che esclude il database degli start. Valori (K, lean extra, soglia stick, polvere) nel profilo DA_AnimSet_Locomotion_Default.

## 2. Arresto (corsa -> stick neutro)
Tratti d'identita' (rif. `smash_run_stop.jpg`, frame video 20-58):
1. Piede avanti piantato e personaggio che resta rivolto in avanti: nessuna rotazione.
2. Il busto si rialza DOPO i piedi, con le braccia che si aprono basse per equilibrio (frame video 30-36).
3. Il morbido arriva per ultimo: mantello ancora orizzontale quando il corpo e' gia' fermo, poi cade.

Proporzioni misurate su Smash: frenata con busto ancora inclinato 0.2 s, rialzo 0.2 s, dritto a 0.6 s, mantello fermo a ~1.25 s. Noi (stile Astro, piu' leggero), a 62.5 Hz:
| Frame | Cosa |
|---|---|
| 1-3 | Taglio quasi netto alla posa di frenata: piede avanti piantato, ginocchio piegato, bacino giu' di 6-8 cm, busto ancora inclinato come in corsa. Sbuffo di polvere al piede. |
| 4-10 | Frenata: il bacino continua in avanti sopra il piede, il piede dietro raggiunge. Braccia si aprono. |
| 11-16 | Rialzo del busto, supera la verticale di ~4 gradi all'indietro al frame 16. |
| 17-24 | Rientro a idle. Testa e braccia chiudono 3-4 frame dopo il busto. |

Curve: entrata netta (nessun ease-in), rialzo ease-out, overshoot singolo senza oscillazioni. La durata della frenata segue la velocita' sim (dallo sprint 2 i frame 4-10 si allungano, il resto no).
Come farlo: le clip di stop GASP, accorciate nell'attacco (si entra direttamente sul contatto del piede) e con il rialzo ritardato rispetto ai piedi. Se alla review in video non mostrano i tre tratti, clip nuova con agy sulle pose sopra.
GASP: restano nel database degli stop, scelte dal motion matching; cambia solo il tempo di blend in ingresso (quasi zero) nel chooser e il taglio delle clip.

## 3. Skid (corsa -> stick opposto -> ripartenza)
Tratti d'identita' (rif. `smash_skid.jpg`, frame video 48-108):
1. Corpo BASSO con il peso all'indietro rispetto alla direzione di marcia: gamba avanti distesa che frena, bacino giu', busto che si oppone allo slancio.
2. Il corpo e' gia' girato verso la nuova direzione mentre scivola ancora in quella vecchia; polvere al piede che frena.
3. Posa TENUTA finche' dura la scivolata, poi spinta esplosiva: in ~0.25 s e' di nuovo in posa di corsa piena.

Perche' la clip stirata oggi non funziona: lo skid dura diversamente a ogni stadio (frenata sim ~11 frame dalla run, ~16 dallo sprint 1, ~20 dallo sprint 2, piu' entrata 4 e uscita 8: valori di FGSTN_RunSkidTuning, conto su carta). Stirando tutta la clip si rallentano anche l'entrata e la spinta, che devono restare secche. In Smash si allunga solo la posa tenuta.

Una clip sola, tre sezioni con marker:
| Sezione | Frame | Cosa | Tempo |
|---|---|---|---|
| Entrata | 1-4 | Taglio quasi netto: piede piantato, bacino giu', rotazione del corpo gia' al ~50%. Sbuffo di polvere. | fisso, = EnterFrames |
| Tenuta | 5..fine frenata | Posa bassa, peso indietro, rotazione che arriva al 100%. Tracce e polvere continue al piede. | guidata dalla velocita' residua, non dal tempo |
| Spinta | ultimi 8 | Il bacino risale e avanza, il busto si butta nel lean di corsa, parte lo scramble della sezione 1 gia' a meta'. | fisso, = ExitFrames |

Curve: entrata netta; nella tenuta la posa avanza con (1 - velocita'/velocita' d'ingresso), ease-out; spinta ease-in e uscita sul lean di corsa + extra di partenza. Follow-through: braccia e testa girano 3 frame dopo il bacino; cio' che pende resta rivolto alla vecchia direzione fino alla spinta.
Clip: si parte da SkidOwner/SS_180_Turn_Root180 (radice che gira 180 una volta, 26 chiavi, giro 10/50/90% alle chiavi 2/7/24): le chiavi 1-7 fanno da entrata, 7-24 da tenuta, 24-26 da spinta. Da verificare in video se la posa e' abbastanza bassa e con il peso indietro: se no, variante con agy sulle pose sopra (la clip dell'owner non si tocca).
GASP: database dedicato allo skid con questa sola clip, riga nel chooser dei database sopra pivot e stop, attiva quando la sim dice "skid" (stato gia' letto da UGSTN_RunPresentationComponent). Il tempo della clip non lo sceglie il motion matching: lo da' la presentazione dalle tre sezioni. Orientation warping copre la differenza tra direzione dell'attore e direzione della scivolata; l'uscita va nel ciclo di corsa per inertialization. Il layer ABP separato di oggi si toglie quando questo passa la review. Il verso dell'attore deve girare UNA volta sola (rischio doppio giro clip + attore gia' visto nel round 1): lo controlla il thread di codice.

## Decisioni per l'owner
1. Partenza: A) scramble procedurale (ciclo run accelerato + lean, piedi che scivolano apposta), ~4 $ - consigliata. B) clip di partenza disegnata con agy, ~10 $ piu' i giri di review. Con A si puo' aggiungere B dopo.
2. Skid: A) clip dell'owner Root180 divisa in tre sezioni, ~5 $ - consigliata; se la review in video boccia la posa si passa a B. B) clip nuova con agy sulle pose di Smash, ~12 $.
Applicate senza chiedere (regola 6): arresto con le clip GASP accorciate (~3 $); tempi sopra come valori di partenza nel profilo; lo stesso comportamento per tutti i kit.
Stime a occhio dai costi di INDICE.md di thread simili; fermo finche' non decide: solo l'implementazione di partenza e skid.

## Ordine proposto
Thread codice cloud (Opus): sezioni dello skid + scramble + valori nel profilo. Poi fisso: build, cattura base e variante, review cloud contro i tratti d'identita' qui sopra (regola 10).
