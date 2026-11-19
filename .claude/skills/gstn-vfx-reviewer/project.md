# gstn-vfx-reviewer: strato di progetto (GSTN)

Metodo generico: `skills-public/vfx-reviewer/SKILL.md` (i § qui sotto si riferiscono a quello). Questo file vince.

## Ruolo e lingua
- Fable 5.2 high (regola 3); solo PASS arriva all'owner (regola 10); max 3 giri, poi decide l'owner.
- All'owner in **italiano**, breve. Handover max 30 righe con il costo. Immagini di altri giochi solo sul desktop o in
  project files, mai nel repo.
- Barra: Astro Bot / DK Bananza per lo stile, Smash per timing e peso. Domande del critico: "starebbe in DK Bananza?" e
  "sembra il materiale vero?". Stella polare di tutte le animazioni: "Ganondorf dentro Astro Bot".

## Cosa leggere (§1)
- Regole del kit: `handoffs/stile-sheet-vfx-review.md`, `vfx-stile-v2-astrobot.md`, `vfx-stile-v3-categorie.md` (la v3
  vince sulle righe in conflitto, `docs/notes/vfx-theory/principi-vfx.md` §15).
- Strato di progetto della teoria: `docs/notes/vfx-theory/principi-vfx.md` (lezioni dell'owner 2026-11-19 in testa:
  leggibile *e* credibile; gas che crescono sfumando ad alpha 0; stile solo nella palette; si giudica sempre sul video di
  riferimento). Teoria generale: `skills-public/knowledge/vfx/vfx-theory.md`, stessa numerazione dei §.
- Riferimenti dell'owner: `handoffs/ref-astrobot-vfx/NOTE.md` (+ clip DK Bananza sul branch `claude/ref-astrobot-vfx`).
- Critico: `docs/notes/review/critico-estetico.md`. Dossier: `docs/notes/review/dossier-riferimento.md`, dossier in
  `docs/notes/reference-dossiers/`, griglie in project files `handoffs/<riferimento>/`.
- Cartella della review: `docs/notes/reviews/<effetto>-r<N>/`. Frame: `skills-public/anim-reviewer/scripts/rv_frames.py`.

## Cattura (§2)
- Livello di prova **L_Test_Hub**: un punto chiaro e uno scuro o saturo (owner 11-18). Sim a 62.5 Hz; frame contati da 1.
- Preset grafico: alto, e basso se il brief lo chiede (il target è il "tostapane"; Niagara tick < 0.5 ms).

## Dubbi tecnici (§3.6)
- Le domande vanno all'**assistente AI dell'Unreal Editor** (owner 11-19, editor aperto). Dump dei parametri Niagara con
  UE 5.9: `docs/notes/review/ue59-strumenti-per-skill.md` (non ancora provato).

## Etichette e decisioni (§5)
- Etichette: forma / timing / colore-valore / materiale / gerarchia / perf / gioco. `gioco` va alla coda del codice e non
  conta come giro (regola 10).
- Forma base, palette di un kit, voto d'importanza = design: decide l'owner. Tutti i kit condividono rifinitura e VFX
  (owner 11-18, `DA_Locomotion_Default`); la coerenza fra kit si controlla solo in una review di famiglia.
- Hitstop: impostazione 2A (segno del colpo a piena velocità durante l'hitstop) approvata 11-19.
- Base di conoscenza (2026-11-19): criteri `skills-public/knowledge/vfx/vfx-criteria.md` (ID `V-*`), teoria
  `skills-public/knowledge/vfx/vfx-theory.md` (stessa numerazione dei §), punti in comune con l'animazione
  `skills-public/knowledge/anim-vfx-links.md`, profilo `docs/knowledge/profile-gstn.md` §3.
