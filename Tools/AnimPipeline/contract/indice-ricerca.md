# Indice dei documenti di ricerca su animazione, picchiaduro, feel e VFX (2026-11-24)

Ricognizione del thread "Make animation research a mandatory review checklist" (owner: "ci sono altri .md di ricerca, non
solo sul corpo ma anche sui picchiaduro"). Cercati: repo (branch claude/project-thread-y073op), storia git di tutti i
branch, project files (`pf:`), skill OG installata sull'account, knowledge base sul branch `claude/project-thread-m6s9y2`.
"Uso prima" = cosa ne faceva la skill di review su y073op prima di questo thread. "PR" = voce di `principi-review.md`
dove ora il documento e' citato e quindi considerato in ogni review finale (INV-13).

## Animazione: corpo e picchiaduro (entrano nella review)
| Documento | Tema | Uso prima | Principi verificabili | PR |
|---|---|---|---|---|
| `docs/notes/anim-theory/principi-animazione-corporea.md` | corpo: timing/spacing, peso, anticipazione, overshoot, overlap, moving hold, archi, squash, silhouette, errori, metodo §13 | citato, letto "solo la sezione che stai per citare" | tutti i §2-§12 con le domande del §13 | 01-04, 08-11, 13-15 |
| `docs/notes/anim-theory/animazione-picchiaduro-action.md` | picchiaduro: fasi, hitstop e posa d'impatto, smear, leggibilita', pose tenute, reazioni, aerei, secondario, review frame per frame §10, fonti Sakurai | citato (§10) | §2 posa d'impatto e dopo l'hitstop, §4, §5, §6, §7, §8, §10 | 01-07, 09, 12, 13, 16-19 |
| OG `Tools/AnimPipeline/contract/archive/gstn-move-animation_2026-11-15.md` (= skill account `gstn-move-animation`) | "Posing fundamentals" 1-11, "How the owner judges" 1-7, "Style principles" Sakurai/Smash 1-12 | solo come riferimento di qualita' (INVARIANTI), le liste non erano voci di review | snap 1-2 f, hold + rinculo, cascata, archi, recovery dall'arto, guardia al FAF, esagerazione, silhouette da ogni lato, articolazioni | 01-05, 08-15 |
| `pf:moveset-template/ricerca.md` | Sakurai GDC "How to Animate a Smash Bros Attack": 4 fasi, anticipazione leggibile da chi difende, strike brevissimo; frame data | ignorato | fasi, anticipazione leggibile | 01, 02 |
| `pf:handoffs/archivio/2026-11/hitstop-sakurai-1.md` | le 8 tecniche di hitstop di Sakurai (codice, non compilato) | ignorato (solo VFX) | tremolio vittima > attaccante, a terra solo di lato, blend ~4 f nella posa di danno, micro-avanzamento | 06 |
| `pf:handoffs/archivio/2026-11/vfx-clarity-ricerca.md` §1 | note di Sakurai: hitstop con tetto, feedback immediato all'input, esagerare | ignorato | feedback al frame 1, esagerazione | 02, 06, 14 |
| `pf:anim-theory/silhouette-multidirezionali.md` | silhouette da dietro e di lato, regole di posa e di movimento, checklist animatore e revisore | citato dalla rubrica, mai caricato | arto fuori sagoma, gomito dentro, pugno visibile, viste | 13 |
| `pf:handoffs/archivio/2026-11/reazioni-ricerca.md` | reazioni agli hit in altri giochi, set minimo, semi-procedurale | ignorato | direzione del colpo, parte colpita guida | 19 |
| `pf:handoffs/archivio/2026-11/rivals2-animazione.md` | Rivals 2: frame 1 conta, hitstun e frame di hitpause, knockdown, coda oltre l'IASA | ignorato | primo frame leggibile, frame 1 della reazione | 02, 19 |
| `pf:handoffs/archivio/2026-11/ritorno-idle-ricerca.md` | ritorno a idle da mosse create per un'altra posa | ignorato | settle verso l'idle, niente scatti al blend | 17 |
| `pf:handoffs/archivio/2026-11/landing-hard-cut-regola.md`, `stomp-atterraggio.md` | regola owner sul landing lag; atterraggio del dair | ignorati | atterraggio che assorbe e dura quanto il lag | 18 |
| `pf:handoffs/archivio/2026-11/stile-astrobot-nintendo.md` §5 | stile Astro Bot/Nintendo, promemoria camera e animazione | ignorato | stile, stella polare | 14 |
| `docs/notes/review/critico-estetico.md` | asse vita, pesi, stella polare | usato | asse vita | 08, 14 |
| KB `m6s9y2:skills-public/knowledge/animation/` (01-10, checklists, review-rubric) | principi generali con ID stabili P-*, segnature misurabili dell'animazione morta, checklist per tipo di mossa | ignorato dalle skill di repo; la skill OG dell'account lo cita ma il branch non e' in main | P-ANTIC, P-SPACE, P-HOLD, P-LIFE, P-OVERLAP, P-ARC, P-POSE, P-GAME | quasi tutte |
| `m6s9y2:docs/knowledge/study/stylized-attacks-part1/2.md`, `body-mechanics.md`, `video-reference.md` | note dei corsi Gnomon (uso interno): attacchi leggeri/medi/pesanti, timing, master control | ignorati | livelli di forza, anticipazione/impatto/follow-through | 02, 03 |
| `m6s9y2:docs/knowledge/profile-gstn.md`, `cases/attacks.md` | profilo di stile del progetto e casi | ignorati | stile del progetto | 14 |

## Feel, gameplay, camera, strumenti (non sono principi di animazione: fuori dalla review della posa)
| Documento | Tema | Uso | Nota |
|---|---|---|---|
| `pf:handoffs/archivio/2026-11/review-feel-defaults.md`, `feel-defaults-piano.md` | valori di tuning (lancio, peso, gravita') confrontati con Smash | ignorato | tuning di gioco: un difetto che ne deriva e' `gioco` |
| `pf:handoffs/archivio/2026-11/attack-aiming-research.md` | mira e rotazione in startup (Monster Hunter) | ignorato | design di movimento, decisioni owner |
| `pf:handoffs/archivio/2026-11/anim-physics-pass-ricerca.md` | strumento "alla Cascadeur" per curve e fisica | ignorato | strumento, non criterio |
| `pf:handoffs/archivio/2026-11/rivals2-studio.md`, `reaction-anim-sources.md` | mod kit Rivals 2; fonti gratuite di clip | ignorati | plugin e fonti |
| `docs/notes/camera-platforming/ricerca-mario-astro.md`, `pf:camera-mario-research.md` | camera alla Mario/Astro Bot | ignorati | camera |
| `docs/notes/research/unreal-assistant-*.md` (6) | risposte dell'assistente Unreal su locomotion GASP | ignorati | regola 14, locomotion |

## VFX (hanno la loro skill `gstn-vfx-reviewer`)
`docs/notes/vfx-theory/principi-vfx.md` (usato dalla skill VFX), `skills-public/vfx-reviewer/vfx-principles.md`,
`pf:handoffs/archivio/2026-11/silhouette-vfx-ricerca.md`, `vfx-clarity-ricerca.md` §2-4, `vfx-stile-v2-astrobot.md`,
`vfx-stile-v3-categorie.md`, `pf:riferimenti/vfx-colpo-dk-bananza/studio_dk_clip_2059.md`, `pf:riferimenti/vfx-fumo-dk-bananza/studio_fumo_breakdown.md`, KB `m6s9y2:skills-public/knowledge/vfx/`.
Non toccati qui: il loro posto e' la review VFX.

## Da far controllare all'owner
1. La knowledge base generale (P-*) e le note dei corsi stanno solo sul branch `claude/project-thread-m6s9y2` (PR #36 chiusa
   apposta): la skill OG dell'account la cita, le skill del repo no. Va portata in main o resta solo come riferimento?
2. `silhouette-multidirezionali.md`, `hitstop-sakurai-1.md`, `reazioni-ricerca.md`, `rivals2-animazione.md`,
   `ritorno-idle-ricerca.md` stanno solo nei project files (archivio): vanno copiati in `docs/notes/anim-theory/`?
3. `animazione-picchiaduro-action.md` §5 (stile a pose tenute "12 fps su 60") e' una scelta di stile ancora da decidere.
4. Le 8 tecniche di hitstop di Sakurai (`hitstop-sakurai-1.md`, branch claude/project-thread-8oy91z) al 18-11 non erano compilate e la KB le dice non in main: da verificare; finche' mancano, PR-06
   segnala i pezzi assenti come `gioco`.
