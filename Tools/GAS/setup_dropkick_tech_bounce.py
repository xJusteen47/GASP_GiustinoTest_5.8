# DROPKICK-BOUNCE-1 (2026-11-17, thread dair-dropkick-anim, brief handoffs/archivio/2026-11/dropkick-tech-bounce-anim-brief.md): the dropkick USER's own landing
# hop clip, as a VARIANT chain next to the current dropkick (nothing the owner plays today changes). Data only, rerun-safe, saves .uasset only.
#   AS/AM_OverlordDropkickTechBounce  copy of the animpipe build (card overlord_dropkick_techbounce, Antigravity blocking + curve sheet); montage
#                                     blend in 1 f (the BREAK from the active pose must read), blend out 3 f into the Knockdown rule's GroundHit
#   GA_Overlord_DropkickTechBounce    Blueprint child of UGSTN_GA_GroundBounce (thin, Forced), its timeline plays the montage, TOTAL_FRAMES long
#   TL_Overlord_DropkickTB            duplicate of TL_Overlord_Dropkick + a Tags window Combat.Move.KnockdownOnLanding over the whole move (the BASE
#                                     capture showed none on TL_Overlord_Dropkick: the user was never downed, the 25 cm Self Landing Bounce never
#                                     ran and the montage's Land section played) + Self Landing Bounce BounceAbility = the GA above (C++ hook)
#   GA_Overlord_DropkickTB            duplicate of GA_Overlord_Dropkick whose timeline_asset is TL_Overlord_DropkickTB
#   KIT_Test_PlaytestTB (+ weapon copy when the binding lives on the weapon)  duplicate of KIT_Test_Playtest with the dropkick binding on
#                                     GA_Overlord_DropkickTB and GA_Overlord_DropkickTechBounce in TriggeredAbilities (the model starts only
#                                     registered classes); its defence extension gets RULES (the kit had none and the standard combat rules carry no
#                                     defence rules, so the pawn had no defence data at all: no self knockdown, no hop; diag3 log, 2026-11-17)
# Editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_dropkick_tech_bounce as s; s.run()
# Output:  Saved/DropkickBounce/setup_dropkick_tech_bounce.txt, at most 5 printed lines.
import os

import unreal

DIR = '/Game/GASNPPTest/Combat/Overlord'
ROOT = '/Game/GASNPPTest/Combat'
# Review rounds (DROPKICK_BOUNCE_ROUND=R2, ...): every asset of the chain gets the suffix, next to round 1, never over it
R = os.environ.get('DROPKICK_BOUNCE_ROUND', '')
SRC = '/Game/AnimPipeline/Rig/Overlord/Dropkick/AS_OverlordDropkickTechBounce' + R
SIM_HZ, ANIM_FPS = 62.5, 60.0
TOTAL_FRAMES = int(os.environ.get('DROPKICK_BOUNCE_FRAMES', '15'))   # the clip's 15 frames (r1 retime to the measured hop), one per tick
WINDOW_TAG = 'Combat.Move.KnockdownOnLanding'
RULES = os.environ.get('DROPKICK_BOUNCE_RULES', '/GSTNDefence/Test/DA_DefenceRules_Test')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DropkickBounce', 'setup_dropkick_tech_bounce.txt')
_lines = []
lib = unreal.EditorAssetLibrary


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dropkick_bounce] {msg}')


def _dup(src, dst):
    if not lib.does_asset_exist(dst):
        if not lib.duplicate_asset(src, dst):
            raise RuntimeError(f'could not duplicate {src} -> {dst}')
        log(f'created {dst} (duplicate of {src})')
    return unreal.load_asset(dst)


def _clip():
    if os.environ.get('DROPKICK_BOUNCE_REFRESH') == '1':   # a new animpipe build: the copies are this script's own, rebuilt from it
        for p in (f'{DIR}/AM_OverlordDropkickTechBounce{R}', f'{DIR}/AS_OverlordDropkickTechBounce{R}'):
            if lib.does_asset_exist(p):
                lib.delete_asset(p)
                log(f'refresh: deleted {p}')
    anim = _dup(SRC, f'{DIR}/AS_OverlordDropkickTechBounce{R}')
    path = f'{DIR}/AM_OverlordDropkickTechBounce{R}'
    montage = unreal.load_asset(path) if lib.does_asset_exist(path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', anim)
        factory.set_editor_property('target_skeleton', anim.get_editor_property('skeleton'))
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AM_OverlordDropkickTechBounce' + R, DIR, unreal.AnimMontage, factory)
        log(f'created {path}')
    # DROPKICK_BOUNCE_BLEND_OUT=<frames> (dropkick-tb-4): with the automatic blend-out trigger the montage gives way 3 frames before its end
    # (r3 showed 11 of its 15 frames, r4 9 of 12) and GroundHit takes the pose early; a set value also sets the trigger to 0, so the clip
    # plays to its last frame and GroundHit enters over that many frames (the dair landing clips do the same, handover dair-v10-dropkick-bounce-3)
    out_frames = os.environ.get('DROPKICK_BOUNCE_BLEND_OUT')
    for prop, frames in (('blend_in', 1.0), ('blend_out', float(out_frames) if out_frames else 3.0)):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', frames / ANIM_FPS)
        montage.set_editor_property(prop, blend)
    if out_frames:
        montage.set_editor_property('blend_out_trigger_time', 0.0)
    log(f'clip {anim.get_play_length() * ANIM_FPS:.1f} frames, montage {montage.get_play_length():.3f} s')
    return anim, montage


def _bounce_ability(montage):
    path = f'{DIR}/GA_Overlord_DropkickTechBounce{R}'
    bp = unreal.load_asset(path) if lib.does_asset_exist(path) else None
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GSTN_GA_GroundBounce.static_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('GA_Overlord_DropkickTechBounce' + R, DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path}')
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    cdo = unreal.get_default_object(cls)
    cdo.set_editor_property('total_frames', TOTAL_FRAMES)
    tl = cdo.get_editor_property('timeline')
    for prop, value in (('total_frames', TOTAL_FRAMES), ('first_actionable_frame', TOTAL_FRAMES), ('montage', montage),
                        ('montage_play_rate', SIM_HZ / ANIM_FPS)):
        tl.set_editor_property(prop, value)
    cdo.set_editor_property('timeline', tl)
    log(f'GA_Overlord_DropkickTechBounce: {TOTAL_FRAMES} frames, montage {montage.get_name()}')
    return bp, cls


def _move_timeline(bounce_cls):
    asset = _dup(f'{DIR}/TL_Overlord_Dropkick', f'{DIR}/TL_Overlord_DropkickTB{R}')
    data = asset.get_editor_property('data')
    text = data.export_text()
    if WINDOW_TAG not in text:
        total = data.get_editor_property('total_frames')
        window = f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Tags(StartFrame=0,EndFrame={total},Tags=(GameplayTags=((TagName="{WINDOW_TAG}"))))'
        head = 'Tracks=('
        i = text.find(head)
        if i < 0:
            raise RuntimeError('no Tracks in the dropkick timeline text')
        j = i + len(head)
        text = text[:j] + window + ('' if text[j] == ')' else ',') + text[j:]
        data.import_text(text)
        if WINDOW_TAG not in data.export_text():
            raise RuntimeError('the KnockdownOnLanding window did not import')
        log(f'TL_Overlord_DropkickTB: Tags window {WINDOW_TAG} [0, {total})')
    data = unreal.GSTN_DefenceContentLibrary.set_self_landing_bounce_ability(data, bounce_cls)
    out = data.export_text()
    if 'GA_Overlord_DropkickTechBounce' not in out or 'SelfLandingBounce' not in out:
        raise RuntimeError('BounceAbility not set on the Self Landing Bounce track')
    asset.set_editor_property('data', data)
    log('TL_Overlord_DropkickTB: Self Landing Bounce BounceAbility = GA_Overlord_DropkickTechBounce')
    return asset


def _move_ability(timeline):
    bp = _dup(f'{DIR}/GA_Overlord_Dropkick', f'{DIR}/GA_Overlord_DropkickTB{R}')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/GA_Overlord_DropkickTB{R}')
    unreal.get_default_object(cls).set_editor_property('timeline_asset', timeline)
    return bp, cls


def _swap(bindings, old_cls, new_cls):
    out, n = [], 0
    for b in bindings:
        ability = b.get_editor_property('ability')
        if ability and ability.get_path_name() == old_cls.get_path_name():
            b.set_editor_property('ability', new_cls)
            n += 1
        out.append(b)
    return out, n


def _kit(move_cls, bounce_cls):
    old_cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/GA_Overlord_Dropkick')
    kit = _dup(f'{ROOT}/KIT_Test_Playtest', f'{ROOT}/KIT_Test_PlaytestTB{R}')
    kit.set_editor_property('display_name', 'PLAYTEST + DROPKICK TECH BOUNCE')
    bindings, n_kit = _swap(list(kit.get_editor_property('kit_bindings')), old_cls, move_cls)
    kit.set_editor_property('kit_bindings', bindings)
    saves, n_wpn = [kit], 0
    weapon = kit.get_editor_property('primary_weapon')
    if weapon:
        wb, n_wpn = _swap(list(weapon.get_editor_property('bindings')), old_cls, move_cls)
        if n_wpn:
            wcopy = _dup(weapon.get_path_name().split('.')[0], f'{ROOT}/WPN_Test_PlaytestTB{R}')
            wb, _ = _swap(list(wcopy.get_editor_property('bindings')), old_cls, move_cls)
            wcopy.set_editor_property('bindings', wb)
            kit.set_editor_property('primary_weapon', wcopy)
            saves.append(wcopy)
    trig = list(kit.get_editor_property('triggered_abilities'))
    # The exact class of this round: KIT_Test_Playtest already lists the owner's round (R3 since 11-17), and a match on the name prefix left a
    # new round unregistered, so the model fell back to the generic GA_GroundBounce_Test (dropkick-tb-4, capture 1)
    if not any(t and t.get_path_name() == bounce_cls.get_path_name() for t in trig):
        trig.append(bounce_cls)
        kit.set_editor_property('triggered_abilities', trig)
    rules = unreal.load_asset(RULES)
    if not rules or not unreal.GSTN_DefenceContentLibrary.set_kit_defence_extension(kit, rules, None):
        raise RuntimeError(f'could not set the defence rules {RULES} on KIT_Test_PlaytestTB')
    log(f'KIT_Test_PlaytestTB: defence extension rules = {RULES} (get-up set kept, no shield)')
    log(f'KIT_Test_PlaytestTB: dropkick bindings swapped kit {n_kit} weapon {n_wpn}; triggered abilities {len(trig)}')
    # A rerun finds the kit already on its weapon copy (nothing left to swap): that copy is the one to save, never the original weapon
    already = n_kit + n_wpn == 0 and f'WPN_Test_PlaytestTB{R}' in str(weapon)
    if n_kit + n_wpn == 0 and not already:
        raise RuntimeError('no GA_Overlord_Dropkick binding found on KIT_Test_Playtest or its weapon')
    if already and weapon not in saves:
        saves.append(weapon)
    return saves


def run():
    del _lines[:]
    ok = False
    try:
        anim, montage = _clip()
        bounce_bp, bounce_cls = _bounce_ability(montage)
        timeline = _move_timeline(bounce_cls)
        move_bp, move_cls = _move_ability(timeline)
        saves = [anim, montage, bounce_bp, timeline, move_bp] + _kit(move_cls, bounce_cls)
        for a in saves:
            lib.save_loaded_asset(a, False)
        log('saved: ' + ', '.join(a.get_name() for a in saves))
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


if __name__ == '__main__':
    run()
