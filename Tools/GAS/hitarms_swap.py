"""Owner decision 1A (2026-11-18, thread hit-arms-1): the character's reaction component uses DA_ReactionAnims_HitArms (HitStand = ArmsV3,
the rest a copy of DA_ReactionAnims_HitAdd). Headless, idempotent; backs the Blueprint up through setup_reaction_anims like the 2A swap:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/hitarms_swap.py" -unattended -nullrhi
Never while someone is in PIE (the save fails). Undo: run hitstand_armlow_and_swap.py with HITSTAND_SWAP=1 (back to the HitAdd set).
Log Saved/Reaction/hitarms_swap.log (DONE_MARK)."""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
VSET = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitArms'
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction', 'hitarms_swap.log')
L = []
try:
    import setup_reaction_anims as sra
    rset = unreal.load_asset(VSET)
    for r in rset.get_editor_property('rules'):
        if str(r.get_editor_property('name')) == 'HitStand':
            L.append(f'HitStand plays {r.get_editor_property("animation")}')
    sra.character_component(rset)
    L.extend(sra._lines)
except Exception as ex:
    import traceback
    L.append('ERROR ' + repr(ex))
    L.append(traceback.format_exc())
os.makedirs(os.path.dirname(LOG), exist_ok=True)
open(LOG, 'w').write('\n'.join(L) + '\nDONE_MARK\n')
