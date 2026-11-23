# Audit stop-and-go di tutte le clip (2026-11-24, cloud, sola lettura)

**Riepilogo (5 righe)**
1. Nel repo non c'e' NESSUN dump di giunti per frame di nessuna clip (i `frames.jsonl` delle review hanno solo posizione e yaw dell'attore, i `gates_joints_*.json` di turn3-v27 solo i risultati dei gate): oggi nessuna fermata e nessun salto si puo' misurare in cloud. Nessun numero e' inventato: ogni riga dice cosa serve.
2. Misura senza editor fatta sulle CARTE animpipe (la causa di K7/K8: tangenti piatte sulle chiavi delle build "tangents"): rischio alto su K7 (6 chiavi interne piatte non dichiarate), settle K6 (3), loop K6 (1), ftilt v3.1 (3), dair v4 (4); nessun rischio da tangenti su jab v12, jab1/jab2 v3, atterraggi dair, tech bounce, reazioni/getups.
3. "Nessun rischio" NON e' un PASS: K8 r2 aveva a1-a5 fermi con solo 2 chiavi a rischio (la causa li' sono chiavi quasi uguali, che senza posizioni non si misurano). Lo stesso vale per run start, w3, turnaround, dash, getups, colpi: serve il dump.
4. w3 verificato sul codice: additivo scritto frame per frame su w2 con inviluppi smoothstep (nessuna chiave, nessuna tangente), quindi il meccanismo K7 non c'e'; resta da misurare (anche le fermate ereditate da w2 e l'inizio da fermo).
5. Pronto per il fisso un solo passaggio (~5 min, nessun editor aperto, niente salvato): dump di tutte le clip per nome e tabella col cancello `no_stop_gate` (comandi in fondo).

Soglie (una sola definizione, `Tools/AnimPipeline/animpipe/curves.py`): passo fermo = ogni punto (bacino in spazio componente, mani, avambracci, testa, piedi rispetto al bacino) < 3,5 cm in un frame; FAIL se piu' di 3 passi fermi di fila fuori dalle tenute dichiarate; salto: max <= 4 x mediana (nello startup).

## Tabella

Legenda rischio: chiavi interne con tangente piatta, non dichiarate tenuta, che entrano in un intervallo di almeno 2 frame (dove la tangente piatta rallenta davvero gli intermedi). Fonte: `logs/flat_keys.tsv`, comando in `logs/flat_keys.cmd.txt` (da `Tools/AnimPipeline`, script `gstn_anim_mcp/flat_key_audit.py`). Fermate e salti: colonne vuote finche' non arriva il dump (sezione "Per il fisso").

| Clip (asset) | Come e' costruita | Rischio K7 dalla carta | Fermate (frame, durata) | Salto max / tipico | Esito | Tenuta dichiarata | Priorita' |
|---|---|---|---|---|---|---|---|
| Dropkick K7 startup (`AS_Pilot_OverlordDropkick_StartupK7`, carta `startup_k7`) | animpipe tangents | **6 chiavi**: 2, 4, 5, 7, 11, 14 (su 9 chiavi, tutte piatte) | gia' viste dall'owner al rallentatore; serie bacino misurata nella diagnosi K8 (3 2 0 0 5 3 3 0 1 ...: fermi ai passi 2-3 e 7) | da dump | FAIL (owner + diagnosi dropkick-k8-3) | nessuna | **P1** (variante spline, tag K7 non ancora spinto) |
| Dropkick settle K6 (ereditato da K7) | animpipe tangents | **3 chiavi**: 2, 4, 7 | da dump (nota skill audit: ~6 frame fermi a terra, v158-163) | da dump | probabile FAIL | nessuna | **P1** |
| Dropkick loop K6 (ereditato da K7) | animpipe tangents | **1 chiave**: 6 (intervalli di 5 frame) | da dump | da dump | da misurare | nessuna (se il loop vuole una tenuta, va dichiarata) | **P1** |
| Dropkick K8 startup (variante) | animpipe tangents | 2 chiavi: 11, 14 (meno di K7) ma a1-a5 fermi visti in review | da dump | da dump | FAIL (owner) | nessuna | gia' in rifacimento (K8R3 spline) |
| FTilt v3.1 (`AS_GanonFTiltV31_Build` / `AS_Pilot_GanonFTilt_V3`) | animpipe tangents (+ eventuale spacing Cascadeur: da verificare quale asset suona nel kit) | **3 chiavi**: 3, 7, 9 (startup charge_long), finestra attiva 9-11 da `move_sheets.json` | da dump | da dump | da misurare, rischio alto | nessuna | **P1** |
| DAir v4 (`AS_Pilot_OverlordDAir_V4` / `AS_OverlordDAirV4_Build`) | animpipe tangents | **4 chiavi**: 3, 15, 17, 31 (attiva 15-17 da `anim_active`) | da dump | da dump | da misurare, rischio alto | nessuna (15-17 e' l'impatto: se voluto fermo va dichiarato `impact`) | **P1** |
| DAir v8 / v9 / v10 (varianti) | animpipe tangents | v8: 7 chiavi; v9 e v10: 11 chiavi | da dump | da dump | rischio alto | nessuna | P3 (solo se tornano in uso) |
| DAir atterraggi (light tuck r4, heavy tuck r3) | animpipe tangents, chiave ogni frame | 0 (intervalli di 1 frame) | da dump (chiavi quasi uguali possibili) | da dump | da misurare | - | P2 |
| Dropkick atterraggio, tech bounce r4 | animpipe tangents, chiave ogni frame | 0 | da dump | da dump | da misurare | - | P2 |
| Jab v12 (`AS_Pilot_JabV12`) | animpipe tangents | 0 (chiavi piatte 1, 2, 3 ma a 1 frame l'una dall'altra) | da dump | da dump | da misurare | - | P2 |
| Jab1 v3 / Jab2 v3 Overlord | animpipe tangents | 0 (piatte 6-9 e 9-12, a 1 frame) | da dump | da dump | da misurare | - | P2 |
| Reazioni / getups / colpi generici (`AS_React_*`: GetUp, GroundHit, HitStand, LieUp, Roll, RollBack, Splat, Tumble; getups r3 e16db0f4) | carte reaction senza spacing (tangenti smart auto) o script react-hook | 0 su tutte le 8 carte | da dump | da dump | da misurare | - | P2 (getups r3 prima di mostrarli all'owner) |
| Dash Ganon r3 (`AS_GSTN_Dash_Ganon*`) | Blender, curva continua con chiave per frame | n/a (nessuna tangente piatta) | da dump | da dump | da misurare | **si'**: affondo "tenuta in movimento" 52->49 (tratto d'identita' 2): deve muoversi, non e' una fermata | P2 |
| Run start v4r5, v4r7, generic w1, w2 (approvate) | Cascadeur / per frame | n/a | da dump (attenzione all'inizio da fermo: va dichiarato o e' difetto) | da dump | da misurare | da decidere per i frame da fermo iniziali | P2 |
| Run start w3 (29w3) | script additivo `runstart_w3_ue.py` su w2, per frame | n/a: verificato, nessuna chiave/tangente; inviluppi smoothstep 0-6, piatto 6-14, sfuma 14-26 | da dump (eredita le fermate di w2, se ce ne sono) | da dump | da misurare | - | **P1 per la misura** (e' la base generica di turnaround e run start) |
| Turnaround 180 L v27 (`..._w2t180Lp`) e v28 (`..._w2t180Ls`) | Cascadeur + breakdown, per frame | n/a | da dump (i `gates_joints_*.json` nel repo non hanno i passi) | da dump | da misurare | - | **P1 per la misura** (owner: tempi morti minimi tra turnaround e w3) |
| Turnaround 180 R / 090 L / 090 R (`..._w2t*s`, basi `..._w2t*f`) | `turnaround_all_ue.py` per frame | n/a | da dump | da dump | da misurare | - | P2 |

## Per il fisso (un passaggio, comandi esatti)

Prerequisiti: checkout `GASP_Giustino2_ue59` su questo branch, nessun editor aperto (Save-Assets lo pretende), script committati.

```
& .\Tools\CI\Save-Assets.ps1 -Script Tools\AnimPipeline\gstn_anim_mcp\audit_stop_dump_ue.py -Tag audit_stop
python Tools\AnimPipeline\gstn_anim_mcp\audit_stop_table.py Saved\AnimPipeline\runstart docs\notes\audit-stop-2026-11-24\logs\dump_gate.tsv --resolved=Saved\AnimPipeline\audit_stop\resolved.json
```

Il primo trova le clip PER NOME o schema (cartelle cambiate col riordino dei Content) e le scarica con `clip_joints_dump_ue.py`; `resolved.json` dice quali schemi non hanno trovato nulla. Il secondo scrive una riga per clip: fermate (serie di frame), tutti i passi fermi, passo mediano e massimo in cm/frame, rapporto e frame del salto. Le tenute volute vanno in un JSON `{clip: "a-b"}` passato con `--holds=`; senza, ogni fermata conta (regola owner). Committare `dump_gate.tsv` e il log `Saved\Logs\ci\save_audit_stop.log` (righe AUDIT_STOP / CJD) per la regola 11.

Non provato: `audit_stop_dump_ue.py` non e' mai girato in Unreal (API asset registry 5.x scritte a memoria); `audit_stop_table.py` provato su due dump sintetici (serie K7 = FAIL, moto costante = OK).
