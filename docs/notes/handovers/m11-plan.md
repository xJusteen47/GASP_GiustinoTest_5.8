# Handover: piano a fette M11 "Kit in gioco" (portatile, solo doc, 2026-11-11)

Branch `claude/laptop-m11-plan` da `claude/desktop-m10-7` (`f9d5e0a3`). Nessun codice, nessuna build.

**Fatto**
- `docs/architecture/plans/m11-kit-piano-a-fette.md`: cosa c'è già (TH-1b, GSTNLink F12, GSTNHeld M10, sweet/sour M4, `ReachAxis`,
  `bPrecisionAim`), 4 fette essenziali prima del playtest (K1 scambio kit col tasto interagisci + audit G0, K2 lanci del kit stile Smash
  e ramo sollevabile/non sollevabile, K3 tre kit di prova diversi, K4 controllo harness), 10 fette dopo il playtest (L1-L10 = G1-G8 della
  card, G4 diviso in due, più mosse a distanza mirate), stime, risposta su giovedì/venerdì, 7 decisioni dell'owner.
- Riga nell'indice `plans/README.md`; voce decisioni nella copia in repo della lista cose a mano e nell'artifact.

**Scelte fatte senza owner (regola 6)**
- Nemici sollevati come oggetti (B posa, RT tap/hold) restano in M10 G9, non in M11; l'RT tenuto con reticolo è il resto di M10 G7
  (AimPolicy camera), segnalato da aggiungere alla coda M10 prima del playtest.
- K1 assorbe il gate G0 (audit di TH-1b) per risparmiare una fetta; K1 e K2 su portatile headless per non intasare il fisso.

**Stime**: essenziale $19-30 (4 fette), dopo playtest $52-87 (10 fette), M11 totale $71-117. Essenziale pronto probabile giovedì
12 sera, sicuro venerdì 13, se G9 non sfora e le decisioni arrivano entro mercoledì sera. M11 intero: no entro venerdì.

**Decisioni**: owner ha approvato tutte e 7 (2026-11-11 07:30Z); cambio kit = interazione = grab (B) tenuto.

**Prossimo**: K1 in un thread nuovo sul portatile (Opus medium) appena la
fetta 8 di M10 è su feature. Brief di K1 = §1 riga K1 del piano + `th-1b-kit-pickup.md`.

**Conflitti**: K2 tocca GSTNLink/GSTNHeld come M10 G9 (mai insieme); L1, L4, L5 toccano il loadout di GSTNCombat.

Costo: ~60k token di contesto, Opus, stimato ~$2-3.
