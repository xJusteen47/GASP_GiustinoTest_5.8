"""Unified UE 5.9 headless geometric engine for gstn-anim-mcp (anim_apply_intent).

Reads GSTN_INTENT_JSON (written by anim_ops.anim_apply_intent), loads src AnimSequence,
applies pelvis/spine-anchored semantic deltas with smooth handoff blending, and saves dst.
"""
from __future__ import annotations

import json
import math
import os
import sys

import unreal

AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
FOLDER = "/Game/GSTN/Animation/RunStart"


def _resolve_asset(name_or_path: str) -> str:
    if name_or_path.startswith("/Game/"):
        return name_or_path
    return f"{FOLDER}/{name_or_path}"


def _smoothstep(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def _window_weight(f: int, f0: int, f1: int, smooth: tuple[int, int] | None) -> float:
    if f < f0 or f > f1:
        return 0.0
    if not smooth:
        return 1.0
    s0, s1 = smooth
    w = 1.0
    if s0 > f0 and f <= s0:
        w = min(w, _smoothstep((f - f0) / max(1, s0 - f0)))
    if s1 < f1 and f >= s1:
        w = min(w, _smoothstep((f1 - f) / max(1, f1 - s1)))
    return w


def _rot_quat(pitch_deg: float, yaw_deg: float, roll_deg: float) -> unreal.Quat:
    return unreal.Rotator(pitch=pitch_deg, yaw=yaw_deg, roll=roll_deg).quaternion()


def main() -> int:
    intent_path = os.environ.get("GSTN_INTENT_JSON", "")
    if not intent_path or not os.path.exists(intent_path):
        print(f"GEOM_INTENT ERROR: missing GSTN_INTENT_JSON={intent_path!r}")
        return 2

    payload = json.loads(open(intent_path, encoding="utf-8").read())
    src_path = _resolve_asset(payload["src"])
    dst_path = _resolve_asset(payload["dst"])
    intent = payload.get("intent", {})
    save = os.environ.get("GSTN_SAVE", "0") == "1"

    if not EAL.does_asset_exist(src_path):
        print(f"GEOM_INTENT ERROR: src not found {src_path}")
        return 2

    if src_path != dst_path:
        if EAL.does_asset_exist(dst_path):
            EAL.delete_asset(dst_path)
        if not EAL.duplicate_asset(src_path, dst_path):
            print(f"GEOM_INTENT ERROR: failed to duplicate {src_path} -> {dst_path}")
            return 2

    seq = unreal.load_asset(dst_path)
    n_frames = int(seq.get_editor_property("number_of_sampled_frames"))
    fr = intent.get("frame_range") or [0, n_frames - 1]
    f0, f1 = max(0, int(fr[0])), min(n_frames - 1, int(fr[1]))
    smooth = tuple(intent["handoff_smooth_range"]) if "handoff_smooth_range" in intent else None

    ctrl = seq.controller
    ctrl.open_bracket(unreal.Text("gstn_anim_intent"))
    try:
        def _apply_bone_delta(bone: str, dp: float = 0.0, dy: float = 0.0, dr: float = 0.0, dt: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> None:
            if abs(dp) < 1e-4 and abs(dy) < 1e-4 and abs(dr) < 1e-4 and all(abs(x) < 1e-4 for x in dt):
                return
            pos_keys, rot_keys, scl_keys = [], [], []
            for f in range(n_frames):
                tr = AL.get_bone_pose_for_frame(seq, bone, f, False)
                w = _window_weight(f, f0, f1, smooth)
                p = tr.translation
                q = tr.rotation
                if w > 0.0:
                    p = unreal.Vector(p.x + dt[0] * w, p.y + dt[1] * w, p.z + dt[2] * w)
                    dq = _rot_quat(dp * w, dy * w, dr * w)
                    q = (q * dq).normal()
                pos_keys.append(p)
                rot_keys.append(q)
                scl_keys.append(tr.scale3d)
            ctrl.set_bone_track_keys(bone, pos_keys, rot_keys, scl_keys)

        # Pelvis CoM shift forward (Y-fwd in UE mannequin space)
        com_fwd = float(intent.get("com_shift_fwd_cm", 0.0))
        if abs(com_fwd) > 1e-4:
            _apply_bone_delta("pelvis", dt=(0.0, com_fwd, 0.0))

        # Spine-anchored torso lean & bank
        lean = float(intent.get("torso_lean_pivot_deg", 0.0))
        bank = float(intent.get("torso_bank_deg", 0.0))
        if abs(lean) > 1e-4 or abs(bank) > 1e-4:
            for sp in ("spine_01", "spine_02", "spine_03"):
                _apply_bone_delta(sp, dp=lean / 3.0, dr=bank / 3.0)

        # Shoulder level compensation
        sh_comp = float(intent.get("shoulder_level_compensate", 0.0))
        if abs(sh_comp) > 1e-4:
            _apply_bone_delta("clavicle_l", dr=-bank * 0.5 * sh_comp)
            _apply_bone_delta("clavicle_r", dr=-bank * 0.5 * sh_comp)

        # Arm back arc
        arm_arc = float(intent.get("arm_back_arc", 0.0))
        if abs(arm_arc) > 1e-4:
            _apply_bone_delta("upperarm_l", dp=arm_arc)
            _apply_bone_delta("upperarm_r", dp=-arm_arc * 0.5)

        # Foot pushoff & W3 knee drop
        pushoff = float(intent.get("foot_pushoff_deg", 0.0))
        if abs(pushoff) > 1e-4:
            _apply_bone_delta("foot_r", dp=pushoff)

        knee_drop = float(intent.get("w3_knee_drop", 0.0))
        if abs(knee_drop) > 1e-4:
            _apply_bone_delta("thigh_l", dp=knee_drop)
            _apply_bone_delta("calf_l", dp=-knee_drop * 1.4)
    finally:
        ctrl.close_bracket()

    if save:
        EAL.save_asset(dst_path, only_if_is_dirty=False)
    print(f"GEOM_INTENT: OK -> {dst_path} (save={save}, frames={f0}..{f1})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
