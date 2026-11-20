# Review: Overlord run start, v4r2 vs v4r1 e v3 (pose round 2) — PASS (posa), con limiti — 2026-11-20

Catena di cattura: valida. `ASSERT: OK mv_run_start_v4s2 AS_GSTN_RunStart_v4r2 f100-f180: weight >= 0.8 on 100% (min 0.96)`.
Attenzione: la cattura v4r2 e' fatta in un altro punto della mappa (sfondo, pavimento e cubi diversi da v3 e v4r1).
I frame coincidono, ma il confronto in camera di gioco e i numeri dei piedi non sono del tutto alla pari.
Visto: 6 griglie, una volta ciascuna. Nessun frame extra dal video.

In breve: lo scatto del busto all'uscita non c'e' piu'. Il rilascio ora e' graduale da ~f160 a ~f184 e finisce dentro
il blend verso il loop. Il resto (stomp, parte frenetica, postura da bruto) e' uguale a v4r1.

## Criteri owner
| criterio | esito | frame, vista | cosa si vede |
|---|---|---|---|
| Parte frenetica "motore che sale di giri" | si' (ampiezza), cadenza = proposta owner | f134-166 az0, az90 | gomiti fuori, sagoma larga, pugno al mento; come v4r1 |
| Leggibile dalla camera di gioco | si', prova debole | f122-182 game | personaggio piccolissimo e inquadratura diversa da v3: non confrontabile bene |
| Stile bruto fino alla fine | si' | f134-162 az90 pieno, f164-180 in calo | a f170-178 e' ancora piu' inclinato di v3; nessun raddrizzamento da velocista a f136-156 |
| Tenuto il buono di v3 | si' (dichiarato) | clip f0-39 invariata | stomp non rivisto in questo giro: griglie stomp non tra gli input |
| Nessuna perdita > 15 % | si' | numeri | actor_x f200 784.5 cm (v3 784.5, v4r1 774.7); ampiezza del rilascio invariata (30 gradi) |

## Tratti di identita'
| tratto | esito | nota |
|---|---|---|
| 1. Stomp 0.3 s | si' (posa), difetto gioco aperto | ingresso a peso 0.15 -> 0.99 su f90-100: non e' un giro di posa |
| 2. Corsa frenetica | si' in parte | ampiezza si'; cadenza uguale al loop, resta proposta all'owner |
| 3. Passaggio pulito al loop | si' | v4r1: dritto in ~8 frame (f172-180). v4r2: busto che sale poco per volta da f164 a f184, nessun salto tra frame vicini a passo 2 |

## Vita (a occhio, prima dei numeri)
| asse | esito | frame | nota |
|---|---|---|---|
| Anticipazione | si' | - | invariata (clip f0-39) |
| Archi | si' | f134-166 az0, az90 | invariati |
| Peso al contatto | probabile si' | f142, f158 az90 | non misurato |
| Follow-through / ritardo mani | non verificabile | - | serve passo 1 frame o il dump delle ossa |
| Rilascio finale | si' | f160-190 az90, az0 | le braccia si stringono e il busto sale insieme, senza scatto; da davanti la sagoma si chiude in modo continuo |
| Root vs capsula | non verificato | - | manca root_travel della clip |

## Tecnico
- Peso clip v4r2: 1.0 da f100 a f175, 0.96 a f180, 0.82 a f185, 0.56 a f190, 0 a f195.
- Piedi piantati f80-240: sinistro 60.0 cm (v4r1 49.2, v3 56.4), destro 52.1 cm (v4r1 52.2, v3 66.0). Il sinistro
  peggiora del 22 % rispetto a v4r1 pur restando nell'intervallo di v3. La modifica tocca solo busto e braccia, quindi
  sospetto la cattura in un altro punto della mappa: da ricontrollare, non da correggere alla cieca.
- Curve GASP non verificate. Risultato clip-in-isolamento di v4r2 non tra gli input.

## Fix
Nessun fix di posa aperto. Restano fuori da questo giro:
1. **verifica (cattura)** — rifare la cattura v4r2 nello stesso punto di v4r1 e confrontare il piede sinistro
   (60.0 contro 49.2 cm). Se la differenza resta, e' un item **posa/dati** nuovo sul piede sinistro, f80-240.
2. **posa, proposta all'owner** — f122-160: primi 2-3 passi piu' corti e rapidi del loop (cadenza). Decide l'owner.
3. **gioco** — f90-100: riga run start riselezionata da Walk_Start con il tempo trascorso; stomp al ~87 % di peso.
4. **dati montage** — nessuno.

## Domande owner
- "E' della stessa qualita' di un gioco action stilizzato alla Smash Bros?" Per pose, carattere e uscita: si', quasi
  pieno. Il limite che resta visibile in gioco e' lo stomp tagliato dal blend d'ingresso (lato gioco).
- "E' alla pari di Smash Bros o altri tripla A simili?" Non ancora. Mancano lo stomp intero in gioco e una cadenza
  che accelera; il ritardo delle mani non e' verificato. La posa da sola non e' piu' il collo di bottiglia.

## Limiti delle prove
- Video non visto a velocita' reale ne' al 25 %: prima di mandarlo all'owner qualcuno deve guardarlo (regola 10).
- Griglia di rilascio a passo 2 frame e celle piccole: un pop di un solo frame non si puo' escludere; il valore
  ~1.1 gradi per frame e' dichiarato dall'esecutore, non misurato da me.
- Cattura v4r2 in un punto diverso della mappa: camera di gioco e numeri dei piedi non alla pari.
- Stomp di v4r2 non guardato. Angoli a occhio. Nessun riferimento Smash da dietro. Nessun critico separato.
