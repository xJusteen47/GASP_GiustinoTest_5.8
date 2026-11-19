# Handover: teoria e principi VFX (2026-11-19)
Thread cloud, Opus medium, solo ricerca e documenti, nessuna modifica al gioco. Richiesta owner 02:05Z
(cmsg_01CRvUdrWm1dJrM4q54YDBLNCY9bE6qgMhjbRePFkZtCLq) + peer review Gemini (02:06Z, cmsg_01CRvUdrWm1dJrM4q54YDBLNXZgRQtUBHH1pZqMmf9rDfs).
Branch `claude/project-thread-mn2nvi` (da feature/gas-npp-5.8), solo file in docs/notes/.

## File
- `docs/notes/vfx-theory/principi-vfx.md` (project files `vfx-theory/`): teoria generale, 17 sezioni: informazione,
  forma, timing in tre fasi, strati, leggibilità a camera, valore/colore, stile Astro Bot/DK Bananza, fumo e polvere,
  hitstop e VFX, flipbook/sprite/mesh/ribbon/luce, prestazioni tostapane, processo, checklist costruttore, contraddizioni, fonti.
- `docs/notes/vfx-theory/gstn-vfx-reviewer.SKILL.md`: checklist per le review Fable (catena di cattura, 12 domande
  sì/no giudicabili dal video + 2 controlli strumentali, etichette di FAIL, max 3 scarti). File, non skill installata.
- v2 dopo la peer review Gemini (esito su claude/locomotion-finish-1 e04141c7): accettato/rifiutato in §16-bis del doc.
- `handoffs/agy-vfx-theory-peer-review-prompt.md`: prompt autonomo per Gemini/agy sul desktop (solo lettura).

## Scelte applicate (regola 6)
- Stessa regola dei doc di animazione: solo teoria generale, numeri indicativi, nessuna correzione di effetti esistenti.
- La v3 (tre categorie) è considerata la regola vigente dove lo sheet v1/v2 la contraddice.

## Da confermare (regola 13d, per il coordinatore)
1. Sheet D2/D12 ("nessun fumo realistico", traslucido = generico) contro v3 categoria B: proposta, valgono solo per energia e solidi.
2. Sheet A "mai Translucent / mai dissolvenza": proposta, eccezione per i gas (v3).
3. Sheet B "colpo = lampo + toro neon" contro la nota owner "luce, non anelli": proposta, segue v3.
4. Hitstop VFX: A) energia (lampo/stella) a velocità piena e materia a 0,15 (consigliata, seconda impostazione); B) tenere 0,15 per tutto (decisione v2 2A). Si sceglie guardando le due versioni.
5. Installare la checklist come skill `gstn-vfx-reviewer` (propose_skills) dopo la peer review Gemini.

## Prove (regola 11)
Fonti lette via WebFetch/WebSearch in questo thread, elencate in §17 del doc. Nessun video guardato (limiti in §16).
Clip DK Bananza: solo NOTE.md e un fotogramma (dk_1.jpg) visto una volta.

## Costo stimato
~8 $ (Opus medium, ~25 letture web + scrittura + integrazione peer review).
