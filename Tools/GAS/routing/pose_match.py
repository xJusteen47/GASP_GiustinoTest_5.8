"""Is the visible in-game pose our clip? (routing fix, 2026-11-19; plain Python, outside the editor)
Compares a capture's per-frame body (frames.jsonl 'body': bones relative to the actor) with a clip's bone tracks (clip_tracks.json from the
editor, 60 Hz, component space) on rotation-free numbers: pelvis / head / hands height over the root, horizontal foot spread.
For every alignment offset (game frame v <-> clip frame v - offset) over a window it reports the RMS error in cm; the best offset and its
error, plus the same for a reference clip (what GASP would show), go to stdout (max 5 lines).
  python pose_match.py <frames.jsonl> <clip_tracks.json> <clip> <reference clip> <from> <to>"""
import json
import math
import sys

def metrics_game(b):
    rz = b["root"][2]
    spread = math.hypot(b["foot_l"][0] - b["foot_r"][0], b["foot_l"][1] - b["foot_r"][1])
    return [b["pelvis"][2] - rz, b["head"][2] - rz, b["hand_l"][2] - rz, b["hand_r"][2] - rz, spread]

def metrics_clip(r):
    rz = r["root"][2]
    spread = math.hypot(r["foot_l"][0] - r["foot_r"][0], r["foot_l"][1] - r["foot_r"][1])
    return [r["pelvis"][2] - rz, r["head"][2] - rz, r["hand_l"][2] - rz, r["hand_r"][2] - rz, spread]

def best(game, clip, lo, hi):
    out = (1e9, None)
    frames = [f for f in sorted(game) if lo <= f <= hi]
    for off in range(lo - len(clip) + 1, hi + 1):
        errs = []
        for f in frames:
            i = f - off
            if 0 <= i < len(clip):
                errs.extend((a - b) ** 2 for a, b in zip(game[f], clip[i]))
        if len(errs) >= 5 * min(len(frames), len(clip)) * 0.8:
            rms = math.sqrt(sum(errs) / len(errs))
            if rms < out[0]:
                out = (rms, off)
    return out

def main():
    frames, tracks, name, ref, lo, hi = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6])
    game = {}
    for line in open(frames):
        if line.strip():
            r = json.loads(line)
            if r.get("body"):
                game[r["f"]] = metrics_game(r["body"])
    t = json.load(open(tracks))
    clip = [metrics_clip(r) for r in t[name]]
    rms, off = best(game, clip, lo, hi)
    print("%s: best RMS %.1f cm at offset %s (clip frame 0 = game v%s), window v%d-v%d" % (name, rms, off, off, lo, hi))
    if ref in t:
        refc = [metrics_clip(r) for r in t[ref]]
        rr, ro = best(game, refc, lo, hi)
        print("%s (reference): best RMS %.1f cm at offset %s" % (ref, rr, ro))

if __name__ == "__main__":
    main()
