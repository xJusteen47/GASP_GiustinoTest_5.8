# Handover: dossier del riferimento nelle review (anim + VFX) — 2026-11-19

Thread cloud, budget ~4 $, costo stimato ~1.5 $. Solo documenti; nessun asset né _ue59 toccati.
Owner: "sì, se migliora i risultati" (cmsg_01CRvUdrWm1dJrM4q54YDBLNMKMgMURaJpb3aJosPXmKVQ).
Branch `claude/project-thread-5gguxu` da `claude/project-thread-fxzhai` (ae7ab3c9, il critico).
Fonte dell'idea: README di github.com/JimGHTB12/video-decompiler, solo letto; nulla installato o eseguito.

Fatto:
- `docs/notes/review/dossier-riferimento.md`: principio (8 punti, non ricette). Prima della build, una volta per
  riferimento video, si smonta il riferimento per iscritto (tempi in frame di gioco da 1, taglie rispetto al corpo,
  forme, curve, luce/materia, opacità e vita, tratti d'identità, cosa NON copiare, cosa non è misurabile).
  Il critico giudica contro il dossier senza il brief né le note di chi costruisce.
- `docs/notes/review/dossier-template.md`: modello compilabile, sezioni facoltative.
- `docs/notes/reference-dossiers/README.md`: dove stanno i dossier (solo testo; griglie in project files).
- `.claude/skills/gstn-anim-reviewer/SKILL.md` §1, §3.6, §3b, §6 e `docs/notes/vfx-theory/gstn-vfx-reviewer.SKILL.md`
  §1, §3.5, §3b, §6: leggono o scrivono il dossier, confrontano prima con esso, lo passano al critico.
- `docs/notes/review/critico-estetico.md` §1: il critico vede il dossier.
- Copie in project files `anim-skills/`, `vfx-theory/`, `handoffs/dossier-riferimento.md`, `handoffs/dossier-template.md`.

Scelte applicate (regola 6): niente dossier senza riferimento video (si giudica coi principi); il dossier non è
legge (scostamenti spiegati da regole dell'owner o dal peso del personaggio si respingono); il critico tiene i
tratti d'identità (regola 13a) ma non il resto del brief; immagini di altri giochi mai nel repo.

Aperto:
- Mai provato: primo uso naturale = redo VFX v3 contro DK Bananza (thread 'Compare VFX dust with DK Bananza').
- `handoffs/dash-ganon-ref/NOTE.md` è già di fatto un dossier: da spostare in `reference-dossiers/` se utile.

## Seguito (stesso thread): setup aggiornato con le lezioni del giorno (owner 14:41-14:44Z)
- `principi-vfx.md`: "Lezioni dell'owner 2026-11-19" in testa; §3.13 e §8.5 i gas crescono sfumando ad alpha 0
  (mai rimpiccioliti opachi); §5.6-bis leggibile *e* credibile; §7 "comportamento reale, stile solo nella palette";
  §12.5-6 confronto sul video/dossier + assistente AI dell'Unreal Editor; §13 checklist 12-14; §15.5 risolta.
- `gstn-vfx-reviewer`: lezioni in §1, impressione "sembra il materiale vero?", confronto sempre col video, domande 8/10/11,
  PASS richiede che regga accanto al riferimento. Non esiste una skill esecutore VFX: chi costruisce usa §13.
- `gstn-anim-reviewer` §3.5 (sei sì non bastano senza il riferimento), §3.7 (domanda all'assistente AI dell'editor);
  `gstn-anim-executor` §3 (`curve.txt`, viste del dossier, non vede critico né verdetto), §5 (assistente AI).
- Bozza per la skill dell'account `gstn-move-animation`: `docs/notes/review/gstn-move-animation-edits-2026-11-19.md`
  (5 coppie old/new verificate sul SKILL.md sincronizzato; la 1 toglie "Never Fable", in conflitto con la regola 3).
- Aperto: lo sheet `stile-sheet-vfx-review.md` dice ancora "mai dissolvenza in trasparenza": va allineato per la categoria B.
