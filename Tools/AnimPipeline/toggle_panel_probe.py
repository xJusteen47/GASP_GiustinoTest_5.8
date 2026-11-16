# Cascadeur (copy scene): activate the "Physics settings" side tab, then (deferred) list checkable controls near the
# AutoPhysics toggle labels. Output prefix PANEL in the Cascadeur log.
import csc, json, re
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
app = csc.app.get_application(); sm = app.get_scene_manager()
sm.set_current_scene([s for s in sm.scenes() if s.name() == "jab_snap_test.casc"][0])
KW = re.compile(r"^(Secondary motion|Compensation motion|Separation of motion|Smooth rotation|Smooth trajectory|Physics corrector|Penetration cleaning)$")

def windows():
    for w in QtGui.QGuiApplication.topLevelWindows():
        if w.isVisible():
            qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
            if qw.inherits("QQuickWindow"):
                yield qw

def all_items(qw):
    stack = [qw.contentItem()]
    while stack:
        it = stack.pop(); stack.extend(it.childItems()); yield it

tab_res = None
for qw in windows():
    for it in all_items(qw):
        if str(it.property("text") or "") == "Physics settings" and "TabButton" in it.metaObject().className():
            bar = it.parentItem()
            while bar is not None and bar.property("currentIndex") is None:
                bar = bar.parentItem()
            idx = None
            if bar is not None:
                buttons = [c for c in bar.findChildren(QtQuick.QQuickItem) if "TabButton" in c.metaObject().className()]
                for i, b in enumerate(buttons):
                    if str(b.property("text") or "") == "Physics settings":
                        idx = i
                old = bar.property("currentIndex")
                bar.setProperty("currentIndex", idx)
                tab_res = {"bar": bar.metaObject().className(), "old_index": old, "new_index": idx}
            break

def report():
    rows = []
    for qw in windows():
        for it in all_items(qw):
            t = str(it.property("text") or "")
            if KW.match(t) and it.isVisible():
                anc, row = it, []
                for _ in range(3):
                    anc = anc.parentItem()
                    if anc is None:
                        break
                    for c in anc.findChildren(QtQuick.QQuickItem):
                        if c.property("checked") is not None:
                            row.append({"cls": c.metaObject().className(), "checked": c.property("checked"), "text": str(c.property("text") or "")[:30]})
                    if row:
                        break
                rows.append({"label": t, "pos": [round(it.mapToScene(QtCore.QPointF(0, 0)).x()), round(it.mapToScene(QtCore.QPointF(0, 0)).y())], "checkables": row[:4]})
    print("PANEL " + json.dumps({"tab": tab_res, "rows": rows}))
QtCore.QTimer.singleShot(700, report)
print(json.dumps({"tab": tab_res}))
