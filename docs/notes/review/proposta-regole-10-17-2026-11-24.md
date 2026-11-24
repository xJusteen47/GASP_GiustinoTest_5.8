# Testo per le regole 10 e 17 (da incollare dall'owner; nessun thread le modifica)
Origine: decisioni 1A e 2A dell'owner del 2026-11-24 01:22 (Roma) sul brief `handoffs/skill-anim-unica-brief-2026-11-24.md`.

## Regola 10 (ANIMAZIONI): sostituire l'ultima frase sulle fermate
Oggi: "Le fermate sono un difetto per default: una tenuta vale solo se voluta, eccezionale e dichiarata con il motivo,
e la review la giudica comunque a video."
Nuovo: "Tenute (ambito: tutte le animazioni). Una tenuta VIVA, in cui bacino o testa continuano a muoversi un poco, è
uno strumento normale e importante: va dichiarata con il motivo e la review la giudica a video. Sono difetti la tenuta
MORTA (corpo fermo come una statua) e lo stop-and-go (una fermata su ogni chiave). I numeri sulle tenute (quante, quanti
frame) sono guide per l'occhio del revisore, non blocchi automatici."

## Regola 17 (QUALITÀ): aggiungere in coda
"Un controllo automatico nuovo nasce come MISURA mostrata al revisore. Diventa bloccante solo quando l'handover porta
la prova di taratura: passa sulle clip approvate e sullo stock GASP e boccia un caso noto come difettoso. Vale anche
per i controlli già scritti e non ancora tarati. I controlli fisici della regola 15b restano sempre bloccanti."

## Nota
Regola 12: la 2A non ammorbidisce nessun controllo tarato. Toglie il potere di blocco solo a ciò che non ha ancora la
prova, perché un blocco non tarato è la fonte dei falsi allarmi che la regola 17 vieta di "risolvere spegnendo".
INV-12 in `Tools/AnimPipeline/contract/invarianti-review.md` va allineato a questo testo dal thread T1.
