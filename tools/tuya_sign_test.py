#!/usr/bin/env python3
"""Reference Tuya OpenAPI client (HMAC-SHA256 'new' signature) — the exact algorithm ported to the ESP32.
Reads credentials from tuya/tinytuya.json and device IDs from tuya/devices.json; never prints secrets.
Usage: python3 tools/tuya_sign_test.py tuya/tinytuya.json   (read-only calls only)"""
import hashlib, hmac, json, time, urllib.request, sys
cfg = json.load(open(sys.argv[1]))
CID, SECRET = cfg["apiKey"], cfg["apiSecret"]
HOST = {"eu": "openapi.tuyaeu.com", "us": "openapi.tuyaus.com", "cn": "openapi.tuyacn.com", "in": "openapi.tuyain.com"}[cfg.get("apiRegion", "eu")]
token = ""

def sign(method, path, body=b"", tok=""):
    t = str(int(time.time() * 1000))
    sts = f"{method}\n{hashlib.sha256(body).hexdigest()}\n\n{path}"
    s = hmac.new(SECRET.encode(), (CID + tok + t + sts).encode(), hashlib.sha256).hexdigest().upper()
    h = {"client_id": CID, "sign": s, "t": t, "sign_method": "HMAC-SHA256"}
    if tok: h["access_token"] = tok
    if body: h["Content-Type"] = "application/json"
    return h

def call(method, path, body=None):
    b = json.dumps(body).encode() if body is not None else b""
    req = urllib.request.Request(f"https://{HOST}{path}", data=b or None, method=method, headers=sign(method, path, b, token))
    return json.load(urllib.request.urlopen(req, timeout=10))

r = call("GET", "/v1.0/token?grant_type=1")
print("token:", r.get("success"), "expire_time:", r.get("result", {}).get("expire_time"), r.get("msg", ""))
token = r["result"]["access_token"]
import os
devs = json.load(open(os.path.join(os.path.dirname(sys.argv[1]), "devices.json")))
hubs_ = [d["id"] for d in devs if d.get("category") == "wnykq"]
for hub in hubs_:
    st = call("GET", f"/v1.0/devices/{hub}/status")
    print("hub status", hub[-6:], st.get("success"), st.get("result"))
    rem = call("GET", f"/v2.0/infrareds/{hub}/remotes")
    print("  remotes:", rem.get("success"), [(x.get("remote_name"), x.get("remote_id")[-6:], x.get("category_id"), x.get("brand_id")) for x in (rem.get("result") or [])], rem.get("msg", ""))
acs = [(d["parent"], d["id"]) for d in devs if d.get("category") == "infrared_ac"]
for hub, ac in acs:
    s = call("GET", f"/v2.0/infrareds/{hub}/remotes/{ac}/ac/status")
    print("ac status", ac[-6:], s.get("success"), s.get("result"), s.get("msg", ""))
