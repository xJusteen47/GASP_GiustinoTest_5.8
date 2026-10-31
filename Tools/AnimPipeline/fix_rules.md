# Fix rules — flag → deterministic model correction

Used by the iteration worker (`iter_worker.md`). Each rule names the model parameter, the direction, the step, its limits
and the re-check. Parameters are the curves at the top of the motion model (`ganon_ftilt_v2_motion.py`; a new move's model
uses the same names). A curve is `{frame: value}`, interpolated monotone (pchip), so **change values at existing keys or add
one key at the flagged frame; never delete keys.** Machine-readable index: `fix_rules.json` (flag text substring → rule id).

Rules were derived on Ganondorf v2 (31/10 night, `ganon_ftilt_log.md` "Modello v2 corretto"): the step sizes are what moved
the number by the amount shown there.

**General limits (all rules):**
- **One rule per flag per iteration.** Change at most 3 curve keys per rule, and only within ±2 frames of the flagged frames.
- **Never move the beats:** the contact frames, the snap (`contact[0]-1 → contact[0]`), the FAF, `KEYS`/`DENSE`, `CONTACT`, `FAF_K`.
- **Never change these,** because they are pose design (escalate instead): `KICK_YAW`, `CONTACT_ANKLE_Y`, `REACH`, the contact/hold `ANK` keys (9–19), `LEAN` at the contact frames, the `ARM_*` "A" keys at the contact frames.
- **A change larger than the "max total" of its rule is a big pose change:** stop and escalate.

## 1. Limbs

| Flag (pre-flight / Unreal) | Correction | Step | Limits | Re-check |
|---|---|---|---|---|
| R1 `limb cannot reach` (arm) | The `ARM_<s>` "A" key nearest the frame: scale the vector so its length = 0.96 × arm length (49 cm on this rig) | one shot | only "A" keys outside the contact frames | pre-flight 1 |
| R2 `limb cannot reach` (leg) | The `ANK` key nearest the frame: pull it toward the hip by (excess + 0.5) cm | one shot | not the contact/hold keys 9–19 → escalate | pre-flight 1, 2 |
| R3 `knee/elbow point off its bone lengths` | Model bug (the IK in the model is wrong) | — | — | **escalate** |
| R4 `elbow direction`: pointing up/sideways, or chicken wing, arm raised past 40° | `POLE_<s>`: add a key at (first flagged frame − `ARM_LAG_<s>` − 1) = `(0.1, -1.0, -0.3)` (down), and one at (last flagged frame − lag + 1) = `(-0.5, -0.85, -0.15)` (back-down) | one shot | **The pole is sampled ARM_LAG frames late: key it that much earlier.** Never "down" (0,-1,0) while the arm hangs (elevation < 20°): the elbow flips inward. Use back-down there. | pre-flight 1 and 6 (a pop means the switch was too sudden: R18) |
| R5 `elbow flared out` | `POLE_<s>` at the flagged frames: outward component (3rd value, sign by side) toward 0 by 0.2 | 0.2 | ≤ 2 steps | pre-flight 1 |
| R6 chicken wing that comes back after R4, with flex < 40° | Straighten: "A" key length → 0.97 × arm length (flex < 30° is not judged) | one shot | only if the pose plan says "almost straight" for that arm; else **escalate** | pre-flight 1 |
| R7 Unreal `forearm/hand over-rolled`, `sudden roll jump` | Not a model parameter (wrist_fix pass) | — | — | **escalate** if wrist_fix leaves it |

## 2. Balance

| Flag | Correction | Step | Limits | Re-check |
|---|---|---|---|---|
| R8 `support knee out of range on one foot`, too bent (> 45°) | `PEL_Y` at the flagged frames (add one key at the worst frame if none is within ±2): **up** by ceil((knee − 44) / 2.5) cm | about 2.5° per cm | pelvis never above 0 (the idle height); max total +3 cm; if the knee is still > 45 at `PEL_Y` 0 → **escalate** | pre-flight 2 |
| R9 support knee too straight (< 10°) | `PEL_Y` down by ceil((11 − knee) / 2.5) cm | about 2.5° per cm | max total −3 cm | pre-flight 2 |
| R10 `off balance`, on two feet | `BAL_S` at the frame: move the target 2 cm toward the support-foot centre (sign: toward 0 if beyond the foot) | 2 cm | max total 6 cm | pre-flight 2 |
| R11 `off balance`, on one foot | `BAL_W` at the frame and the one before → 1.0; if already 1: R10 on `BAL_S` | one shot | — | pre-flight 2 |
| R12 `weight not shifted before lift-off` | `BAL_W` = 1.0 from lift-off − 1 back to (lift-off − 3); if the lift-off is frame ≤ 1 the foot is hovering: R13 | one shot | keep frame 0 ≥ 0.55 (the strong load from the idle) | pre-flight 2 |
| R13 lift-off at frame 0–1 / `no foot on the ground` before the planned lift-off | Kicking foot hovers: `HEEL` angles up 5° on frames 0–5 (the foot rolls about the ball); the ankle keys before the lift-off must come from `HEEL`, not from fixed offsets | 5° | `HEEL` ≤ 40° | pre-flight 2 (lift-off frame = planned) |
| R14 `hips do not drive into the kick` | `BAL_S` at contact +2 cm toward the target | 2 cm | max total +6 cm; keep balance passing | pre-flight 2 |
| R15 `no counterbalance` | `LEAN` at the keys from contact − 1 to the hold end: −2° (back) | 2° | max total −6°; the contact frames are design → **escalate** if the flag needs more | pre-flight 2 |

## 3. Key plan (sparse plans only; dense passes by construction)

| Flag | Correction | Limits | Re-check |
|---|---|---|---|
| R16 `IK chain …: layers keyed on different frames / different lags` | Remove the layer's own `keys`/`lag` in `LAYERS` (all chain layers on the global `KEYS`) | — | pre-flight 3 |
| R17 `Bezier overshoot` / `drift` / `clamped key in mid-motion` | Add the flagged frame to `KEYS`; if it comes back: switch to `DENSE=1` | 2 tries | pre-flight 3 |

## 4–6. Spacing, overlap, arcs

| Flag | Correction | Step | Limits | Re-check |
|---|---|---|---|---|
| R18 `one-frame pop outside the snap` | Add an intermediate key on the part's curve (`ANK`, `ARM_*`, `POLE_*`) halfway between the two frames' values, at the second frame; if the part's curve already has a key there, move the next key 1 frame later | one shot | not across the snap | pre-flight 6 |
| R19 `velocity reversals back and forth` (head/chest) | Check the lag code first (a switch between lagged and unlagged values is a bug) → **escalate**; otherwise smooth that part's lag (`HEAD_LAG`/`CHEST_LAG` +0.3) | 0.3 frames | ≤ 2 steps | pre-flight 6 |
| R20 `straight paths` (relative to the root): hand | Add an "A" key at the middle frame of the window on the circle about the shoulder: same length as the neighbouring keys (normalise the midpoint to their mean length) | one shot | — | pre-flight 6 |
| R21 `straight paths`: foot/ball (in the air) | `ANK` key at the middle frame: midpoint + 7 cm down (the knee folds) | 7 cm | ≤ 2 steps | pre-flight 6 |
| R22 `straight paths`: head | `PEL_Y` dip of −1.5 cm two frames after the window start, on two-foot frames only | 1.5 cm | max −5 total; no dip on one-foot frames (R8) | pre-flight 2, 6 |
| R23 `turning zig-zag` | Remove the middle one of three keys on that part's curve inside the window (the only allowed deletion) | — | not beats | pre-flight 6 |
| R24 `spacing too even` / `too little of the path in the last 2 frames` / `speed peak not into contact` | Wind-up keys (`ANK` at contact − 2, contact − 1): pull them 3 cm back along the attack | 3 cm | max total 9 cm; never the contact key | pre-flight 4 |
| R25 `slow out of the hold` | The first recovery key after the hold: move 15% of its distance toward the next key (it leaves faster) | 15% | ≤ 2 steps | pre-flight 4 |
| R26 `no slow-in to the guard` | Add a key at arrival − 1 at 85% of the way from arrival − 2 to the arrival pose (ease in) | one shot | — | pre-flight 4 |
| R27 overlap: `peaks out of order` / `peak together` | `CHEST_LAG` +0.3, `HEAD_LAG` +0.5 | as given | CHEST_LAG ≤ 2, HEAD_LAG ≤ 4 | pre-flight 5 |
| R28 `simultaneous arrivals` | The latest-arriving part's lag +0.5 (head, then arms) | 0.5 | as R27 | pre-flight 5 |
| R29 `twinning` | The rear arm's "A" key at that frame: rotate 10° about the vertical | 10° | ≤ 2 steps | pre-flight 6 |
| R30 `head not toward the target` | `CHEST_YAW` at the flagged frames: 5° toward the target | 5° | ≤ 3 steps | pre-flight 6 |
| R31 `planted foot slides` | The support foot: `PIVOT` key at the flagged frames → the previous key's value (stop the pivot) | one shot | — | pre-flight 6 |
| R32 `linear motion (constant speed over 4 frames)` | On that part's curve, add a key at the window's middle frame, moved 20% of a step back toward the window start (ease out of it), or forward when the window ends in a stop | 20% of a step | ≤ 2 steps; not across the snap | pre-flight 4 |
| R33 `no landing absorption` | `PEL_Y`: add a key 2 frames after the landing at (value there − 1.5) cm, on two-foot frames only | 1.5 cm | max −5 total; the support knee on one foot stays under R8 | pre-flight 2, 4 |

## Unreal-side checks (after the pass)

| Flag | Rule |
|---|---|
| clearance < 2 cm (arm/torso) | the arm's "A" key at that frame: +2 cm outward (third value, sign by side); legs: **escalate** |
| legibility: attacking part hidden / rear arm over it | **escalate** (pose design) |
| tangency (report): one pair in ≥ 3 game views at a key frame | the limb's "A"/`ANK` key: 3 cm away from the other part, perpendicular to the view; else **escalate** |
| wrist flexion/deviation, frozen fist | wrist_fix pass; **escalate** if it remains |
| joints (ankle, knee over toes, foot roll) | **escalate** |
| motion_check weight / hold / recoil | weight: R14; the rest: **escalate** |
| any pre-flight rule's flag | same rule as above |

## Escalate (always)

- A flag with no rule here, or a rule marked **escalate**.
- The same flag comes back after 2 applications of its rule.
- A fix makes another check fail that passed before (revert the fix first).
- A change past a rule's "max total", or on a pose-design parameter (big pose change).
- The model throws an exception, or the pre-flight takes more than 2 s.
