# Prompt per Gemini (Antigravity / agy): peer review della teoria VFX (2026-11-19)

Da incollare ad Antigravity sul desktop. **Solo lettura e ricerca: non modificare nessun file del progetto, non aprire
l'editor, non toccare `_ue59`.** Scrivi una sola risposta nel file indicato dal coordinatore (default:
`docs/notes/reviews/agy-vfx-theory/esito.md` in un worktree tuo sul branch `claude/project-thread-mn2nvi`), in italiano,
massimo ~250 righe.

## Cosa leggere
Fai `git fetch origin claude/project-thread-mn2nvi` e leggi con `git show origin/claude/project-thread-mn2nvi:<path>`
(nessun checkout sul worktree dell'owner):
1. `docs/notes/vfx-theory/principi-vfx.md` — teoria generale dei VFX di gioco (il documento da recensire).
2. `docs/notes/vfx-theory/gstn-vfx-reviewer.SKILL.md` — checklist per le review VFX fatte da un altro modello.
Contesto (solo se serve): il gioco è un action/picchiaduro 3D in Unreal 5.9, stile visivo "Astro Bot / DK Bananza"
(effetti semi-realistici: lampi emissivi netti, fumo morbido illuminato colorato dal materiale, detriti solidi lucidi),
timing e peso dei colpi "alla Smash Bros" (hitstop di Sakurai), camera larga in terza persona da dietro, target hardware
molto debole ("tostapane"), sim a 62,5 Hz.

## Cosa ti chiedo (numera le risposte come qui; per ogni punto: verdetto breve + esempio concreto + fonte se ne hai una)
1. **Errori di fatto**: c'è qualcosa di sbagliato o attribuito male (Sakurai, Riot, Blizzard, Gilland *Elemental Magic*,
   Nintendo, Epic/Niagara)? Cita la riga del documento.
2. **Principi mancanti**: quali principi importanti della teoria VFX mancano? Pensa a: GDC VFX Bootcamp/Summit,
   RealtimeVFX, libri (*Elemental Magic*, *The Illusion of Life* applicata agli effetti), artisti VFX Nintendo/Riot/
   Blizzard/Insomniac/Team Asobi. Massimo 8, i più importanti per primi.
3. **Astro Bot e DK Bananza**: conosci fonti (interviste, talk GDC/CEDEC, breakdown) sui loro VFX che noi non abbiamo
   trovato? Cosa dicono su forma, timing, materiali, luce degli effetti? Se non ne conosci, scrivi "nessuna fonte".
4. **Sezione 9 (hitstop e VFX)**: l'ordine degli eventi nel frame del contatto e il rallentamento dei VFX durante
   l'hitstop (non fermi del tutto) sono corretti rispetto a Smash e ad altri giochi d'azione? Esempi contrari?
5. **Sezione 10 (flipbook / sprite / mesh / ribbon / luce)**: la tabella è giusta? Cosa cambieresti per un hardware molto
   debole in Unreal 5.9 Niagara?
6. **Sezione 11 (prestazioni)**: i numeri (overdraw, ~1000 particelle per la GPU, istruzioni shader, budget ms) sono
   ragionevoli? Quali sono superati?
7. **Fumo e polvere (sezione 8)**: il comportamento descritto è corretto? Manca qualcosa per un fumo "stilizzato
   semi-realistico" credibile e economico?
8. **Checklist del revisore**: quali domande sono vaghe, doppie o impossibili da giudicare guardando un video? Quale
   domanda aggiungeresti? Le 26 domande sono troppe per una review?
9. **Sezione 15 (contraddizioni)**: le proposte di risoluzione hanno senso?
10. **Le tre modifiche più utili** al documento, in ordine di priorità, ognuna in una riga.

## Regole per la risposta
- Se non sai una cosa, scrivi "non so" invece di supporre. Distingui fonte verificata da tua opinione.
- Niente riscrittura del documento: solo critica e proposte puntuali.
- Fonti come URL o titolo + autore + anno. Niente immagini di altri giochi nel repo.
- In fondo: tempo impiegato e cosa non hai potuto verificare.
