"""Tiny client for UE_MCP_Bridge: call(method, params) and py(entry, **kwargs) on capture_ue.py."""
import base64, json, os, socket, struct, ast

PORT = 55976
HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")

def get_port():
    if "UE_MCP_PORT" in os.environ:
        return int(os.environ["UE_MCP_PORT"])
    for base in [
        os.path.abspath(os.path.join(HERE, "../..")),
        "C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_feature",
        "C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2",
    ]:
        pj = os.path.join(base, "Saved/UE_MCP_Bridge/port.json")
        if os.path.exists(pj):
            try:
                with open(pj) as f:
                    return int(json.load(f)["port"])
            except Exception:
                pass
    return PORT

def _recv_exact(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise ConnectionError("closed")
        b += c
    return b

def _send(s, txt):
    d = txt.encode(); h = bytes([0x81]); n = len(d)
    h += bytes([0x80 | n]) if n < 126 else (bytes([0xFE]) + struct.pack(">H", n) if n < 65536 else bytes([0xFF]) + struct.pack(">Q", n))
    m = os.urandom(4)
    s.sendall(h + m + bytes(x ^ m[i % 4] for i, x in enumerate(d)))

def _recv(s):
    parts = []
    while True:
        b1, b2 = _recv_exact(s, 2); op, n = b1 & 15, b2 & 127
        if n == 126: n = struct.unpack(">H", _recv_exact(s, 2))[0]
        elif n == 127: n = struct.unpack(">Q", _recv_exact(s, 8))[0]
        mk = _recv_exact(s, 4) if b2 & 128 else None
        d = _recv_exact(s, n)
        if mk: d = bytes(x ^ mk[i % 4] for i, x in enumerate(d))
        if op == 8: raise ConnectionError("server closed")
        if op == 9: continue
        parts.append(d)
        if b1 & 128: return b"".join(parts).decode()

_sock = None
_id = 0
def call(method, params=None, timeout=300):
    global _sock, _id
    port = get_port()
    if _sock is None:
        _sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        k = base64.b64encode(os.urandom(16)).decode()
        _sock.sendall(f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {k}\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
        r = b""
        while b"\r\n\r\n" not in r: r += _sock.recv(1024)
    _id += 1
    _send(_sock, json.dumps({"id": _id, "method": method, "params": params or {}}))
    while True:
        msg = json.loads(_recv(_sock))
        if msg.get("id") == _id:
            if "error" in msg: raise RuntimeError(msg["error"])
            return msg["result"]

def py(entry, **kwargs):
    r = call("run_python_file", {"filePath": HERE + "/capture_ue.py", "entryPoint": entry, "kwargs": kwargs})
    if r.get("logErrorCount"):
        raise RuntimeError(r.get("log_output"))
    v = r.get("result")
    if isinstance(v, str) and "Traceback" in v:
        raise RuntimeError(v)
    try:
        return ast.literal_eval(v) if isinstance(v, str) else v
    except Exception:
        return v
