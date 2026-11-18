"""Polish M2 + VFX v2: the Niagara systems of the VFX kit (v2: lit glossy meshes, see the block before the systems) (/GSTNPresentation/VFX/NS_GSTN_*) and the default presentation table.

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

def run_start_dust():
    """RunStart, at the feet, X along the run: 4 "cotton" balls (20 / 16 / 12 cm) kicked backwards in a fan, two speeds (big ~50 cm
    back, small ~30 cm). Lit, roughness 0.6: their contact shadow comes from the real light"""
    s = _system("NS_GSTN_RunStartDust")
    if s:
        _balls(s, "Big", 2, (0.30, 0.34), (20, 24), 380, 6.0, cone=((-1, 0, 0.55), 50), box=((20, 44, 2), (-14, 0, 12)))
        _balls(s, "Small", 2, (0.26, 0.30), (15, 17), 170, 6.0, cone=((-1, 0, 0.05), 50), box=((20, 44, 2), (-6, 0, 8)))
    return s


def skid_dust():
    """Skid (sustained, attached to the feet, world space): a cotton ball (14-18 cm) every 37 cm slid, left behind as a row"""
    s = _system("NS_GSTN_SkidDust")
    if s:
        e = Emitter(s, "Fountain", "Trail").settings(False)
        e.off("ParticleUpdate", "ScaleColor")
        e.per_distance(37.0)
        e.mesh_particle((0.28, 0.32), (14, 18)).sphere(3, (1, 1, 0.3), (0, 0, 15)).cone((0, 0, 1), 30, 40)
        e.drag(6.0).mesh("Ball", "Dust")
    return s


def jump_puff():
    """Jump from the ground: a thin glossy torus on the floor, ~86 cm. No dust balls (owner 2026-11-17: dust only on run start and skid)"""
    s = _system("NS_GSTN_JumpPuff")
    if s:
        _torus(s, "Ring", "TorusZ", "Gloss", 0.26, 86)
    return s


def land_ring():
    """Landing (scaled by the fall speed; knockdown 1.2): a glossy torus ~80 cm that widens from 45% and pops out at 300 ms, and 6 cotton
    balls (14-18 cm) born on a 58 cm circle, pushed out and up, falling back on the floor plane once (one hop, then rest)"""
    s = _system("NS_GSTN_LandRing")
    if s:
        _ground_plane(s)
        _torus(s, "Ring", "TorusZ", "Gloss", 0.30, 80)
        _balls(s, "Puffs", 6, (0.34, 0.42), (20, 26), 440, 6.0, ring=(58, 10), origin=(0, 0, -40), gravity=-2200, bounce=True)
    return s


def _lines(system, name, side):
    """One side of the body: capsules spawned 50-80 cm out, 60-150 cm high, sliding backwards (-Y), X along the motion"""
    e = Emitter(system, "Fountain", name).settings(True)
    e.off("ParticleUpdate", "ScaleColor")
    e.set("EmitterUpdate", "SpawnRate", "SpawnRate", 8.0)
    e.mesh_particle((0.24, 0.30), (90, 110), scale=(1.0, 0.35, 0.35))
    e.set("ParticleSpawn", "ShapeLocation", "Shape Primitive", "Box / Plane")
    e.set("ParticleSpawn", "ShapeLocation", "Box Size", (30, 60, 90))
    e.shape_offset((65 * side, -20, 105))
    e.set("ParticleSpawn", "AddVelocity", "Velocity Mode", "Linear").set("ParticleSpawn", "AddVelocity", "Velocity", (0, -450, 0))
    return e.drag(2.0).mesh("Capsule", "Streak", facing="velocity")


def speed_lines():
    """Sprint 2 (sustained, attached to the root bone, local space): glossy white tapered capsules at the body's sides only. The table
    stops them under 450 cm/s (owner playtest: lines trailing a walk)"""
    s = _system("NS_GSTN_SpeedLines")
    if s:
        _lines(s, "Left", -1)
        _lines(s, "Right", 1)
    return s


def hit_spark():
    """Hit (X along the push; cue intensity 0.5 / 0.74 / 1.1 for S / M / L, which scales everything): flash sprite 64 cm (frames 1-2), a
    "neon" torus ~105 cm facing the camera, 4/6/8 glossy gem shards (User.Count) in an 80 degree fan, braked hard, gone by frame ~18.
    Strong hit (User.StrongCount = 3): 3 debris gems in the kit's accent colour that fall, bounce on the floor (max 2) with a shadow
    under them, and pop out by ~0.9 s"""
    s = _system("NS_GSTN_HitSpark")
    if s:
        _ground_plane(s)
        flash = Emitter(s, "OmnidirectionalBurst", "Flash").settings(True)
        flash.off("ParticleUpdate", "ScaleColor")
        # r1 review: on the contact, not over the attacker (-30 -> +10); a bigger flash from the game camera (83 -> 110)
        flash.burst(1).particle((0.034, 0.034), (110, 110)).sized().sphere(0.01, offset=(10, 0, 0)).radial(0.0)
        flash.toward_camera(60.0).material("HitFlash")  # r2 review: a dark square behind the attacker from the game camera
        _torus(s, "Ring", "TorusX", "Neon", 0.20, 85, offset=(10, 0, 0), facing="camera")
        shards = Emitter(s, "OmnidirectionalBurst", "Shards").settings(True)
        shards.off("ParticleUpdate", "ScaleColor")
        shards.burst(8).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.Count")
        # Fan regular-ish: a cone of +-45 degrees (120 set), no curl, no vortex
        # r2 review: drag 14 -> 20 (they flew ~3 m), born on a 12 cm sphere in a wider fan (a pile on the ground bounce)
        shards.mesh_particle((0.24, 0.30), (18, 22)).sphere(12, offset=(5, 0, 0)).cone((1, 0, 0), 150, 3000).drag(20.0)
        shards.toward_camera(30.0).mesh("Gem", "Gem", facing="velocity")
        debris = Emitter(s, "OmnidirectionalBurst", "Debris").settings(True)
        debris.off("ParticleUpdate", "ScaleColor")
        debris.burst(3).link("EmitterUpdate", "SpawnBurst_Instantaneous", "Spawn Count", "User.StrongCount")
        debris.mesh_particle((0.75, 0.90), (18, 22)).sphere(4, offset=(5, 0, 0)).cone((1, 0, 0.6), 110, 900).drag(2.5)
        debris.gravity(-2600).bounce()
        debris.link("ParticleSpawn", "InitializeParticle", "Color", "User.AccentTint")
        debris.mesh("Gem", "Debris", facing="velocity", shadow="Debris")
    return s


def impact():
    """Wall, ceiling or ground contact of a launched victim (X along the surface normal): a glossy torus on the surface (~80 cm) and 5
    cotton balls born on its rim, pushed off the surface, falling to the floor and bouncing there, with shadows"""
    s = _system("NS_GSTN_Impact")
    if s:
        _ground_plane(s)
        _torus(s, "Ring", "TorusX", "Gloss", 0.30, 80, offset=(3, 0, 0))
        _balls(s, "Puffs", 5, (0.45, 0.60), (16, 22), 440, 5.0, sphere=(72, (0.05, 1, 1), (14, 0, 0)), rim=True, gravity=-2200, bounce=True,
               shadow="Dust")
    return s


def tumble_trail():
    """Tumble (sustained, attached to the pelvis, world space; starts at the launch): a chain of glossy balls (18 cm) one every 60 cm
    flown, shrinking out in 300 ms, each with its shadow on the floor under the launch"""
    s = _system("NS_GSTN_TumbleTrail")
    if s:
        _ground_plane(s)
        balls = Emitter(s, "Fountain", "Balls").settings(False)
        balls.off("ParticleUpdate", "ScaleColor")
        balls.per_distance(60.0)
        balls.mesh_particle((0.28, 0.32), (18, 18)).sphere(2).cone((0, 0, 1), 180, 10).mesh("Ball", "Dust", shadow="Dust")
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
