"""Fast crouch content wiring (crouch-veloce-2), headless, idempotent. Run AFTER crouch_fk_build.py made the clips.
  1. Blend profile BLP_GSTN_FollowThrough (Time Factor: hips first, arms/head trail) next to the crouch clips.
  2. DA_ReactionAnims, AirCrouch rule: Animation = AS_AirCrouch_Enter, ThenAnimation = the loop, BlendOutProfile = the profile.
Report (max 5 lines printed): Saved/Crouch/crouch_setup.log
"""
import os
import unreal

FOLDER = "/Game/GSTN/Animation/Crouch"
PROFILE = "BLP_GSTN_FollowThrough"
SKEL_FROM = FOLDER + "/AS_GSTN_Crouch_Enter"
AIR = "/Game/GASNPPTest/Combat/AirCrouch"
REACTION_SET = "/Game/GASNPPTest/Combat/Reaction/DA_ReactionAnims"
# Handover crouch-veloce-1 starting values (Time Factor: < 1 arrives sooner, > 1 later)
SCALES = {"pelvis": 0.5, "thigh_l": 0.5, "thigh_r": 0.5, "calf_l": 0.5, "calf_r": 0.5, "spine_01": 0.8,
          "clavicle_l": 1.2, "clavicle_r": 1.2, "upperarm_l": 1.2, "upperarm_r": 1.2,
          "lowerarm_l": 1.5, "lowerarm_r": 1.5, "hand_l": 1.5, "hand_r": 1.5, "head": 1.5}
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Crouch", "crouch_setup.log")
lines = []


def log(s):
    lines.append(str(s))


def make_profile():
    path = f"{FOLDER}/{PROFILE}"
    skel = unreal.load_asset(SKEL_FROM).get_editor_property("skeleton")
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        bp = None
        for fac_name in ("BlendProfileFactory",):
            fac = getattr(unreal, fac_name, None)
            try:
                f = fac() if fac else None
                if f and hasattr(f, "set_editor_property"):
                    try:
                        f.set_editor_property("target_skeleton", skel)
                    except Exception as ex:
                        log(f"factory skeleton: {ex}")
                bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(PROFILE, FOLDER, unreal.BlendProfile, f)
            except Exception as ex:
                log(f"create via {fac_name}: {ex}")
        if bp is None:
            raise RuntimeError("blend profile asset could not be created")
    G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
    done = G.call_method("SetBlendProfileScales", (bp, skel, SCALES, 0))
    log(f"profile {path}: {done} bones")
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return bp


def wire_air(bp):
    rset = unreal.load_asset(REACTION_SET)
    rules = list(rset.get_editor_property("rules"))
    hit = 0
    for r in rules:
        if str(r.get_editor_property("name")) != "AirCrouch":
            continue
        r.set_editor_property("animation", unreal.load_asset(f"{AIR}/AS_AirCrouch_Enter"))
        r.set_editor_property("then_animation", unreal.load_asset(f"{AIR}/AS_AirCrouch_Loop_PLACEHOLDER"))
        r.set_editor_property("blend_out_profile", bp)
        hit += 1
    rset.set_editor_property("rules", rules)
    unreal.EditorAssetLibrary.save_asset(REACTION_SET, only_if_is_dirty=False)
    log(f"DA_ReactionAnims: AirCrouch rules updated {hit}")


try:
    prof = make_profile()
    wire_air(prof)
    status = "ESITO: OK"
except Exception:
    import traceback
    log(traceback.format_exc()); status = "ESITO: ERRORE"
os.makedirs(os.path.dirname(LOG), exist_ok=True)
open(LOG, "w", encoding="utf-8").write("\n".join(lines + [status]) + "\n")
for l in (lines[-3:] + [status])[:5]:
    print(l)
