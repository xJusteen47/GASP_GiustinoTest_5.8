# Simple hurtbox fitted to the mannequin (owner playtest 2026-11-17: "too tall, below the floor, too wide"). Scope: HBS_Test_Pawn, the set of
# BP_GASNPP_TestCharacter (the playtest pawn and the dummies). Shapes are root-relative = the actor origin, the capsule centre (half height 86),
# floor at -88 (mesh root). Mannequin (SKM_UEFN_Mannequin, reference pose, measured Saved/PF1117c/body_dump.txt): head bone z 62, top of the
# head ~82, shoulders (upperarm) z 44 at y +-17.6, feet z -79, toes x +15.
# Before: torso r35 z15 +-55 (-40..70), head r22 z68 +-22 (46..90), feet r30 z-62 +-30 (-92..-32: 4 cm under the floor), 70 cm wide.
# After:  feet r20 -88..-44 (on the floor, one capsule across both legs: no gap between them), torso r22 -50..50 (overlaps the feet: the
#         anti low-gap fill stays), head r12 50..82. 44 cm wide = the shoulders.
# Rerun-safe. Out: Saved/PlaytestFixes/hurtbox_1117.txt
import os
import unreal

SET = '/Game/GSTN/_Test/Combat/HBS_Test_Pawn'
# name -> (radius, half height incl. hemispheres, centre z, centre x)
FIT = {'feet': (20.0, 22.0, -66.0, 4.0), 'torso': (22.0, 50.0, 0.0, 0.0), 'head': (12.0, 16.0, 66.0, 2.0)}
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(_PROJ, 'Saved', 'PlaytestFixes', 'hurtbox_1117.txt')


def _desc(h):
    s = h.get_editor_property('shape')
    e = s.get_editor_property('extent')
    t = s.get_editor_property('local').translation
    return f'{h.get_editor_property("name")}: r{e.x:.0f} half{e.z:.0f} z{t.z:.0f} ({t.z - e.z:.0f}..{t.z + e.z:.0f}) x{t.x:.0f}'


def run():
    lines = []
    hs = unreal.load_asset(SET)
    boxes = list(hs.get_editor_property('hurtboxes'))
    out = []
    for h in boxes:
        before = _desc(h)
        name = str(h.get_editor_property('name'))
        if name in FIT:
            r, half, z, x = FIT[name]
            s = h.get_editor_property('shape')
            s.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
            s.set_editor_property('extent', unreal.Vector(r, r, half))
            s.set_editor_property('local', unreal.Transform(location=unreal.Vector(x, 0.0, z)))
            h.set_editor_property('shape', s)
        lines.append(f'{before}  ->  {_desc(h)}')
        out.append(h)
    hs.set_editor_property('hurtboxes', out)
    unreal.EditorAssetLibrary.save_loaded_asset(hs, False)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w').write('\n'.join(lines) + '\n')
    print(f'{len(lines)} hurtboxes -> {OUT}')


if __name__ == '__main__':
    run()
