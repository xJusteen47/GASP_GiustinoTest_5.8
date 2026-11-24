# Headless: write the FK jab takes into the game clips AS_OverlordJab{1,2}V3 (backups first), root motion on jab 2.
import json, unreal, traceback
S = r"C:/Users/jacop/AppData/Local/Temp/claude/8c81644554604783/a175c3df-7a30-40e8-8b2e-52e6b18b94b7/scratchpad/jab"
OUT = S + "/pub_result.txt"
eal = unreal.EditorAssetLibrary
lines = []
try:
    for n, rm in ((1, False), (2, True)):
        game = f"/Game/GSTN/Characters/Overlord/Moves/Jab/AS_OverlordJab{n}V3"
        bk = f"/Game/AnimPipeline/Backup/AS_OverlordJab{n}V3_r1_2026-11-18"
        if not eal.does_asset_exist(bk):
            eal.duplicate_asset(game, bk); eal.save_asset(bk, only_if_is_dirty=False)
        d = json.load(open(f"{S}/sheets/raw/AS_OverlordJab{n}FK_tracks.json"))
        anim = eal.load_asset(game); ctrl = anim.get_editor_property("controller")
        k = int(d["keys"])
        before = anim.get_play_length()
        ctrl.open_bracket(unreal.Text("GSTN FK jab"), True)
        ctrl.set_number_of_frames(unreal.FrameNumber(k - 1), True)
        ok = 0
        for b, keys in d["bones"].items():
            pos = [unreal.Vector(x[0], x[1], x[2]) for x in keys]
            rot = [unreal.Quat(x[3], x[4], x[5], x[6]) for x in keys]
            sc = [unreal.Vector(1, 1, 1) for _ in keys]
            ok += bool(ctrl.set_bone_track_keys(unreal.Name(b), pos, rot, sc, True))
        ctrl.close_bracket(True)
        anim.set_editor_property("enable_root_motion", rm)
        if rm:
            anim.set_editor_property("force_root_lock", False)
        eal.save_asset(game, only_if_is_dirty=False)
        lines.append(f"jab{n}: {ok}/{len(d['bones'])} tracks, len {before:.3f}->{anim.get_play_length():.3f} s, root motion {anim.get_editor_property('enable_root_motion')}")
except Exception:
    lines.append("ERROR " + traceback.format_exc()[-800:])
open(OUT, "w").write("\n".join(lines) + "\n")
