#!/usr/bin/env python3
"""Listen for Tuya UDP 6667 discovery broadcasts and decrypt them (well-known broadcast key)."""
import socket, json, hashlib, time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
KEY = hashlib.md5(b"yGAdlopoPVldABfn").digest()

def ecb_decrypt(b):
    d = Cipher(algorithms.AES(KEY), modes.ECB()).decryptor()
    out = d.update(b) + d.finalize()
    return out[:-out[-1]] if out and 0 < out[-1] <= 16 else out

def gcm_decrypt(pkt):  # protocol 3.5 (prefix 00006699)
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    hdr = pkt[4:18]; nonce = pkt[18:30]; body = pkt[30:-4]
    return AESGCM(KEY).decrypt(nonce, body, hdr)[4:]

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try: s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
except Exception: pass
s.bind(("", 6667)); s.settimeout(0.5)
seen = {}
end = time.time() + 15
while time.time() < end and len(seen) < 2:
    try: pkt, a = s.recvfrom(4096)
    except socket.timeout: continue
    if a[0] in seen: continue
    try:
        if pkt[:4] == b"\x00\x00\x55\xaa":
            body = pkt[20:-8]
            txt = ecb_decrypt(body)
        elif pkt[:4] == b"\x00\x00\x66\x99":
            txt = gcm_decrypt(pkt)
        else:
            continue
        j = json.loads(txt[txt.index(b"{"):].decode())
        seen[a[0]] = j
    except Exception as e:
        seen[a[0]] = {"error": str(e), "prefix": pkt[:4].hex()}
for ip, j in sorted(seen.items()):
    print(ip, json.dumps(j))
