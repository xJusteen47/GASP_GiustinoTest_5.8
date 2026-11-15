# Prompt per Antigravity: review del piano "revisore ed esecutore" (2026-11-15)

Da incollare ad Antigravity sul desktop. Non modificare nessun file di lavoro: questa prima parte è solo domande.
Scrivi le risposte in `docs/notes/reviews/agy-check/esito.md`, numerate come qui, brevi, con un esempio concreto ciascuna.
Se una cosa non la sai o non l'hai mai provata, scrivi "non provato" invece di supporre.

## Il piano in dieci righe
1. Un REVISORE (Claude) guarda griglie e video in gioco, giudica con i principi di `docs/notes/anim-theory/`, con la soglia
   "è alla pari di Smash Bros o di un tripla A simile?" e con riferimenti misurati da altri giochi. Non tocca le pose.
2. Il revisore scrive UNA istruzione alla volta. Tu sei l'ESECUTORE: la applichi, rilanci i controlli numerici, produci la
   griglia prima/dopo sugli stessi frame. Non decidi cosa correggere dopo.
3. Due livelli di istruzione: di principio ("più follow-through sul braccio dopo il colpo, f12-f18") o misurata (cm e gradi).
   Tutte hanno sette campi: chiave, tipo (sposta posa | sposta nel tempo | cambia tangente | aggiungi chiave | togli chiave),
   parte, intento, modifica, principio, verifica.
4. Chiavi sempre sparse: si lavora su chiavi, mai una chiave per frame; "aggiungi chiave" è l'ultima scelta; tetto proposto
   di 8 chiavi aggiunte per mossa.
5. Le chiavi dell'owner sono bloccate: se un'istruzione le tocca, non si applica e si propone all'owner.
6. Le pose di partenza non nascono mai dal solo testo: pose dell'owner, una clip libera esistente, o pose chiave misurate.
7. Stop: stessa istruzione fallita due volte; sei istruzioni per ciclo; dopo tre istruzioni, se il problema è l'idea della posa.
8. Video in gioco solo all'inizio e alla fine; in mezzo si giudica la griglia.
9. L'owner deve poter modificare pose e chiavi da Unreal (Sequencer, Control Rig) e ritrovarle rispettate al passo dopo.
10. Lo scambio avviene per file: un file con l'istruzione corrente, un file con l'esito.

## A. Cosa sai eseguire
1. Quali modifiche a una mossa esistente fai in modo affidabile, e quali no?
2. Di quante pose chiave e di quali informazioni hai bisogno per partire? Cosa ti mancava con le tre pose di jab 1 e jab 2?
3. Quando ricevi "aggiungi più follow-through qui", cosa cambi di preciso: quali controlli, quali chiavi, quali tangenti?
   Aggiungi chiavi nuove? Quante?
4. Quando ti serve un'istruzione in cm e gradi invece di una di principio?
5. Quali principi di `docs/notes/anim-theory/` applichi con sicurezza e quali no? Li leggi a ogni sessione?

## B. Come lavori oggi
6. Dove scrivi le chiavi: direttamente nel Sequencer o nella scheda di `animpipe` (`Tools/AnimPipeline/cards/`)? Con quali comandi?
7. Come leggi le modifiche fatte dall'owner in Unreal? Distingui le sue chiavi dalle tue e riesci a non toccarle?
   Che cosa è `user_tweaked_keys.json` e quando lo scrivi?
8. Quante chiavi usi per mossa? Scrivi mai una chiave per ogni frame? In quali casi?
9. Quali controlli lanci da solo dopo una modifica? Cosa non puoi vedere (immagini, video)?
10. Sai produrre una griglia prima/dopo sugli stessi frame e un PASS/FAIL per una verifica numerica (`animpipe verify`)?
11. Puoi lavorare leggendo una istruzione alla volta da un file e scrivendo l'esito in un file, senza altro testo dall'owner?
12. Cosa c'è di non committato nel tuo worktree?

## C. Critica del piano
13. Quale dei dieci punti del piano NON riusciresti a rispettare, e perché?
14. Dove fallirebbe più spesso questo ciclo? Indica i tre casi peggiori, con una mossa di esempio.
15. I sette campi dell'istruzione: quale ti manca per eseguire senza interpretare, e quale è inutile?
16. Chiavi sparse e tetto di 8 chiavi aggiunte: è realistico per follow-through, overlap e rientro? Che numero proporresti?
17. Modifiche dell'owner da Unreal: qual è il modo più sicuro per rileggerle e non sovrascriverle? Cosa si rompe oggi?
18. Se `animpipe build` ricostruisce la sequenza dalla scheda e tu scrivi nel Sequencer, chi vince? Come eviteresti il conflitto?
19. Quali controlli faresti tu e quali lasceresti al revisore, per non fare lo stesso lavoro due volte?
20. Che cosa aggiungeresti al piano che qui manca?

## D. Prove pratiche (solo dopo le risposte, ognuna su una COPIA variante di jab 1, mai sull'originale)
Per ogni prova annota in `esito.md`: chiavi prima e dopo, chiavi dell'owner toccate (sì/no), esito dei controlli, tempo,
percorso della griglia prima/dopo.
| Prova | Istruzione | Passa se |
|---|---|---|
| 1 Principio | "Aggiungi più follow-through al braccio dopo il colpo." | cambiano solo i frame dopo la tenuta, max 2 chiavi nuove, controlli verdi |
| 2 Misurato | lo stesso difetto con i sette campi e una verifica numerica | la verifica dà PASS, nessun'altra parte cambiata |
| 3 Tempo | "Anticipa lo scatto di un frame." | una chiave si sposta nel tempo, il numero di chiavi non cresce |
| 4 Blocco | un'istruzione che richiederebbe di muovere una chiave dell'owner | rifiuta e propone, senza applicare |
| 5 Base minima | la stessa mossa da tre pose, poi da cinque o sei | si trova il numero di pose sotto cui il risultato non regge |
