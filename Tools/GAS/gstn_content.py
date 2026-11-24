"""Where our content lives under /Game/GSTN: one rule table, used by the setup scripts and by the phase 2b move.

Phase 2b of the Content reorganisation (docs/notes/content-riorganizzazione.md, owner choice A 2026-11-24) emptied
/Game/GASNPPTest into /Game/GSTN, with one folder per kit and one per move. A script must not hard-code a folder for an
asset any more: it asks `path(name)` (existing or new asset) or `folder(name)` (where to create it). The same table
generates Tools/Reorg/content-map-2b.csv, so the move and the scripts cannot disagree.

Pure Python (no `unreal` import): it also runs outside the editor (reorg_rewrite, the consistency check).
Names are unique across the project, so the asset name alone decides the folder. Versions of a move (V7, R2, K5...)
stay together in the move folder: which one is approved is in docs/notes/anim-approvate.md, not in the path.
"""
import re

GSTN = '/Game/GSTN'
OVERLORD = GSTN + '/Characters/Overlord'
OVERLORD_KITS = OVERLORD + '/Kits'
OVERLORD_MOVES = OVERLORD + '/Moves'
TEST_COMBAT = GSTN + '/_Test/Combat'
TEST_MAPS = GSTN + '/_Test/Maps'
EXAMPLES = GSTN + '/_Examples'
REACTIONS = GSTN + '/Combat/Reactions'
ARCHETYPES = GSTN + '/Combat/Archetypes'
HURTBOXES = GSTN + '/Combat/Hurtboxes'
AIR_CROUCH = GSTN + '/Animation/AirCrouch'
INPUT = GSTN + '/Input'
CORE = GSTN + '/Core'
HUB = GSTN + '/Maps/Hub'
LOCOMOTION_DATA = GSTN + '/Data/Locomotion'

# Folder that every kit, weapon and super lives under (recursive scans, Asset Manager).
KIT_ROOT = GSTN


def move_folder(move):
    """Folder of one Overlord move: DAir, Dropkick, Jab, FThrow, Playtest."""
    return f'{OVERLORD_MOVES}/{move}'


# First match wins. (regex on the asset name, folder). Order matters: specific before generic.
RULES = [
    (r'^DA_Locomotion_', LOCOMOTION_DATA),
    # Overlord moves: every version of a move in its folder (AM_, AS_, GA_, TL_).
    (r'^(AM|AS)_OverlordDAir', move_folder('DAir')),
    (r'^(GA|TL)_Overlord_DAir', move_folder('DAir')),
    (r'^(AM|AS)_OverlordDropkick', move_folder('Dropkick')),
    (r'^(GA|TL)_Overlord_Dropkick', move_folder('Dropkick')),
    (r'^(AM|AS)_OverlordJab', move_folder('Jab')),
    (r'^(GA|TL)_Overlord_Jab', move_folder('Jab')),
    (r'^(AM|AS)_OverlordFThrow', move_folder('FThrow')),
    (r'^(GA|TL)_Overlord_FThrow', move_folder('FThrow')),
    (r'^GA_Playtest_', move_folder('Playtest')),
    # Overlord kits: the main one, its move variants and the playtest kits (they carry the Overlord dropkick).
    (r'^(KIT|WPN)_Test_(Overlord|Playtest)', OVERLORD_KITS),
    # Shared combat content used by game kits.
    (r'^(AM|AS)_React_|^DA_ReactionAnims$', REACTIONS),
    (r'^GA_(Light|Medium|Heavy)_', ARCHETYPES),
    (r'^HBS_FromPhysics$', HURTBOXES),
    (r'^AS_AirCrouch_', AIR_CROUCH),
    (r'^(IA|IMC|IC|SS|BS)_', INPUT),
    (r'^(BP_GASNPP_TestCharacter|GM_GASNPPTest)$', CORE),
    (r'^(MI_Hub_|BP_Test_BouncePad$)', HUB),
    # Designer examples.
    (r'^BP_\w+_Example$', EXAMPLES + '/Combat'),
    (r'^GCN_Example_', EXAMPLES + '/Cosmetics'),
    (r'^(DA_Example_Shot|(GA|TL)_Example_DoubleShot_)', EXAMPLES + '/DoubleShot'),
    (r'^\w+_Example(_|$)', EXAMPLES),
    # Fixtures of the automated tests.
    (r'^L_Test_Combat$', TEST_MAPS),
    (r'^DA_Stance_', GSTN + '/_Test/Stances'),
    (r'^DA_Minion_', GSTN + '/_Test/Minions'),
    (r'^(KIT|WPN|SUP|MOD|HBS|PM|PS|SIS|TL|BPK)_Test_|^GSTN_CombatRules_Test', TEST_COMBAT),
]
_COMPILED = [(re.compile(rx), f) for rx, f in RULES]

# Old folders that scripts named directly (setup scripts that create assets there) and where they went.
FOLDERS = {
    '/Game/GASNPPTest/Combat/Reaction': REACTIONS,
    '/Game/GASNPPTest/Combat/AirCrouch': AIR_CROUCH,
    '/Game/GASNPPTest/Combat/Archetypes': ARCHETYPES,
    '/Game/GASNPPTest/Combat/Editor': TEST_COMBAT + '/Editor',
    '/Game/GASNPPTest/Examples/DoubleShot': EXAMPLES + '/DoubleShot',
    '/Game/GASNPPTest/Examples/Cosmetics': EXAMPLES + '/Cosmetics',
    '/Game/GASNPPTest/Examples/FactoryCheck': EXAMPLES + '/FactoryCheck',
    '/Game/GASNPPTest/Examples': EXAMPLES,
    '/Game/GASNPPTest/Input': INPUT,
    '/Game/GASNPPTest/TestMaps/Hub': HUB,
    '/Game/GASNPPTest/TestMaps/Level01': TEST_MAPS + '/Level01',
}


def folder(name):
    """Folder of the asset called `name` (existing or about to be created). Raises on a name no rule covers."""
    for rx, f in _COMPILED:
        if rx.search(name):
            return f
    raise KeyError(f'gstn_content: no folder rule for {name}: add one to RULES in Tools/GAS/gstn_content.py')


def path(name):
    """Package path of the asset called `name`: '/Game/GSTN/.../NAME'."""
    return f'{folder(name)}/{name}'


def object_path(name):
    """Object path 'PKG.NAME' (for load_object / soft references)."""
    return f'{path(name)}.{name}'
