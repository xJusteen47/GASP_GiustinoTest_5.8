# CP3 of the Blueprint seams: the owner's double-press special (press the special, a shot; press it again in time, a second shot) made twice,
# the same move two ways, so a designer can open both and compare:
#   A, data only (like jab 1 -> jab 2): two abilities, each a timeline with an Event key whose Fire Projectile action shoots DA_Example_Shot.
#     Shot 1 opens a Tags window (CHAIN_TAG) and a cancel window into the special; the kit binds the special to shot 1 while the tag is down and
#     to shot 2 while it is up. No graph at all.
#   B, one ability with a Blueprint graph (the CP1/CP2 move events and request nodes): On Move Key "Fire" shoots and arms a follow-up
#     (FollowUpState, a Sync Integer: rolled back); On Move Frame asks the combat component whether the special was entered again; if so it opens
#     a tag window (the timeline's buffer reset rule then drops that press, so it does not also start a new special after the move) and waits
#     SecondShotDelayFrames combat frames; On Combat Frames Elapsed shoots the second, faster shot. A "WindowEnd" key closes the follow-up.
# Nothing is wired by hand: the graph is laid down through UGSTN_MoveGraphAuthoring (GSTNCombatEditor). No input key, no project setting: the
# kit binds commands, B names commands (FollowUpCommand*), every number is a Class Defaults / timeline value a designer retunes.
#
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi
# Editor:   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_double_shot_examples as s; s.run()   (s.run(force=True) rewrites)
# Writes, new assets only unless force (a rerun leaves the designer's edits alone):
#   /Game/GSTN/_Examples/DoubleShot/DA_Example_Shot                    the projectile (UGSTN_ProjectileDefinition)
#   /Game/GSTN/_Examples/DoubleShot/TL_Example_DoubleShot_{A1,A2,B}    the timelines
#   /Game/GSTN/_Examples/DoubleShot/GA_Example_DoubleShot_{A1,A2,B}    the abilities (Blueprint children of GSTN_GA_BaseCombat)
#   /Game/GSTN/_Examples/KIT_Example_DoubleShotA, KIT_Example_DoubleShotB   copies of KIT_Example with the special rebound (hub pads)
# Output: Saved/Examples/setup_double_shot.log (+ the B graph in Saved/Examples/double_shot_graph.txt), at most 5 printed lines.
import os

import unreal

EX = '/Game/GSTN/_Examples'
DIR = EX + '/DoubleShot'
SOURCE_KIT = EX + '/KIT_Example'
KITS = {'A': ('KIT_Example_DoubleShotA', 'DOPPIO COLPO A (DATI)', 'Kit.Example.DoubleShotA'),
        'B': ('KIT_Example_DoubleShotB', 'DOPPIO COLPO B (GRAFO)', 'Kit.Example.DoubleShotB')}
COMMANDS = ('Command.Special.Neutral', 'Command.Special.Directional')
CHAIN_TAG = 'Example.State.DoubleShotChain'
FOLLOW_UP_TAG = 'Example.State.DoubleShotFollowUp'
KEY_FIRE, KEY_WINDOW_END = 'Example.Key.DoubleShot.Fire', 'Example.Key.DoubleShot.WindowEnd'
# PLACEHOLDER animation (a GASP shove, read as a cast), the same on all three: the moves are about the logic, the owner picks the real ones
MONTAGE = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_F_A'
MONTAGE_RATE = 1.3
# The shot (PLACEHOLDERS for the playtest): a 15 cm ball at 1800 cm/s for 1.5 s, a light hit
SHOT = dict(speed=1800.0, lifetime_frames=90, radius=15.0, damage=5.0, base_knockback=250.0, angle_deg=25.0, muzzle=(80.0, 0.0, 30.0))
SECOND_SHOT_SPEED = 1.5
# Timelines in simulation frames (62.5 Hz). fire = the Event key, chain/cancel = (first, last) frame of the windows
TIMELINES = {
    'A1': dict(total=32, faf=24, fire=8, chain=(8, 20), cancel=(9, 20), adjust=1.0),
    'A2': dict(total=30, faf=22, fire=6, adjust=SECOND_SHOT_SPEED),
    'B': dict(total=36, faf=28, fire=8, window_end=20),
}
# B's settings (Class Defaults of GA_Example_DoubleShot_B): (name, type, default text, tooltip)
B_VARIABLES = [
    ('FollowUpState', 'struct:/Script/AbilitySystemSimulation.SyncInteger', '(Value=0)',
     'Simulation state, rolled back: 0 before the first shot, 1 while the follow-up can be entered, 2 once taken or closed'),
    ('Projectile', 'object:/Script/GSTNProjectileNP.GSTN_ProjectileDefinition', f'{DIR}/DA_Example_Shot.DA_Example_Shot', 'The shot both presses fire'),
    ('FirstShotAdjust', 'struct:/Script/GSTNProjectileNP.GSTN_ProjectileShotAdjust', '(YawOffsetDeg=0.000000,PitchOffsetDeg=0.000000,SpeedScale=1.000000)',
     'What the first shot changes on the projectile'),
    ('SecondShotAdjust', 'struct:/Script/GSTNProjectileNP.GSTN_ProjectileShotAdjust',
     f'(YawOffsetDeg=0.000000,PitchOffsetDeg=0.000000,SpeedScale={SECOND_SHOT_SPEED:.6f})', 'What the second shot changes (faster by default)'),
    ('FollowUpCommand', 'struct:/Script/GameplayTags.GameplayTag', f'(TagName="{COMMANDS[0]}")', 'A command that counts as the second press'),
    ('FollowUpCommandAlt', 'struct:/Script/GameplayTags.GameplayTag', f'(TagName="{COMMANDS[1]}")', 'Another command that counts (the special with the stick)'),
    ('FollowUpBufferFrames', 'int', '8', 'How old (combat frames) a second press may be when the follow-up opens: an early press is kept, like the buffer'),
    ('SecondShotDelayFrames', 'int', '6', 'Combat frames from the second press to the second shot'),
]
CATEGORY = 'Double Shot'
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Examples')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[double_shot] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        log(f'ERROR: tag {name} is not registered (Config/Tags/GSTNExamples.ini)')
    return tag


def _container(names):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag(n) for n in names])


def _names(container):
    return [str(t.get_editor_property('tag_name')) for t in unreal.GameplayTagLibrary.break_gameplay_tag_container(container)]


def _tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def _exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


# ---- the projectile

def _projectile(force):
    path = f'{DIR}/DA_Example_Shot'
    asset, created = (unreal.load_asset(path), False) if _exists(path) else (_tools().create_asset('DA_Example_Shot', DIR, unreal.GSTN_ProjectileDefinition,
                                                                                                   unreal.DataAssetFactory()), True)
    if not asset:
        raise RuntimeError(f'could not create {path}')
    if created or force:
        spec = asset.get_editor_property('shot')
        params = spec.get_editor_property('projectile')
        shape = params.get_editor_property('shape')
        shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
        shape.set_editor_property('extent', unreal.Vector(SHOT['radius'], SHOT['radius'], SHOT['radius']))
        hit = params.get_editor_property('hit')
        for prop in ('damage', 'base_knockback', 'angle_deg'):
            hit.set_editor_property(prop, SHOT[prop])
        params.set_editor_property('shape', shape)
        params.set_editor_property('hit', hit)
        params.set_editor_property('speed', SHOT['speed'])
        params.set_editor_property('lifetime_frames', SHOT['lifetime_frames'])
        spec.set_editor_property('projectile', params)
        spec.set_editor_property('muzzle_offset', unreal.Vector(*SHOT['muzzle']))
        spec.set_editor_property('aim', unreal.GSTN_ProjectileAim.FACING)
        asset.set_editor_property('shot', spec)
        log(f'{"created" if created else "rewrote"} DA_Example_Shot: {SHOT}')
    return asset, created or force


# ---- the timelines

def _fire_action(projectile, speed_scale):
    """The text of one Fire Projectile move action (an instanced struct: Python cannot put it in the Actions array, the text import can)"""
    action = unreal.GSTN_MoveAction_FireProjectile()
    action.set_editor_property('definition', projectile)
    adjust = unreal.GSTN_ProjectileShotAdjust()
    adjust.set_editor_property('speed_scale', speed_scale)
    action.set_editor_property('adjust', adjust)
    return '/Script/GSTNProjectileNP.GSTN_MoveAction_FireProjectile' + action.export_text()


def _event_key(frame, key_tag, actions=()):
    text = f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Event(StartFrame={frame},EndFrame={frame + 1},KeyTag=(TagName="{key_tag}")'
    return text + (f',Actions=({",".join(actions)}))' if actions else ')')


def _timeline(name, spec, projectile, force):
    path = f'{DIR}/TL_Example_DoubleShot_{name}'
    if _exists(path) and not force:
        return unreal.load_asset(path), False
    data = unreal.GSTN_CombatTimelineData()
    montage = unreal.load_asset(MONTAGE) if _exists(MONTAGE) else None
    if not montage:
        log(f'WARNING: no placeholder montage {MONTAGE}: the moves play without animation')
    for prop, value in (('total_frames', spec['total']), ('first_actionable_frame', spec['faf']), ('override_frames', 0), ('montage', montage),
                        ('montage_play_rate', MONTAGE_RATE)):
        data.set_editor_property(prop, value)
    tracks = []
    if name == 'B':
        # The graph shoots: the keys only name the moments (On Move Key compares KeyTag)
        tracks += [_event_key(spec['fire'], KEY_FIRE), _event_key(spec['window_end'], KEY_WINDOW_END)]
    else:
        tracks.append(_event_key(spec['fire'], KEY_FIRE, [_fire_action(projectile, spec['adjust'])]))
    if 'chain' in spec:
        c0, c1 = spec['chain']
        tracks.append(f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Tags(StartFrame={c0},EndFrame={c1 + 1},Tags=(GameplayTags=((TagName="{CHAIN_TAG}"))))')
    if 'cancel' in spec:
        c0, c1 = spec['cancel']
        query = unreal.GameplayTagLibrary.make_gameplay_tag_query_match_any_tags(_container(COMMANDS)).export_text()
        tracks.append(f'/Script/GSTNCombatCore.GSTN_TimelineTrack_CancelWindow(StartFrame={c0},EndFrame={c1 + 1},IntoCommands={query},Condition=Always)')
    text = '(Tracks=(' + ','.join(tracks) + ')'
    if name == 'B':
        # The follow-up press must not also start a new special once the move ends: the tag the graph opens on it flushes the buffer
        text += f',BufferResetRules=(/Script/GSTNInputCore.GSTN_BufferResetRule_OnTagAdded(Tag=(TagName="{FOLLOW_UP_TAG}")))'
    data.import_text(text + ')')
    out = data.export_text()
    want = {'Event': 'GSTN_TimelineTrack_Event(', 'Tags': 'GSTN_TimelineTrack_Tags(' if 'chain' in spec else None,
            'Cancel': 'GSTN_TimelineTrack_CancelWindow(' if 'cancel' in spec else None,
            'Fire action': 'GSTN_MoveAction_FireProjectile' if name != 'B' else None, 'Shot asset': 'DA_Example_Shot' if name != 'B' else None,
            'Reset rule': 'GSTN_BufferResetRule_OnTagAdded' if name == 'B' else None}
    missing = [k for k, v in want.items() if v and v not in out]
    if missing:
        raise RuntimeError(f'TL {name}: {missing} missing after the text import: {out[:500]}')
    asset = unreal.load_asset(path) if _exists(path) else _tools().create_asset(f'TL_Example_DoubleShot_{name}', DIR, unreal.GSTN_CombatTimeline,
                                                                                 unreal.DataAssetFactory())
    asset.set_editor_property('data', data)
    log(f'TL {name}: total {spec["total"]}, FAF {spec["faf"]}, keys at {spec["fire"]}{"/" + str(spec["window_end"]) if "window_end" in spec else ""}, '
        f'chain {spec.get("chain")}, cancel {spec.get("cancel")}')
    return asset, True


# ---- the abilities

def _ability(name):
    path = f'{DIR}/GA_Example_DoubleShot_{name}'
    if _exists(path):
        return unreal.load_asset(path), False
    # the plain Blueprint factory: on 5.9 the GameplayAbilities factory ignores parent_class (the class would not be a GSTN_GA_BaseCombat)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.GSTN_GA_BaseCombat.static_class())
    bp = _tools().create_asset(f'GA_Example_DoubleShot_{name}', DIR, unreal.Blueprint, factory)
    if not bp:
        raise RuntimeError(f'could not create {path}')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f'created {path}')
    return bp, True


def _set_default(cdo, names, value):
    """Sets a property of a class default object by its first name that works (Python and reflected names differ for bools and Blueprint vars)"""
    for n in names:
        try:
            cdo.set_editor_property(n, value)
            return True
        except Exception:
            continue
    log(f'WARNING: could not set {names[0]} on {cdo.get_name()}')
    return False


class _Graph:
    """Thin wrapper over UGSTN_MoveGraphAuthoring: every call that fails raises with the library's reason"""

    def __init__(self, bp):
        self.bp, self.lib = bp, unreal.GSTN_MoveGraphAuthoring

    @staticmethod
    def _check(result, what):
        if isinstance(result, str):  # bool function with an out error: Python returns the error text only, empty = success
            result = (not result, result)
        value, error = (result[0], result[1]) if isinstance(result, tuple) else (result, '')
        if not value:
            raise RuntimeError(f'graph: {what}: {error} (raw {result!r})')
        return value

    def event(self, name, x, y):
        return self._check(self.lib.add_event_node(self.bp, name, x, y), name)

    def call(self, cls, fn, x, y, **defaults):
        node = self._check(self.lib.add_call_node(self.bp, cls, fn, x, y), fn)
        for pin, value in defaults.items():
            self.default(node, pin, value)
        return node

    def get(self, var, x, y):
        return self._check(self.lib.add_variable_node(self.bp, var, False, x, y), f'get {var}')

    def set(self, var, x, y):
        return self._check(self.lib.add_variable_node(self.bp, var, True, x, y), f'set {var}')

    def make_sync_int(self, value, x, y):
        node = self._check(self.lib.add_struct_node(self.bp, '/Script/AbilitySystemSimulation.SyncInteger', True, x, y), 'make SyncInteger')
        self.default(node, 'Value', str(value))
        return node

    def break_sync_int(self, x, y):
        return self._check(self.lib.add_struct_node(self.bp, '/Script/AbilitySystemSimulation.SyncInteger', False, x, y), 'break SyncInteger')

    def branch(self, x, y):
        return self._check(self.lib.add_branch_node(self.bp, x, y), 'branch')

    def link(self, a, pin_a, b, pin_b):
        self._check(self.lib.connect_pins(self.bp, a, pin_a, b, pin_b), f'{pin_a or "(value)"} -> {pin_b or "(value)"}')

    def default(self, node, pin, value):
        self._check(self.lib.set_pin_default(self.bp, node, pin, value), f'default {pin}={value}')

    def chain(self, *nodes):
        """Exec links: then of each node (or the named pin of a (node, pin) pair) to execute of the next"""
        for a, b in zip(nodes, nodes[1:]):
            node_a, pin_a = a if isinstance(a, tuple) else (a, 'then')
            self.link(node_a, pin_a, b, 'execute')


TAGS = '/Script/GameplayTags.BlueprintGameplayTagLibrary'
MATH = '/Script/Engine.KismetMathLibrary'
COMBAT = '/Script/GSTNCombatNP.GSTN_CombatSimComponent'
SHOTS = '/Script/GSTNProjectileNP.GSTN_ProjectileMoveLibrary'


def _set_state(g, value, x, y):
    s = g.set('FollowUpState', x, y)
    g.link(g.make_sync_int(value, x - 200, y + 140), '', s, 'FollowUpState')
    return s


def _fire(g, adjust_var, x, y):
    fire = g.call(SHOTS, 'FireMoveProjectile', x, y)
    g.link(g.get('Projectile', x - 250, y + 120), '', fire, 'Definition')
    g.link(g.get(adjust_var, x - 250, y + 200), '', fire, 'Adjust')
    return fire


def _key_is(g, key_event, tag, x, y):
    """MatchesTag(KeyTag of On Move Key, tag, exact) -> a Branch; returns the branch"""
    match = g.call(TAGS, 'MatchesTag', x, y + 120, TagTwo=f'(TagName="{tag}")', bExactMatch='true')
    g.link(key_event, 'KeyTag', match, 'TagOne')
    branch = g.branch(x + 300, y)
    g.link(match, 'ReturnValue', branch, 'Condition')
    return branch


def _build_graph_b(bp):
    g = _Graph(bp)
    lib = g.lib
    removed = lib.clear_event_graph(bp)
    for name, kind, default, tip in B_VARIABLES:
        # a bool function with an out error: Python hands back the error text only (empty = success)
        err = lib.set_member_variable(bp, name, kind, default, CATEGORY, tip)
        err = err[1] if isinstance(err, tuple) else err
        if err:
            raise RuntimeError(f'graph: variable {name}: {err}')
    errors, messages = lib.compile_and_report(bp)
    if errors:
        raise RuntimeError(f'B with variables only does not compile: {messages[:3]}')

    # 1. On Move Started: FollowUpState = 0 (the instance is reused by the next activation)
    started = g.event('K2_OnMoveStarted', 0, 0)
    g.chain(started, _set_state(g, 0, 400, 0))

    # 2. On Move Key: "Fire" -> first shot, FollowUpState = 1; "WindowEnd" -> FollowUpState = 2
    key = g.event('K2_OnMoveKey', 0, 400)
    is_fire = _key_is(g, key, KEY_FIRE, 300, 400)
    g.chain(key, is_fire)
    fire1 = _fire(g, 'FirstShotAdjust', 1000, 300)
    g.chain((is_fire, 'then'), fire1, _set_state(g, 1, 1500, 300))
    is_end = _key_is(g, key, KEY_WINDOW_END, 1000, 700)
    g.chain((is_fire, 'else'), is_end, _set_state(g, 2, 1700, 700))

    # 3. On Move Frame (bTickMoveInBlueprint): FollowUpState == 1 and the special entered again within FollowUpBufferFrames ->
    #    FollowUpState = 2, open the follow-up tag window (the buffer reset rule drops the press), wait SecondShotDelayFrames
    frame = g.event('K2_OnMoveFrame', 0, 1200)
    state = g.break_sync_int(300, 1350)
    g.link(g.get('FollowUpState', 100, 1350), '', state, '')
    armed = g.call(MATH, 'EqualEqual_IntInt', 550, 1350, B='1')
    g.link(state, 'Value', armed, 'A')
    component = g.call('', 'GetCombatComponent', 100, 1550)
    asks = []
    for i, var in enumerate(('FollowUpCommand', 'FollowUpCommandAlt')):
        ask = g.call(COMBAT, 'WasCommandEnteredWithin', 550, 1500 + 200 * i)
        g.link(component, 'ReturnValue', ask, 'self')
        g.link(g.get(var, 300, 1500 + 200 * i), '', ask, 'Command')
        g.link(g.get('FollowUpBufferFrames', 300, 1580 + 200 * i), '', ask, 'Frames')
        asks.append(ask)
    either = g.call(MATH, 'BooleanOR', 850, 1550)
    g.link(asks[0], 'ReturnValue', either, 'A')
    g.link(asks[1], 'ReturnValue', either, 'B')
    both = g.call(MATH, 'BooleanAND', 1050, 1400)
    g.link(armed, 'ReturnValue', both, 'A')
    g.link(either, 'ReturnValue', both, 'B')
    take = g.branch(1250, 1200)
    g.link(both, 'ReturnValue', take, 'Condition')
    open_window = g.call('', 'OpenMoveTagWindow', 1900, 1200, Tag=f'(TagName="{FOLLOW_UP_TAG}")', Frames='30')
    wait = g.call('', 'WaitCombatFrames', 2200, 1200)
    g.link(g.get('SecondShotDelayFrames', 2000, 1350), '', wait, 'Frames')
    g.chain(frame, take)
    g.chain((take, 'then'), _set_state(g, 2, 1600, 1200), open_window, wait)

    # 4. On Combat Frames Elapsed: the second shot
    elapsed = g.event('K2_OnCombatFramesElapsed', 0, 1900)
    g.chain(elapsed, _fire(g, 'SecondShotAdjust', 400, 1900))

    errors, messages = lib.compile_and_report(bp)
    for m in messages:
        log(f'B compile {m}')
    if errors:
        raise RuntimeError(f'B graph: {errors} compile error(s): {messages[:3]}')
    rule_errors, findings = lib.check_simulation_rules(bp)
    for f in findings:
        log(f'B sim rules {f}')
    if rule_errors:
        raise RuntimeError(f'B graph breaks the simulation rules: {findings[:3]}')
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, 'double_shot_graph.txt'), 'w') as f:
        f.write('\n'.join(lib.describe_event_graph(bp)) + '\n')
    log(f'B graph built ({removed} old nodes removed), {len(findings)} rule finding(s)')


def _check_b_defaults(cls, projectile):
    """The graph reads its settings from the class defaults: make sure the compile put them there (else set them)"""
    cdo = unreal.get_default_object(cls)
    ok = True
    for names, value in ((('Projectile', 'projectile'), projectile),):
        try:
            current = cdo.get_editor_property(names[0])
        except Exception:
            current = None
        if current != value:
            ok = _set_default(cdo, names, value) and ok
    for name, kind, default, _tip in B_VARIABLES:
        try:
            cdo.get_editor_property(name)
        except Exception:
            log(f'WARNING: B class default {name} not readable from Python; check Class Defaults by eye')
    return ok


# ---- the kits

def _binding_for(binding, cls, chain):
    """A copy of binding for cls; chain None = as is, True = requires CHAIN_TAG, False = ignores it"""
    out = binding.copy()
    req = out.get_editor_property('requirements')
    for prop, wanted in (('require_tags', chain is True), ('ignore_tags', chain is False)):
        req.set_editor_property(prop, _container([t for t in _names(req.get_editor_property(prop)) if t != CHAIN_TAG] + [CHAIN_TAG] * wanted))
    out.set_editor_property('requirements', req)
    out.set_editor_property('ability', cls)
    return out


def _command(binding):
    return str(binding.get_editor_property('command').get_editor_property('tag_name'))


def _kit(which, classes, force):
    name, label, kit_tag = KITS[which]
    path = f'{EX}/{name}'
    created = not _exists(path)
    if created and not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_KIT, path):
        raise RuntimeError(f'could not copy {SOURCE_KIT} to {path} (run setup_designer_examples.py first)')
    kit = unreal.load_asset(path)
    if not (created or force):
        return kit, False
    kit.set_editor_property('display_name', label)
    kit.set_editor_property('kit_tags', _container([kit_tag]))
    source = list(unreal.load_asset(SOURCE_KIT).get_editor_property('kit_bindings'))
    specials = [b for b in source if _command(b) in COMMANDS]
    if not specials:
        raise RuntimeError(f'{SOURCE_KIT} binds no special: nothing to replace')
    out = [b for b in source if _command(b) not in COMMANDS]
    for b in specials:
        out += [_binding_for(b, classes['A1'], False), _binding_for(b, classes['A2'], True)] if which == 'A' else [_binding_for(b, classes['B'], None)]
    kit.set_editor_property('kit_bindings', out)
    log(f'{name}: ' + '; '.join(f'{_command(b)} +{_names(b.get_editor_property("requirements").get_editor_property("require_tags"))} '
                                 f'-> {str(b.get_editor_property("ability")).rsplit(".", 1)[-1]}' for b in out if _command(b) in COMMANDS))
    return kit, True


def run(force=False):
    del _lines[:]
    ok = False
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(DIR):
            unreal.EditorAssetLibrary.make_directory(DIR)
        if not hasattr(unreal, 'GSTN_MoveGraphAuthoring') or not hasattr(unreal, 'GSTN_ProjectileDefinition'):
            raise RuntimeError('GSTN_MoveGraphAuthoring or GSTN_ProjectileDefinition missing: build the CP3 branch first')
        for t in (CHAIN_TAG, FOLLOW_UP_TAG, KEY_FIRE, KEY_WINDOW_END):
            _tag(t)
        saves = []
        projectile, dirty = _projectile(force)
        if dirty:
            saves.append(projectile)
        classes = {}
        for name, spec in TIMELINES.items():
            timeline, t_dirty = _timeline(name, spec, projectile, force)
            bp, created = _ability(name)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/GA_Example_DoubleShot_{name}')
            classes[name] = cls
            if created or force or t_dirty:
                cdo = unreal.get_default_object(cls)
                _set_default(cdo, ('timeline_asset', 'TimelineAsset'), timeline)
                if name == 'B':
                    _set_default(cdo, ('tick_move_in_blueprint', 'bTickMoveInBlueprint'), True)
            if name == 'B' and (created or force):
                _build_graph_b(bp)
                cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/GA_Example_DoubleShot_B')
                classes[name] = cls
                cdo = unreal.get_default_object(cls)
                _set_default(cdo, ('timeline_asset', 'TimelineAsset'), timeline)
                _set_default(cdo, ('tick_move_in_blueprint', 'bTickMoveInBlueprint'), True)
                _check_b_defaults(cls, projectile)
            saves += [a for a in (timeline if t_dirty else None, bp if (created or force or t_dirty) else None) if a]
        for which in ('A', 'B'):
            kit, dirty = _kit(which, classes, force)
            if dirty:
                saves.append(kit)
        for asset in saves:
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        log(f'saved {len(saves)}: ' + ', '.join(a.get_name() for a in saves))
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, 'setup_double_shot.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else 'no output']:
        print(line)
    return ok


if __name__ == '__main__':
    run()
