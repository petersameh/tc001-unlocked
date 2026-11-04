#!/usr/bin/env python3
"""Show the live YouTube subscriber count on the TC001 via the AWTRIX 1 protocol.

Needs tools/mini_broker.py running and the clock's Awtrix Simulator pointed at it.
Channel ID + API key are read from the clock's own settings page (nothing stored here).

  python3 tools/youtube_display.py            # refresh every 5 minutes
  python3 tools/youtube_display.py --once     # draw once and exit
"""
import json, os, re, sys, time, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from awtrix1 import send, clear, show, icon_and_text, youtube_icon, WHITE

CLOCK = "http://" + os.environ.get("TC001_HOST", "tc001.local")
REFRESH_S = 300
MAX_CHARS = 5  # 7px icon + 2px gap + 5*4px text = 29px <= 32


def creds():
    h = urllib.request.urlopen(f"{CLOCK}/app_switch", timeout=5).read().decode("utf-8", "replace")
    uid = re.search(r"name='youtubeUid'[^>]*value='([^']*)'", h).group(1)
    key = re.search(r"name='youtubeApikey'[^>]*value='([^']*)'", h).group(1)
    return uid, key


def subscribers(uid, key):
    url = f"https://www.googleapis.com/youtube/v3/channels?part=statistics&id={uid}&key={key}"
    r = json.load(urllib.request.urlopen(url, timeout=10))
    return int(r["items"][0]["statistics"]["subscriberCount"])


def fmt(n):
    s = str(n)
    if len(s) <= MAX_CHARS:
        return s
    for div, suf in ((1_000_000_000, "B"), (1_000_000, "M"), (1_000, "K")):
        if n >= div:
            v = n / div
            for dec in (2, 1, 0):
                t = f"{v:.{dec}f}".rstrip("0").rstrip(".") + suf
                if len(t) <= MAX_CHARS:
                    return t
    return s[:MAX_CHARS]


def draw(n):
    send(clear(), *icon_and_text(youtube_icon(), 7, 7, fmt(n), WHITE), show())


def main():
    once = "--once" in sys.argv
    uid, key = creds()
    while True:
        try:
            n = subscribers(uid, key)
            draw(n)
            print(f"{time.strftime('%H:%M:%S')} subscribers={n} shown={fmt(n)!r}", flush=True)
        except Exception as e:
            print(f"{time.strftime('%H:%M:%S')} error: {e}", flush=True)
        if once:
            break
        time.sleep(REFRESH_S)


if __name__ == "__main__":
    main()
