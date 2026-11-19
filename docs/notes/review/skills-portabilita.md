# Rendere le skill di review riusabili (piano, 2026-11-19)

Owner 11-19: "possiamo pubblicarle su GitHub e renderle riusabili in altri contesti". **Nulla è pubblicato**: la
pubblicazione è un'azione verso l'esterno e aspetta l'OK esplicito dell'owner. Questo file dice cosa è generico, cosa è
del progetto e come separarli. Ambito: `gstn-anim-reviewer`, `gstn-anim-executor`, `gstn-vfx-reviewer`,
`principi-vfx.md`, critico, dossier (account skill `gstn-move-animation` in un secondo momento).

## Principio
Due strati. **Nucleo generico** (il metodo: come si guarda, chi giudica, critico cieco, dossier, domande, formato
del verdetto) senza percorsi, nomi di asset o regole dell'owner. **Strato di progetto** (un file `project.md` per
skill) con tutto il resto. Il nucleo dice "leggi lo strato di progetto" e funziona anche senza.

## Cosa sta dove
| Parte | Nucleo generico | Strato di progetto (GSTN) |
|---|---|---|
| Critico (`critico-estetico.md`) | tutto il principio | stella polare "Ganondorf dentro Astro Bot", etichette |
| Dossier (`dossier-riferimento.md`, template) | tutto | 62.5 Hz, cartelle, `rv_frames.py` |
| Revisore anim | catena di cattura, estrazione frame, 4 momenti × 2 viste, domande, critico, dossier, curve dell'animation BP, una istruzione alla volta, verdetto | contratto `Tools/AnimPipeline/`, card, `animpipe`, etichette `v/m/a`, regole 4/10/13, Smash/AAA, GASP, italiano |
| Esecutore anim | un'istruzione, snapshot, gate, prima/dopo, rifiuti | `animpipe`, Antigravity/agy-bridge, git e permessi del fisso |
| Revisore VFX + principi | domande 1-12 e principi §1-§14 (sono già teoria generale con fonti) | sheet e v3, DK Bananza/Astro Bot, L_Test_Hub, preset, kit, lezioni dell'owner come esempio |
| Strumenti UE 5.9 (`ue59-strumenti-per-skill.md`) | elenco per motore (Unreal) come appendice | cosa è provato da noi |

## Passi (un thread nuovo, Sonnet medium, solo documenti, ~3-4 $)
1. Per ogni skill: spostare le righe di progetto in `project.md` accanto allo `SKILL.md`; nel nucleo un rimando.
2. Tradurre i nuclei in inglese (le skill VFX sono in italiano); lo strato di progetto può restare in italiano.
3. Rileggere i nuclei cercando nomi del progetto (`GSTN`, percorsi, asset, nomi): nessuno deve restare.
4. Preparare una cartella `skills-public/` con nuclei + README + licenza da scegliere (decisione dell'owner).
5. Pubblicazione: solo dopo l'OK dell'owner (repo, licenza, nome).
Rischi: due copie che divergono. Per evitarlo il progetto usa il nucleo + `project.md`, mai una copia modificata.
