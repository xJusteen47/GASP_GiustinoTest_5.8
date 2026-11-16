# Runs inside UnrealEditor (Python): moves every top-level window of THIS process to a target virtual desktop, every ~0.5 s.
# IVirtualDesktopManager::MoveWindowToDesktop only works on the caller's own windows, hence it runs in the editor.
import ctypes, ctypes.wintypes as wt, os, uuid, unreal

TARGET = os.environ.get('VD_TARGET_GUID', '94a28a1c-cfe5-4e5c-807e-df5a7493ba76')
LOG = os.environ.get('VD_LOG')

class GUID(ctypes.Structure):
    _fields_ = [('d1', ctypes.c_ulong), ('d2', ctypes.c_ushort), ('d3', ctypes.c_ushort), ('d4', ctypes.c_ubyte * 8)]
    @classmethod
    def of(cls, s):
        b = uuid.UUID(s).bytes_le
        g = cls(); ctypes.memmove(ctypes.byref(g), b, 16); return g
    def __str__(self):
        return str(uuid.UUID(bytes_le=bytes(ctypes.string_at(ctypes.byref(self), 16))))

ole32 = ctypes.OleDLL('ole32'); user32 = ctypes.WinDLL('user32')
ole32.CoInitializeEx.restype = ctypes.c_long
try:
    ctypes.windll.ole32.CoInitializeEx(None, 2)
except Exception:
    pass
_mgr = ctypes.c_void_p()
ctypes.windll.ole32.CoCreateInstance(ctypes.byref(GUID.of('aa509086-5ca9-4c25-8f95-589d3c07b48a')), None, 1 | 4,
    ctypes.byref(GUID.of('a5cd92ff-29be-454c-8d04-d82879fb3f1b')), ctypes.byref(_mgr))
_vt = ctypes.cast(ctypes.cast(_mgr, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
_GetId = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, wt.HWND, ctypes.POINTER(GUID))(_vt[4])
_Move = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, wt.HWND, ctypes.POINTER(GUID))(_vt[5])
_target = GUID.of(TARGET)
_pid = os.getpid()
_moved = set()
_acc = [0.0]

def _log(msg):
    if LOG:
        with open(LOG, 'a') as f:
            f.write(msg + '\n')

def sweep():
    hwnds = []
    cb = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(lambda h, l: hwnds.append(h) or True)
    user32.EnumWindows(cb, 0)
    for h in hwnds:
        p = wt.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value != _pid or not user32.IsWindowVisible(h):
            continue
        cur = GUID()
        if _GetId(_mgr, h, ctypes.byref(cur)) != 0 or str(cur) == TARGET:
            continue
        hr = _Move(_mgr, h, ctypes.byref(_target))
        if h not in _moved:
            _moved.add(h); _log('move hwnd %s from %s hr=%d' % (h, cur, hr & 0xffffffff))

def _tick(dt):
    _acc[0] += dt
    if _acc[0] >= 0.5:
        _acc[0] = 0.0
        try:
            sweep()
        except Exception as e:
            _log('sweep error %r' % e)

_log('vd2_mover start pid=%d mgr=%s' % (_pid, _mgr.value))
sweep()
HANDLE = unreal.register_slate_post_tick_callback(_tick)
