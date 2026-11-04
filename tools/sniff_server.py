#!/usr/bin/env python3
"""Passive TCP listener: logs everything the TC001 sends when pointed at us as an 'Awtrix server'."""
import socket, sys, time, threading, binascii
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 7001
LOG = sys.argv[2] if len(sys.argv) > 2 else "sniff.log"

def log(msg):
    line = f"{time.strftime('%H:%M:%S')} {msg}"
    print(line, flush=True)
    with open(LOG, "a") as f: f.write(line + "\n")

def handle(c, addr):
    log(f"CONNECT {addr}")
    c.settimeout(None)
    try:
        while True:
            d = c.recv(4096)
            if not d: break
            log(f"RX {len(d)}B hex={binascii.hexlify(d).decode()} ascii={d!r}")
    except Exception as e:
        log(f"ERR {e}")
    log(f"CLOSE {addr}")

s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", PORT)); s.listen(5)
log(f"listening on {PORT}")
while True:
    c, a = s.accept()
    threading.Thread(target=handle, args=(c, a), daemon=True).start()
