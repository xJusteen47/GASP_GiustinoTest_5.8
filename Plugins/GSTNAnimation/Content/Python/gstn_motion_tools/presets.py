"""Named presets and chains of GSTN Motion Tools. Agents and the move sheets name these instead of writing tangents.

Overlap presets set the three hand sliders (delay, softness, bounce) and the default mode; any slider can be overridden
per call. Chains are lists of control-name patterns, root first; a pattern matches the first control whose lower-case
name contains it, missing ones are skipped, so the same chain works on rigs that differ a little (mannequin FK rig
'head_ctrl', 'neck_01_ctrl'; a custom rig 'Head', 'Neck').
"""
from . import core

OVERLAP_PRESETS = {
    # name:      delay (frames at the chain tip), softness 0..1, bounce 0..1, mode
    "leggero":  {"delay": 2.0, "softness": 0.2, "bounce": 0.3, "mode": core.MODE_SPRING,
                 "label": "Leggero", "hint": "Segue appena, piccolo rimbalzo: capelli corti, testa nei colpi veloci."},
    "pesante":  {"delay": 4.0, "softness": 0.5, "bounce": 0.1, "mode": core.MODE_SPRING,
                 "label": "Pesante", "hint": "Ritardo lungo, si assesta senza oscillare: braccio libero, mantello pesante."},
    "frusta":   {"delay": 3.0, "softness": 0.1, "bounce": 0.6, "mode": core.MODE_OFFSET,
                 "label": "Frusta", "hint": "L'onda scende lungo la catena e schiocca: coda, catena, braccio che sferza."},
    "rigido":   {"delay": 1.0, "softness": 0.0, "bounce": 0.0, "mode": core.MODE_OFFSET,
                 "label": "Rigido", "hint": "Solo un frame di sfasamento: armatura, busto."},
}

OVERLAP_ALIASES = {"light": "leggero", "heavy": "pesante", "whip": "frusta", "stiff": "rigido"}

MODE_ALIASES = {"molla": core.MODE_SPRING, "spring": core.MODE_SPRING,
                "sfasamento": core.MODE_OFFSET, "offset": core.MODE_OFFSET, "time": core.MODE_OFFSET}

EASING_LABELS = {
    "lineare": "Lineare", "morbido": "Morbido", "scatto": "Scatto (colpo)", "anticipo": "Anticipo",
    "overshoot": "Overshoot", "assestamento": "Assestamento",
}

# Chains, root first. Label shown in the panel -> patterns.
CHAINS = {
    "testa":          {"label": "Testa", "patterns": ["neck_01", "neck_02", "head"]},
    "busto":          {"label": "Busto", "patterns": ["spine_03", "spine_04", "spine_05", "chest"]},
    "braccio_l":      {"label": "Braccio sinistro", "patterns": ["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"]},
    "braccio_r":      {"label": "Braccio destro", "patterns": ["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]},
    "mano_l":         {"label": "Polso sinistro", "patterns": ["lowerarm_l", "hand_l"]},
    "mano_r":         {"label": "Polso destro", "patterns": ["lowerarm_r", "hand_r"]},
    "mantello":       {"label": "Mantello", "patterns": ["cape_01", "cape_02", "cape_03", "cape_04", "cape_05"]},
    "coda":           {"label": "Coda", "patterns": ["tail_01", "tail_02", "tail_03", "tail_04", "tail_05"]},
}

# Collision sphere radii (cm) by body part, the same first guesses as the runtime ground-contact fix
# (GSTN_GroundContactSettings, branch claude/project-thread-27htqa) so authoring and game agree.
BODY_RADII = [("pelvis", 12.0), ("spine", 13.0), ("chest", 13.0), ("head", 11.0), ("neck", 8.0), ("calf", 6.0),
              ("lowerarm", 5.0), ("hand", 4.0), ("foot", 7.0), ("ball", 4.0), ("thigh", 8.0), ("upperarm", 5.0)]
DEFAULT_RADIUS = 5.0


def overlap_preset(name):
    key = (name or "").strip().lower()
    key = OVERLAP_ALIASES.get(key, key)
    if key not in OVERLAP_PRESETS:
        raise ValueError("Unknown overlap preset '%s'. Known: %s" % (name, ", ".join(OVERLAP_PRESETS)))
    return key, OVERLAP_PRESETS[key]


def mode_name(name):
    key = (name or "").strip().lower()
    if key not in MODE_ALIASES:
        raise ValueError("Unknown mode '%s'. Use 'molla' (spring) or 'sfasamento' (offset)." % name)
    return MODE_ALIASES[key]


def chain_key(name):
    key = (name or "").strip().lower()
    for k, c in CHAINS.items():
        if key == k or key == c["label"].lower():
            return k
    raise ValueError("Unknown chain '%s'. Known: %s" % (name, ", ".join(CHAINS)))


def resolve_chain(name, controls):
    """Control names of the chain on this rig, root first. Empty if none of its parts exist."""
    lowered = [(c, c.lower()) for c in controls]
    out = []
    for pattern in CHAINS[chain_key(name)]["patterns"]:
        # Prefer FK controls over IK ones when both exist ('hand_l_fk_ctrl' before 'hand_l_ik_ctrl').
        hits = [c for c, lc in lowered if pattern in lc and c not in out]
        hits.sort(key=lambda c: (("ik" in c.lower().split("_")), len(c)))
        if hits:
            out.append(hits[0])
    return out


def available_chains(controls):
    """Chain key -> resolved controls, only chains with at least one control on this rig."""
    out = {}
    for k in CHAINS:
        res = resolve_chain(k, controls)
        if res:
            out[k] = res
    return out


def radius_for(control):
    lc = control.lower()
    for part, r in BODY_RADII:
        if part in lc:
            return r
    return DEFAULT_RADIUS
