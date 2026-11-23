# The instruction: what the reviewer hands to the executor

One file per step, `docs/notes/reviews/<move>-c<N>/passo-NN/istruzione.json`. The executor answers with `esito.json` in the
same folder. Both travel as files (git on the cycle branch, or pasted whole into a message: the brief of the thread says
which). Nobody rewrites or summarizes an instruction on the way.

## Eight fixed fields, plus a header
```json
{
  "mossa": "overlord_jab1_v3", "ciclo": 1, "passo": 3, "tentativo": 1,
  "livello": "misurato",
  "vista": "dietro",
  "non_peggiorare": ["di lato a f9 la mano resta sul petto del manichino"],

  "chiave":    {"anim": 9, "video": 30, "esiste": true, "tratto": [9, 12]},
  "tipo":      "posa",
  "parte":     "arm_r",
  "controllo": "hand_r_fk_ctrl, spazio del personaggio",
  "intento":   "posa d'impatto: il braccio deve uscire dalla sagoma in vista da dietro",
  "modifica":  "mano circa 12 cm più avanti e 6 cm più su, polso aperto verso l'esterno",
  "principio": "silhouette e linea d'azione (principi-animazione-corporea §11)",
  "verifica":  {"testo": "a f9, da dietro, la mano è almeno 10 cm fuori dalla linea della spalla",
                "req": null,
                "misura": "hand_r.x - upperarm_r.x a f9 (spazio del componente) <= -10 cm"}
}
```

| Field | Content | Rule |
|---|---|---|
| `chiave` | Anim frame and video frame, whether the key exists, the span of frames affected | Frames come from the `--video` map (`glossary.md`), never translated by hand. An owner key here makes the instruction a proposal to the owner. |
| `tipo` | `curva` \| `tempo` \| `tween` \| `posa` \| `aggiungi_chiave` \| `togli_chiave` | Listed in the order to try them. `aggiungi_chiave` only when the reviewer says why curve, time and tween are not enough. |
| `parte` | One of `arm_l arm_r leg_l leg_r trunk head hand_l hand_r` | One part. Two parts are two instructions, unless they move as one gesture (say so in `intento`). |
| `controllo` | The rig control and the space (`glossary.md`) | Mandatory at the `misurato` level. At the `principio` level it may be `"a scelta dell'esecutore"`. |
| `intento` | What the viewer must read, with a word from the glossary | Never "better", "heavier", "more natural" alone. |
| `modifica` | `principio`: one sentence. `misurato`: direction in character terms (avanti, su, fuori), cm or degrees | The smallest change that reaches the intent. |
| `principio` | The principle and the section of the theory document it comes from | Kept for the owner (why it changed) and so the executor re-reads only that section. |
| `verifica` | `testo`, plus `req` (an `animpipe verify` request) or `misura` (bones, coordinate, frame, threshold on the bone dump) | Mandatory. Without it the instruction is not valid. It is a PASS or FAIL, not an opinion. |

Header: `livello` (`principio` or `misurato`), `vista` (which view wins if two conflict: `dietro`, `lato`, `camera`),
`non_peggiorare` (what must stay true after the change; each line is checked like `verifica` when it has a number).

## Two levels
| Level | When | How it is written |
|---|---|---|
| `principio` (normal) | Motion between poses: follow-through, overlap, anticipation, hold, return. | `modifica` is a sentence ("più follow-through sul braccio dopo il colpo, f12-f18"). The executor chooses controls, keys and tangents, following the tool order. `verifica` is still mandatory, even simple ("la mano supera la posa di tenuta e torna entro f18"). |
| `misurato` | An exact pose: contact with the target, a part hidden in one view, interpenetration, a failed gate. Always on the second attempt. | `controllo` named, `modifica` in cm and degrees, `verifica` numeric. |

Example at the `principio` level:
```json
{"mossa": "overlord_jab1_v3", "ciclo": 1, "passo": 4, "tentativo": 1, "livello": "principio", "vista": "dietro",
 "non_peggiorare": ["posa di contatto f6 invariata", "hitstop_snap entro 10 cm"],
 "chiave": {"anim": 8, "video": 31, "esiste": false, "tratto": [6, 12]},
 "tipo": "curva", "parte": "arm_r", "controllo": "a scelta dell'esecutore",
 "intento": "follow-through: dopo il colpo la mano continua oltre la posa di contatto e poi rientra",
 "modifica": "più follow-through sul braccio destro dopo il colpo, f6-f12; prima tangenti e tween, chiave nuova solo sulla mano se non basta",
 "principio": "overshoot e settle, overlap (principi-animazione-corporea §6 e §7.3)",
 "verifica": {"testo": "la mano supera la posa di contatto entro f8 e torna entro f12",
              "req": [{"id": "ft_mano", "type": "amplitude", "bone": "hand_r", "coord": "y", "window": [6, 12], "min": 3}],
              "misura": null}}
```

## Rules for the reviewer who writes it
- One instruction per step, the gravest gap first. Never a list.
- Work on keys and curves. Never ask for a frame-by-frame fix.
- Give numbers from the bone dump the executor returned, not from the picture: centimetres read off an image are wrong.
- State what wins when two things pull against each other (`vista`, `non_peggiorare`).
- Second attempt of the same instruction: `tentativo: 2`, `livello: misurato`, same `passo`.

## What the executor returns: `esito.json`
```json
{
  "mossa": "overlord_jab1_v3", "ciclo": 1, "passo": 3, "tentativo": 1,
  "applicata": true,
  "rifiuto": null,
  "snapshot_prima": "2026-11-15T10-42-07",
  "cambi": [{"frame": 9, "controllo": "hand_r_fk_ctrl", "strumento": "posa", "prima": [-17.0, 78.0, 134.0],
             "dopo": [-29.0, 90.0, 140.0], "perche": "istruzione passo 3"}],
  "chiavi": {"prima": 13, "dopo": 13, "aggiunte": [], "tolte": []},
  "segnali": [],
  "gate": {"check": "PASS", "fidelity": "PASS", "dettaglio": "<the file named by `detail` in the check line>"},
  "verifica": [{"id": "mano_fuori", "esito": "PASS", "misurato": -12.4, "atteso": "<= -10"}],
  "non_peggiorare": [{"testo": "di lato a f9 la mano resta sul petto del manichino", "esito": "PASS", "misurato": 2.1}],
  "griglia": "docs/notes/reviews/overlord_jab1_v3-c1/passo-03/prima_dopo.png",
  "ossa": {"frame": 9, "hand_r": [-29.0, 90.0, 140.0], "upperarm_r": [-16.6, 12.0, 141.0]},
  "note": "fatti, massimo tre righe"
}
```
- `strumento` in `cambi`: `curva`, `tempo`, `tween`, `posa`, `chiave_nuova`, `chiave_tolta`. Every added key has its `perche`.
- `segnali`: `chiave_su_ogni_frame` (keys on consecutive frames, with the span), `chiave_ridondante` (a key where the curve
  between its neighbours gave the same result within 0.5 cm / 1°). Warnings, they do not block.
- `ossa`: the dump values (component space, cm) of the bones named in `controllo`, `verifica` and `non_peggiorare`, at the
  frames touched. The reviewer writes the next numbers from these.
- `note` holds facts only. No opinion on how it looks, no proposal for the next step.

`rifiuto` (then `applicata` is false and the card is unchanged):
| Value | Meaning |
|---|---|
| `chiave_owner` | The change would move an owner key or a locked part. The executor quotes frame and part. |
| `ambigua` | A field is missing or can be read two ways. The executor says which field and the two readings. It does not guess. |
| `gate` | Applied in a trial, a blocking gate failed, the card was restored. The failing line is quoted. |
| `formato_scheda` | The change needs something the card cannot hold yet (per-control key, per-key tangent). |
| `serve_owner` | An app must be opened, or the owner edited the sequence in Unreal and a build would erase it. |
| `fuori_ruolo` | The instruction asks to judge, to choose, or to change startup, active frames or FAF. |
