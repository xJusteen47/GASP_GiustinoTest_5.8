# Network check of the grab-link hold under rollback (2026-11-23). batch_driver.py with one in-memory change first: the grab test type's
# Grabber role gets UGSTN_GE_GrabHold as its LinkEffect, as Tools/GAS/setup_defence_link_content.py writes it (grab-link), so the walk/jump
# scales of the hold run inside the simulated scenarios even where that content is not saved yet. Nothing is saved: the editor quits after
# the batch. Same environment variables as batch_driver.py (BATCH_MODULE, BATCH_NAMES, BATCH_MODE, BATCH_LOSS, BATCH_REPORT).
import os
import unreal

_DA = '/GSTNLink/Test/DA_LinkType_Grab_Test'
_report = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', os.environ.get('BATCH_REPORT', 'netgrab/batch.log'))
os.makedirs(os.path.dirname(_report), exist_ok=True)
try:
    if os.environ.get('NETGRAB_NO_HOLD') == '1':
        raise RuntimeError('skipped (NETGRAB_NO_HOLD=1, control run)')
    da = unreal.load_asset(_DA)
    role = da.get_editor_property('initiator_role')
    hold = unreal.GSTN_LinkEffectDef()
    hold.set_editor_property('effect', unreal.SoftClassPath('/Script/GSTNLinkNP.GSTN_GE_GrabHold'))
    role.set_editor_property('link_effect', hold)
    da.set_editor_property('initiator_role', role)
    back = da.get_editor_property('initiator_role').get_editor_property('link_effect').get_editor_property('effect')
    open(_report + '.setup', 'w').write(f'grab hold effect in memory: {back.export_text()}\n')
except Exception as e:
    open(_report + '.setup', 'w').write(f'grab hold effect NOT set: {e}\n')

# NETGRAB_CVARS: ';'-separated console commands run once before the batch (one process: they reach every PIE world), e.g. 'gstn.Grab.HoldByNeck 2'
# Diagnosis of an unforced reconcile stream: 'np.LogReconcileField 1;gstn.Combat.LogFragmentReconcile 1', then
#   python Tools/GASNPPTest/reconcile_fields.py <the .ue.log> link_spike   (a rollback with no field logged is Mover's: its ShouldReconcile does not trace)
for _cmd in [c.strip() for c in os.environ.get('NETGRAB_CVARS', '').split(';') if c.strip()]:
    unreal.SystemLibrary.execute_console_command(None, _cmd)
    open(_report + '.setup', 'a').write(f'cvar: {_cmd}\n')

exec(open(os.path.join(unreal.Paths.project_dir(), 'Tools', 'GASNPPTest', 'batch_driver.py')).read())
