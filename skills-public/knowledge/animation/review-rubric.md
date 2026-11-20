# Animation review rubric (generic)

Use for any clip review. A project profile may add questions; it may not remove a FAIL axis without saying why.
Every answer is **yes / no + frames + view**, and every "no" cites a criterion ID.

**A review that answers only the CLEAN and DATA axes cannot give a PASS.** (`P-LIFE-07`)

## Order
0. Capture chain valid (the new clip is really playing; same start frame as the base; labels readable). If not, the
   review is void.
1. Isolation report attached (`P-GAME-01`, `P-MEASURE-02`). Missing = stop, ask for it.
2. Axes below, in this order. Stop at the first FAIL axis only if it makes the others meaningless (e.g. wrong clip).
3. Keep the three gravest gaps; each with the smallest fix and the label `pose` / `data` / `game`.

## Axes

| Axis | Question | FAIL when | IDs |
|---|---|---|---|
| **LIFE** | Does the body move like something with mass and intent, or like poses being morphed? | `stop_sync_share` > 0.5, or on video: parts start/stop together, eased arrival on every key, dead frames, no moving holds, twinning | P-LIFE-01..06, P-OVERLAP-01/02, P-HOLD-01/02 |
| **WEIGHT** | Does the body load before it pushes and absorb after contacts? Is the mass centre where the action needs it? | no dip before take-off/start, no absorption after landing/stop, held pose off balance | P-WEIGHT-01..04, P-LIFE-05 |
| **TIMING** | Is there contrast between fast and slow? Does spacing follow force? Do strikes accelerate into contact? | flat timing, eased strike, first frame not a new pose on input actions | P-TIME-01/02, P-SPACE-01..03, P-ANTIC-04 |
| **ARCS & POSES** | Do extremities travel on arcs? Do the key poses read in silhouette from the game camera and the side? | straight paths on organic moves, zig-zags, action hidden in a required view | P-ARC-01/02, P-POSE-01..03 |
| **CONTACT** | Do planted feet stay planted, do feet re-plant one at a time? | unmarked slide, no plants, both feet airborne to change stance | P-CONTACT-01..03 |
| **ROOT & HAND-OFF** | Does the root move with the body? Do the first and last poses hand off without pops? Loops close with velocity? | root leads/lags the pelvis, slide in game at used speeds, pop at blend, loop hitch | P-GAME-02..06 |
| **DATA** | Are the curves/markers/notifies the system reads present and right? | missing or wrong data | P-GAME-09 |
| **CLEAN** | Interpenetration, reversed joints, pops, jitter? | any visible | P-ARC-02, P-SETTLE-03 |
| **STYLE** | Does it match the project profile (north star, exaggeration, timing scale)? | profile rule broken | the profile's own IDs |
| **BAR** | Next to two references of the same class, does it hold up? | doubt = no | — |

## Verdict format
```
<move> <version> — PASS | FAIL — <date>
LIFE: yes/no (frames, view, IDs)          ROOT & HAND-OFF: ...
WEIGHT: ...                               DATA: ...
TIMING: ...                               CLEAN: ...
ARCS & POSES: ...                         STYLE: ...
CONTACT: ...                              BAR: ...
Gaps (max 3): 1. <ID> f<a>-<b> <view>: <what is seen> -> <smallest fix> [pose|data|game]
Isolation report: <path>
```
