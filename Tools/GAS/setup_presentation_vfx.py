"""Polish M2: the Niagara systems of the VFX kit v1 (/GSTNPresentation/VFX/NS_GSTN_*) and the default presentation table.

Style sheet handoffs/stile-sheet-vfx-review.md: few big shapes (puffs 3-7, hit 4-8 shards + 1 flash + 1 ring, cap 12 per burst),
fast start and high drag, CPU sim, sprite/ribbon renderers, <= 3 emitters per system, fixed bounds, nothing replicated.
The look of every particle over its life (birth at 60%, shrink, erosion, colour ramp) is in the material instances
(setup_presentation_vfx_materials.py, run it first): the systems only spawn, place, push and slow the particles.
Every system exposes User.Intensity (float, the table's intensity) and User.Tint (colour, multiplies Particle Color: a kit or skin
recolours an effect from its table row). One-shots run in local space (scaled and oriented by the cue); sustained effects (skid,
sprint lines, tumble) run in world space so their particles stay behind.

Run in the open editor: python -I Tools/GAS/run_in_editor.py setup_presentation_vfx      (idempotent: systems are rebuilt)
"""
import unreal

ROOT = "/GSTNPresentation/VFX"
MATS = f"{ROOT}/Materials"
TPL = "/Niagara/DefaultAssets/Templates/Emitters"
MOD = "/Niagara/Modules"
L = unreal.GSTN_VfxAuthoringLibrary
EAL = unreal.EditorAssetLibrary
ATL = unreal.AssetToolsHelpers.get_asset_tools()
ERRORS = []


def _mi(name):
    return unreal.load_asset(f"{MATS}/MI_GSTN_VFX_{name}")


class Emitter:
    def __init__(self, system, template, name):
        self.system = system
        self.name = L.add_emitter(system, f"{TPL}/{template}", name)
        if self.name.startswith("!"):
            ERRORS.append(f"{name}: {self.name}")

    def set(self, stack, module, inp, value):
        if isinstance(value, (list, tuple)):
            value = ",".join(str(v) for v in value)
        err = L.set_input(self.system, self.name, stack, module, inp, str(value))
        if err:
            ERRORS.append(f"{self.name}.{module}.{inp}: {err}")
        return self

    def link(self, stack, module, inp, param):
        err = L.link_module_input(self.system, self.name, stack, module, inp, param)
        if err:
            ERRORS.append(f"{self.name}.{module}.{inp} -> {param}: {err}")
        return self

    def add(self, stack, path):
        err = L.add_module(self.system, self.name, stack, f"{MOD}/{path}")
        if err:
            ERRORS.append(f"{self.name} + {path}: {err}")
        return self

    def off(self, stack, module):
        err = L.set_module_enabled(self.system, self.name, stack, module, False)
        if err:
            ERRORS.append(f"{self.name} off {module}: {err}")
        return self

    def settings(self, local_space, bounds=300.0):
        err = L.set_emitter_settings(self.system, self.name, local_space, bounds)
        if err:
            ERRORS.append(f"{self.name} settings: {err}")
        return self

    # Common particle setup -------------------------------------------------------------------------------------------------------

    def particle(self, life, size, rotation=False, non_uniform=None):
        """Lifetime range (s), uniform size range (cm) or non-uniform (x, y), colour from User.Tint"""
        s = "ParticleSpawn"
        self.set(s, "InitializeParticle", "Lifetime Mode", "Random")
        self.set(s, "InitializeParticle", "Lifetime Min", life[0]).set(s, "InitializeParticle", "Lifetime Max", life[1])
        if non_uniform:
            self.set(s, "InitializeParticle", "Sprite Size Mode", "Non-Uniform")
            self.set(s, "InitializeParticle", "Sprite Size", non_uniform)
        else:
            self.set(s, "InitializeParticle", "Sprite Size Mode", "Random Uniform")
            self.set(s, "InitializeParticle", "Uniform Sprite Size Min", size[0])
            self.set(s, "InitializeParticle", "Uniform Sprite Size Max", size[1])
        self.set(s, "InitializeParticle", "Sprite Rotation Mode", "Random" if rotation else "Unset")
        self.set(s, "InitializeParticle", "Color Mode", "Direct Set")
        self.link(s, "InitializeParticle", "Color", "User.Tint")
        return self

    def burst(self, count):
        """One burst per activation: the burst template loops by default (a new burst every loop while the system lives)"""
        self.set("EmitterUpdate", "EmitterState", "Loop Behavior", "Once")
        self.set("EmitterUpdate", "EmitterState", "Loop Duration", 0.6)
        return self.set("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", int(count))

    def sphere(self, radius, scale=(1, 1, 1), offset=(0, 0, 0)):
        s = "ParticleSpawn"
        self.set(s, "ShapeLocation", "Shape Primitive", "Sphere").set(s, "ShapeLocation", "Sphere Radius", radius)
        self.set(s, "ShapeLocation", "Non Uniform Scale", scale).set(s, "ShapeLocation", "Offset", offset)
        return self

    def cone(self, axis, angle, speed):
        s = "ParticleSpawn"
        self.set(s, "AddVelocity", "Velocity Mode", "In Cone")
        self.set(s, "AddVelocity", "Cone Axis", axis).set(s, "AddVelocity", "Cone Angle", angle)
        self.set(s, "AddVelocity", "Velocity Speed", speed)
        return self

    def radial(self, speed):
        self.set("ParticleSpawn", "AddVelocity", "Velocity Mode", "From Point")
        return self.set("ParticleSpawn", "AddVelocity", "Velocity Speed", speed)

    def drag(self, value):
        return self.add("ParticleUpdate", "Update/Forces/Drag").set("ParticleUpdate", "Drag", "Drag", value)

    def gravity(self, z):
        return self.add("ParticleUpdate", "Update/Forces/GravityForce").set("ParticleUpdate", "GravityForce", "Gravity", (0, 0, z))

    def facing(self, vector):
        """Sprites lie flat with their normal along vector (local space): rings on the ground or on a wall"""
        self.add("ParticleSpawn", "Update/Renderers/Sprite/SpriteFacingAndAlignment")
        self.set("ParticleSpawn", "SpriteFacingAndAlignment", "Sprite Facing", vector)
        self.set("ParticleSpawn", "SpriteFacingAndAlignment", "Facing Coordinate Space", 2)  # Local: turns with the cue
        renderer = L.get_renderer(self.system, self.name, 0)
        renderer.set_editor_property("facing_mode", unreal.NiagaraSpriteFacingMode.CUSTOM_FACING_VECTOR)
        return self

    def material(self, mi, velocity_aligned=False):
        renderer = L.get_renderer(self.system, self.name, 0)
        if renderer is None:
            ERRORS.append(f"{self.name}: no renderer")
            return self
        renderer.set_editor_property("material", _mi(mi))
        if velocity_aligned:
            renderer.set_editor_property("alignment", unreal.NiagaraSpriteAlignment.VELOCITY_ALIGNED)
        return self


def _system(name):
    path = f"{ROOT}/{name}"
    system = unreal.load_asset(path) if EAL.does_asset_exist(path) else ATL.create_asset(name, ROOT, unreal.NiagaraSystem,
                                                                                         unreal.NiagaraSystemFactoryNew())
    L.clear_emitters(system)  # rebuilt in place: the tables keep their reference
    L.add_user_float(system, "Intensity", 1.0)
    L.add_user_color(system, "Tint", unreal.LinearColor(1, 1, 1, 1))
    L.set_system_fixed_bounds(system, 300.0)
    return system


def _burst_puffs(system, name, count, life, size, speed, drag, cone=None, sphere=(8, (1, 1, 0.4), (0, 0, 0)), gravity=None, ring=None):
    """ring=(radius, z): the puffs start on a ground ring and fly outward, so 3-5 balls read apart instead of one lump"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(count).particle(life, size)
    if ring:
        e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Ring / Disc").set("ParticleSpawn", "ShapeLocation", "Ring Radius", ring[0])
        e.set("ParticleSpawn", "ShapeLocation", "Offset", (0, 0, ring[1]))
    else:
        e.sphere(*sphere)
    if cone:
        e.cone(cone[0], cone[1], speed)
    else:
        e.radial(speed)
    e.drag(drag)
    if gravity:
        e.gravity(gravity)
    return e.material("DustBall")


def _ring(system, name, mi, life, size, facing=(0, 0, 1)):
    """One flat particle, no motion: the material widens and thins it over its life"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(1).particle((life, life), (size, size)).sphere(0.01).radial(0.0).facing(facing)
    return e.material(mi)


# The systems --------------------------------------------------------------------------------------------------------------------

def run_start_dust():
    """RunStart, spawned at the feet, X along the run: 4 puffs kicked backwards and a little up"""
    s = _system("NS_GSTN_RunStartDust")
    if s:
        _burst_puffs(s, "Puffs", 4, (0.30, 0.42), (30, 42), 380, 7.0, cone=((-1, 0, 0.35), 45),
                     sphere=(10, (1, 1, 0.3), (-15, 0, 8)))
    return s


def skid_dust():
    """Skid (sustained, attached to the feet, world space): one puff every 3-4 frames, rising a little"""
    s = _system("NS_GSTN_SkidDust")
    if s:
        e = Emitter(s, "Fountain", "Trail").settings(False)
        e.off("ParticleUpdate", "ScaleColor")
        e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 17.0)
        e.particle((0.30, 0.42), (26, 36)).sphere(8, (1, 1, 0.3), (0, 0, 8)).cone((0, 0, 1), 50, 80)
        e.drag(5.0).material("DustBall")
    return s


def jump_puff():
    """Jump from the ground: 3 small puffs out to the sides and a thin ring on the ground (smaller than the landing)"""
    s = _system("NS_GSTN_JumpPuff")
    if s:
        _burst_puffs(s, "Puffs", 3, (0.25, 0.32), (18, 24), 220, 8.0, ring=(18, 6))
        _ring(s, "Ring", "JumpRing", 0.26, 90)
    return s


def land_ring():
    """Landing (scaled by the fall speed): a widening ring and 5 radial puffs"""
    s = _system("NS_GSTN_LandRing")
    if s:
        _ring(s, "Ring", "LandRing", 0.30, 150)
        _burst_puffs(s, "Puffs", 5, (0.30, 0.40), (20, 26), 340, 7.0, ring=(26, 8))
    return s


def speed_lines():
    """Sprint 2 (sustained, attached, world space): 3-5 white tapered streaks at the sides, left behind as the runner moves on"""
    s = _system("NS_GSTN_SpeedLines")
    if s:
        e = Emitter(s, "Fountain", "Lines").settings(False)
        e.off("ParticleUpdate", "ScaleColor")
        e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 12.0)
        # The streak fills ~22 % of its cell's width: a 40 cm wide sprite draws a ~9 cm line
        e.particle((0.22, 0.30), None, non_uniform=(40, 110))
        e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Ring / Disc")
        e.set("ParticleSpawn", "ShapeLocation", "Ring Radius", 60.0).set("ParticleSpawn", "ShapeLocation", "Offset", (0, 0, 90))
        # Streaks point along the run: a small share of the owner's velocity gives the alignment, drag holds them back
        e.set("ParticleSpawn", "AddVelocity", "Velocity Mode", "Linear")
        e.link("ParticleSpawn", "AddVelocity", "Velocity", "Engine.Owner.Velocity")
        e.set("ParticleSpawn", "AddVelocity", "Velocity Speed Scale", 0.25)
        e.drag(3.0).material("SpeedStreak", velocity_aligned=True)
    return s


def hit_spark():
    """Hit (X along the push): 1 flash (2 frames), 1 ring, up to 8 star shards; User.Intensity = share of the 8 that spawn"""
    s = _system("NS_GSTN_HitSpark")
    if s:
        flash = Emitter(s, "OmnidirectionalBurst", "Flash").settings(True)
        flash.off("ParticleUpdate", "ScaleColor")
        # Everything sits on the contact side (local -X = toward the attacker, the cue turns X along the push), not over the torso
        flash.burst(1).particle((0.034, 0.034), (70, 70)).sphere(0.01, offset=(-30, 0, 0)).radial(0.0).material("HitFlash")
        ring = Emitter(s, "OmnidirectionalBurst", "Ring").settings(True)
        ring.off("ParticleUpdate", "ScaleColor")
        ring.burst(1).particle((0.18, 0.18), (90, 90)).sphere(0.01, offset=(-30, 0, 0)).radial(0.0).material("HitRing")
        stars = Emitter(s, "OmnidirectionalBurst", "Stars").settings(True)
        stars.off("ParticleUpdate", "ScaleColor")
        stars.burst(8).set("EmitterUpdate", "SpawnBurst_Instantaneous", "Use Spawn Probability", True)
        stars.link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Probability", "User.Intensity")
        stars.particle((0.18, 0.28), (26, 40), rotation=True).sphere(6, offset=(-20, 0, 0)).cone((1, 0, 0), 60, 650).drag(8.0)
        stars.material("HitStar")
    return s


def impact():
    """Wall, ceiling or ground contact of a launched victim (X along the surface normal): the landing recipe, turned"""
    s = _system("NS_GSTN_Impact")
    if s:
        _ring(s, "Ring", "LandRing", 0.30, 160, facing=(1, 0, 0))
        _burst_puffs(s, "Puffs", 5, (0.30, 0.40), (24, 32), 300, 7.0, sphere=(12, (0.05, 1, 1), (4, 0, 0)))
    return s


def tumble_trail():
    """Tumble (sustained, attached to the pelvis, world space): a short ribbon and a ball every 4-5 frames"""
    s = _system("NS_GSTN_TumbleTrail")
    if s:
        balls = Emitter(s, "Fountain", "Balls").settings(False)
        balls.off("ParticleUpdate", "ScaleColor")
        balls.set("EmitterUpdate", "SpawnRate", "SpawnRate", 13.0)
        balls.particle((0.28, 0.34), (24, 30)).sphere(4).cone((0, 0, 1), 180, 10).material("TrailBall")
        ribbon = Emitter(s, "Fountain", "Ribbon").settings(False)
        ribbon.off("ParticleUpdate", "ScaleColor")
        ribbon.set("EmitterUpdate", "SpawnRate", "SpawnRate", 40.0)
        ribbon.particle((0.16, 0.16), (10, 10)).sphere(0.1).cone((0, 0, 1), 0, 0)
        ribbon.set("ParticleSpawn", "InitializeParticle", "Ribbon Width Mode", "Direct Set")
        ribbon.set("ParticleSpawn", "InitializeParticle", "Ribbon Width", 16.0)
        renderer = L.set_ribbon_renderer(s, ribbon.name)
        if renderer:
            renderer.set_editor_property("material", _mi("TrailRibbon"))
        else:
            ERRORS.append("Ribbon: no renderer")
    return s


SYSTEMS = [run_start_dust, skid_dust, jump_puff, land_ring, speed_lines, hit_spark, impact, tumble_trail]


def run():
    del ERRORS[:]
    built = []
    for make in SYSTEMS:
        system = make()
        if system is None:
            continue
        L.compile_and_wait(system)
        EAL.save_loaded_asset(system)
        built.append(system.get_name())
    probe = unreal.load_asset(f"{ROOT}/NS_GSTN_JumpPuff")
    built += [l[:400] for l in L.describe_emitter(probe, "Puffs").splitlines() if "EmitterState" in l]
    out = unreal.Paths.project_saved_dir() + "Logs/ci/m2-vfx-setup.txt"
    open(out, "w", encoding="utf-8").write("\n".join(built + ["ERRORS:"] + ERRORS))
    return f"{len(built)} systems, {len(ERRORS)} errors (Saved/Logs/ci/m2-vfx-setup.txt)"
