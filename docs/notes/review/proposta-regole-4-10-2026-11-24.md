# Proposta di testo per le regole 4 e 10 (da incollare dall'owner; nessun thread le modifica)
Origine: consulto review finale 2026-11-24 (scelta SC) e regola dell'owner del 24/11. Fonte degli invarianti:
`Tools/AnimPipeline/contract/invarianti-review.md` (INV-01..INV-12).

## Regola 4 (OUTPUT): sostituire la frase sulle immagini
Oggi: "Immagini: una griglia per vista, guardata una volta; review video max 8 immagini."
Proposta: "Immagini: i tetti (una griglia per vista guardata una volta, max 8 immagini) valgono SOLO per il livello 1
(controlli e anteprime che scartano un giro). La review finale (livello 2) e' esclusa da ogni ottimizzazione di costo:
il suo tetto e' 'nessun frame scoperto' e il materiale lo prepara lo strumento `anim_review_pack` a 0 token. Un
subagente di review riceve pacchetto, criteri e brief (non handover, codice o log), e anche i video."

## Regola 10 (ANIMAZIONI): aggiungere in coda
"Livello 2 non negoziabile (INVARIANTI della skill): identita' per prima; vita prima dei numeri sui video reale e
slow4x; slowmo di tutta la mossa nelle 3 viste (az90, az180, game); schede velocita' con fermate e scie per ogni clip;
pose chiave grandi; confronto con la base anche sulle parti ereditate; critico cieco. I controlli automatici possono
scartare un giro, mai promuoverlo. Un controllo che da' falsi allarmi si corregge, non si ammorbidisce: un blocco che
fallisce anche sulla base approvata blocca, salvo esenzione scritta dall'owner in anim-approvate.md. Le fermate sono un
difetto per default: una tenuta vale solo se voluta, eccezionale e dichiarata con il motivo, e la review la giudica
comunque a video."
