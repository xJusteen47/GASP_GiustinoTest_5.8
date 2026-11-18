"""Director Strip & Onion Generator for Overlord Jab 1 & Jab 2.

Usage:
    python cards/overlord/jab/director/strip_jab.py <variant_folder>
"""
import json
import math
import os
import sys
import numpy as np

# Ensure Tools/AnimPipeline is importable
HERE = os.path.dirname(os.path.abspath(__file__))
PIPELINE_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
if PIPELINE_ROOT not in sys.path:
    sys.path.insert(0, PIPELINE_ROOT)

from animpipe import author as A

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle


# Color definitions
COL_TORSO = "#1a252f"
COL_RIGHT_ARM = "#d32f2f"  # Red
COL_LEFT_ARM = "#1976d2"   # Blue
COL_LEFT_LEG = "#2c3e50"   # Dark grey
COL_RIGHT_LEG = "#7f8c8d"  # Lighter grey


def solve_clip(poser, card_path):
    card = json.load(open(card_path))
    au = card["author"]
    frame_dict = A.frames(au)
    root = card.get("root") or []
    solved = {}
    for f in sorted(frame_dict.keys()):
        P, R = poser.solve(frame_dict[f])
        rf = root[f] if f < len(root) else (root[-1] if root else 0.0)
        # Apply optional root list (cm forward per frame, add to Y axis)
        P_world = {k: np.array([v[0], v[1] + rf, v[2]], dtype=float) for k, v in P.items()}
        solved[f] = (P_world, R, frame_dict[f])
    return card, solved


def calc_metrics(solved):
    """Calculate max planted-foot drift in cm and min knee angle in degrees."""
    # Foot drift: a foot counts as planted when its height is within 1.5 cm of its minimum
    drifts = []
    for s in ("foot_l", "foot_r"):
        zs = [solved[f][0][s][2] for f in sorted(solved.keys())]
        min_z = min(zs)
        # Group contiguous planted frames
        runs = []
        cur = []
        for f in sorted(solved.keys()):
            if solved[f][0][s][2] <= min_z + 1.5:
                cur.append(f)
            else:
                if len(cur) > 1:
                    runs.append(cur)
                cur = []
        if len(cur) > 1:
            runs.append(cur)

        for r in runs:
            p0 = solved[r[0]][0][s]
            d = max(float(np.linalg.norm(solved[f][0][s] - p0)) for f in r)
            drifts.append(d)

    max_drift = max(drifts) if drifts else 0.0

    # Min knee angle across all frames and both knees
    min_knee = 999.0
    for f in sorted(solved.keys()):
        P = solved[f][0]
        for s in ("r", "l"):
            u = P[f"thigh_{s}"] - P[f"calf_{s}"]
            v = P[f"foot_{s}"] - P[f"calf_{s}"]
            cos_a = np.dot(u, v) / (np.linalg.norm(u) * np.linalg.norm(v))
            ang = math.degrees(math.acos(np.clip(cos_a, -1.0, 1.0)))
            if ang < min_knee:
                min_knee = ang

    return max_drift, min_knee


def draw_figure(ax, P, R, poser, view_mode="side", alpha=1.0, is_active=False, is_onion=False):
    """Draw thick limb stick figure in SIDE, BACK, or TOP projection.
    
    view_mode:
        'side': (forward to right -> horizontal = y, vertical = z)
        'back': (camera behind character, right on image right -> horizontal = -x, vertical = z)
        'top':  (forward up, right on image right -> horizontal = -x, vertical = y)
    """
    if view_mode == "side":
        pj = lambda p: (p[1], p[2])
    elif view_mode == "back":
        pj = lambda p: (-p[0], p[2])
    elif view_mode == "top":
        pj = lambda p: (-p[0], p[1])
    else:
        raise ValueError(f"Unknown view_mode: {view_mode}")

    lw_scale = 1.3 if is_active else (0.8 if is_onion else 1.0)

    # In side view, far limbs (right) behind torso, near limbs (left) in front.
    # In back/top view, draw legs then torso then arms.
    chains = [
        (COL_RIGHT_LEG, ["thigh_r", "calf_r", "foot_r", "ball_r"], 4.2 * lw_scale),
        (COL_LEFT_LEG, ["thigh_l", "calf_l", "foot_l", "ball_l"], 4.8 * lw_scale),
        (COL_TORSO, ["pelvis"] + A.SPINE + A.NECK, 5.0 * lw_scale),
        (COL_RIGHT_ARM, ["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"], 3.8 * lw_scale),
        (COL_LEFT_ARM, ["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"], 4.2 * lw_scale),
    ]

    if view_mode == "side":
        draw_order = [0, 3, 2, 1, 4]  # right leg, right arm, torso, left leg, left arm
    else:
        draw_order = [0, 1, 2, 3, 4]

    for idx in draw_order:
        col, ch, lw = chains[idx]
        pts = [pj(P[b]) for b in ch if b in P]
        if len(pts) > 1:
            ax.plot([p[0] for p in pts], [p[1] for p in pts], color=col, lw=lw,
                    alpha=alpha, solid_capstyle="round", solid_joinstyle="round")

    # Pelvis indicator
    pp = pj(P["pelvis"])
    ax.add_patch(Circle(pp, 3.8 * lw_scale, color=COL_TORSO, alpha=alpha, zorder=4))

    # Foot soles (ankle to ball)
    for s, cfoot in (("r", COL_RIGHT_LEG), ("l", COL_LEFT_LEG)):
        if f"foot_{s}" in P and f"ball_{s}" in P:
            p_ankle = pj(P[f"foot_{s}"])
            p_ball = pj(P[f"ball_{s}"])
            ax.plot([p_ankle[0], p_ball[0]], [p_ankle[1], p_ball[1]], color=cfoot,
                    lw=3.2 * lw_scale, alpha=alpha, solid_capstyle="round")

    # Head circle & nose gaze vector
    if "head" in P and "head" in R:
        head_center = P["head"] + R["head"] @ poser.R0["head"].T @ np.array([0, 2.0, 8.5])
        hc = pj(head_center)
        r_head = 7.0 * lw_scale
        ax.add_patch(Circle(hc, r_head, facecolor="#ecf0f1", edgecolor=COL_TORSO,
                            lw=1.8 * lw_scale, alpha=alpha, zorder=5))
        if view_mode in ("side", "top"):
            gaze_tip = head_center + R["head"] @ poser.R0["head"].T @ np.array([0, 14.0, 0])
            gt = pj(gaze_tip)
            ax.plot([hc[0], gt[0]], [hc[1], gt[1]], color=COL_TORSO, lw=2.0 * lw_scale,
                    alpha=alpha, zorder=6)


def render_strip(solved, max_frame, active_range, out_path, poser):
    """Render strip_jab{N}.png: frames 0..max_frame with 3 rows (SIDE, BACK, TOP) per wrap-row."""
    frame_keys = [f for f in range(max_frame + 1) if f in solved]
    nframes = len(frame_keys)
    ncols = 16
    wrap_rows = math.ceil(nframes / ncols)
    total_grid_rows = wrap_rows * 3

    # Cell size ~165px -> at dpi 100, figsize width 26.4 inches (<2700 px)
    fig_w = 26.4
    fig_h = 1.7 * total_grid_rows
    fig, axs = plt.subplots(total_grid_rows, ncols, figsize=(fig_w, fig_h), squeeze=False)

    # Compute fixed axes bounds across all frames to make world travel visible
    all_x = [pos[0] for f in frame_keys for pos in solved[f][0].values()]
    all_y = [pos[1] for f in frame_keys for pos in solved[f][0].values()]
    all_z = [pos[2] for f in frame_keys for pos in solved[f][0].values()]

    # Fixed limits with margins
    xlim_side = (min(all_y) - 15, max(all_y) + 15)
    ylim_z = (-10, max(all_z) + 15)
    xlim_horiz = (-max(all_x) - 15, -min(all_x) + 15)
    ylim_top = (min(all_y) - 15, max(all_y) + 15)

    views = [
        ("side", xlim_side, ylim_z, True),
        ("back", xlim_horiz, ylim_z, True),
        ("top", xlim_horiz, ylim_top, False),
    ]

    for idx in range(wrap_rows * ncols):
        w = idx // ncols
        c = idx % ncols

        if idx < nframes:
            f = frame_keys[idx]
            P, R, _ = solved[f]
            is_active = (active_range[0] <= f <= active_range[1])

            for v_idx, (v_name, xlim, ylim, has_ground) in enumerate(views):
                r = w * 3 + v_idx
                ax = axs[r][c]

                if has_ground:
                    ax.axhline(0, color="#7f8c8d", lw=1.2, ls="-", zorder=1)

                draw_figure(ax, P, R, poser, view_mode=v_name, alpha=1.0, is_active=is_active)

                # Title & frame number
                if v_idx == 0:
                    tag = f"f{f}"
                    bg = "#e74c3c" if is_active else "#2c3e50"
                    ax.set_title(tag, fontsize=8.5, fontweight="bold", color="white",
                                 backgroundcolor=bg, pad=2)
                elif c == 0:
                    ax.set_ylabel(v_name.upper(), fontsize=7.5, fontweight="bold", color="#7f8c8d")

                ax.set_xlim(xlim)
                ax.set_ylim(ylim)
                ax.set_aspect("equal")
                ax.set_xticks([])
                ax.set_yticks([])

                # Border: coloured for active frames
                for spine in ax.spines.values():
                    spine.set_color("#e74c3c" if is_active else "#bdc3c7")
                    spine.set_linewidth(2.2 if is_active else 0.8)
        else:
            for v_idx in range(3):
                axs[w * 3 + v_idx][c].axis("off")

    fig.tight_layout(pad=0.5)
    fig.savefig(out_path, dpi=100)
    plt.close(fig)


def render_onion(solved, frame_range, active_range, out_path, poser, title=""):
    """Render onion_jab{N}.png: BACK and TOP panels with right hand, pelvis, head paths."""
    fig, (ax_back, ax_top) = plt.subplots(1, 2, figsize=(15.0, 8.5))

    onion_frames = [f for f in range(frame_range[0], frame_range[1] + 1) if f in solved]

    # Ground line on back view
    ax_back.axhline(0, color="#7f8c8d", lw=1.2, ls="--", label="Ground (z=0)")

    # Draw figures
    for f in onion_frames:
        P, R, _ = solved[f]
        is_act = (active_range[0] <= f <= active_range[1])
        alpha = 1.0 if is_act else 0.35
        draw_figure(ax_back, P, R, poser, view_mode="back", alpha=alpha, is_active=is_act, is_onion=True)
        draw_figure(ax_top, P, R, poser, view_mode="top", alpha=alpha, is_active=is_act, is_onion=True)

    # Extract trajectory paths: right hand, pelvis, head
    hand_pts = [solved[f][0]["hand_r"] for f in onion_frames]
    pel_pts = [solved[f][0]["pelvis"] for f in onion_frames]
    head_pts = [solved[f][0]["head"] for f in onion_frames]

    # Back panel: horizontal = -x, vertical = z
    ax_back.plot([-p[0] for p in hand_pts], [p[2] for p in hand_pts], "-o",
                 color="#d32f2f", lw=2.5, ms=5, label="Right Hand", zorder=10)
    ax_back.plot([-p[0] for p in pel_pts], [p[2] for p in pel_pts], "-s",
                 color="#1976d2", lw=2.2, ms=5, label="Pelvis", zorder=9)
    ax_back.plot([-p[0] for p in head_pts], [p[2] for p in head_pts], "-^",
                 color="#27ae60", lw=2.0, ms=5, label="Head", zorder=8)

    # Top panel: horizontal = -x, vertical = y
    ax_top.plot([-p[0] for p in hand_pts], [p[1] for p in hand_pts], "-o",
                color="#d32f2f", lw=2.5, ms=5, label="Right Hand", zorder=10)
    ax_top.plot([-p[0] for p in pel_pts], [p[1] for p in pel_pts], "-s",
                color="#1976d2", lw=2.2, ms=5, label="Pelvis", zorder=9)
    ax_top.plot([-p[0] for p in head_pts], [p[1] for p in head_pts], "-^",
                color="#27ae60", lw=2.0, ms=5, label="Head", zorder=8)

    # Annotate frame numbers on points
    for idx, f in enumerate(onion_frames):
        if f in (onion_frames[0], onion_frames[-1]) or (active_range[0] <= f <= active_range[1]):
            ax_back.annotate(f"f{f}", (-hand_pts[idx][0], hand_pts[idx][2]),
                             textcoords="offset points", xytext=(0, 6), fontsize=8,
                             fontweight="bold", color="#d32f2f", ha="center")
            ax_top.annotate(f"f{f}", (-hand_pts[idx][0], hand_pts[idx][1]),
                            textcoords="offset points", xytext=(0, 6), fontsize=8,
                            fontweight="bold", color="#d32f2f", ha="center")

    ax_back.set_title(f"BACK View Onion Skin ({title})", fontsize=11, fontweight="bold")
    ax_back.set_xlabel("Lateral (-X: Character's Right ->, cm)", fontsize=9.5)
    ax_back.set_ylabel("Height (Z, cm)", fontsize=9.5)
    ax_back.set_aspect("equal")
    ax_back.grid(True, linestyle=":", alpha=0.5)
    ax_back.legend(loc="lower left", fontsize=8.5)

    ax_top.set_title(f"TOP View Onion Skin ({title})", fontsize=11, fontweight="bold")
    ax_top.set_xlabel("Lateral (-X: Character's Right ->, cm)", fontsize=9.5)
    ax_top.set_ylabel("Forward (Y, cm)", fontsize=9.5)
    ax_top.set_aspect("equal")
    ax_top.grid(True, linestyle=":", alpha=0.5)
    ax_top.legend(loc="lower left", fontsize=8.5)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    plt.close(fig)


def process_variant(variant_folder):
    variant_dir = os.path.abspath(variant_folder)
    if not os.path.isdir(variant_dir):
        # Try relative to script directory
        alt = os.path.join(HERE, variant_folder)
        if os.path.isdir(alt):
            variant_dir = alt
        else:
            print(f"Error: directory '{variant_folder}' not found.")
            sys.exit(1)

    poser = A.Poser("along")

    configs = [
        ("overlord_jab1.json", 1, 26, (6, 8), (2, 12)),
        ("overlord_jab2.json", 2, 30, (9, 11), (4, 15)),
    ]

    for fname, jab_num, max_f, active_range, onion_range in configs:
        card_path = os.path.join(variant_dir, fname)
        if not os.path.isfile(card_path):
            continue

        card, solved = solve_clip(poser, card_path)

        # Print ONE line: max planted-foot drift and min knee angle
        drift, min_knee = calc_metrics(solved)
        print(f"{fname}: max planted-foot drift {drift:.1f} cm, min knee angle {min_knee:.1f} deg")

        # Strip render
        out_strip = os.path.join(variant_dir, f"strip_jab{jab_num}.png")
        render_strip(solved, max_f, active_range, out_strip, poser)

        # Onion render
        out_onion = os.path.join(variant_dir, f"onion_jab{jab_num}.png")
        render_onion(solved, onion_range, active_range, out_onion, poser, title=f"Jab {jab_num} f{onion_range[0]}-f{onion_range[1]}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python cards/overlord/jab/director/strip_jab.py <variant_folder>")
        sys.exit(1)

    process_variant(sys.argv[1])


if __name__ == "__main__":
    main()
