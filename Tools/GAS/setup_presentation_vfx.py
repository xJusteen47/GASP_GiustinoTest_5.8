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

    def add_force(self, path, module):
        """A force goes before SolveForcesAndVelocity: appended after it, the solver never sees it and the particle keeps its speed (review
        r2: the shards and the run puffs slid at a constant speed). A template that already has the module keeps its own"""
        modules = L.list_modules(self.system, self.name, "ParticleUpdate")
        if module in modules.split(" > "):
            return self
        err = L.add_module_before(self.system, self.name, "ParticleUpdate", f"{MOD}/{path}", "SolveForcesAndVelocity")
        if err:
            ERRORS.append(f"{self.name} + {path}: {err} [{modules}]")
        return self

    def per_distance(self, spacing):
        """Sustained effects: one particle every <spacing> cm the owner moves, instead of a rate (review r2: a skid that slows down piled its
        balls on the feet, a fast tumble left one ball every 2 m)"""
        self.off("EmitterUpdate", "SpawnRate")
        self.add("EmitterUpdate", "Emitter/SpawnPerUnit")
        return self.set("EmitterUpdate", "SpawnPerUnit", "Spawn Spacing", spacing)

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
        # Some templates carry their own GravityForce and Drag before the solver: no gravity unless gravity() asks for it, no drag unless
        # drag() sets it (r3: these template modules were unreachable before, their defaults were never known)
        update = L.list_modules(self.system, self.name, "ParticleUpdate").split(" > ")
        if "GravityForce" in update:
            self.set("ParticleUpdate", "GravityForce", "Gravity", (0, 0, 0))
        if "Drag" in update:
            self.set("ParticleUpdate", "Drag", "Drag", 0.0)
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
        return self.shape_offset(offset).set(s, "ShapeLocation", "Non Uniform Scale", scale)

    def shape_offset(self, offset):
        """The template's ShapeLocation ignores Offset (Offset Mode None): turn it on, then set it"""
        self.set("ParticleSpawn", "ShapeLocation", "Offset Mode", "Default")
        return self.set("ParticleSpawn", "ShapeLocation", "Offset", offset)

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
        return self.add_force("Update/Forces/Drag", "Drag").set("ParticleUpdate", "Drag", "Drag", value)

    def gravity(self, z):
        return self.add_force("Update/Forces/GravityForce", "GravityForce").set("ParticleUpdate", "GravityForce", "Gravity", (0, 0, z))

    def facing(self, vector):
        """Sprites lie flat with their normal along vector (local space): rings on the ground or on a wall"""
        self.add("ParticleSpawn", "Update/Renderers/Sprite/SpriteFacingAndAlignment")
        self.set("ParticleSpawn", "SpriteFacingAndAlignment", "Sprite Facing", vector)
        self.set("ParticleSpawn", "SpriteFacingAndAlignment", "Facing Coordinate Space", 2)  # Local: turns with the cue
        renderer = L.get_renderer(self.system, self.name, 0)
        renderer.set_editor_property("facing_mode", unreal.NiagaraSpriteFacingMode.CUSTOM_FACING_VECTOR)
        return self

    def sized(self):
        """Sprite size times User.SizeScale (the cue's intensity): Niagara never scales a sprite by its component (review r1: the three hit
        sizes looked the same). In the spawn stage, after InitializeParticle: applied once"""
        self.add("ParticleSpawn", "Update/Size/ScaleSpriteSize")
        # Its mode switch defaults to a curve mode, which ignores Scale Factor (r2b: the scaled sprites still vanished)
        self.set("ParticleSpawn", "ScaleSpriteSize", "Scale Sprite Size Mode", "Non-Uniform")
        # Its 'Initial Sprite Size' defaults to Particles.Initial.SpriteSize, still zero in the spawn stage (r2a: every scaled sprite vanished)
        self.link("ParticleSpawn", "ScaleSpriteSize", "Initial Sprite Size", "Particles.SpriteSize")
        return self.link("ParticleSpawn", "ScaleSpriteSize", "Scale Factor", "User.SizeScale")

    def toward_camera(self, amount):
        """Moves the particles toward the camera when drawn: shards born behind the victim's body stay in front of it"""
        self.add("ParticleUpdate", "Update/Camera/CameraOffset")
        return self.set("ParticleUpdate", "CameraOffset", "Camera Offset Amount", amount)

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
    L.add_user_color(system, "AccentTint", unreal.LinearColor(1, 1, 1, 1))
    L.add_user_vector2(system, "SizeScale", unreal.Vector2D(1, 1))
    L.add_user_float(system, "Strong", 0.0)
    # Particle counts set by the cue (FGSTN_PresentationCue::CountMax / StrongCount), linked to a burst's Spawn Count
    L.add_user_int(system, "Count", 8)
    L.add_user_int(system, "StrongCount", 0)
    L.set_system_fixed_bounds(system, 300.0)
    return system


# Sizes: the atlas draws a shape at 0.84 of its cell, so a ball is 0.77 of its sprite, a ring 0.78 (outer), a thin ring 0.77, a star 0.8.
# Sizes below are sprite sizes at intensity 1; the comments give the drawn size. Ground effects sit above the floor (a flat ring on the
# floor plane z-fights with it and vanishes: review r1, the landing ring was never seen)
BALL = 1.0 / 0.77


def _burst_puffs(system, name, count, life, size, speed, drag, cone=None, sphere=(8, (1, 1, 0.4), (0, 0, 0)), gravity=None, ring=None,
                 rim=False, box=None):
    """ring=(radius, z): the puffs start on a ground ring and fly outward, so 3-6 balls read apart instead of one lump. rim: on the sphere's
    surface only (with a flat scale, the rim of a disc). box=(size, offset): spawned in a box"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(count).particle(life, size).sized()
    if ring:
        e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Ring / Disc").set("ParticleSpawn", "ShapeLocation", "Ring Radius", ring[0])
        e.shape_offset((0, 0, ring[1]))
    elif box:
        e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Box / Plane").set("ParticleSpawn", "ShapeLocation", "Box Size", box[0])
        e.shape_offset(box[1])
    else:
        e.sphere(*sphere)
        if rim:
            e.set("ParticleSpawn", "ShapeLocation", "Surface Only", True)
    if cone:
        e.cone(cone[0], cone[1], speed)
    else:
        e.radial(speed)
    e.drag(drag)
    if gravity:
        e.gravity(gravity)
    return e.material("DustBall")


def _ring(system, name, mi, life, size, facing=(0, 0, 1), offset=(0, 0, 3)):
    """One flat particle, no motion: the material widens and thins it over its life. 3 cm off the surface"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(1).particle((life, life), (size, size)).sized().sphere(0.01, offset=offset).radial(0.0).facing(facing)
    return e.material(mi)


# The systems --------------------------------------------------------------------------------------------------------------------

def run_start_dust():
    """RunStart, spawned at the feet, X along the run: 4 separate balls (20 / 16 / 12 cm) kicked backwards in a 60 degree fan, spread
    across the feet. Two emitters, two speeds (review r2: one lump sliding back at a constant speed): the big pair is kicked harder and
    stops ~50 cm back, the small pair ~30 cm (drag 6)"""
    s = _system("NS_GSTN_RunStartDust")
    if s:
        # The side camera sees X and Z only (Y is depth): the two pairs must part along X and in height, not only across (probe r3: same X)
        _burst_puffs(s, "Big", 2, (0.30, 0.34), (16 * BALL, 20 * BALL), 380, 6.0, cone=((-1, 0, 0.55), 50),
                     box=((20, 44, 2), (-14, 0, 12)))
        _burst_puffs(s, "Small", 2, (0.26, 0.30), (12 * BALL, 13 * BALL), 170, 6.0, cone=((-1, 0, 0.05), 50),
                     box=((20, 44, 2), (-6, 0, 8)))
    return s


def skid_dust():
    """Skid (sustained, attached to the feet, world space): a ball (14-18 cm) every 37 cm slid, left behind as a row, max 4-5 alive.
    By distance: a nearly stopped skid adds no ball on the feet (review r2). Centred 15 cm up, so the floor never cuts a ball"""
    s = _system("NS_GSTN_SkidDust")
    if s:
        e = Emitter(s, "Fountain", "Trail").settings(False)
        e.off("ParticleUpdate", "ScaleColor")
        e.per_distance(37.0)
        e.particle((0.28, 0.32), (14 * BALL, 18 * BALL)).sphere(3, (1, 1, 0.3), (0, 0, 15)).cone((0, 0, 1), 30, 40)
        e.drag(6.0).material("DustBall")
    return s


def jump_puff():
    """Jump from the ground (at the last grounded location): 3 small balls out to the sides and a thin ring on the floor"""
    s = _system("NS_GSTN_JumpPuff")
    if s:
        _burst_puffs(s, "Puffs", 3, (0.25, 0.32), (17 * BALL, 21 * BALL), 220, 8.0, ring=(18, 11))
        _ring(s, "Ring", "JumpRing", 0.26, 110)
    return s


def land_ring():
    """Landing (scaled by the fall speed; the knockdown plays it at 1.2): a ring 60 -> 140 cm in 6 frames that thins out and dies at 18
    frames (300 ms), and 6 radial balls of 16-20 cm born on a 38 cm circle (off the feet, review r2) that stop ~40 cm further out"""
    s = _system("NS_GSTN_LandRing")
    if s:
        _ring(s, "Ring", "GroundRing", 0.30, 182)
        _burst_puffs(s, "Puffs", 6, (0.30, 0.40), (16 * BALL, 20 * BALL), 300, 7.0, ring=(38, 10))
    return s


def _lines(system, name, side):
    """One side of the body: spawned in a box 50-80 cm out from the axis, 60-150 cm high, sliding backwards (-Y)"""
    e = Emitter(system, "Fountain", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 8.0)
    # The streak is 0.185 of its sprite wide and 0.75 long: 24 x 187 draws ~4.5 x 140 cm
    e.particle((0.24, 0.30), None, non_uniform=(24, 187))
    e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Box / Plane")
    e.set("ParticleSpawn", "ShapeLocation", "Box Size", (30, 60, 90))
    e.shape_offset((65 * side, -20, 105))
    e.set("ParticleSpawn", "AddVelocity", "Velocity Mode", "Linear").set("ParticleSpawn", "AddVelocity", "Velocity", (0, -450, 0))
    return e.drag(2.0).material("SpeedStreak", velocity_aligned=True)


def speed_lines():
    """Sprint 2 (sustained, attached to the root bone, LOCAL space): 3-5 white tapered streaks only at the body's sides (never over it,
    review r1: from the game camera they covered the legs). The mannequin faces +Y in mesh space, so the sides are +-X"""
    s = _system("NS_GSTN_SpeedLines")
    if s:
        _lines(s, "Left", -1)
        _lines(s, "Right", 1)
    return s


def hit_spark():
    """Hit (X along the push): 1 flash, 1 ring, up to 8 star shards, and 3 accent stars on a strong hit. Sizes at intensity 1 (the cue
    gives 0.56 / 0.74 / 1.1 for S / M / L): flash 64 cm, ring 105 cm, shards 24 cm -> S 36 / 58 / 13, M 47 / 77 / 18, L 70 / 115 / 26.
    User.Intensity = share of the 8 shards that spawn (4 / 6 / 8)"""
    s = _system("NS_GSTN_HitSpark")
    if s:
        flash = Emitter(s, "OmnidirectionalBurst", "Flash").settings(True)
        flash.off("ParticleUpdate", "ScaleColor")
        # Everything sits on the contact side (local -X = toward the attacker, the cue turns X along the push), not over the torso
        flash.burst(1).particle((0.034, 0.034), (83, 83)).sized().sphere(0.01, offset=(-30, 0, 0)).radial(0.0).material("HitFlash")
        ring = Emitter(s, "OmnidirectionalBurst", "Ring").settings(True)
        ring.off("ParticleUpdate", "ScaleColor")
        ring.burst(1).particle((0.18, 0.18), (134, 134)).sized().sphere(0.01, offset=(-30, 0, 0)).radial(0.0).material("HitRing")
        # Shards: an 80 degree fan along the push, fast and braked hard, drawn 40 cm toward the camera so the victim never hides them.
        # 2000 cm/s braked by drag 12: ~55 cm out by the 3rd frame, ~160 cm at rest (times the hit's scale); hidden on the flash frame
        # (material AppearAt). Review r2: 600 cm/s with no working drag left a lump of stars on the victim
        stars = Emitter(s, "OmnidirectionalBurst", "Stars").settings(True)
        stars.off("ParticleUpdate", "ScaleColor")
        # Count: the cue sets User.Count = 8 x intensity (4 / 6 / 8). Speed 3000 braked by drag 14 (in-game probe r3: drag 12 at 2000 gave
        # ~40 cm by frame 3 at scale 1, ~23 cm on a small hit); cone 120 = a fan of about +-45 degrees (40 measured +-14)
        stars.burst(8).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.Count")
        stars.particle((0.20, 0.28), (27, 33), rotation=True).sized().sphere(4, offset=(-25, 0, 0)).cone((1, 0, 0), 120, 3000).drag(14.0)
        stars.toward_camera(60.0).material("HitStar")
        # Strong hit only (User.Strong, set by the cue from its StrongIntensity): 3 five-point stars in the row's accent colour
        accent = Emitter(s, "OmnidirectionalBurst", "Accent").settings(True)
        accent.off("ParticleUpdate", "ScaleColor")
        accent.burst(3).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.StrongCount")
        accent.particle((0.22, 0.30), (24, 30), rotation=True).sized().sphere(4, offset=(-25, 0, 0)).cone((1, 0, 0), 140, 2600).drag(14.0)
        accent.link("ParticleSpawn", "InitializeParticle", "Color", "User.AccentTint")
        accent.toward_camera(60.0).material("HitAccent")
    return s


def impact():
    """Wall, ceiling or ground contact of a launched victim (X along the surface normal): the landing recipe, turned, 3 cm off the surface"""
    s = _system("NS_GSTN_Impact")
    if s:
        _ring(s, "Ring", "LandRing", 0.30, 182, facing=(1, 0, 0), offset=(3, 0, 0))
        # Born on a 40 cm circle on the surface, not in its middle (review r2: overlapping balls over the victim's thighs)
        _burst_puffs(s, "Puffs", 5, (0.30, 0.40), (18 * BALL, 24 * BALL), 300, 7.0, sphere=(40, (0.05, 1, 1), (14, 0, 0)), rim=True)
    return s


def tumble_trail():
    """Tumble (sustained, attached to the pelvis, world space; starts at the launch, after the hitstop): a ball of 18 cm every 60 cm flown
    (3-4 in a row at launch speed; review r2: at a rate they were ~2 m apart), 300 ms. No ribbon: it never read in r1 (decision applied in round 2, see the handover)"""
    s = _system("NS_GSTN_TumbleTrail")
    if s:
        balls = Emitter(s, "Fountain", "Balls").settings(False)
        balls.off("ParticleUpdate", "ScaleColor")
        balls.per_distance(60.0)
        balls.particle((0.28, 0.32), (18 * BALL, 18 * BALL)).sphere(2).cone((0, 0, 1), 180, 10).material("TrailBall")
    return s


def _lin(hex_srgb):
    rgb = [int(hex_srgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb]


# Colour calibration: what an emissive value turns into on screen (eye adaptation inverse, tone curve, the capture's post). One row per
# hue at increasing gains, plus a grey ramp. Captured with PIE_SWATCH=1 (Tools/GASNPPTest/pie_capture_vfx.py), read by vfx_swatch.py
SWATCH_GAINS = [0.7, 1.0, 1.5, 2.2, 3.0, 4.5]
SWATCH_ROWS = [("grey", [1, 1, 1], [0.25, 0.5, 1.0, 2.0, 4.0, 8.0]), ("dust", _lin("FFF6E8"), None), ("dust_shade", _lin("D9C7B0"), None),
               ("hit_end", _lin("FF8A1E"), None), ("hit_mid", _lin("FFD23C"), None), ("accent", _lin("FF3B2F"), None)]
SWATCH_STEP = 26.0


def swatch_cells():
    """[(row, gain, (r, g, b) value, (y, z) local offset)], 6 x 6, centred on the system"""
    cells = []
    for j, (row, base, gains) in enumerate(SWATCH_ROWS):
        for i, k in enumerate(gains or SWATCH_GAINS):
            cells.append((row, k, [c * k for c in base], ((i - 2.5) * SWATCH_STEP, (2.5 - j) * SWATCH_STEP)))
    return cells


def swatch():
    """NS_GSTN_Swatch (calibration only, not in the presentation table): 36 flat discs of 20 cm facing the camera, one per emitter, alive
    30 s, X toward the camera"""
    del ERRORS[:]
    s = _system("NS_GSTN_Swatch")
    if s:
        for n, (_row, _k, value, (y, z)) in enumerate(swatch_cells()):
            e = Emitter(s, "OmnidirectionalBurst", f"S{n:02d}").settings(True)
            e.off("ParticleUpdate", "ScaleColor")
            e.burst(1).set("EmitterUpdate", "EmitterState", "Loop Duration", 30.0)
            e.particle((30.0, 30.0), (26.0, 26.0))
            e.set("ParticleSpawn", "InitializeParticle", "Color", (value[0], value[1], value[2], 1.0))
            e.sphere(0.01, offset=(0, y, z)).radial(0.0).material("Swatch")
        L.compile_and_wait(s)
        EAL.save_loaded_asset(s)
    return f"swatch: {len(swatch_cells())} cells, {len(ERRORS)} errors: {' | '.join(ERRORS)[:600]}"


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
