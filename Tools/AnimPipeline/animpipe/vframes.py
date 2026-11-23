"""Video frames: the owner's numbers. The owner counts frames of the in-game video (mp4, 60 fps, first frame = 0, the number
printed as `v<N>` in every video and grid); the card counts anim frames (`poses[].frame`). They differ by the hitstop freeze:
the video repeats the first active frame `hit` times (ingame_judge.py), so

    video v <= a0            -> anim v
    a0 < v <= a0 + hit       -> anim a0 (frozen)
    v > a0 + hit             -> anim v - hit        (the whiff video has no freeze: v == anim)

a0 = first contact frame (card active window, or --at), hit = hitstop frames (--hitstop, default 8: the review default).
Pure functions, no engine: `frame_map` is what every timeline command prints before it acts.
"""
HITSTOP = 8


def params(c, o=None):
    """(a0, hit) from the options (--at, --hitstop) and the card's active window; a0 None if the window is unknown."""
    o = o or {}
    hit = int(o.get("hitstop") or HITSTOP)
    if o.get("at"):
        return int(o["at"]), hit
    try:
        from animpipe.card import windows
        return int(windows(c)["anim"]["active"][0]), hit
    except (OSError, KeyError, IndexError, ValueError):
        return None, hit


def video_to_anim(v, a0, hit):
    if a0 is None or v <= a0:
        return v
    return a0 if v <= a0 + hit else v - hit


def anim_to_video(f, a0, hit):
    """First video frame that shows anim frame f."""
    return f if a0 is None or f <= a0 else f + hit


def parse(text):
    """'13,14' | 'v13, v14' -> [13, 14]"""
    return [int(x.strip().lstrip("vV")) for x in str(text).split(",") if x.strip()]


def frame_map(c, anim_frames, a0, hit, owner=()):
    """One row per requested anim frame: {video, anim, frozen, key, owner_key, segment: [prev key, next key]}.
    A frame on a key touches that key; a frame between keys touches the segment between its neighbours."""
    keys = sorted(p["frame"] for p in c["poses"])
    rows = []
    for f in anim_frames:
        prev = max([k for k in keys if k <= f], default=None)
        nxt = min([k for k in keys if k >= f], default=None)
        rows.append({"video": anim_to_video(f, a0, hit), "anim": f, "frozen": a0 is not None and f == a0 and hit > 0,
                     "key": f in keys, "owner_key": f in set(owner), "segment": [prev, nxt]})
    return rows


def line(r):
    """'v13 = anim 13: between keys 11 and 14' (one line per row)"""
    what = ("owner KEY" if r["owner_key"] else "key") + f" {r['anim']}" if r["key"] else \
        f"between keys {r['segment'][0]} and {r['segment'][1]}"
    return f"v{r['video']} = anim {r['anim']}: {what}" + (" (hitstop freeze starts here)" if r["frozen"] else "")


def to_anim_list(frames, a0, hit):
    """Video frames -> anim frames; a frame inside the freeze is refused (it has no frame of its own)."""
    out = []
    for v in frames:
        if a0 is not None and a0 < v <= a0 + hit:
            raise ValueError(f"video frame {v} is inside the hitstop freeze (v{a0 + 1}..v{a0 + hit} all show anim {a0}); "
                             f"say v{a0} or v{a0 + hit + 1}")
        out.append(video_to_anim(v, a0, hit))
    return out
