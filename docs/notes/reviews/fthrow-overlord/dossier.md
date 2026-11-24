# Dossier: Zetterburn F throw (Rivals 2) -> Overlord F. throw (presa) — 2026-11-23

Fonte: piano `fthrow/fthrow-piano-2026-11-23.md` (project files, thread "Plan F throw from Zetterburn reference"), griglie
`fthrow/zetterburn_fthrow_grid_f44-82.png` e `..._f76-112.png`. Video owner 30 fps -> 1 frame video = 2 frame di gioco a 60, errore +-2.
Il riferimento serve SOLO per idee (coordinatore + owner): la carica lunga di 28 fg NON si tiene.

## Timing del riferimento (fg dall'input, dal piano)
| fg | fase | posa |
|---|---|---|
| 0 | input | flash, vittima spinta indietro |
| 4-28 | carica indietro | busto indietro, peso dietro, accovacciato al picco |
| 32-52 | affondo | scatto in avanti con scia |
| ~56 | impatto | |
| 60-84 | follow-through | scivola accovacciato oltre la vittima |
| ~96 | FAF | |
Ritmo: carica lunghissima, affondo lungo, impatto, scivolata. Troppo startup per l'owner.

## Overlord (owner 1C 2A 3A): ritmo voluto
Strattone corto (3A) -> carica indietro corta tenuta viva -> testata dall'alto con passo (2A) -> overshoot -> mani che si aprono -> assestamento.
| g (gioco) | a (clip 60) | chiave scheda |
|---|---|---|
| 0 | 0 | presa a due mani sul collo, petto girato verso la vittima |
| 4 | 4 | strattone: spalle su, mani tirano 4 cm, ginocchia caricano, testa parte indietro |
| 8 | 8 | carica: busto e testa indietro, peso sul piede dietro, gomiti stretti |
| 12-15 | 12-14 | picco tenuto con deriva (fronte al punto più alto e più indietro) |
| 18 | 17 | scatto: busto passa la verticale, passo in aria, la testa guida in giù |
| 20 | 19 | IMPATTO: fronte sulla faccia della vittima dall'alto, piede avanti piantato a 30 cm |
| 21 | 20 | overshoot: la testa continua in giù |
| 24 | 23 | rilascio: mani aperte in avanti (mano da cattivo), testa ancora giù |
| 29-35 | 28-34 | risalita, piede che rientra |
| 41 | 39 | assestamento oltre la verticale |
| 46 | 44 | FAF, posa idle |

## Tratti d'identità (regola 13a)
1. Due mani sul collo fino all'impatto. 2. Testata dall'alto in basso con arco ampio. 3. Piantato, un solo passo (<= 40 cm).

## Dimensioni (dalla scheda, misura offline Poser: `make_fthrow_cards.py`, report `%TEMP%/fthrow/fthrow_report.txt`)
- Fronte: picco a14 (y -11.6, z 163.2) -> impatto a19 (y 30.7, z 146.9): 42 cm avanti, 16 cm giù. Overshoot a20 altri 6 cm.
- Passo: piede sinistro +30 cm (a15-a18), rientra a29-a38. Bacino -7 cm all'impatto.
- Mani sul punto collo (5, 38, 136 cm, spazio componente) con errore 0 da a8 a a20; strattone 4,3 cm a a4.

## Dove il riferimento non vale per noi
- Zetterburn rilascia la vittima e poi carica: Overlord la TIENE fino all'impatto (tratto 1).
- Vittima ferma nel gioco: l'àncora del collo è un punto fisso (GSTN_GrabHoldTypes.h). Tirare la vittima verso di sé di 10 cm
  (piano) staccherebbe le mani: la tirata si legge dal corpo che entra con i gomiti che si piegano. Àncora per frame = codice ("gioco").
- Vittima della stessa taglia: il collo a 136 cm la mette ~9 cm sotto il pavimento se resta dritta; serve la posa "tenuto per il
  collo" della vittima (ginocchia piegate) o un bersaglio più basso. Voce "gioco" in coda.

## Non misurabile qui
Pesi delle curve del riferimento (video 30 fps); hitstop del riferimento (incluso nell'impatto).
