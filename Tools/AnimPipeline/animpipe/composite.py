"""animpipe composite: the ONE review image of `render` (review.md "Input", rubric.md panels). Pure offline: bones come from
the cache of the built take (sheets/raw/<take>_legbones.json, component space X left, Y forward, Z up, cm) and from the
owner's bone files; no editor, no per-frame files. 2D orthographic views: "game" = the Duel side camera (Y forward, Z up),
"behind" = X (left) vs Z. Panels, top to bottom:
  1 side grid: every frame, arms, legs, trunk and head in different colours; on owner keys the owner's pose is a grey
    ghost under it (owner bones, moved so the pelvis coincides with the built pelvis)
  2 key poses (card key frames): game camera row and behind row, ghost on owner keys
  3 trails: hand / foot / head dots per frame in game and behind view (hitbox frames ringed, hitstop on the first one)
    and the per-frame step length (cm) per part with the hitbox window shaded
Width is exactly WIDTH px (the long side)."""
import json, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

WIDTH = 1568
GREY = "#b8b8b8"
PART_COL = {"trunk": "#222222", "arm_l": "#d62728", "arm_r": "#1f77b4", "leg_l": "#ff7f0e", "leg_r": "#2ca02c", "head": "#9467bd"}
CHAINS = {"trunk": ("pelvis", "spine_01", "spine_03", "spine_05", "neck_01"),
          "arm_l": ("spine_05", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "middle_01_l"),
          "arm_r": ("spine_05", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "middle_01_r"),
          "leg_l": ("pelvis", "thigh_l", "calf_l", "foot_l", "ball_l"),
          "leg_r": ("pelvis", "thigh_r", "calf_r", "foot_r", "ball_r")}
VIEWS = {"game": (1, 2), "behind": (0, 2)}           # (horizontal, vertical) component-space axes
TRAILS = {"hand_l": "#d62728", "hand_r": "#1f77b4", "foot_l": "#ff7f0e", "foot_r": "#2ca02c", "head": "#9467bd"}


def grid_shape(n, per_row=11):
    return (n + per_row - 1) // per_row, min(n, per_row)


def owner_ghosts(c, base):
    """{frame: {bone: xyz}} of the owner's pose on his keys (poses with origin owner); base = repo root that
    source.file paths are relative to."""
    from animpipe.cascadeur import load_bones_json
    out, cache = {}, {}
    for p in c["poses"]:
        s = p.get("source")
        if p.get("origin") != "owner" or not s or not str(s.get("file", "")).endswith(".json"):
            continue
        f = os.path.join(base, s["file"])
        if f not in cache:
            cache[f] = load_bones_json(f) if os.path.exists(f) else {}
        b = cache[f].get(int(s.get("frame", p["frame"])))
        if b:
            out[int(p["frame"])] = {k: np.array(v, float) for k, v in b[0].items()}
    return out


def _xyz(fr):
    return {b: np.array(v[:3], float) for b, v in fr.items()}


def shift_to_pelvis(ghost, built):
    """The owner pose relative to its pelvis, placed on the built pelvis."""
    if "pelvis" not in ghost or "pelvis" not in built:
        return ghost
    d = built["pelvis"] - ghost["pelvis"]
    return {b: p + d for b, p in ghost.items()}


def draw_figure(ax, bones, view, grey=False, lw=1.6):
    i, j = VIEWS[view]
    for part, ch in CHAINS.items():
        ch = [b for b in ch if b in bones]
        if len(ch) > 1:
            ax.plot([bones[b][i] for b in ch], [bones[b][j] for b in ch], color=GREY if grey else PART_COL[part],
                    lw=lw + (1.4 if grey else 0), solid_capstyle="round", zorder=1 if grey else 3)
    if "head" in bones and "neck_01" in bones:
        h, n = bones["head"], bones["neck_01"]
        col = GREY if grey else PART_COL["head"]
        ax.plot([n[i], h[i]], [n[j], h[j]], color=col, lw=lw + 1.4, zorder=1 if grey else 4)
        ax.plot(h[i], h[j], "o", ms=6 if grey else 5, color=col, zorder=1 if grey else 4)


def _limits(F, ghosts, view, pad=14):
    i, j = VIEWS[view]
    a = np.array([p for fr in list(F.values()) + list(ghosts.values()) for p in fr.values()])
    return (a[:, i].min() - pad, a[:, i].max() + pad), (a[:, j].min() - pad, a[:, j].max() + pad)


def _cell(ax, view, fr, ghost, lim, title, tcol="k"):
    if ghost is not None:
        draw_figure(ax, ghost, view, grey=True)
    draw_figure(ax, fr, view)
    ax.set_xlim(*lim[0]); ax.set_ylim(*lim[1]); ax.set_aspect("equal"); ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_color("#cccccc")
    ax.set_title(title, fontsize=6.5, pad=1.5, color=tcol, fontweight="bold" if tcol != "k" else "normal")


def trail_steps(F, bone):
    """Step length (cm) from frame f-1 to f for a bone (0 at the first frame)."""
    p = np.array([F[f][bone] for f in sorted(F)])
    return np.r_[0.0, np.linalg.norm(np.diff(p, axis=0), axis=1)]


def render(F, ghosts, keys, active, hitstop, title, out):
    """F = {frame: {bone: xyz}} (built). Writes the PNG at WIDTH px wide; returns (w, h)."""
    from PIL import Image
    n = len(F)
    rows, cols = grid_shape(n)
    ghosts = {f: shift_to_pelvis(g, F[f]) for f, g in ghosts.items() if f in F}
    W = WIDTH
    cellw = W / 11
    h_grid, h_key, h_tr = rows * 148 + 24, 2 * 175 + 26, 330
    H = h_grid + h_key + h_tr + 30
    fig = plt.figure(figsize=(W / 100, H / 100), dpi=100)
    fig.text(0.006, 1 - 10 / H, title, fontsize=8, va="center", fontweight="bold")
    fig.text(0.50, 1 - 10 / H, "grey = owner pose (ghost) | red/blue = left/right arm | orange/green = left/right leg | black = trunk | violet = head",
             fontsize=6.3, va="center")
    lim = _limits(F, ghosts, "game")
    lim_b = _limits(F, ghosts, "behind")
    for k, f in enumerate(sorted(F)):
        r, ci = divmod(k, cols)
        ax = fig.add_axes([(ci * cellw) / W, 1 - (24 + (r + 1) * 148) / H, (cellw - 3) / W, 140 / H])
        hit = active[0] <= f <= active[1]
        tag = f"{f}" + (" OWNER" if f in ghosts else "") + (" HIT" if hit else "")
        _cell(ax, "game", F[f], ghosts.get(f), lim, tag, "#c0392b" if hit else ("#1a5fb4" if f in ghosts else "k"))
    ky = [f for f in keys if f in F]
    kw = W / max(len(ky), 1)
    fig.text(0.006, 1 - (h_grid + 8) / H, "KEY POSES (card keys): game camera (side) above, from behind below", fontsize=6.8, va="center", color="#444")
    for k, f in enumerate(ky):
        for r, view in enumerate(("game", "behind")):
            top = h_grid + 16 + r * 175
            ax = fig.add_axes([(k * kw) / W, 1 - (top + 165) / H, (kw - 3) / W, 150 / H])
            _cell(ax, view, F[f], ghosts.get(f), lim if view == "game" else lim_b,
                  f"key {f} {'game cam' if view == 'game' else 'behind'}", "#1a5fb4" if f in ghosts else "k")
    top = h_grid + h_key + 10
    pw = W / 3
    act = [f for f in sorted(F) if active[0] <= f <= active[1]]
    for k, view in enumerate(("game", "behind")):
        ax = fig.add_axes([(k * pw + 28) / W, 1 - (top + h_tr - 20) / H, (pw - 40) / W, (h_tr - 40) / H])
        i, j = VIEWS[view]
        for b, col in TRAILS.items():
            p = np.array([F[f][b] for f in sorted(F)])
            ax.plot(p[:, i], p[:, j], color=col, lw=0.8, alpha=0.6)
            ax.scatter(p[:, i], p[:, j], s=5, color=col)
            ax.scatter([F[f][b][i] for f in act], [F[f][b][j] for f in act], s=42, facecolors="none", edgecolors="k", lw=1)
        if active[0] in F:
            q = F[active[0]]["hand_l"]
            ax.annotate(f"hitstop x{hitstop} at f{active[0]}", (q[i], q[j]), xytext=(6, 10), textcoords="offset points",
                        fontsize=6.5, arrowprops=dict(arrowstyle="-", lw=0.6))
        ax.set_aspect("equal"); ax.tick_params(labelsize=5.5); ax.grid(alpha=0.25)
        ax.set_title(f"TRAILS {'game camera' if view == 'game' else 'from behind'}: dot per frame, ring = hitbox", fontsize=6.8)
    ax = fig.add_axes([(2 * pw + 30) / W, 1 - (top + h_tr - 20) / H, (pw - 42) / W, (h_tr - 40) / H])
    for b, col in TRAILS.items():
        ax.plot(sorted(F), trail_steps(F, b), color=col, lw=1, marker="o", ms=2.2, label=b)
    ax.axvspan(active[0] - 0.5, active[1] + 0.5, color="#f4c7c3", alpha=0.6)
    ax.axvline(active[0], color="k", lw=0.8, ls="--")
    for f in keys:
        ax.axvline(f, color="#999", lw=0.3)
    ax.text(active[0] + 0.3, ax.get_ylim()[1] * 0.92, f"hitbox, hitstop x{hitstop}", fontsize=6.5)
    ax.set_title("STEP per frame (cm): hitbox shaded, thin lines = keys", fontsize=6.8)
    ax.tick_params(labelsize=5.5); ax.legend(fontsize=5.5, ncol=3, loc="upper left"); ax.grid(alpha=0.25)
    fig.savefig(out, dpi=100)
    plt.close(fig)
    return Image.open(out).size


def build(c, card_sha, base, tools, out, hitstop=8):
    """Bone cache of the built take + the owner files named by the card -> `out`; returns (w, h, info)."""
    from animpipe.card import key_frames, windows
    take = c["anim"].rsplit("/", 1)[1][3:]
    p = os.path.join(tools, "sheets", "raw", f"{take}_legbones.json")
    if not os.path.exists(p):
        raise FileNotFoundError(f"no bone cache {p}: run check first")
    F = {int(f): _xyz(v) for f, v in json.load(open(p))["frames"].items()}
    active = windows(c)["anim"]["active"]
    w, h = render(F, owner_ghosts(c, base), key_frames(c), active, hitstop, f"{c['move']}  ({take}, {len(F)} frames, card {card_sha[:8]})", out)
    return w, h, {"active": active, "hitstop": hitstop, "frames": len(F)}
