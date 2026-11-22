"""Polish M2 + VFX v2 + VFX v3: the Niagara systems of the VFX kit (v3: light, smoke and toys, see the block before the systems; run setup_presentation_vfx_v3 first) (/GSTNPresentation/VFX/NS_GSTN_*) and the default presentation table.

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

    # VFX v2: meshes ----------------------------------------------------------------------------------------------------------------

    def mesh(self, shape, look, facing=None, shadow=None):
        """Lit mesh renderer (replaces the template's sprite): SM_GSTN_VFX_<shape> with MI_GSTN_VFX_Toy<look>. facing: "velocity" (shards,
        streaks: the mesh's X along the motion) or "camera" (the hit's ring faces the viewer). shadow: a second renderer, the same particles
        flattened on the floor (MI_GSTN_VFX_ToyShadow<shadow>), its height from User.GroundHeight through dynamic parameter 0"""
        mesh = unreal.load_asset(f"{ROOT}/Meshes/SM_GSTN_VFX_{shape}")
        mode = {None: 0, "velocity": 1, "camera": 2}[facing]
        renderer = L.add_mesh_renderer(self.system, self.name, mesh, unreal.load_asset(f"{MATS}/MI_GSTN_VFX_Toy{look}"), True, mode)
        if renderer is None:
            ERRORS.append(f"{self.name}: no mesh renderer ({shape})")
            return self
        if shadow:
            L.add_mesh_renderer(self.system, self.name, mesh, unreal.load_asset(f"{MATS}/MI_GSTN_VFX_ToyShadow{shadow}"), False, mode)
            self.add("ParticleUpdate", "Update/Material/DynamicMaterialParameters")
            self.link("ParticleUpdate", "DynamicMaterialParameters", MESH_NAMES["dyn0"], "User.GroundHeight")
        return self

    def mesh_particle(self, life, size, rotation=False, scale=(1, 1, 1)):
        """Lifetime (s) and size range in cm (the meshes are 100 cm across, so mesh scale = cm / 100), times a non-uniform shape scale.
        No SizeScale: a local-space mesh follows its component's scale (the cue's intensity), unlike a sprite"""
        s = "ParticleSpawn"
        update = L.list_modules(self.system, self.name, "ParticleUpdate").split(" > ")
        if "GravityForce" in update:
            self.set("ParticleUpdate", "GravityForce", "Gravity", (0, 0, 0))
        if "Drag" in update:
            self.set("ParticleUpdate", "Drag", "Drag", 0.0)
        self.set(s, "InitializeParticle", "Lifetime Mode", "Random")
        self.set(s, "InitializeParticle", "Lifetime Min", life[0]).set(s, "InitializeParticle", "Lifetime Max", life[1])
        self.set(s, "InitializeParticle", MESH_NAMES["scale_mode"], MESH_NAMES["scale_random"])
        self.set(s, "InitializeParticle", MESH_NAMES["scale_min"], [size[0] / 100.0 * k for k in scale])
        self.set(s, "InitializeParticle", MESH_NAMES["scale_max"], [size[1] / 100.0 * k for k in scale])
        if rotation:
            self.set(s, "InitializeParticle", MESH_NAMES["orient_mode"], MESH_NAMES["orient_random"])
        self.set(s, "InitializeParticle", "Color Mode", "Direct Set")
        self.link(s, "InitializeParticle", "Color", "User.Tint")
        return self

    def bounce(self, restitution=0.55, friction=0.6):
        """Bounces on the floor under the event: an analytical plane (User.GroundPoint / GroundNormal in the system's local space, set by
        the cue with one trace per event). No Niagara trace, no depth buffer (toaster, CPU sim). Restitution 0.55 + friction + drag: the
        second bounce is a hop, then the piece rests and pops out"""
        self.add("ParticleUpdate", "Collision/Collision")
        c = "ParticleUpdate"
        for key, value in MESH_NAMES["collision"].items():
            self.set(c, "Collision", key, value)
        self.link(c, "Collision", MESH_NAMES["plane_pos"], "User.GroundPoint")
        self.link(c, "Collision", MESH_NAMES["plane_normal"], "User.GroundNormal")
        self.set(c, "Collision", MESH_NAMES["restitution"], restitution)
        return self.set(c, "Collision", MESH_NAMES["friction"], friction)

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


# VFX v2 (2026-11-17, owner 1A/2A/3A; docs/notes/stile/vfx-stile-v2-astrobot.md): lit glossy meshes instead of sprites. Sizes are the drawn
# size in cm at intensity 1 (the meshes are 100 cm across). Only the hit's flash stays a sprite (v1 master). One-shots are local space: the
# cue's scale (intensity) scales positions and meshes alike. Ground effects sit above the floor.
# Owner playtest 2026-11-17: dust only on run start and skid (the jump keeps its ring, no balls); the sprint lines stop under 450 cm/s
# (table); the hit and impacts are pulled toward the camera (table, TowardCamera).
BALL = 1.0 / 0.77  # v1 sprite sizes (kept for the swatch)

# Input names of the 5.9 stock modules (probe: Saved/Logs/ci/v2-probe.txt)
MESH_NAMES = {
    "scale_mode": "Mesh Scale Mode", "scale_random": "Random Non-Uniform", "scale_min": "Mesh Scale Min", "scale_max": "Mesh Scale Max",
    "orient_mode": "Mesh Orientation Mode", "orient_random": "Random",
    "dyn0": "Index 0 Param 1",
    "plane_pos": "Analytical Collision Plane Position 1", "plane_normal": "Analytical Collision Normal 1",
    "restitution": "Restitution", "friction": "Friction",
    "collision": {"CPU Collision Type": "Analytical Planes", "Analytical Collision Plane Space": "Local", "Enable Rest State": True,
                  "EnableMaxCollisionCount": True, "Max Number Of Collisions": 2},
}


def _balls(system, name, count, life, size, speed, drag, look="Dust", cone=None, ring=None, sphere=(8, (1, 1, 0.4), (0, 0, 0)), rim=False,
           box=None, gravity=None, bounce=False, shadow=None, origin=None):
    """Round pieces: ring=(radius, z) born on a ground ring flying out; box=(size, offset); else a sphere. origin: the radial push's
    centre (local), below the ring = the balls hop up and out"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(count).mesh_particle(life, size)
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
        if origin:
            e.set("ParticleSpawn", "AddVelocity", "Velocity Origin", origin)
    e.drag(drag)
    if gravity:
        e.gravity(gravity)
    if bounce:
        e.bounce()
    return e.mesh("Ball", look, shadow=shadow)


def _torus(system, name, shape, look, life, size, offset=(0, 0, 3), facing=None):
    """One torus, no motion: the material grows it from StartScale and pops it out"""
    e = Emitter(system, "OmnidirectionalBurst", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.burst(1).mesh_particle((life, life), (size, size)).sphere(0.01, offset=offset).radial(0.0)
    return e.mesh(shape, look, facing=facing)


def _ground_plane(system):
    """User.GroundPoint / GroundNormal (local) and GroundHeight (world Z), set by the cue at spawn (UGSTN_PresentationComponent)"""
    L.add_user_vector(system, "GroundPoint", unreal.Vector(0, 0, 0))
    L.add_user_vector(system, "GroundNormal", unreal.Vector(0, 0, 1))
    L.add_user_float(system, "GroundHeight", 0.0)


# The systems --------------------------------------------------------------------------------------------------------------------
# VFX v3 (2026-11-18, docs/notes/stile/vfx-stile-v3-design-sheet.md, owner refs docs/notes/reviews/ref-astrobot-vfx/astro_1-5): energy is
# light (MI_GSTN_VFX_Flash*: white core + saturated fringe, translucent, bloom), air is real smoke (MI_GSTN_VFX_Smoke*: lit translucent
# puffs with Depth Fade, the floor's colour through User.SurfaceColor), solids are toys (v2 Toy meshes, strong-hit debris only). No tori,
# no rings, no glossy balls. Sprites never scale with their component: every sprite emitter goes through sized() (User.SizeScale).


def _surface(system):
    """User.SurfaceColor: the colour of the floor under the event (UGSTN_PresentationComponent, Physical Material surface type)"""
    L.add_user_color(system, "SurfaceColor", unreal.LinearColor(0.78, 0.74, 0.68, 1))


def _sprites(system, name, template="OmnidirectionalBurst", local=True):
    e = Emitter(system, template, name).settings(local)
    e.off("ParticleUpdate", "ScaleColor")  # the material fades the particle, not the template's colour curve
    return e


def _smoke(system, name, count, life, size, speed, drag, look="SmokeDust", cone=None, ring=None, sphere=None, origin=None, rise=0.0):
    """Lit translucent puffs in the floor's colour. ring=(radius, z): born on a ground ring, pushed out (origin below = out and up)"""
    e = _sprites(system, name)
    e.burst(count).particle(life, size, rotation=True).sized()
    e.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
    if ring:
        e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Ring / Disc").set("ParticleSpawn", "ShapeLocation", "Ring Radius", ring[0])
        e.shape_offset((0, 0, ring[1]))
    elif sphere:
        e.sphere(*sphere)
    else:
        e.sphere(6)
    if cone:
        e.cone(cone[0], cone[1], speed)
    else:
        e.radial(speed)
        if origin:
            e.set("ParticleSpawn", "AddVelocity", "Velocity Origin", origin)
    e.drag(drag)
    if rise:
        e.gravity(rise)  # a slight lift: warm dust floats, it never falls like a ball
    return e.material(look)


def _grains(system, name, count, life, size, speed, cone, gravity=-700.0, drag=3.0, offset=(0, 0, 10)):
    """astro_3: fine grains of snow / dust in the floor's colour, small glints that fall"""
    e = _sprites(system, name)
    e.burst(count).particle(life, size, rotation=True).sized()
    e.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
    e.sphere(8, offset=offset).cone(cone[0], cone[1], speed).drag(drag).gravity(gravity)
    return e.material("Grain")


def run_start_dust():
    """RunStart, at the feet, X along the run: 3 light puffs kicked backwards low + 3 grains (astro_3, small)"""
    s = _system("NS_GSTN_RunStartDust")
    if s:
        _surface(s)
        # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md): x1.4 count, bigger, faster, grains readable on a phone
        # hd1 attempt 1: still invisible: the runner's body hid dust kicked straight back -> wider, flatter, faster, longer
        _smoke(s, "Puffs", 9, (0.55, 0.70), (130, 180), 320, 6.0, look="SmokeLight", cone=((-1, 0, 0.15), 70), sphere=(10, (1, 1, 0.3), (-6, 0, 14)),
               rise=60.0)
        _grains(s, "Grains", 8, (0.35, 0.45), (12, 18), 420, ((-1, 0, 0.8), 45))  # r5: 3-5 cm grains invisible on the dark floor
    return s


def skid_dust():
    """Skid (sustained, attached to the feet, world space): a light puff every 30 cm slid, left behind as a soft row"""
    s = _system("NS_GSTN_SkidDust")
    if s:
        _surface(s)
        # r5 Fable: no dust in the slide pose (v142); the puff seen at v167 was the RunStart of the turn (log: Skid.Enter v128, Exit v143,
        # RunStart v146). Spawn per unit never fired on the skid, so: a burst of 3 at Skid.Enter (dust on frame 1) and a steady rate while
        # the cue lives (it stops on Skid.Exit; a skid is ~15 frames, so the rate cannot pile up)
        kick = _sprites(s, "Kick", local=False)
        kick.burst(6).particle((0.45, 0.55), (140, 190), rotation=True).sized()  # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md):a wide low wedge, not one puff
        kick.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
        kick.sphere(35, (0.5, 1, 0.3), (0, 0, 14)).cone((0, 0, 1), 60, 200).drag(5.0).material("SmokeLight")
        e = _sprites(s, "Trail", "Fountain", local=False)
        e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 30.0)
        e.particle((0.45, 0.55), (120, 160), rotation=True).sized()  # r4 Fable: x3, the skid row was invisible
        e.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
        e.sphere(30, (0.5, 1, 0.3), (0, 0, 14)).cone((0, 0, 1), 35, 70).drag(5.0).material("SmokeLight")
        _grains(s, "Grains", 8, (0.35, 0.45), (12, 18), 380, ((0, 0, 1), 60))
    return s


def run_stop_dust():
    """Run stop (locomotion-finish-3, Fable r2: the skid dust was thigh high, 1.5 m long and hid the brake legs; star glints read as hit
    sparks): the skid's two emitters at about half size and half life, no grains, so the feet stay readable"""
    s = _system("NS_GSTN_RunStopDust")
    if s:
        _surface(s)
        kick = _sprites(s, "Kick", local=False)
        kick.burst(4).particle((0.22, 0.28), (70, 95), rotation=True).sized()
        kick.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
        kick.sphere(18, (0.5, 1, 0.3), (0, 0, 7)).cone((0, 0, 1), 60, 100).drag(5.0).material("SmokeLight")
        e = _sprites(s, "Trail", "Fountain", local=False)
        e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 24.0)
        e.particle((0.22, 0.28), (60, 80), rotation=True).sized()
        e.link("ParticleSpawn", "InitializeParticle", "Color", "User.SurfaceColor")
        e.sphere(15, (0.5, 1, 0.3), (0, 0, 7)).cone((0, 0, 1), 35, 35).drag(5.0).material("SmokeLight")
    return s


def jump_puff():
    """Jump from the ground: 3 small light puffs at the feet, flat and quick (v3: no ring)"""
    s = _system("NS_GSTN_JumpPuff")
    if s:
        _surface(s)
        # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md): invisible at 3 x 30-40 cm
        _smoke(s, "Puffs", 5, (0.30, 0.36), (70, 100), 260, 7.0, look="SmokeLight", ring=(18, 8), origin=(0, 0, -6))
    return s


def land_ring():
    """Landing (scaled by the fall speed; knockdown 1.2): 6 lit puffs born on a 30 cm circle, pushed out low along the floor, growing and
    eroding in ~0.55 s, + 6 grains (astro_3). No torus, no balls"""
    s = _system("NS_GSTN_LandRing")
    if s:
        _surface(s)
        # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md): a faint smudge at knee height: DK's ring is ~2x the character's width and flat on the floor
        _smoke(s, "Puffs", 10, (0.50, 0.62), (150, 210), 380, 6.0, ring=(45, 12), origin=(0, 0, -4), rise=15.0)
        _grains(s, "Grains", 12, (0.40, 0.55), (12, 20), 420, ((0, 0, 1), 50), gravity=-900.0)  # attempt 1: drifted at head height
    return s


def _lines(system, name, side):
    """astro_2: one side of the body, thin very bright white-cyan lines 50-80 cm out, 60-150 cm high, sliding backwards (-Y)"""
    e = _sprites(system, name, "Fountain")
    # r9 real-speed check: from the game camera (behind the runner) 8/s lines of 3 x 110 cm read as 2-4 short flecks changing place every
    # frame, none on the light floor. Twice the rate, longer life (lines overlap from frame to frame) and 6 x 170 cm
    e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 16.0)
    e.particle((0.30, 0.38), None, non_uniform=(6.0, 170.0)).sized()
    e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Box / Plane")
    e.set("ParticleSpawn", "ShapeLocation", "Box Size", (30, 60, 90))
    e.shape_offset((65 * side, -20, 105))
    e.set("ParticleSpawn", "AddVelocity", "Velocity Mode", "Linear").set("ParticleSpawn", "AddVelocity", "Velocity", (0, -450, 0))
    return e.drag(2.0).material("FlashLaser", velocity_aligned=True)


def speed_lines():
    """Sprint 2 (sustained, attached to the root bone, local space): thin bright lines at the body's sides only. The table stops them
    under 450 cm/s (owner playtest: lines trailing a walk)"""
    s = _system("NS_GSTN_SpeedLines")
    if s:
        _lines(s, "Left", -1)
        _lines(s, "Right", 1)
    return s


def hit_spark():
    """Hit (X along the push; cue intensity 0.5 / 0.74 / 1.1 for S / M / L). astro_1: a sharp white star-burst with an orange fringe,
    biggest on frame 1 and shrinking to nothing (~5 frames before the hitstop slows it), 4/6/8 short coloured streaks (User.Count) flying
    out in a 150 degree fan, 6 tiny sparks that fall. Strong hit (User.StrongCount = 3): 3 small glossy debris in the kit's accent colour
    that bounce on the floor (max 2) with a shadow, gone by ~0.9 s (owner 3A)"""
    s = _system("NS_GSTN_HitSpark")
    if s:
        _ground_plane(s)
        star = _sprites(s, "Star")
        # r5 Fable: the strong star stayed near full size v35-v44 and hid both torsos: the hitstop slows the whole system to 0.15 after 2
        # free frames (owner 2A), so a 5-frame life became ~12 frames. 2.5 frames of life: 100 % on frame 1, ~15 % by frame 4, gone by
        # frame 6; the streaks and sparks carry the rest of the hitstop
        # locomotion-finish-2 (owner 2A): the hit is now energy (cue HitstopTime Energy, full speed in the hitstop), so the star gets its own
        # real life back: 0.083 s = 5 frames at 60 (hd1 open item 2). The debris (matter, slowed) moved to NS_GSTN_HitDebris
        star.burst(1).particle((0.083, 0.083), (240, 240), rotation=True).sized().sphere(0.01, offset=(10, 0, 0)).radial(0.0)
        star.toward_camera(90.0).material("FlashStar")
        streaks = _sprites(s, "Streaks")
        streaks.burst(8).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.Count")
        # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md): thin 30 cm streaks hidden behind the attacker: longer, wider, a fan tilted up, nearer the camera
        streaks.particle((0.18, 0.22), None, non_uniform=(16.0, 130.0)).sized()
        streaks.sphere(10, offset=(5, 0, 0)).cone((1, 0, 0.5), 150, 2200).drag(14.0)
        streaks.toward_camera(90.0).material("FlashStreak", velocity_aligned=True)
        sparks = _sprites(s, "Sparks")
        sparks.burst(10).particle((0.22, 0.32), (12, 18), rotation=True).sized()
        sparks.sphere(8, offset=(5, 0, 0)).cone((1, 0, 0.3), 170, 1300).drag(6.0).gravity(-900.0)
        sparks.toward_camera(90.0).material("FlashSpark")
    return s


def hit_debris():
    """The matter half of a hit (cue MatterNiagara of the Hit row, slowed by the hitstop; owner 2A): the strong hit's debris, as it was
    in NS_GSTN_HitSpark"""
    s = _system("NS_GSTN_HitDebris")
    if s:
        _ground_plane(s)
        debris = Emitter(s, "OmnidirectionalBurst", "Debris").settings(True)
        debris.off("ParticleUpdate", "ScaleColor")
        debris.burst(3).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.StrongCount")
        # hd1 DK review (docs/notes/reviews/vfx-v3/hd1/REVIEW-DK.md): 10-13 cm gems were specks; big solid chunks are the layer DK relies on most
        debris.mesh_particle((1.0, 1.2), (22, 35)).sphere(4, offset=(5, 0, 0)).cone((1, 0, 0.6), 110, 1100).drag(2.5)
        debris.gravity(-2600).bounce()
        debris.link("ParticleSpawn", "InitializeParticle", "Color", "User.AccentTint")
        debris.mesh("Gem", "Debris", facing="velocity", shadow="Debris")
    return s


def impact():
    """Wall, ceiling or ground contact of a launched victim (X along the surface normal): astro_3, a cloud of 5 lit puffs off the surface
    in the floor's colour, growing and eroding in ~0.7 s, + 10 grains that fall. r5 Fable: 2-3 blobs of 20-40 cm next to astro_3's cloud
    wider than the character: 7 puffs of 170-240 cm (StartScale 0.45 -> ~120-180 cm at peak), born on a wide flat disc, spread low"""
    s = _system("NS_GSTN_Impact")
    if s:
        _surface(s)
        # r7 Fable: 7 equal puffs born together on one disc averaged into one flat blob (dark v119); dark v76, 4-5 separate lumps, is the
        # target. Billows = a few big lobes first, then smaller ones born 2-3 and 5 frames later, further out and higher, each with its own
        # shading (SmokeCloud: darker underside and rim) so overlapping lobes separate. Sizes 100-240 cm, 7 lobes in all
        core = _smoke(s, "Core", 4, (0.65, 0.80), (200, 240), 300, 6.0, look="SmokeCloud", cone=((1, 0, 0), 70),
                      sphere=(40, (0.05, 1.4, 1.0), (20, 0, 0)), rise=25.0)
        lobes = _smoke(s, "Lobes", 2, (0.55, 0.70), (140, 180), 520, 6.0, look="SmokeCloud", cone=((1, 0, 0), 85),
                       sphere=(90, (0.05, 1.8, 1.3), (35, 0, 0)), rise=45.0)
        lobes.set("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Time", 0.04)
        tops = _smoke(s, "Tops", 2, (0.45, 0.60), (100, 130), 620, 6.0, look="SmokeCloud", cone=((1, 0, 0), 60),
                      sphere=(70, (0.05, 1.6, 1.0), (70, 0, 0)), rise=70.0)
        tops.set("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Time", 0.085)
        _grains(s, "Grains", 16, (0.45, 0.60), (16, 28), 800, ((1, 0, 0), 75), gravity=-900.0, offset=(10, 0, 0))
    return s


def tumble_trail():
    """Tumble (sustained, attached to the pelvis, world space; starts at the launch): a dotted energy trail, one small bright spark every
    25 cm flown, shrinking out in 250 ms (v3: no glossy balls)"""
    s = _system("NS_GSTN_TumbleTrail")
    if s:
        e = _sprites(s, "Dots", "Fountain", local=False)
        e.per_distance(25.0)
        e.particle((0.22, 0.26), (14, 16), rotation=True).sized()
        e.sphere(2).cone((0, 0, 1), 180, 10).material("FlashSpark")
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


SYSTEMS = [run_start_dust, skid_dust, run_stop_dust, jump_puff, land_ring, speed_lines, hit_spark, hit_debris, impact, tumble_trail]


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
