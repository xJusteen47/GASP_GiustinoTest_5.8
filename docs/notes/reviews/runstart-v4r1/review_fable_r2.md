# Review: Overlord run start, v4r1 vs v3 (pose round 1) — FAIL (stretto, 1 gap posa) — 2026-11-20

Catena di cattura: valida. `ASSERT: OK mv_run_start_v4s AS_GSTN_RunStart_v4r1 f100-f180: weight >= 0.8 on 100% (min 0.98)`.
Stessi frame per v3 e v4r1, etichette leggibili. Visto: 6 griglie + 2 frame del video (v135, v165, 7 viste).

In breve: i due FAIL del giro precedente sono chiusi (parte frenetica ora grande, postura da bruto tenuta fino a f170).
Resta un difetto nuovo all'uscita: il busto si raddrizza di colpo tra f170 e f178, a clip ancora a peso pieno.

## Criteri owner
| criterio | esito | frame, vista | cosa si vede |
|---|---|---|---|
| Parte frenetica "motore che sale di giri" | si' (ampiezza), dubbio (cadenza) | f134-170 az0, az90 | gomiti fuori, sagoma larga quasi il doppio di v3 da davanti; pugno davanti al mento a f135 e f165 |
| Leggibile dalla camera di gioco | si', con limite | v135, v165 game | sagoma larga e ingobbita distinguibile; in griglia il personaggio e' ~45 px, giudizio debole |
| Stile bruto fino alla fine | si' fino a f170, no dopo | f134-170 az90 | busto ~40-45 gradi, testa bassa, spalle chiuse; v3 era gia' dritto a f150 |
| Tenuto il buono di v3 (ginocchio, stomp, accucciata) | si' | f86-110 az90, az0 | righe identiche fino a f110; da f113 solo piu' inclinazione |
| Nessuna perdita > 15 % | si' | numeri | actor_x f200 774.7 vs 784.5 cm (-1.2 %); ampiezze aumentate |

## Tratti di identita' (brief)
| tratto | esito | nota |
|---|---|---|
| 1. Stomp 0.3 s | si' (posa), difetto gioco | posa invariata e buona; i primi frame di carico si perdono per il blend (vedi gioco) |
| 2. Corsa frenetica | si' in parte | braccia e cosce grandi; la cadenza dei passi sembra quella del loop (~16 frame a passo, stima da griglia a 4 frame) |
| 3. Passaggio pulito al loop | NO | f170 busto molto avanti, f178 gia' dritto: ~25-30 gradi in ~8 frame, peso clip 1.0 fino a f175 quindi e' nella clip |

## Vita (a occhio, prima dei numeri)
| asse | esito | frame | nota |
|---|---|---|---|
| Anticipazione | si' | f89-98 az90 | accucciata e carico in avanti, invariati |
| Archi | si' | f134-166 az0, az90 | braccia su arco fianco-mento, coscia quasi orizzontale a f135 e f138 |
| Peso al contatto | probabile si' | f142, f158 az90 | bacino piu' basso di v3 ai contatti; non misurato |
| Follow-through / ritardo mani | non verificabile | - | campionamento a 4 frame: ritardo di 1-2 frame e overshoot non si vedono |
| Rilascio finale | NO | f170-178 az90 | la postura si spegne di scatto, nessun assestamento |
| Root vs capsula | non verificato | - | manca root_travel della clip; piedi piantati 49/52 cm contro 56/66 di v3 (meglio) |

## Tecnico
- Peso clip: v4r1 1.0 da f100 a f175, 0.98 a f180, 0.86 a f185, 0.62 a f190, 0 a f195. v3 usciva a f140-145.
- Nessuna compenetrazione o flip visibile nei frame guardati. Curve GASP non verificate (nessuna lista nel brief).
- Risultato clip-in-isolamento per v4r1 non tra gli input: da allegare prima del giro 2.

## Fix
1. **posa** (unico gap che decide il FAIL) — f166-182, vista az90. Il rilascio di inclinazione, gobba e contro-testa
   avviene in ~8 frame. Fix minimo: stessa ampiezza, rilascio spalmato con ease-in-out da f160 a f190 (max ~1.2 gradi
   per frame sul busto), cosi' finisce dentro il blend verso il loop. Non toccare f100-160. Verifica: griglia az90
   f160-196 ogni 2 frame, nessun salto del busto > 3 gradi tra due frame consecutivi.
2. **posa, proposta all'owner (non blocca)** — f122-160. La frenesia viene dall'ampiezza, non dal ritmo. Se l'owner
   vuole il "motore che sale di giri" alla Mario/Astro Bot, i primi 2-3 passi dovrebbero essere piu' corti e rapidi del
   loop. Cambia contatti e root: e' design del movimento, decide l'owner.
3. **gioco** — f90-100. La riga run start viene riselezionata da Walk_Start a f90 tenendo il tempo trascorso: v4r1
   parte a 0.15 s ed entra con peso 0.15 -> 0.99; il contatto dello stomp (~f97) e' all'87 %. Per la sessione codice:
   far partire la riga da tempo 0 oppure accorciare il blend d'ingresso. Domanda per l'assistente dell'editor: "Nel
   chooser con colonna Pose Search, come si forza il tempo iniziale a 0 quando la riga cambia da Walk Start a Run Start?"
4. **dati montage** — nessuno.

## Domande owner
- "E' della stessa qualita' di un gioco action stilizzato alla Smash Bros?" Per forza delle pose e carattere: quasi.
  Stomp e corsa da bruto ora si leggono. Non ancora per l'uscita (scatto del busto) e per lo stomp tagliato dal blend.
- "E' alla pari di Smash Bros o altri tripla A simili?" No, non ancora. Mancano: uscita morbida, stomp intero in gioco,
  ritardo delle mani verificato, cadenza che accelera. Dopo il fix 1 e il fix gioco 3 la distanza e' piccola.

## Limiti delle prove
- Griglie a 3-4 frame: ritardi di 1-2 frame, overshoot e pop di un frame non sono verificabili; lo scatto f170-178
  va confermato a 1-2 frame prima del giro 2.
- Camera di gioco: personaggio molto piccolo nelle griglie; video non visto a velocita' reale ne' al 25 % (solo 2 frame).
- Nessun riferimento Smash da dietro: giudizio per sagoma. Nessun critico separato in questo giro.
- Angoli stimati a occhio, non dal dump delle ossa. Abbassamento del bacino e contro-rotazione spalle non misurati.
