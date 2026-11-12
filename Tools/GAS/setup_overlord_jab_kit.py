# ANIM-JAB-3: the Overlord's two-hit jab on the light attack of a test kit, so the built animations can be watched in a real Play session.
# Data only: no C++, no input mapping, no project setting, no Blueprint graph. Idempotent (a rerun rewrites the specs below over hand edits).
#
# Reads (never writes) the built animations of the anim pipeline, /Game/AnimPipeline/Rig/AS_OverlordJab{1,2}V2_Build. Content/AnimPipeline is one
# folder shared by every worktree through a junction and holds the owner's work in progress: this script only duplicates out of it.
# Writes, under /Game/GASNPPTest/Combat/Overlord (new assets only; KIT_Test_Hits, WPN_Test_Hits and the test character are never edited):
#   AS_OverlordJab{1,2}V2    copies of the built animations (versioned with the kit; rerun with refresh=True after a new `animpipe build`)
#   AM_OverlordJab{1,2}V2    one montage each (default slot, 2 frame blend in)
#   TL_Overlord_Jab{1,2}     UGSTN_CombatTimeline: frames from the move cards, a PLACEHOLDER root hitbox, jab 1's cancel window into the
#                            neutral attack and the tag window that tells the two bindings apart
#   GA_Overlord_Jab{1,2}     thin Blueprint children of UGSTN_GA_TestKitStrike whose TimelineAsset is the timeline above (no graph)
#   WPN_Test_Overlord        a copy of WPN_Test_Hits: Command.Attack.Neutral -> jab 1, and -> jab 2 while CHAIN_TAG is up
#   KIT_Test_Overlord        a copy of KIT_Test_Hits with that weapon
# Frame data comes from Tools/AnimPipeline/cards/overlord/jab (anim frames at 60 fps, counted from 0). The hit numbers, the hitbox shape, the
# cancel window and the montage blend are PLACEHOLDERS for the review, not design: the owner decides them.
#
# Headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_overlord_jab_kit.py" -unattended -nullrhi
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_overlord_jab_kit as s; s.run()
# Output:    Saved/AnimJab3/setup_overlord_jab_kit.txt, at most 5 printed lines.
import os

import unreal

SRC = '/Game/AnimPipeline/Rig/AS_OverlordJab{}V2_Build'
ROOT = '/Game/GASNPPTest/Combat'
DIR = ROOT + '/Overlord'
SOURCE_KIT, SOURCE_WEAPON = 'KIT_Test_Hits', 'WPN_Test_Hits'
KIT, WEAPON = 'KIT_Test_Overlord', 'WPN_Test_Overlord'
COMMAND = 'Command.Attack.Neutral'
# Up on jab 1 from the frame before its cancel window to the window's end: the jab 2 binding requires it, the jab 1 binding ignores it
CHAIN_TAG = 'Test.State.OverlordJabChain'
BLEND_IN_SECONDS = 2.0 / 60.0
# total frames, first actionable frame, active (first, last), cancel window into COMMAND (first, last) or None,
# hit PLACEHOLDERS: radius, reach from, reach to, height, damage, knockback, angle, victim hitstop, attacker hitstop
MOVES = {
    1: {'total': 29, 'faf': 22, 'active': (6, 8), 'cancel': (9, 13), 'hit': (45.0, 60.0, 110.0, 30.0, 3.0, 150.0, 30.0, 4, 3)},
    2: {'total': 53, 'faf': 26, 'active': (9, 11), 'cancel': None, 'hit': (50.0, 60.0, 120.0, 30.0, 7.0, 550.0, 35.0, 6, 4)},
}
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimJab3', 'setup_overlord_jab_kit.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_jab_kit] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _container(names):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag(n) for n in names])


def _names(container):
    return [str(t.get_editor_property('tag_name')) for t in unreal.GameplayTagLibrary.break_gameplay_tag_container(container)]


def _tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _anim_and_montage(n, refresh):
    """The versioned copy of the built animation and its montage. refresh: both are deleted and made again from the pipeline's build"""
    anim_path, montage_path = f'{DIR}/AS_OverlordJab{n}V2', f'{DIR}/AM_OverlordJab{n}V2'
    lib = unreal.EditorAssetLibrary
    if refresh:
        for p in (montage_path, anim_path):
            if lib.does_asset_exist(p):
                lib.delete_asset(p)
                log(f'refresh: deleted {p}')
    if not lib.does_asset_exist(anim_path):
        if not lib.does_asset_exist(SRC.format(n)) or not lib.duplicate_asset(SRC.format(n), anim_path):
            log(f'ERROR: could not copy {SRC.format(n)} to {anim_path}')
            return None, None
        log(f'created {anim_path} (copy of {SRC.format(n)})')
    anim = unreal.load_asset(anim_path)
    montage = unreal.load_asset(montage_path) if lib.does_asset_exist(montage_path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', anim)
        factory.set_editor_property('target_skeleton', anim.get_editor_property('skeleton'))
        montage = _tools().create_asset(f'AM_OverlordJab{n}V2', DIR, unreal.AnimMontage, factory)
        log(f'created {montage_path}')
    blend = montage.get_editor_property('blend_in')
    blend.set_editor_property('blend_time', BLEND_IN_SECONDS)
    montage.set_editor_property('blend_in', blend)
    log(f'jab {n}: anim {anim.get_play_length() * 60.0:.1f} frames at 60 fps, skeleton {anim.get_editor_property("skeleton").get_name()}, '
        f'montage {montage.get_play_length():.3f} s')
    return anim, montage


def _append_tracks(data, extra):
    """Tracks are instanced structs, which Python cannot build: the extra ones are added through the struct's own text form"""
    text = data.export_text()
    start = text.find('Tracks=(')
    if start < 0:
        log('ERROR: no Tracks in the timeline text: ' + text[:300])
        return False
    i, depth, quoted = start + len('Tracks=('), 1, False
    while depth and i < len(text):
        c = text[i]
        if c == '"' and text[i - 1] != '\\':
            quoted = not quoted
        elif not quoted:
            depth += (c == '(') - (c == ')')
        i += 1
    inner = text[start + len('Tracks=('):i - 1]
    data.import_text('(Tracks=(' + ','.join(([inner] if inner else []) + extra) + '))')
    return True


def _timeline(n, spec, montage):
    radius, reach0, reach1, height, damage, knockback, angle, hitstop, attacker_hitstop = spec['hit']
    a0, a1 = spec['active']
    data = unreal.GSTN_CombatTimelineData()
    for prop, value in (('total_frames', spec['total']), ('first_actionable_frame', spec['faf']), ('override_frames', 0), ('montage', montage),
                        ('montage_play_rate', 1.0)):
        data.set_editor_property(prop, value)
    hit = unreal.GSTN_HitData()
    for prop, value in (('damage', damage), ('base_knockback', knockback), ('angle_deg', angle), ('hitstop_mode', unreal.GSTN_HitstopMode.OVERRIDE),
                        ('hitstop_frames_override', hitstop), ('attacker_hitstop_frames_override', attacker_hitstop)):
        hit.set_editor_property(prop, value)
    shape = unreal.GSTN_HitShape()
    shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
    shape.set_editor_property('extent', unreal.Vector(radius, radius, radius))
    keys = []
    for frame, reach in ((0, reach0), (a1 - a0, reach1)):
        key = unreal.GSTN_HitboxKey()
        key.set_editor_property('frame', frame)
        key.set_editor_property('offset', unreal.Vector(reach, 0.0, height))
        keys.append(key)
    box = unreal.GSTN_HitboxSpec()
    for prop, value in (('shape', shape), ('space', unreal.GSTN_HitboxSpace.ROOT), ('hit_group', 0), ('data', hit), ('keys', keys),
                        ('sweep_between_frames', True)):
        box.set_editor_property(prop, value)
    data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, a0, a1 + 1, unreal.GSTN_PartSelector(), [box])
    if spec['cancel']:
        c0, c1 = spec['cancel']
        query = unreal.GameplayTagLibrary.make_gameplay_tag_query_match_any_tags(_container([COMMAND])).export_text()
        extra = [f'/Script/GSTNCombatCore.GSTN_TimelineTrack_CancelWindow(StartFrame={c0},EndFrame={c1 + 1},IntoCommands={query},Condition=Always)',
                 f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Tags(StartFrame={c0 - 1},EndFrame={c1 + 1},Tags=(GameplayTags=((TagName="{CHAIN_TAG}"))))']
        if not _append_tracks(data, extra):
            return None
    text = data.export_text()
    kinds = [k for k in ('Hitbox', 'CancelWindow', 'Tags') if f'GSTN_TimelineTrack_{k}(' in text]
    if spec['cancel'] and len(kinds) != 3:
        log(f'ERROR: jab {n} timeline has tracks {kinds}, expected Hitbox, CancelWindow, Tags: ' + text[:400])
        return None
    name = f'TL_Overlord_Jab{n}'
    asset = unreal.load_asset(f'{DIR}/{name}') if unreal.EditorAssetLibrary.does_asset_exist(f'{DIR}/{name}') else None
    if not asset:
        asset = _tools().create_asset(name, DIR, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        log(f'created {DIR}/{name}')
    asset.set_editor_property('data', data)
    log(f'jab {n}: timeline total {spec["total"]}, FAF {spec["faf"]}, active {a0}-{a1}, cancel {spec["cancel"]}, tracks {kinds}')
    return asset


def _ability(n, timeline):
    name = f'GA_Overlord_Jab{n}'
    path = f'{DIR}/{name}'
    bp = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not bp:
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GSTN_GA_TestKitStrike.static_class())
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = _tools().create_asset(name, DIR, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path}')
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    unreal.get_default_object(cls).set_editor_property('timeline_asset', timeline)
    return bp, cls


def _describe(bindings):
    out = []
    for b in bindings:
        req = b.get_editor_property('requirements')
        ability = b.get_editor_property('ability')
        out.append(f"{b.get_editor_property('command').get_editor_property('tag_name')} +{_names(req.get_editor_property('require_tags'))} "
                   f"-{_names(req.get_editor_property('ignore_tags'))} -> {str(ability).rsplit('.', 1)[-1] if ability else None}")
    return out


def _with_chain(binding, cls, required):
    """A copy of `binding` for `cls`: the chain tag joins its required tags (jab 2) or its ignored tags (jab 1), the rest is kept"""
    out = binding.copy()
    req = out.get_editor_property('requirements')
    prop = 'require_tags' if required else 'ignore_tags'
    req.set_editor_property(prop, _container([t for t in _names(req.get_editor_property(prop)) if t != CHAIN_TAG] + [CHAIN_TAG]))
    out.set_editor_property('requirements', req)
    out.set_editor_property('ability', cls)
    return out


def _copy(source, name):
    path = f'{ROOT}/{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{ROOT}/{source}', path):
            log(f'ERROR: could not copy {source} to {name}')
            return None
        log(f'created {path} (copy of {source})')
    return unreal.load_asset(path)


def _weapon_and_kit(classes):
    weapon, kit = _copy(SOURCE_WEAPON, WEAPON), _copy(SOURCE_KIT, KIT)
    if not weapon or not kit:
        return []
    weapon.set_editor_property('display_name', WEAPON)
    weapon.set_editor_property('weapon_tags', _container(['Weapon.Test.Overlord', 'Test.Role.Primary']))
    old = list(weapon.get_editor_property('bindings'))
    log('weapon bindings before: ' + '; '.join(_describe(old)))
    mine = [b for b in old if str(b.get_editor_property('command').get_editor_property('tag_name')) == COMMAND]
    if not mine:
        log(f'ERROR: {WEAPON} has no {COMMAND} binding to give the jab')
        return []
    # One pair per context the source weapon bound the neutral attack in; a rerun starts from the bindings that do not name the chain tag
    base = [b for b in mine if CHAIN_TAG not in _names(b.get_editor_property('requirements').get_editor_property('require_tags'))]
    new = [b for b in old if b not in mine]
    for b in base:
        new += [_with_chain(b, classes[1], False), _with_chain(b, classes[2], True)]
    weapon.set_editor_property('bindings', new)
    log('weapon bindings after: ' + '; '.join(_describe(new)))
    kit.set_editor_property('display_name', 'OVERLORD JAB')
    kit.set_editor_property('kit_tags', _container(['Kit.Test.Overlord']))
    kit.set_editor_property('primary_weapon', weapon)
    kit_bindings = list(kit.get_editor_property('kit_bindings'))
    log('kit bindings: ' + '; '.join(_describe(kit_bindings)))
    if any(str(b.get_editor_property('command').get_editor_property('tag_name')) == COMMAND for b in kit_bindings):
        log(f'WARNING: the kit itself binds {COMMAND}: check which binding wins')
    return [weapon, kit]


def run(refresh=False):
    del _lines[:]
    ok = False
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(DIR):
            unreal.EditorAssetLibrary.make_directory(DIR)
        saves, classes = [], {}
        for n, spec in MOVES.items():
            anim, montage = _anim_and_montage(n, refresh)
            timeline = _timeline(n, spec, montage) if montage else None
            if not timeline:
                raise RuntimeError(f'jab {n} not built')
            bp, classes[n] = _ability(n, timeline)
            saves += [anim, montage, timeline, bp]
        tail = _weapon_and_kit(classes)
        if not tail:
            raise RuntimeError('weapon or kit not built')
        for asset in saves + tail:
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        log(f'saved {len(saves + tail)} assets: ' + ', '.join(a.get_name() for a in saves + tail))
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return ok


if __name__ == '__main__':
    run()
