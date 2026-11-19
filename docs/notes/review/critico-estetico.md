# Il critico estetico: strato di progetto (GSTN)

Owner 2026-11-19 (scelta 2A). Il principio generico (in inglese, pubblicabile) sta in
`skills-public/shared/aesthetic-critic.md`; le skill che lo applicano: `gstn-anim-reviewer` e `gstn-vfx-reviewer` (§3b).
Qui solo ciò che è del progetto; dove differisce, vince questo file.

- **Modello**: Fable 5.2 high, agente separato (regola 3).
- **Pesi**: `grave` / `medio` / `lieve` (= `major` / `medium` / `minor`). `grave` anche quando tradisce la stella polare
  "Ganondorf dentro Astro Bot" o, per i VFX, "starebbe in DK Bananza?".
- **Asse `vita`** (owner 2026-11-19, ambito: tutte le animazioni): rigida, senza passaggio di peso, senza overlap o
  follow-through, "sembra una posa tenuta" è sempre almeno `medio`, e `grave` se si vede a velocità di gioco. Il critico lo
  controlla per primo, dopo i tratti d'identità. Pavimento: la clip mocap GASP della stessa classe misurata allo stesso modo
  (`Tools/AnimPipeline/contract/rubric.md`, sezione `vita`). Pulita non vuol dire viva.
- **Riferimenti validi** per una voce: regole dell'owner, DK Bananza / Astro Bot, Smash, Ganondorf.
- **Etichette**: animazioni `posa` / `dati montage` / `gioco`; VFX quelle di `gstn-vfx-reviewer` §5.
- **Tratti d'identità**: regola 13a. Cambiarli = decisione dell'owner.
- **Chi arriva all'owner**: solo PASS senza `grave` accolti aperti; `medio`/`lieve` aperti in una riga, in italiano.
- **Restano uguali**: catena di cattura, regole 4, 10, 12 e 13, massimo 3 giri di posa; una passata del critico per giro
  con le stesse griglie (regola 4), non conta come giro; i difetti `gioco` vanno alla coda del codice.
