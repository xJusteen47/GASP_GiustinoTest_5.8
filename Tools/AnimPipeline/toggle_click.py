# Cascadeur (copy scene jab_snap_test.casc): flip an AutoPhysics toggle in the Physics settings panel with a synthetic
# mouse click on its row, read the scene setting before/after (deferred), output prefix TOGGLE in the Cascadeur log.
# Params: LABEL (e.g. "Secondary motion").
import csc, json
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
LABEL = globals().get("LABEL") or "Secondary motion"
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_snap_test.casc"][0]
sm.set_current_scene(tab)
sh = tab.get_setting_handler()
before = sh.get_bool_value("AutoPhysics", LABEL)
res = {"label": LABEL, "before": before, "clicked": None}
for w in QtGui.QGuiApplication.topLevelWindows():
    if not w.isVisible():
        continue
    qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
    if not qw.inherits("QQuickWindow"):
        continue
    stack = [qw.contentItem()]
    while stack and res["clicked"] is None:
        it = stack.pop(); stack.extend(it.childItems())
        if str(it.property("text") or "") == LABEL and it.isVisible():
            par = it.parentItem()
            ma = [c for c in par.childItems() if c.metaObject().className() == "QQuickMouseArea"]
            tgt = ma[0] if ma else it
            c = tgt.mapToScene(QtCore.QPointF(tgt.width() / 2, tgt.height() / 2))
            for t, b in ((QtCore.QEvent.MouseButtonPress, QtCore.Qt.LeftButton), (QtCore.QEvent.MouseButtonRelease, QtCore.Qt.NoButton)):
                ev = QtGui.QMouseEvent(t, c, qw.mapToGlobal(c), QtCore.Qt.LeftButton, b, QtCore.Qt.NoModifier)
                QtGui.QGuiApplication.sendEvent(qw, ev)
            res["clicked"] = {"target": tgt.metaObject().className(), "scene_pos": [round(c.x()), round(c.y())]}
def after():
    res["after"] = sh.get_bool_value("AutoPhysics", LABEL)
    print("TOGGLE " + json.dumps(res))
QtCore.QTimer.singleShot(800, after)
print(json.dumps(res))
