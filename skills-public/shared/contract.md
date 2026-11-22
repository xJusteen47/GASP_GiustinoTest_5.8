# The reviewer/executor contract (animation)

Two roles, one file each way. The **reviewer** looks and writes one instruction; the **executor** applies it and
returns a result. Neither does the other's job. A project may replace this file with its own contract (its project
layer says so); field names below are the default.

## The cycle
1. Entry conditions: starting poses that are not invented from text (at least five key poses with frames), the
   timing landmarks of the move (startup, active frames, hit-pause if any, first actionable frame), and a baseline
   in-game video. Missing one: ask for it and stop.
2. Reviewer: video review → one instruction (`step-NN/instruction.json`).
3. Executor: snapshot → apply → gates → check → before/after picture → `step-NN/result.json`.
4. Reviewer reads the result, writes the next instruction or stops. About six instructions per cycle, then a fresh
   in-game capture and a full review.
Cycle folder: `<reviews>/<move>-c<N>/` with `start/`, `step-NN/`, `end/`, `cycle.md` (one line per step), `review.md`.

## The instruction: eight fields plus a header
```json
{
  "move": "<move id>", "cycle": 1, "step": 3, "attempt": 1,
  "level": "measured", "view": "behind",
  "do_not_worsen": ["side view, f9: the hand stays on the dummy's chest"],

  "key":       {"anim": 9, "video": 30, "exists": true, "span": [9, 12]},
  "type":      "pose",
  "part":      "arm_r",
  "control":   "<rig control>, character space",
  "intent":    "impact pose: the arm must leave the silhouette in the rear view",
  "change":    "hand ~12 cm further forward and 6 cm higher, wrist open outwards",
  "principle": "silhouette and line of action (<theory doc> §11)",
  "check":     {"text": "at f9, from behind, the hand is at least 10 cm outside the shoulder line",
                "request": null,
                "measure": "hand_r.x - upperarm_r.x at f9 (component space) <= -10 cm"}
}
```
| Field | Rule |
|---|---|
| `key` | Anim frame and video frame from the capture's frame map, never translated by hand; whether the key exists; the span affected. A locked key here makes it a proposal to the approver. |
| `type` | `curve` \| `timing` \| `tween` \| `pose` \| `add_key` \| `remove_key`, in the order to try them. `add_key` only with the reason the others are not enough. |
| `part` | One body part. Two parts are two instructions unless they move as one gesture (say so in `intent`). |
| `control` | Rig control and space. Mandatory at the `measured` level; at `principle` level may be "executor's choice". |
| `intent` | What the viewer must read. Never "better", "heavier", "more natural" alone. |
| `change` | `principle`: one sentence. `measured`: direction in character terms, cm or degrees. The smallest change. |
| `principle` | The principle and the theory section it comes from. |
| `check` | Mandatory: `text` plus a machine `request` or a `measure` on the bone dump. PASS or FAIL, not an opinion. |
Header: `level` (`principle` for motion between poses; `measured` for an exact pose, interpenetration, a failed gate,
and always on a second attempt), `view` (which view wins in a conflict), `do_not_worsen` (each line checked like
`check` when it has a number).

## The result (`result.json`)
`applied` (bool), `refusal` (or null), `changes` (control, tool, before, after, why), `keys_before`/`keys_after`,
`signals` (`key_every_frame`, `redundant_key`), gate lines, `check` PASS/FAIL with the measured value, `bones` (dump
values of the bones the instruction names, at the frames touched), paths, `note` (facts only, no opinions).

| Refusal | Meaning |
|---|---|
| `locked_key` | The change would move a key or part the approver locked. Quote frame and part. |
| `ambiguous` | A field is missing or can be read two ways. Name the field and both readings. Never guess. |
| `gate` | Applied in a trial, a blocking gate failed, the source was restored. Quote the failing line. |
| `format` | The source format cannot hold the change (e.g. per-control key, per-key tangent). |
| `needs_approver` | An app must be opened, or the approver edited the asset by hand and a build would erase it. |
| `out_of_role` | The instruction asks to judge, to choose, or to change startup, active frames or FAF. |
