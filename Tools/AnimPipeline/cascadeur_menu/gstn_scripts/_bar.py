"""The GSTN button bar: a small window that stays above Cascadeur's main window (not above other apps).

Cascadeur 2026.2.3 has no custom toolbar: its toolbars are fixed QML, and a button pushed into them from Python would
vanish on the next layout rebuild. This bar is the stable stand-in. Every button calls the same Scripts-menu action
the owner can also bind to a key (Settings > Hotkeys > Scripts), so there is one code path per action.
Opened by the application_started event (gstn_events) and by Scripts > GSTN > Mostra barra pulsanti.
"""
import csc
from PySide6 import QtCore, QtGui, QtQml

BUTTONS = [   # (label, Scripts action id, tooltip)
    ("Esporta per Unreal", "Scripts.GSTN.Esporta per Unreal", "Salva, esporta le ossa e chiede a Claude l'import nella scheda"),
    ("Segna chiave", "Scripts.GSTN.Segna chiave (frame corrente)", "Il frame corrente diventa (o smette di essere) una chiave tua"),
    ("Annulla import", "Scripts.GSTN.Annulla ultimo import (chiedi a Claude)", "Chiede a Claude di rimettere la scheda com'era"),
    ("Stato", "Scripts.GSTN.Stato", "Mossa, chiavi, richieste in attesa"),
]

QML = """
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Window {
    id: win
    title: "GSTN"
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: "#202020"
    width: row.implicitWidth + 6; height: 26
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Repeater {
            model: gstnButtons
            delegate: Button {
                required property var modelData
                text: modelData.label
                font.pixelSize: 12
                implicitHeight: 22
                topPadding: 2; bottomPadding: 2; leftPadding: 8; rightPadding: 8
                ToolTip.visible: hovered
                ToolTip.text: modelData.tip
                onClicked: gstnBridge.call(modelData.action)
            }
        }
    }
}
"""


class _Bridge(QtCore.QObject):
    @QtCore.Slot(str)
    def call(self, action):
        # Deferred: the action may save the scene, which should not run inside the QML click handler.
        QtCore.QTimer.singleShot(0, lambda: csc.app.get_application().get_action_manager().call_action(action))


_state = {}


def _main_window():
    best = None
    for w in QtGui.QGuiApplication.topLevelWindows():
        if w.isVisible() and w.title().endswith("Cascadeur") and (best is None or w.width() > best.width()):
            best = w
    return best


CAPTION_BUTTONS = 150   # width of Windows' minimize/maximize/close on the main window's title bar


def _place():
    """Sits in the empty right end of the main window's title bar, left of the caption buttons, and follows it."""
    win, main = _state.get("win"), _state.get("main")
    if win is None or main is None:
        return
    fg = main.frameGeometry(); top = main.geometry().y() - fg.y()
    win.setPosition(fg.x() + fg.width() - CAPTION_BUTTONS - win.width(), fg.y() + max(0, (top - win.height()) // 2))


def show():
    win = _state.get("win")
    if win is None:
        eng = QtQml.QQmlApplicationEngine()
        bridge = _Bridge()
        eng.rootContext().setContextProperty("gstnBridge", bridge)
        eng.rootContext().setContextProperty("gstnButtons", [{"label": l, "action": a, "tip": t} for l, a, t in BUTTONS])
        eng.loadData(QML.encode("utf-8"))
        roots = eng.rootObjects()
        if not roots:
            raise RuntimeError("GSTN bar: QML did not load")
        win = roots[0]
        _state.update(engine=eng, bridge=bridge, win=win)
    main = _main_window()
    if main is not None and _state.get("main") is not main:
        win.setTransientParent(main)
        _state["main"] = main
        for sig in (main.xChanged, main.yChanged, main.widthChanged, main.windowStateChanged):
            sig.connect(lambda *_: QtCore.QTimer.singleShot(0, _place))
    _place()
    win.show(); win.raise_()
    return win
