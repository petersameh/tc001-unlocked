#!/usr/bin/env python3
"""Minimal MQTT 3.1.1 'broker' for reverse engineering the TC001 Awtrix client.
Acks CONNECT/SUBSCRIBE/PING, logs every packet, and lets us publish to the clock
by dropping lines into a control file:  <topic> <hex-payload>   (one per line)."""
import socket, sys, time, threading, binascii, os
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 7001
LOG = sys.argv[2] if len(sys.argv) > 2 else "broker.log"
CTRL = sys.argv[3] if len(sys.argv) > 3 else "broker.cmd"
clients = []

def log(m):
    line = f"{time.strftime('%H:%M:%S')} {m}"
    print(line, flush=True)
    with open(LOG, "a") as f: f.write(line + "\n")

def enc_len(n):
    out = bytearray()
    while True:
        b = n % 128; n //= 128
        out.append(b | (0x80 if n else 0))
        if not n: return bytes(out)

def publish(c, topic, payload):
    t = topic.encode()
    body = len(t).to_bytes(2, "big") + t + payload
    c.sendall(bytes([0x30]) + enc_len(len(body)) + body)

def read_pkt(c):
    h = c.recv(1)
    if not h: return None
    mult, n = 1, 0
    while True:
        b = c.recv(1)[0]; n += (b & 0x7F) * mult; mult *= 128
        if not b & 0x80: break
    data = b""
    while len(data) < n:
        chunk = c.recv(n - len(data))
        if not chunk: return None
        data += chunk
    return h[0], data

def handle(c, addr):
    log(f"CONNECT-TCP {addr}")
    clients.append(c)
    try:
        while True:
            p = read_pkt(c)
            if p is None: break
            h, d = p; typ = h >> 4
            if typ == 1:
                cid = d[12:12 + int.from_bytes(d[10:12], "big")]
                log(f"MQTT CONNECT client={cid!r} keepalive={int.from_bytes(d[8:10],'big')} raw={d.hex()}")
                c.sendall(b"\x20\x02\x00\x00")
            elif typ == 8:
                pid = d[:2]; i = 2; topics = []
                while i < len(d):
                    l = int.from_bytes(d[i:i+2], "big"); topics.append((d[i+2:i+2+l].decode(errors='replace'), d[i+2+l])); i += 3 + l
                log(f"SUBSCRIBE {topics}")
                c.sendall(bytes([0x90, 2 + len(topics)]) + pid + bytes(q for _, q in topics))
            elif typ == 3:
                qos = (h >> 1) & 3; l = int.from_bytes(d[:2], "big"); topic = d[2:2+l].decode(errors='replace')
                i = 2 + l
                if qos: pid = d[i:i+2]; i += 2; c.sendall(b"\x40\x02" + pid)
                pl = d[i:]
                log(f"PUBLISH topic={topic} {len(pl)}B hex={pl.hex()} ascii={pl!r}")
            elif typ == 12:
                c.sendall(b"\xd0\x00")
            elif typ == 14:
                log("DISCONNECT"); break
            else:
                log(f"PKT type={typ} flags={h&15} data={d.hex()}")
    except Exception as e:
        log(f"ERR {e}")
    if c in clients: clients.remove(c)
    log(f"CLOSE {addr}")

def ctrl_loop():
    while True:
        time.sleep(0.3)
        if os.path.exists(CTRL) and os.path.getsize(CTRL):
            lines = open(CTRL).read().splitlines(); open(CTRL, "w").close()
            for ln in lines:
                if not ln.strip(): continue
                topic, _, hx = ln.partition(" ")
                for c in list(clients):
                    try: publish(c, topic, bytes.fromhex(hx.strip())); log(f"TX topic={topic} hex={hx.strip()}")
                    except Exception as e: log(f"TX ERR {e}")

threading.Thread(target=ctrl_loop, daemon=True).start()
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", PORT)); s.listen(5)
log(f"mini broker listening on {PORT}")
while True:
    c, a = s.accept()
    threading.Thread(target=handle, args=(c, a), daemon=True).start()
