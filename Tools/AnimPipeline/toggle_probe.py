# Cascadeur (copy scene jab_snap_test.casc), read-only: are the AutoPhysics toggles scene SETTINGS reachable through
# data_viewer / data_editor, and which QML items represent them in the Physics settings panel?
import csc, json, re
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_snap_test.casc"][0]
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer()
KW = re.compile(r"(?i)secondary|compensation|separation|smooth rotation|smooth trajectory|physics corrector")
hits = []
for o in mv.get_objects():
    try:
        sids = dv.get_all_settings_id(o)
    except Exception:
        continue
    for sid in sids:
        try:
            nm = dv.get_setting_name(sid)
        except Exception:
            continue
        if KW.search(nm or ""):
            try:
                v = dv.get_setting_value(sid)
            except Exception as e:
                v = "ERR " + str(e)[:60]
            hits.append({"obj": mv.get_object_name(o), "setting": nm, "value": str(v)})
sh = tab.get_setting_handler()
handler = {k: sh.get_bool_value("AutoPhysics", k) for k in ("Secondary motion", "Compensation motion", "Separation of motion", "Smooth rotation", "Smooth trajectory")}
qml = []
for w in QtGui.QGuiApplication.topLevelWindows():
    if not w.isVisible():
        continue
    qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
    if not qw.inherits("QQuickWindow"):
        continue
    stack = [qw.contentItem()]
    while stack:
        it = stack.pop(); stack.extend(it.childItems())
        t = str(it.property("text") or "")
        if KW.search(t):
            par = it.parentItem()
            sib = [c for c in (par.childItems() if par else [])]
            qml.append({"text": t, "cls": it.metaObject().className(), "visible": it.isVisible(),
                        "checked": str(it.property("checked")),
                        "siblings": [(c.metaObject().className(), str(c.property("checked"))) for c in sib][:6]})
print(json.dumps({"scene_settings_hits": hits[:20], "setting_handler": handler, "qml": qml[:20]}))
