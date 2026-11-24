"""Regenerate the stock run-start / run-loop bone dumps in Cascadeur Y-up space.

Writes:
    Saved/AnimPipeline/runloop/startL_stock_dump.npz
    Saved/AnimPipeline/runloop/loop_relaxed_dump.npz

from the UE mannequin AnimSequences. Uses pure Python stdlib (math, struct, zipfile)
so it runs inside UE's embedded Python without third-party numpy installed, while
writing standard NumPy .npz v1.0 archives readable by numpy.load().
Each key ``P|*`` is stored as an ``(n, 3)`` little-endian float64 array in Cascadeur
Y-up space ``[ue_x, ue_z, ue_y]``.
"""
from __future__ import annotations

import io
import math
import os
import shutil
import struct
import sys
import zipfile

try:
    import unreal
except ImportError:
    unreal = None


def ue_to_casc_pos(x: float, y: float, z: float) -> list[float]:
    """Convert a UE position (x, y, z) to Cascadeur Y-up space [x, z, y]."""
    return [float(x), float(z), float(y)]


def _vsub(a: list[float], b: list[float]) -> list[float]:
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def _vadd(a: list[float], b: list[float]) -> list[float]:
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def _vmul(a: list[float], s: float) -> list[float]:
    return [a[0] * s, a[1] * s, a[2] * s]


def _vdot(a: list[float], b: list[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _vcross(a: list[float], b: list[float]) -> list[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def _norm(v: list[float], fallback: list[float]) -> list[float]:
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if n < 1e-8:
        fn = math.sqrt(fallback[0] ** 2 + fallback[1] ** 2 + fallback[2] ** 2)
        if fn < 1e-8:
            return [0.0, 1.0, 0.0]
        return [fallback[0] / fn, fallback[1] / fn, fallback[2] / fn]
    return [v[0] / n, v[1] / n, v[2] / n]


def _save_npz(path: str, arrays: dict[str, list[list[float]]]) -> None:
    """Write a standard NumPy .npz archive (v1.0 '<f8' (N, 3) arrays) using stdlib."""
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for key, rows in arrays.items():
            n = len(rows)
            header_dict = f"{{'descr': '<f8', 'fortran_order': False, 'shape': ({n}, 3), }}"
            raw_hdr = header_dict.encode("latin1")
            # Total header size (10 + len) must be divisible by 64, ending with '\n'
            pad_len = (64 - ((10 + len(raw_hdr) + 1) % 64)) % 64
            hdr_bytes = raw_hdr + (b" " * pad_len) + b"\n"
            buf = io.BytesIO()
            buf.write(b"\x93NUMPY\x01\x00")
            buf.write(struct.pack("<H", len(hdr_bytes)))
            buf.write(hdr_bytes)
            for r in rows:
                buf.write(struct.pack("<ddd", float(r[0]), float(r[1]), float(r[2])))
            zf.writestr(f"{key}.npy", buf.getvalue())


def synthesize_helpers(pts: dict[str, list[list[float]]]) -> dict[str, list[list[float]]]:
    """Add the 10 helper P|* arrays (5 per side) derived from direct bone points."""
    helpers: dict[str, list[list[float]]] = {}
    for s in ("l", "r"):
        ball = pts[f"P|ball_MainPoint_{s}"]
        foot = pts[f"P|foot_MainPoint_{s}"]
        calf = pts[f"P|calf_MainPoint_{s}"]
        thigh = pts[f"P|thigh_MainPoint_{s}"]
        upperarm = pts[f"P|upperarm_MainPoint_{s}"]
        lowerarm = pts[f"P|lowerarm_MainPoint_{s}"]
        hand = pts[f"P|hand_MainPoint_{s}"]

        n = len(ball)
        dir_pts: list[list[float]] = []
        add_pts: list[list[float]] = []
        self0_pts: list[list[float]] = []
        knee_dirs: list[list[float]] = []
        elbow_dirs: list[list[float]] = []

        for i in range(n):
            fwd_hz = _norm([ball[i][0] - foot[i][0], 0.0, ball[i][2] - foot[i][2]], [0.0, 0.0, 1.0])
            dir_pts.append(_vadd(ball[i], _vmul(fwd_hz, 10.0)))

            lat = _norm(_vcross([0.0, 1.0, 0.0], fwd_hz), [1.0, 0.0, 0.0])
            foot_lat = lat if s == "l" else _vmul(lat, -1.0)
            add_pts.append(_vadd(ball[i], _vmul(foot_lat, 5.0)))

            s0 = _vsub(foot[i], _vmul(fwd_hz, 5.0))
            s0[1] = max(0.0, float(foot[i][1]) - 4.0)
            self0_pts.append(s0)

            vk = _vsub(calf[i], thigh[i])
            ak = _norm(_vsub(foot[i], thigh[i]), [0.0, -1.0, 0.0])
            pk = _norm(_vsub(vk, _vmul(ak, _vdot(vk, ak))), fwd_hz)
            knee_dirs.append(_vadd(calf[i], _vmul(pk, 45.0)))

            ve = _vsub(lowerarm[i], upperarm[i])
            ae = _norm(_vsub(hand[i], upperarm[i]), [0.0, -1.0, 0.0])
            pe = _norm(_vsub(ve, _vmul(ae, _vdot(ve, ae))), [0.0, 0.0, -1.0])
            elbow_dirs.append(_vadd(lowerarm[i], _vmul(pe, 35.0)))

        helpers[f"P|ball_DirectionPoint_{s}"] = dir_pts
        helpers[f"P|ball_AdditionalPoint_{s}"] = add_pts
        helpers[f"P|foot_Self0Point_{s}"] = self0_pts
        helpers[f"P|calf_LimbDir_{s}"] = knee_dirs
        helpers[f"P|lowerarm_LimbDir_{s}"] = elbow_dirs
    return helpers


_BONE_MAP = {
    "root": "root",
    "pelvis_MainPoint": "pelvis",
    "spine_02_MainPoint": "spine_02",
    "spine_04_MainPoint": "spine_04",
    "neck_01_MainPoint": "neck_01",
    "head_MainPoint": "head",
    "ik_foot_root": "ik_foot_root",
    "ik_hand_root": "ik_hand_root",
}
for _s in ("l", "r"):
    _BONE_MAP[f"clavicle_MainPoint_{_s}"] = f"clavicle_{_s}"
    _BONE_MAP[f"upperarm_MainPoint_{_s}"] = f"upperarm_{_s}"
    _BONE_MAP[f"lowerarm_MainPoint_{_s}"] = f"lowerarm_{_s}"
    _BONE_MAP[f"hand_MainPoint_{_s}"] = f"hand_{_s}"
    _BONE_MAP[f"thigh_MainPoint_{_s}"] = f"thigh_{_s}"
    _BONE_MAP[f"calf_MainPoint_{_s}"] = f"calf_{_s}"
    _BONE_MAP[f"foot_MainPoint_{_s}"] = f"foot_{_s}"
    _BONE_MAP[f"ball_MainPoint_{_s}"] = f"ball_{_s}"
    _BONE_MAP[f"ik_foot_{_s}"] = f"ik_foot_{_s}"


def sample_sequence(asset_path: str) -> dict[str, list[list[float]]]:
    """Sample a UE AnimSequence into 36 P|* Cascadeur Y-up arrays (n, 3)."""
    if unreal is None:
        raise RuntimeError("requires UE environment")

    seq = unreal.load_asset(asset_path)
    if seq is None:
        raise RuntimeError(f"could not load asset: {asset_path}")

    n = (
        int(seq.get_editor_property("number_of_sampled_frames"))
        if hasattr(seq, "number_of_sampled_frames")
        else unreal.AnimationLibrary.get_num_frames(seq) + 1
    )
    fps = 30.0
    if hasattr(seq, "get_sampling_frame_rate"):
        try:
            fr = seq.get_sampling_frame_rate()
            if fr and getattr(fr, "denominator", 1):
                fps = float(fr.numerator) / float(fr.denominator)
        except Exception:
            fps = 30.0

    opts = unreal.AnimPoseEvaluationOptions()
    if hasattr(opts, "should_retarget"):
        opts.should_retarget = False

    pts: dict[str, list[list[float]]] = {f"P|{name}": [] for name in _BONE_MAP}
    for f in range(n):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / fps, opts)
        for name, bone in _BONE_MAP.items():
            tr = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
            pts[f"P|{name}"].append(ue_to_casc_pos(float(tr.x), float(tr.y), float(tr.z)))

    pts.update(synthesize_helpers(pts))
    return pts


def main() -> int:
    if unreal is None:
        print("DUMP_STOCK ERROR: requires UE environment")
        return 2

    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    out_dir = os.environ.get("GSTN_DUMP_DIR") or os.path.join(proj, "Saved", "AnimPipeline", "runloop")
    os.makedirs(out_dir, exist_ok=True)

    dumps = {
        "startL_stock_dump.npz": "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot",
        "loop_relaxed_dump.npz": "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F",
    }

    written: list[str] = []
    key_count = 0
    for filename, asset_path in dumps.items():
        pts = sample_sequence(asset_path)
        key_count = len(pts)
        out_path = os.path.join(out_dir, filename)
        _save_npz(out_path, pts)
        written.append(out_path)
        print(f"DUMP_STOCK: wrote {out_path} keys={key_count}")

    mirror = os.environ.get("GSTN_DUMP_MIRROR")
    if mirror and os.path.abspath(mirror) != os.path.abspath(out_dir):
        os.makedirs(mirror, exist_ok=True)
        for filename in dumps:
            shutil.copy2(os.path.join(out_dir, filename), os.path.join(mirror, filename))

    print(f"DUMP_STOCK: OK out={out_dir} keys={key_count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
