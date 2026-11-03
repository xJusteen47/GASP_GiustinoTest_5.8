"""The GSTN button bar: a small window that stays above Cascadeur's main window (not above other apps).

Cascadeur 2026.2.3 has no custom toolbar: its toolbars are fixed QML, and a button pushed into them from Python would
vanish on the next layout rebuild. This bar is the stable stand-in. Every button calls the same Scripts-menu action
the owner can also bind to a key (Settings > Hotkeys > Scripts), so there is one code path per action.
Opened by the application_started event (gstn_events) and by Scripts > GSTN > Mostra barra pulsanti.
"""
import csc
from PySide6 import QtCore, QtGui, QtQml

# Plain QtQuick items, no QtQuick.Controls: after a restart Cascadeur's Controls style drew the buttons as white boxes and
# showed every ToolTip at once (owner screenshot 2026-11-03). Tooltips dropped: the labels say what each button does.
# Only what needs the open scene (proposal 2026-11-03 §c): the rest (undo import, ok/approve, variants, snapshots) is in
# the GSTN Studio app. The status line replaces the old "Stato" button.
BUTTONS = [   # (label, Scripts action id, description for the docs)
    ("Segna chiave", "Scripts.GSTN.Segna chiave (frame corrente)", "Il frame corrente diventa (o smette di essere) una chiave tua"),
    ("Invia", "Scripts.GSTN.Invia", "Salva, esporta le ossa e lascia la richiesta di import nella scheda"),
]
STATUS_MS = 2000

QML = """
import QtQuick
import QtQuick.Layouts
Window {
    id: win
    title: "GSTN"
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: "#262626"
    width: row.implicitWidth + 8; height: 26
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 3
        Repeater {
            model: gstnButtons
            delegate: Rectangle {
                required property var modelData
                implicitWidth: label.implicitWidth + 16; implicitHeight: 20
                radius: 3
                color: area.pressed ? "#1e6fb8" : (area.containsMouse ? "#4a4a4a" : "#383838")
                border.color: "#505050"
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#e6e6e6"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: gstnBridge.call(modelData.action)
                }
            }
        }
        Text {
            text: gstnBridge.status
            color: "#a8a8a8"
            font.pixelSize: 11
            Layout.leftMargin: 6
            Layout.rightMargin: 4
        }
    }
}
"""


class _Bridge(QtCore.QObject):
    statusChanged = QtCore.Signal()

    def __init__(self):
        super().__init__()
        self._status = ""

    def _get_status(self):
        return self._status

    def refresh(self):
        from gstn_scripts import _bridge as B
        s = B.status_text()
        if s != self._status:
            self._status = s; self.statusChanged.emit()

    status = QtCore.Property(str, _get_status, notify=statusChanged)

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


CAPTION_BUTTONS = 190   # Windows' minimize/maximize/close on the title bar, plus a margin


def _place():
    """Sits in the empty right end of the main window's title bar, left of the caption buttons, and follows it."""
    win, main = _state.get("win"), _state.get("main")
    if win is None or main is None:
        return
    fg = main.frameGeometry(); top = main.geometry().y() - fg.y()
    avail = main.screen().availableGeometry()
    right = min(fg.x() + fg.width(), avail.x() + avail.width())        # a maximized window's frame runs off screen
    y = max(fg.y(), avail.y()) if top >= win.height() else main.geometry().y()
    win.setPosition(right - CAPTION_BUTTONS - win.width(), y + max(0, (top - win.height()) // 2))


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
        win.widthChanged.connect(lambda *_: QtCore.QTimer.singleShot(0, _place))
        timer = QtCore.QTimer(); timer.setInterval(STATUS_MS); timer.timeout.connect(bridge.refresh); timer.start()
        bridge.refresh()
        _state.update(engine=eng, bridge=bridge, win=win, timer=timer)
    main = _main_window()
    if main is not None and _state.get("main") is not main:
        win.setTransientParent(main)
        _state["main"] = main
        for sig in (main.xChanged, main.yChanged, main.widthChanged, main.windowStateChanged):
            sig.connect(lambda *_: QtCore.QTimer.singleShot(0, _place))
    _place()
    win.show(); win.raise_()
    return win
