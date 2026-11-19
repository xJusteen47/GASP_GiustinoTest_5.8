# Handover: dossier del riferimento + setup review aggiornato (anim + VFX) — 2026-11-19

Thread cloud, budget ~4 $, costo stimato ~4 $. Solo documenti; nessun asset né _ue59 toccati.
Branch `claude/project-thread-5gguxu` da `claude/project-thread-fxzhai` (ae7ab3c9, il critico). Owner: "sì, se migliora
i risultati" + lezioni sul fumo e "aggiorniamo i setup" (14:34-14:45Z). video-decompiler: solo README letto, nulla eseguito.

Fatto:
- Dossier: `docs/notes/review/dossier-riferimento.md` (principio), `dossier-template.md`, `docs/notes/reference-dossiers/`.
  Con un video di riferimento lo si smonta per iscritto prima della build; il critico giudica contro il dossier senza brief.
- `principi-vfx.md`: lezioni dell'owner in testa; gas crescono sfumando ad alpha 0 (§3.13, §8.5); leggibile *e* credibile
  (§5.6-bis); §7 comportamento reale, stile solo nella palette; §12.5-6 confronto sul video + assistente AI dell'editor;
  checklist §13 punti 12-14; §15.5 risolta.
- `gstn-vfx-reviewer`: lezioni, "sembra il materiale vero?", confronto sempre col video, domande 8/10/11, PASS aggiornato.
- `gstn-anim-reviewer` §1/§3.5-3.7/§3b e `gstn-anim-executor` §3/§5: dossier, sei sì non bastano senza riferimento,
  `curve.txt` per le curve GASP, assistente AI dell'Unreal Editor per i dubbi sul motore.
- `critico-estetico.md` §1: il critico vede il dossier.
- Bozza edits per la skill dell'account `gstn-move-animation`: `docs/notes/review/gstn-move-animation-edits-2026-11-19.md`
  (5 coppie old/new verificate; la 1 toglie "Never Fable", in conflitto con la regola 3).
- `docs/notes/review/ue59-strumenti-per-skill.md`: novità UE 5.9 preview 2 utili alle skill; nessuna provata.
- `docs/notes/review/skills-portabilita.md`: piano per pubblicarle (nucleo generico + `project.md` per skill).
- Copie in project files `anim-skills/`, `vfx-theory/`, `handoffs/`.

Scelte applicate (regola 6): niente dossier senza video di riferimento; il dossier non è legge; il critico tiene i
tratti d'identità ma non il resto del brief. Coordinatore: sheet `handoffs/stile-sheet-vfx-review.md` allineato (gas
traslucidi che sfumano ad alpha 0; "mai Translucent / mai dissolvenza" solo per energia e solidi; righe A, D2/6/10/12).
Non esiste una skill esecutore VFX: chi costruisce usa `principi-vfx.md` §13.

Aperto:
- Mai provato in una review vera: primo uso = rifacimento VFX contro DK Bananza.
- Split generico/progetto in un thread nuovo (~3-4 $); pubblicazione su GitHub solo con OK dell'owner (repo, licenza).
- Novità UE 5.9: provarle sul fisso prima di usarle nelle skill.
