"""Creates (or refreshes) the project's surface response table, DA_GSTN_SurfaceResponse_Default in /Game/GSTN/Audio.

Run in the open UE 5.9 editor (Python console or Tools/UE59 runner) after the C++ build. Idempotent: rows are rewritten from ROWS below,
except the Sound column of a row whose key already exists (a designer's MetaSound is kept). Prints at most 5 lines; details go to
Saved/SurfaceFeedback/create_assets.json. Design: docs/notes/surface-feedback.md.
"""
import json
import os

import unreal

PATH = '/Game/GSTN/Audio'
NAME = 'DA_GSTN_SurfaceResponse_Default'

# (event, footwear or '', floor index or -1 = any, haptic intensity, duration s, motors, min magnitude, scale (in0, in1, out0, out1))
# First pass: rumble only. Sound stays stock GASP (no Sound) until the layered MetaSound MSS_GSTN_Footstep exists (doc step 6).
ROWS = [
    ('Foley.Event.Run', '', -1, 0.12, 0.05, 'SMALL', 0.0, (200.0, 800.0, 0.7, 1.0)),
    ('Foley.Event.Walk', '', -1, 0.06, 0.04, 'SMALL', 0.0, (0.0, 1.0, 1.0, 1.0)),
    ('Foley.Event.Run', 'Presentation.Footwear.Metal', -1, 0.22, 0.06, 'BOTH', 0.0, (200.0, 800.0, 0.7, 1.0)),
    ('Foley.Event.Run', 'Presentation.Footwear.Barefoot', -1, 0.05, 0.04, 'SMALL', 0.0, (200.0, 800.0, 0.7, 1.0)),
    ('Presentation.Event.Land', '', -1, 0.6, 0.10, 'LARGE', 200.0, (300.0, 1500.0, 0.4, 1.0)),
    ('Presentation.Event.Hit', '', -1, 0.8, 0.12, 'BOTH', 0.0, (5.0, 40.0, 0.4, 1.0)),
    ('Presentation.Event.Shield.Hit', '', -1, 0.4, 0.08, 'SMALL', 0.0, (0.0, 1.0, 1.0, 1.0)),
    ('Presentation.Event.GroundBounce', '', -1, 0.5, 0.10, 'LARGE', 0.0, (0.0, 1.0, 1.0, 1.0)),
]


def _tag(name):
    tag = unreal.GameplayTag()
    if name:
        tag.import_text(f'(TagName="{name}")')
    return tag


def _key(row):
    return (str(row.get_editor_property('event_tag')), str(row.get_editor_property('footwear')),
            row.get_editor_property('b_any_floor'), int(row.get_editor_property('floor')))


def main():
    asset_path = f'{PATH}/{NAME}'
    created = False
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        table = unreal.EditorAssetLibrary.load_asset(asset_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.GSTN_SurfaceResponseTable)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.GSTN_SurfaceResponseTable, factory)
        created = True
    old_sounds = {_key(r): r.get_editor_property('sound') for r in table.get_editor_property('rows')}

    rows = []
    for event, footwear, floor, haptic, duration, motors, min_mag, scale in ROWS:
        row = unreal.GSTN_SurfaceResponseRow()
        row.set_editor_property('event_tag', _tag(event))
        row.set_editor_property('footwear', _tag(footwear))
        row.set_editor_property('b_any_floor', floor < 0)
        if floor >= 0:
            row.set_editor_property('floor', unreal.PhysicalSurface.cast(floor))
        row.set_editor_property('min_magnitude', min_mag)
        s = unreal.GSTN_PresentationScale()
        s.set_editor_property('input_range', unreal.Vector2D(scale[0], scale[1]))
        s.set_editor_property('output_range', unreal.Vector2D(scale[2], scale[3]))
        row.set_editor_property('scale', s)
        row.set_editor_property('haptic_intensity', haptic)
        row.set_editor_property('haptic_duration', duration)
        row.set_editor_property('haptic_motors', getattr(unreal.GSTN_HapticMotors, motors))
        sound = old_sounds.get(_key(row))
        if sound:
            row.set_editor_property('sound', sound)
        rows.append(row)
    table.set_editor_property('rows', rows)
    unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)

    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'SurfaceFeedback')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'create_assets.json'), 'w') as f:
        json.dump({'asset': asset_path, 'created': created, 'rows': len(rows),
                   'kept_sounds': sum(1 for r in rows if r.get_editor_property('sound'))}, f, indent=1)
    print(f'SURFACE TABLE {"created" if created else "updated"}: {asset_path} rows={len(rows)}')


main()
