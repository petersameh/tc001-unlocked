#!/usr/bin/env python3
"""Find smart IR blasters on the LAN: Tuya (TCP 6668 + UDP 6666/6667 broadcasts) and Broadlink (UDP 80 discovery)."""
import socket, struct, time, threading, sys, datetime

hosts = [l.split()[0] for l in open(sys.argv[1]) if not l.startswith("192.168.0.255")]
res = {"tuya_tcp": [], "tuya_bcast": {}, "broadlink": {}}

def tcp_probe(ip):
    s = socket.socket(); s.settimeout(0.8)
    try:
        if s.connect_ex((ip, 6668)) == 0: res["tuya_tcp"].append(ip)
    finally: s.close()

def tuya_listen(port, secs):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try: s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except Exception: pass
    s.bind(("", port)); s.settimeout(0.5)
    end = time.time() + secs
    while time.time() < end:
        try:
            d, a = s.recvfrom(4096)
            res["tuya_bcast"].setdefault(a[0], set()).add((port, len(d)))
        except socket.timeout: pass

def broadlink_discover():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind(("", 0)); s.settimeout(0.5)
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); probe.connect(("10.255.255.255", 1))
    local_ip = probe.getsockname()[0]; probe.close(); port = s.getsockname()[1]
    p = bytearray(0x30)
    now = datetime.datetime.now()
    tz = -int(time.timezone / 3600)
    struct.pack_into("<i", p, 0x08, tz)
    struct.pack_into("<H", p, 0x0C, now.year)
    p[0x0E] = now.minute; p[0x0F] = now.hour; p[0x10] = now.year % 100; p[0x11] = now.isoweekday(); p[0x12] = now.day; p[0x13] = now.month
    p[0x18:0x1C] = bytes(map(int, local_ip.split(".")))[::-1]
    struct.pack_into("<H", p, 0x1C, port)
    p[0x26] = 6
    cs = (sum(p) + 0xBEAF) & 0xFFFF
    struct.pack_into("<H", p, 0x20, cs)
    for _ in range(3): s.sendto(p, ("255.255.255.255", 80)); time.sleep(0.2)
    end = time.time() + 4
    while time.time() < end:
        try:
            d, a = s.recvfrom(1024)
            if len(d) >= 0x40:
                devtype = struct.unpack_from("<H", d, 0x34)[0]
                name = d[0x40:].split(b"\0")[0].decode(errors="replace")
                res["broadlink"][a[0]] = (hex(devtype), name)
        except socket.timeout: pass

threads = [threading.Thread(target=tcp_probe, args=(h,)) for h in hosts]
threads += [threading.Thread(target=tuya_listen, args=(p, 12)) for p in (6666, 6667)]
threads += [threading.Thread(target=broadlink_discover)]
for t in threads: t.start()
for t in threads: t.join()
print("Tuya local port 6668 open:", sorted(res["tuya_tcp"], key=lambda x: int(x.split('.')[-1])) or "none")
print("Tuya UDP broadcasts:", {k: sorted(v) for k, v in res["tuya_bcast"].items()} or "none")
print("Broadlink replies:", res["broadlink"] or "none")
