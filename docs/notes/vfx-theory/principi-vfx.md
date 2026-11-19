# Principi dei VFX di gioco: teoria generale e checklist

La teoria generale (in inglese, pubblicabile) sta in `skills-public/vfx-reviewer/vfx-principles.md` con la stessa numerazione; questo file tiene solo ciò che è del progetto.

Quarto documento di teoria, gemello di quelli sull'animazione (`docs/notes/anim-theory/`). Richiesta owner
2026-11-19 02:05Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNCY9bE6qgMhjbRePFkZtCLq): "indagare la teoria e i principi dei VFX allo
stesso modo dell'animazione". Thread cloud, Opus medium, solo ricerca, nessuna modifica al gioco.

**Solo teoria generale (ora nel file inglese, vedi sotto)**, con la stessa regola dei documenti di animazione (owner 2026-11-07): niente correzioni di
effetti già fatti, niente regole nate da un solo effetto; i numeri sono **intervalli indicativi**, il verdetto è
sempre a occhio sul video in gioco (regola 10). Le regole operative del nostro kit restano in
`handoffs/stile-sheet-vfx-review.md`, `vfx-stile-v2-astrobot.md`, `vfx-stile-v3-categorie.md`; qui c'è il **perché**.
La checklist per le review Fable sta in `.claude/skills/gstn-vfx-reviewer/` (SKILL.md + project.md).

Legenda: **[F n]** = detto da una fonte (§17). **[O]** = osservato nei riferimenti dell'owner (Astro Bot `astro_1-13`,
clip DK Bananza 2026-11-19). **[A]** = adattamento o ragionamento nostro, da verificare in gioco.
**[G]** = aggiunto dopo la peer review di Gemini (2026-11-19, `docs/notes/reviews/agy-vfx-theory/esito.md` su
`claude/locomotion-finish-1` e04141c7); cosa è stato accettato e cosa no è in §16-bis.
Frame: contati da 1; sim a 62,5 Hz, 1 frame ≈ 16 ms.

**Lezioni dell'owner 2026-11-19** (dopo i video 1080p del fumo v3; ambito: tutti i VFX, il punto 2 i gas). Sono
principi, non ricette; dove contraddicono il testo sotto, vincono loro e il testo è già allineato.
1. **La leggibilità non si compra uccidendo il realismo.** Opacità piena, taglia gonfiata o densità alta fanno
   "leggere" una nuvola ma la trasformano in un PNG piatto. Un gas si giudica su due domande insieme: si legge? sembra
   polvere (fumo, neve) vera? (§5.6-bis)
2. **La polvere vera si disperde**: mentre cresce sfuma fino ad alpha 0. Non si rimpicciolisce restando opaca (§3.13, §8.5).
3. **"Stile DK / Astro Bot" = comportamento realistico, stile solo nel colore e nella palette.** Non usare
   "stilizzato" per il comportamento: fisica, dispersione e luce sono reali (§7).
4. **Si giudica sempre contro il video di riferimento** (il dossier, `docs/notes/review/dossier-riferimento.md`), non
   solo contro la checklist: una checklist tutta "sì" non basta se accanto al riferimento non torna (§12.5).

## 0. Cosa c'era già (non ripetuto, solo richiamato)
- `vfx-clarity-ricerca.md` (11-17): Sakurai su hitstop e risalto dei personaggi, gerarchia Riot, portata onesta, ombre di 3D Land.
- `silhouette-vfx-ricerca.md` (11-15, approvato): la posa porta le fasi lunghe, i VFX le fasi brevi (frame attivi, contatto);
  review in due passate, VFX spenti e poi accesi.
- `stile-sheet-vfx-review.md`: regole di costruzione e checklist D 1-21 (v1/v2).
- `vfx-stile-v3-categorie.md` (11-18): tre categorie di materiale (A detriti solidi, B gassosi, C lampi emissivi).
- `ref-astrobot-vfx/NOTE.md` (+ clip DK Bananza): riferimenti dell'owner.
Questo documento li mette in un quadro unico e aggiunge ciò che mancava: forma, timing in fasi, strati, valore,
fumo e polvere, scelta tecnica, prestazioni, processo. Le contraddizioni trovate fra i documenti sono in §15.

---

## 1. Che cos'è un VFX di gioco → `skills-public/vfx-reviewer/vfx-principles.md` §1

## 2. Forma (shape language) → `skills-public/vfx-reviewer/vfx-principles.md` §2

## 3. Timing: le tre fasi → `skills-public/vfx-reviewer/vfx-principles.md` §3

## 4. Volume e strati → `skills-public/vfx-reviewer/vfx-principles.md` §4

## 5. Leggibilità alla camera di gioco → `skills-public/vfx-reviewer/vfx-principles.md` §5

## 6. Colore e valore rispetto allo sfondo → `skills-public/vfx-reviewer/vfx-principles.md` §6

## 7. Il look Astro Bot / DK Bananza → `skills-public/vfx-reviewer/vfx-principles.md` §7

## 8. Fumo e polvere → `skills-public/vfx-reviewer/vfx-principles.md` §8

## 9. Impatto, hitstop e frame d'impatto → `skills-public/vfx-reviewer/vfx-principles.md` §9

## 10. Scelta tecnica → `skills-public/vfx-reviewer/vfx-principles.md` §10

## 11. Prestazioni sul "tostapane" → `skills-public/vfx-reviewer/vfx-principles.md` §11

## 12. Processo → `skills-public/vfx-reviewer/vfx-principles.md` §12

## 13. Checklist per chi costruisce → `skills-public/vfx-reviewer/vfx-principles.md` §13

## 14. Checklist per chi revisiona → `skills-public/vfx-reviewer/vfx-principles.md` §14

## 16. Limiti → `skills-public/vfx-reviewer/vfx-principles.md` §16

## 17. Fonti → `skills-public/vfx-reviewer/vfx-principles.md` §17

---

## Note di progetto per sezione
Righe specifiche del progetto tolte dal testo generale; i numeri di § sono quelli del file inglese.
- **Legenda**: [O] = osservato nei riferimenti dell'owner (Astro Bot `astro_1-13`, clip DK Bananza 2026-11-19; nel testo originale i riferimenti astro_N indicavano le singole clip). [G] = peer review di Gemini (2026-11-19, `docs/notes/reviews/agy-vfx-theory/esito.md` su `claude/locomotion-finish-1` e04141c7). [F23] = `ref-astrobot-vfx/NOTE.md` (principio dell'owner: lampi emissivi, fumo illuminato, detriti solidi) e `vfx-stile-v3-categorie.md`. Sim a 62,5 Hz.
- **Intro**: quarto documento di teoria, gemello di quelli sull'animazione (`docs/notes/anim-theory/`); regola dei documenti di animazione (owner 2026-11-07): niente correzioni di effetti già fatti, niente regole nate da un solo effetto; verdetto sempre a occhio sul video in gioco (regola 10). Regole operative del kit in `handoffs/stile-sheet-vfx-review.md`, `vfx-stile-v2-astrobot.md`, `vfx-stile-v3-categorie.md`. Checklist Fable: `.claude/skills/gstn-vfx-reviewer/` (SKILL.md + project.md).
- **§1.3**: da noi voto d'importanza per evento, due voti (chi fa / chi subisce).
- **§1.4**: nel nostro progetto la posa porta le fasi lunghe e i VFX le brevi (approvato 11-15, `silhouette-vfx-ricerca.md`).
- **§3 (intervalli)**: gli intervalli indicativi vengono dal nostro sheet (frame a ~60 fps).
- **§3.13**: la regola sulla morte dei gas è l'owner 11-19 (lezione 2 sopra).
- **§4.6**: da noi eventi di presentazione → strati Niagara separati.
- **§5.1**: camera larga da noi: FOV ~86-95° orizz.
- **§5.2**: le nostre viste di giudizio sono dietro (camera di gioco) e lato (approvato 11-12/11-15).
- **§5.6-bis**: regola dell'owner 11-19 (lezione 1 sopra).
- **§6.7**: i due sfondi obbligatori sono una regola dell'owner 11-18.
- **§7.2**: "comportamento reale, stile nel colore" è dell'owner 11-19 (lezione 3 sopra).
- **§7.7**: già nello sheet D12.
- **§8.5**: dispersione dei gas: owner 11-19.
- **§8.6**: coerente col FAIL r8 "nuvola troppo grigia".
- **§8.9**: la soluzione (a) è la v2 del nostro kit, adatta al preset basso.
- **§9.2**: il nostro default attuale `HitstopVfxTimeScale` = 0,15 per tutti gli effetti del colpo è una **decisione dell'owner** (v2, 2A): cambiarlo spetta a lui (§15.4). Categorie del progetto: energia = categoria C; materia = A e B. Il rallentamento globale resta per lo slow-motion cinematografico.
- **§9.5**: usare l'impact frame solo per colpi di voto d'importanza massimo (come da sheet).
- **§10 (decal)**: da noi il decal blob è risultato invisibile, si usa il disco (`vfx-stile-v2`).
- **§11 (titolo)**: "tostapane" = hardware di riferimento minimo del progetto; obiettivo adottato < 0,5 ms per i VFX di combattimento, picchi fino a 1 ms.
- **§11.3**: i nostri burst sono di decine di particelle (tetto 12 per burst nello sheet) e restano su CPU; si misura solo se un effetto supera ~100.
- **§12.3**: da noi lo style guide è lo sheet + v3.
- **§12.5**: il dossier del riferimento è `docs/notes/review/dossier-riferimento.md` (owner 11-19).
- **§12.6**: l'assistente AI integrato nell'Unreal Editor per i dubbi tecnici è una indicazione dell'owner 11-19; strumenti UE 5.9 in `docs/notes/review/ue59-strumenti-per-skill.md`.
- **§14**: la checklist è la skill `.claude/skills/gstn-vfx-reviewer/` (SKILL.md + project.md).
- **§16**: le osservazioni [O] vengono dai riferimenti dell'owner; i numeri di prestazioni vanno misurati sul nostro hardware.
- **§17 (G)**: peer review Gemini in `docs/notes/reviews/agy-vfx-theory/esito.md` (claude/locomotion-finish-1 e04141c7).

## 15. Contraddizioni fra i documenti esistenti (da confermare, regola 13d)
1. `stile-sheet-vfx-review.md` D2 ("nessun fumo realistico") e D12 (il traslucido morbido come segnale di "Unreal
   generico") contraddicono la **v3** (categoria B: fumo realistico, traslucido, illuminato) e la scelta dell'owner
   11-18 (fumo morbido colorato dal materiale). Proposta: D2 e D12 valgono solo per energia e solidi; per i gas vale v3.
2. Sheet A "Materiale: mai Translucent" e "Morte: mai dissolvenza in trasparenza": superati dalla v3 per la sola
   categoria B (decadimento morbido di opacità permesso). Proposta: aggiornare lo sheet con l'ambito per categoria,
   con il vincolo di §3.13 (prima erosione o contrazione, l'opacità cala solo a fine vita) [G].
3. Sheet B "Colpo: lampo + toro neon": la memoria dell'owner dice "luce, non anelli" per il colpo (VFX v3).
   Proposta: lo sheet B segue la v3; l'anello resta solo se l'owner lo rivuole.
4. **Hitstop dei VFX** (da peer review): default 0,15 per tutti (decisione owner v2 2A) contro hit mark a velocità piena
   e materia frenata (§9.2). Proposta: A) energia a 1,0 e materia a 0,15 (consigliata, una seconda impostazione);
   B) tenere 0,15 per tutto. Si decide guardando le due versioni in gioco.
5. **Dissolvenza dei gas** (owner 11-19): lo sheet dice "mai dissolvenza in trasparenza" e la versione precedente
   di §3.13 permetteva ai gas di calare di opacità solo a fine vita. L'owner ha deciso: i gas crescono sfumando ad
   alpha 0 per tutta la dissipazione. Risolta; sheet allineato per la categoria B il 2026-11-19.
Il coordinatore le porta all'owner; questo documento non cambia lo sheet.

## 16-bis. Peer review di Gemini: cosa è entrato e cosa no
- **Accettato**: nome Chamberlin (verificato) e Oona Salla; principi mancanti (squash and stretch, follow-through
  degli strati, orientamento, decadimento termico, erosione invece del velo, contrasto di frequenza, impronta
  sull'ambiente come idea); trappole del preset basso (luci dinamiche, flipbook con motion vector); tetto di 3-5
  strati; numero di texture lette; budget < 0,5 ms come obiettivo; checklist del revisore ridotta da 26 a 12 domande
  + 2 controlli strumentali, con la domanda nuova su camera e orientamento.
- **Accettato come proposta all'owner**: hit mark a velocità piena nell'hitstop (§9.2, §15.4).
- **Non accettato o corretto**: (1) "Miyamoto è a pagina 3": verificato, la frase è a **pagina 2** (link F9 giusto).
  (2) Soglia GPU "~100-200 raccomandata da Epic": la pagina Epic non dà numeri; resta non verificato (§11.3) e non
  cambia nulla per i nostri burst. (3) Fonti Astro Bot: la sessione CEDEC 2025 esiste [F32], ma riguarda fisica,
  fluidi e distruzione; **non dice nulla** su clearcoat, fumo che campiona le sonde o stelle emissive di 1 frame, che
  Gemini le attribuisce; il relatore grafico è Hikaru Yamaguchi, non "Futoshi". Quelle affermazioni restano
  osservazioni, non fonti. (4) "Impronta ambientale" citata a Team Asobi: idea buona, fonte non trovata.
  (5) "Mesh GPU = 1 draw call": anche le mesh su CPU sono istanziate per emitter; la scelta CPU resta (§11.3).
