# Diagnosi di sistema: turnaround del run start sul w3 (cloud, Opus high, sola lettura, 2026-11-23)
Letti solo: handoffs/stato-turnaround-hit-2026-11-23.md, INDICE.md righe 50-68, costi.csv, e su `claude/runstart-generic-desktop` (230ad65a)
gli handover runstart-turnaround-{brief,desktop}, runstart-v23, turnarounds-{2,3,4-style}, runstart-w3-cheap-skill, gasp-divergenze 21c-21g, runstart_w3_ue.py.
Regola owner nuova (22:18Z, AMBITO: turnaround 90/180 L/R e run start): base = w3 (29); downtime minimo girata -> run start; girata e partenza un solo movimento.
## Causa comune dei giri costosi (~113 $ da costi.csv, 14 righe, quasi tutto sulla sola 180 L)
1. **Il turnaround è una COPIA del rev, non dipende dal run start.** Ogni base nuova (w1 -> w2 -> 22-25 -> w3) lascia i turnaround indietro e li fa ricostruire
   a mano (21c, 26, 27, 28 = 4 ricostruzioni). Il w3 invece è già un additivo scritto da script su w2 (runstart_w3_ue.py, 22 parametri via env, envelope sui contatti f0-f29).
2. **La cucitura girata -> rev è un taglio, non un movimento.** 27/28 = passo di perno (H 4 f) + rev w2 appeso dopo. I FAIL si concentrano lì: v27 r1 yaw fermo v89-v97,
   v28 P6 braccia parcheggiate v95-v101, prima spinta solo a v102, piede R carico a v99. Sono ~6-8 frame (0,1-0,13 s) di tempo morto: proprio quello che l'owner ora vieta.
3. **Difetti di GIOCO giudicati con giri di POSA.** Ingresso al frame 3 con blend 0,1 s (piede R di P5 nascosto, anticipazione f0-f2 mai vista), coda dello yaw del capsule
   (ultimi ~20° mai raggiunti), yaw iniziale della cattura ruotato ~60° (w3 punto 4). P5 è in coda dal 21/11 e mai fatto: 3 giri di posa hanno inseguito un difetto di codice.
4. **Design cambiato a metà e un thread per giro.** composer (21c) -> fix (26) -> perno (27) -> stile Cascadeur (28); ogni giro = build desktop 4-18 $ + review Fable 3-4 $.
   Il "un po' di più/meno" (braccia, gomito, passo) è stato fatto con pose nuove invece che con parametri (regola 15c). 27 ha avuto PASS ma l'owner non l'ha mai visto.
## Opzioni (decisione dell'owner, una sola)
- **A (consigliata) Turnaround = perno + w3 per script, con sovrapposizione regolabile. ~22-28 $.**
  1) Codice sul fisso, Opus medium ~5 $: ingresso della clip (f3 + blend 0,1 s), coda dello yaw del capsule, reset yaw della cattura; prima l'Unreal Assistant (regola 14).
  2) Meccanico, Sonnet medium ~5 $: runstart_w3_ue.py generico (SRC/DST, offset = frame di hand-off, delta ruotato con lo yaw del root) applicato al rev del turnaround;
     composer con parametro OVERLAP (frame in cui il rev parte DENTRO il perno, braccia che pompano già nel perno); gate nuovo "tempo morto" in runstart_pivot_gates.py:
     frame tra anche >= 150° e primo contatto di spinta <= 2, velocità mani mai sotto soglia nella cucitura (Haiku).
  3) Cattura batch 180 L con OVERLAP 0/2/4 in una sessione (~33 s/variante), review cloud una mossa ~4 $, l'owner sceglie il valore guardando.
  4) Dopo l'OK: 180 R, 90 L, 90 R con la stessa ricetta (Sonnet ~6 $) + una review ~4 $. Ogni ritocco futuro del w3 = rilanciare lo script, niente ricostruzioni.
  Resta fermo: 28 non si ritocca (P6 si risolve con la sovrapposizione), 27 resta il ripiego; Variant 21 resta default finché l'owner non approva.
- **B Additivo w3 a runtime (w3 meno w2) nell'ABP, pesato da una curva sul rev, ampiezza da cvar. ~28-35 $.** Dipendenza vera e gratuita per il futuro,
  ma nuovo nodo ABP (divergenza regola 14), allineamento incerto sul rev ri-temporizzato e ruotato dei turnaround; la cucitura va comunque rifatta come in A.
- **C Minimo. ~5 $.** Solo il codice del punto A1; l'owner guarda 27 e 28 così come sono; le altre 3 restano w2. Non rispetta la regola nuova (base w2, cucitura con tempo morto).
Cosa resta fermo con A: niente laptop (A non lo tocca), gli hit generici seguono la scelta "A hit" già data; il fisso fa A1 dopo il fix di rete.
Da confermare con l'owner (regola 13d): braccio destro "davanti" o "gomito di lato" nella girata.
SUBAGENTI: 0
COSTO: ~2 $ | 0 min desktop | 0 token review
