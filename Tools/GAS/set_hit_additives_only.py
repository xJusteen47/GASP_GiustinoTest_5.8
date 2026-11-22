"""Variant reaction set with the new directional hit additives, WITHOUT touching DA_ReactionAnims (the owner edits that one).
Copies /Game/GASNPPTest/Combat/Reaction/DA_ReactionAnims to /Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd (once; later runs
only rewrite the 8 directional slots) and points its DirectionalHits at the clips of Tools/GAS/build_hit_additives.py.
The game keeps using DA_ReactionAnims until the owner approves; to try the variant, point the character's GSTN_ReactionAnim
component at the copy in memory (capture) or swap it by hand. Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/set_hit_additives_only.py" -unattended -nullrhi
Log: Saved/Reaction/set_hit_additives_only.log."""
import os

import unreal

SRC = '/Game/GASNPPTest/Combat/Reaction/DA_ReactionAnims'
DST = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd'
HITADD = '/Game/GSTN/Animation/Reaction/HitAdditive/AS_GSTN_HitAdd'
SIDES = ('Front', 'Back', 'Left', 'Right')

out = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction', 'set_hit_additives_only.log')
lines = []
try:
    EAL = unreal.EditorAssetLibrary
    if not EAL.does_asset_exist(DST):
        EAL.duplicate_asset(SRC, DST)
        lines.append(f'copied {SRC} -> {DST}')
    asset = unreal.load_asset(DST)
    settings = asset.get_editor_property('directional_hits')
    for key, var in (('ground', 'Ground'), ('air', 'Air')):
        adds = settings.get_editor_property(key)
        for s in SIDES:
            adds.set_editor_property(s.lower(), unreal.load_asset(f'{HITADD}_{var}_{s}'))
        settings.set_editor_property(key, adds)
    asset.set_editor_property('directional_hits', settings)
    EAL.save_loaded_asset(asset, False)
    asset = unreal.load_asset(DST)
    adds = asset.get_editor_property('directional_hits')
    for key in ('ground', 'air'):
        a = adds.get_editor_property(key)
        for s in SIDES:
            c = a.get_editor_property(s.lower())
            lines.append(f'{key}.{s.lower()} = {c.get_name() if c else None}')
except Exception as ex:
    import traceback
    lines.append('ERROR ' + repr(ex))
    lines.append(traceback.format_exc())
os.makedirs(os.path.dirname(out), exist_ok=True)
open(out, 'w').write('\n'.join(lines) + '\nDONE_MARK\n')
