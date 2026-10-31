"""Minimal stdlib WebSocket client for the UE_MCP_Bridge plugin (ws://127.0.0.1:<port>).
Usage: python -I bridge_call.py <method> '<json params>' [port] [timeout]"""
import base64, json, os, socket, struct, sys

def recv_exact(s, n):
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("closed")
        buf += chunk
    return buf

def send_frame(s, payload, opcode=1):
    data = payload.encode()
    hdr = bytes([0x80 | opcode])
    n = len(data)
    if n < 126:
        hdr += bytes([0x80 | n])
    elif n < 65536:
        hdr += bytes([0x80 | 126]) + struct.pack(">H", n)
    else:
        hdr += bytes([0x80 | 127]) + struct.pack(">Q", n)
    mask = os.urandom(4)
    s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

def recv_message(s):
    parts = []
    while True:
        b1, b2 = recv_exact(s, 2)
        op, n = b1 & 0x0F, b2 & 0x7F
        if n == 126:
            n = struct.unpack(">H", recv_exact(s, 2))[0]
        elif n == 127:
            n = struct.unpack(">Q", recv_exact(s, 8))[0]
        mask = recv_exact(s, 4) if b2 & 0x80 else None
        data = recv_exact(s, n)
        if mask:
            data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
        if op == 9:  # ping -> pong
            continue
        if op == 8:
            raise ConnectionError("server closed")
        parts.append(data)
        if b1 & 0x80:
            return b"".join(parts).decode()

method = sys.argv[1]
params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
port = int(sys.argv[3]) if len(sys.argv) > 3 else 55976
timeout = float(sys.argv[4]) if len(sys.argv) > 4 else 120
s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
key = base64.b64encode(os.urandom(16)).decode()
s.sendall((f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
resp = b""
while b"\r\n\r\n" not in resp:
    resp += s.recv(1024)
if b" 101 " not in resp.split(b"\r\n")[0]:
    raise SystemExit("handshake failed: " + resp.decode(errors="replace"))
send_frame(s, json.dumps({"id": 1, "method": method, "params": params}))
while True:
    msg = json.loads(recv_message(s))
    if msg.get("id") == 1:
        print(json.dumps(msg, indent=1)[:12000])
        break
s.close()
