# VFX v3 "Astro Bot semi-realistico": design sheet e piano per categoria (2026-11-18)
Fonti: brief `handoffs/vfx-stile-v3-categorie.md`, note owner `handoffs/ref-astrobot-vfx/NOTE.md` (astro_1-5), base v2 (branch `claude/project-thread-aait2t` 9bc3d6d3, handover `vfx-v2-r1.md`). Ambito: TUTTI i VFX di impatto e movimento, uno stile per tutti i kit. Target toaster.

## Diagnosi v2 (perché l'owner dice "non è Astro Bot")
v2 ha reso tutto "giocattolo lucido": anello giallo, palline, gemme. Ma in Astro Bot solo i **detriti** sono giocattoli. L'energia è **luce** (stella bianca netta, linee), il fumo è **fumo vero** (morbido, traslucido, illuminato, del colore del materiale). L'anello giallo con le palline attorno al colpo è esattamente il "cartoon" da togliere. Lezione di processo: Fable ha passato v2 confrontando v2 con r4, non con Astro Bot. In v3 il paragone è sempre con le catture dell'owner.

## Le 4 regole di stile (valgono per ogni effetto, ogni kit)
1. **Energia = luce, non oggetto.** Colpi, lampi, getti, linee: Unlit, emissivo alto (bloom), bordi netti, vita 2-8 frame. Niente tori, niente palline, niente mesh lucide per l'energia.
2. **Aria = fumo vero.** Polvere, neve, fumo di skid/partenza: sprite Translucent morbidi, illuminati dalla scena, Depth Fade, colorati dalla superficie colpita (neve bianca-azzurra, erba verde-sabbia, pietra grigio caldo, rosa sul rosa). Burst veloce, frenata forte, si allarga e svanisce in opacità.
3. **Solidi = giocattoli, pochi e piccoli.** Le mesh lucide restano SOLO per detriti fisici e coriandoli (solo colpi forti, rotture, portali). Rimbalzano e spariscono < 1 s (3A).
4. **Leggibile su chiaro e su scuro.** Ogni lampo ha nucleo bianco + frangia satura del colore d'accento del kit; mai solo additivo (sparisce sul bianco) né solo bianco (sparisce sulla neve). Ombra di contatto netta sotto il personaggio come ancora di profondità.

## Categorie: cosa cambia
| Cat. | Riferimento | v3 | Materiale / renderer | Perf toaster |
|---|---|---|---|---|
| **C1 Colpo** (S/M/L) | astro_1 | Stella bianca a 6-8 raggi affusolati, netta, frame 1 al massimo, scala grande→0 in 3-4 frame + 6/8/10 streak brevi colorate (accento kit) velocity-aligned, vita 5-8 frame + 4-8 scintille puntiformi. **Via anello toro e gemme gialle.** | `M_GSTN_VFX_Flash` Unlit Translucent (non additivo), emissivo 6-15, texture stella procedurale 256² con frangia colorata da parametro. Streak: sprite allungato stesso materiale. | 2 emitter sprite, ≤ 18 particelle, 1 texture condivisa |
| **C2 Getti/laser/linee** | astro_2 | Linee sprint e ogni getto/raggio: linee sottili (2-4 px a 1080p) bianco-azzurre molto luminose, nucleo bianco. Sostituiscono le capsule lucide. | Stesso `M_GSTN_VFX_Flash` (variante linea) su sprite velocity-aligned o ribbon corto | ≤ 6 particelle |
| **B1 Polvere/neve d'impatto** | astro_3 | Nuvola morbida traslucida, 5-8 sprite grandi + 6-10 granelli fini, colore dalla superficie, illuminata (luce calda + sky), Depth Fade. Su colpo forte e atterraggio pesante. Scia di "strappo" del colpo forte = streak C1 lunghe nel colore d'accento. | `M_GSTN_VFX_Smoke` Translucent, lighting Volumetric PerVertex NonDirectional (il lit più economico), texture puff procedurale 4×4 flipbook 512², erosione morbida | ≤ 16 particelle, sprite medi per limitare overdraw |
| **B2 Polvere di locomozione** (partenza, skid, atterraggio, salto) | astro_3 | Come B1 in piccolo: 3-5 puff + 3-4 granelli, colore superficie. **Via anello di atterraggio**: l'atterraggio diventa puff radiale basso. | come B1 | ≤ 9 particelle |
| **A Detriti/coriandoli** | astro_1, astro_4 | Solo colpi L, rotture, portali: 3-5 schegge piccole (metà v2) nel colore della superficie (sassolini, zolle, neve compatta) o coriandoli lucidi nel colore kit per eventi "premio". Rimbalzi sul piano (v2), < 1 s. | `M_GSTN_VFX_Toy` v2 (riuso), Gem/Ball piccoli | invariato v2, spenti nel preset basso |
| **D Ombra di contatto** | astro_2 | Disco scuro netto sotto il personaggio (bordo 1-2 cm di sfumatura), opacità che cala con la quota, blu-viola scuro non nero. Anche sotto detriti in aria (v2). | Mesh piatta `M_GSTN_VFX_ToyShadow` (riuso v2) sul componente presentazione, proiettata con 1 trace/frame già esistente del ground plane | 1 draw per personaggio |

Colore superficie: `SurfaceType` del Physical Material colpito → tabella colori nella DA di presentazione (5 righe: default, neve, erba, pietra, sabbia), passata come `User.SurfaceColor`. Fallback: colore di default del livello. Il trace c'è già (ground plane v2).

Restano valide: hitstop VFX 0,15 (2A), effetti tirati verso la camera, chiarezza dello sheet (`stile-sheet-vfx-review.md` D1-D20), Sakurai per la clarity, stile nostro.

## Piano di lavoro (fisso, editor _ue59, un tentativo + max 2 fix)
1. Texture procedurali da script Python (stella, puff flipbook, granello): `Tools/GAS/setup_presentation_vfx_tex.py`, numpy→PNG→import. Nessun asset esterno.
2. Materiali `M_GSTN_VFX_Flash`, `M_GSTN_VFX_Smoke` + istanze (`setup_presentation_vfx_v3.py`).
3. Ricostruzione dei 9 sistemi in `setup_presentation_vfx.py` secondo la tabella; tori e capsule fuori.
4. C++ piccolo: `User.SurfaceColor` dal trace + tabella; ombra di contatto sul componente.
5. Cattura: per C1, B1, B2, C2 e D, video in camera di gioco a **velocità reale** (60 fps) su un punto CHIARO e uno SCURO di L_Test_Hub + griglia attorno all'hit.
6. Pacchetto review: per ogni categoria un **foglio affiancato** (cattura owner a sinistra, nostro frame equivalente a destra) e il mp4 a velocità reale.

## Review Fable (regola 10, nuova per v3)
Ordine: (1) **paragone con le catture owner**, una domanda per categoria: "messo accanto ad astro_N, sembra lo stesso gioco?" SÌ/NO con motivo; (2) leggibilità su chiaro e su scuro; (3) mp4 a velocità reale guardato come giocatore (il colpo si legge? il fumo sembra fumo?); (4) checklist D1-D20. Vietato confrontare solo con v2. FAIL etichettati stile / forma / timing / colore / gioco.

## Costo (fisso, Opus medium + Fable high; base reale v2 ~38 + 9 $)
Build v3-r1 ~22-28 $ (meno sistemi nuovi, riuso pipeline v2) + review Fable ~8 $; un giro di fix ~10 + 5 $. **Totale stimato ~45 $ (38-52).**

## Scelte applicate senza owner (regola 6)
- Via tutti i tori/anelli (lamentela esplicita dell'owner), anche dall'atterraggio.
- Polvere colorata dalla superficie via Physical Material; dove manca, colore di default del livello.
- Lampo Translucent con frangia colorata invece di additivo puro (leggibilità su neve/chiaro).
- Ombra di contatto del personaggio inclusa in v3 (è nelle note dell'owner, astro_2).
