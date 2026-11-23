"""Minimal client of the UE_MCP_Bridge editor WebSocket (stdlib only, no websockets package): one JSON request, one JSON answer.

  python Tools/GAS/ue_bridge_client.py <method> '<json params>'      -> prints the answer (truncated to 1500 chars)
  from ue_bridge_client import Bridge; Bridge().call("list_niagara_systems", {})

The port is read from the bridge's state file (Saved/UE-MCP/*.json or similar) or UE_MCP_PORT, default 9877. Requests are
{"id", "method", "params"}; answers carry "result" or "error" (BridgeServer.cpp).
"""
import base64
import glob
import json
import os
import socket
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _port():
    if os.environ.get("UE_MCP_PORT"):
        return int(os.environ["UE_MCP_PORT"])
    for path in sorted(glob.glob(os.path.join(ROOT, "Saved", "**", "*.json"), recursive=True), key=os.path.getmtime, reverse=True):
        if "mcp" not in path.lower() or "bridge" not in path.lower() and "ue-mcp" not in path.lower():
            continue
        try:
            data = json.load(open(path, encoding="utf-8"))
        except Exception:
            continue
        if isinstance(data, dict) and isinstance(data.get("port"), int):
            return data["port"]
    return 9877


class Bridge:
    def __init__(self, port=None, timeout=300.0):
        self.port = port or _port()
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{self.port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("bridge closed during handshake")
            head += chunk
        if b" 101" not in head.split(b"\r\n", 1)[0]:
            raise ConnectionError(head.split(b"\r\n", 1)[0].decode(errors="replace"))
        self.buffer = head.split(b"\r\n\r\n", 1)[1]
        self.next_id = 1

    def _send(self, text):
        payload = text.encode("utf-8")
        mask = os.urandom(4)
        n = len(payload)
        header = bytearray([0x81])
        if n < 126:
            header.append(0x80 | n)
        elif n < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", n)
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", n)
        header += mask
        self.sock.sendall(bytes(header) + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def _read(self, n):
        while len(self.buffer) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("bridge closed")
            self.buffer += chunk
        out, self.buffer = self.buffer[:n], self.buffer[n:]
        return out

    def _recv(self):
        message = b""
        while True:
            b0, b1 = self._read(2)
            opcode, n = b0 & 0x0F, b1 & 0x7F
            if n == 126:
                n = struct.unpack(">H", self._read(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", self._read(8))[0]
            mask = self._read(4) if b1 & 0x80 else None
            data = self._read(n)
            if mask:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            if opcode == 0x9:  # ping -> pong
                continue
            if opcode == 0x8:
                raise ConnectionError("bridge sent close")
            message += data
            if b0 & 0x80:
                return message.decode("utf-8")

    def call(self, method, params=None):
        request_id = self.next_id
        self.next_id += 1
        self._send(json.dumps({"id": request_id, "method": method, "params": params or {}}))
        while True:
            answer = json.loads(self._recv())
            if answer.get("id") == request_id:
                if "error" in answer and answer["error"]:
                    raise RuntimeError(f"{method}: {json.dumps(answer['error'])[:800]}")
                return answer.get("result")

    def python(self, code):
        return self.call("execute_python", {"code": code})


if __name__ == "__main__":
    result = Bridge().call(sys.argv[1], json.loads(sys.argv[2]) if len(sys.argv) > 2 else {})
    print(json.dumps(result)[:1500])
