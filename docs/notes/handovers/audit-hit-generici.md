# Handover AUDIT-HIT-GENERICI (cloud, sola lettura, 2026-11-23, Opus medium)
Branch `claude/project-thread-rn81kh` (da main). Scelta owner "A per gli hit generici". Nessuna modifica, nessuna cattura, nessun asset.
Report completo: `docs/notes/audit-hit-generici-2026-11-23.md` (copia in project files `handoffs/`).
## Esito
- 8 clip (HitStand, Tumble, Splat, GroundHit, LieUp, Roll, RollBack, GetUp): nessuna approvata/bloccata, nessuna con tratti d'identità,
  sheet delle curve o controlli 15b. Review vecchie: 5 PASS, Splat PASS con riserva, sequenza knockdown PASS dopo fix di codice.
- I difetti noti sono quasi tutti gioco/dati, non posa:
  - piedi che scivolano ~12 cm + bacino giù 4-5 cm = root delle clip a ~16 cm dall'origine (night-abp, misurato): dati o lock ABP;
  - accovacciamento dopo HitStand/Tumble: in parte la stessa causa, probabile seconda causa allo stato del frame d'uscita (da provare);
  - GroundHit un frame a ~90 cm: montage/gioco; possibile conflitto col rimbalzo di gioco di ~18 cm (night-knockdown);
  - chiavi di scala non unitarie (m1, 11-17): pipeline corretta, ma non risulta che le clip in gioco siano state ricotte.
- Set HitArms (ArmsV3, scelta 1A del 18/11) mai applicato in gioco: `hitarms_swap.py` non eseguito, hit-arms-1 non in integ.
- Posa: solo GetUp primi 3 frame, bacino di Splat sul muro, facoltative tuck Roll e diagonale RollBack; 4 additivi mai visti.
## Piano (dettaglio e costi nel report)
Fase 0 dati headless ~2-3 $ -> Fase 1 un thread di codice ~8-12 $ + una cattura ~4 $ -> 4-5 review cloud 12-20 $ -> Fase 2 posa solo
per i FAIL "posa" rimasti, 8-25 $ a clip. Fase 0+1 con review: ~25-40 $.
## Scelta applicata (regola 6)
Root: consigliata la correzione dati nelle 8 clip (non tocca l'ABP condiviso). Resta da confermare all'owner con le altre 2 voci.
## Da confermare (13d)
1. Root: dati (consigliata) o ABP. 2. Rimbalzo a terra: gioco ~18 cm o clip. 3. Giro verso l'attaccante sui colpi forti (aperta dal 16/11).
## Prove
Ogni voce cita il file di review/handover da cui viene (report, riga "Fonti"); letti con `git show <ref>:<path>`, ref nel report.
SUBAGENTI: 0
COSTO: ~2 $ | 0 min desktop | 0 token review
