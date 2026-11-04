# TC001 Unlocked — custom firmware for the Ulanzi TC001 pixel clock

Reverse-engineered and replaced the stock firmware of the **Ulanzi TC001** (ESP32, 32×8 RGB LED matrix) with an
open, English, fully customisable firmware — built step by step with **Claude Code**.
Everything below is the exact path we took, including the prompts you can give your own Claude to repeat it.

```
 ┌────────────────────────────────┐
 │ 16:22  31°  ▮▮▯                │   Dashboard: clock · weather · battery
 │ 126.5          10100           │              stock price · YouTube subscribers
 └────────────────────────────────┘
```

**What the custom firmware does**

| Screen / feature | What it shows |
|---|---|
| **Dashboard** | Two styles: *Compact* (two lines: clock, weather, battery / stock price, YouTube subs — nothing scrolls) or *Ticker* (pinned clock + scrolling ticker) |
| **Home** | Room temperature + humidity from Tuya IR hubs, and whether each room's AC is on (colour = mode) |
| **Clock / Date** | 24 h (optionally with seconds) or 12 h with AM/PM, weekday bar |
| **YouTube** | Live subscriber count fetched by the clock itself (YouTube Data API v3) |
| **Temp / Humidity / Battery** | Built-in SHT3x sensor and battery level |
| **Drawing** | Pixel art you draw in the web app, 6 save slots stored on the clock |
| **Notifications** | Push text + icon + colour + beep from any script or the web app |
| **Recording mode** | Camera overlay for YouTube videos: subscribers → ringing bell → like → flashing SUBSCRIBE (+ your logo). Mutes notifications and beeps |
| **Automations** | Rules like *"outdoor ≥ 32 °C and living room ≥ 27 °C between 12:00–23:00 → Living AC on, cool 24°"* using Tuya Wi-Fi IR blasters |
| **Web app** | `http://<clock-ip>/` — Home · Dashboard · Draw · Automations · Recording · Settings, with a live preview of the LEDs |
| **OTA** | Flash new firmware over Wi-Fi from the web app or `curl` |

> ⚠️ **Disclaimer.** Flashing replaces Ulanzi's firmware. Take the backup in [Step 3](#step-3--back-up-the-stock-firmware-usb) first — with it you can always restore the clock exactly as it shipped. You do this at your own risk.

---

## Contents

1. [What you need](#what-you-need)
2. [How we approached it (overview)](#how-we-approached-it)
3. [Step 1 — Find the clock on your network](#step-1--find-the-clock-on-your-network)
4. [Step 2 — Control it over Wi-Fi without flashing (optional)](#step-2--control-it-over-wi-fi-without-flashing-optional)
5. [Step 3 — Back up the stock firmware (USB)](#step-3--back-up-the-stock-firmware-usb)
6. [Step 4 — Verify the hardware pin map](#step-4--verify-the-hardware-pin-map)
7. [Step 5 — Flash the custom firmware](#step-5--flash-the-custom-firmware)
8. [Step 6 — First setup (Wi-Fi, YouTube, time zone)](#step-6--first-setup)
9. [Step 7 — Dashboard, stock & weather](#step-7--dashboard-stock--weather)
10. [Step 8 — Drawings & your logo](#step-8--drawings--your-logo)
11. [Step 9 — Recording mode](#step-9--recording-mode)
12. [Step 10 — Smart home: Tuya IR remotes & automations](#step-10--smart-home-tuya-ir-remotes--automations)
13. [Updating over Wi-Fi (OTA)](#updating-over-wi-fi-ota)
14. [Restoring the stock firmware](#restoring-the-stock-firmware)
15. [Buttons](#buttons)
16. [HTTP API](#http-api)
17. [Prompts for your Claude (copy-paste)](#prompts-for-your-claude)
18. [Troubleshooting & lessons learned](#troubleshooting--lessons-learned)
19. [Privacy: what never goes on GitHub](#privacy-what-never-goes-on-github)
20. [Project layout](#project-layout) · [Credits](#credits) · [License](#license)

---

## What you need

- **Ulanzi TC001** already on your Wi-Fi (set up once with the Ulanzi app / its own web page).
- A **USB-C data cable** (some cables are charge-only — if no serial port appears, try another cable).
- A computer with **Python 3.9+**. Tested on macOS; Linux and Windows work too
  (Windows may need the **CH340** USB-serial driver).
- Optional: **Claude Code** to drive the whole process ([prompts below](#prompts-for-your-claude)).
- Optional: a **YouTube Data API v3 key** (free, Google Cloud Console) for the subscriber counter.
- Optional: **Tuya / Smart Life Wi-Fi IR blasters** for the automations.

Install the tools once (in the repo folder):

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt     # esptool, platformio, pillow, tinytuya, cryptography
```

> Tip: plug the clock into a **wall charger or power bank**, not a laptop port, during long sessions — see
> [Troubleshooting](#troubleshooting--lessons-learned).

---

## How we approached it

1. **Look before touching.** Find the clock on the LAN and read its web pages (read-only).
2. **Use what's already there.** The stock firmware has an "Awtrix Simulator" mode: it connects as an MQTT
   client to a server you choose and draws AWTRIX 1 binary commands. A tiny Python broker gives full display control
   **without flashing** ([PROTOCOL.md](PROTOCOL.md)).
3. **Back up the whole 4 MB flash** over USB and verify it. Check eFuses: no flash encryption, no secure boot.
4. **Flash a hardware-test sketch** to confirm every pin (LEDs, buttons, buzzer, sensors) on the real unit.
5. **Write our own firmware** (Arduino + PlatformIO), with its own web app, and iterate over Wi-Fi (OTA).

---

## Step 1 — Find the clock on your network

The clock shows its IP when it boots (or check your router). Then:

```sh
curl -s http://<clock-ip>/info      # stock firmware: version, MAC, Wi-Fi name
```

What we found on stock firmware **1.7.6**: only TCP/80 open, an unauthenticated config site
(`/`, `/app_switch`, `/info`, `/update`), ESP32 MAC prefix `88:57:21` (Espressif).

> Note: the stock `/app_switch` page shows your **YouTube API key in plain text** to anyone on your Wi-Fi.

---

## Step 2 — Control it over Wi-Fi without flashing (optional)

The stock firmware's **Awtrix Simulator** = an **AWTRIX 1 MQTT client**:
it connects to `awtrixServer:7001`, subscribes to `awtrixmatrix/#`, publishes `matrixClient: connected`,
and draws binary commands (`00` text, `01` bitmap, `04` pixel, `05` rect, `06` line, `07` fill, `08` show, `09` clear…).
Full table in **[PROTOCOL.md](PROTOCOL.md)**.

```sh
mkdir -p recon && python3 tools/mini_broker.py 7001 recon/broker.log recon/broker.cmd   # tiny MQTT broker on your computer
# In the clock's web page → App Switch: enable "Awtrix Simulator", Server IP = your computer, Port = 7001, Save.
# Power-cycle the clock. Then:
python3 tools/awtrix1.py shapes        # test frame: pixels, rect, line, circles
python3 tools/awtrix1.py bmp           # YouTube icon + number
TC001_HOST=<clock-ip> python3 tools/youtube_display.py --once   # live subscriber count (stock firmware)
```

Limitation: your computer must stay on. That's why we went on to custom firmware.

---

## Step 3 — Back up the stock firmware (USB)

**Do not skip this.** Connect USB-C and find the port (`ls /dev/cu.*` on macOS — e.g. `/dev/cu.usbserial-XXXX`;
`/dev/ttyUSB0` on Linux; `COMx` on Windows).

```sh
PORT=/dev/cu.usbserial-XXXX
.venv/bin/esptool.py --port $PORT flash_id                       # chip info (read-only)
.venv/bin/espefuse.py --port $PORT summary | grep -E "FLASH_CRYPT_CNT|ABS_DONE"   # encryption / secure boot?
mkdir -p backup
.venv/bin/esptool.py --port $PORT --baud 460800 read_flash 0 0x400000 backup/tc001_stock_full.bin
shasum -a 256 backup/tc001_stock_full.bin > backup/SHA256SUMS
```

Our unit: **ESP32-D0WD rev 1.1, 4 MB flash, `FLASH_CRYPT_CNT = 0`, no secure boot** → readable backup, custom firmware allowed.

- `921600` baud failed for us ("serial noise"); **`460800` works**.
- `verify_flash` right after a dump can report a mismatch because the firmware **rewrites one NVS sector on every boot**.
  Do a second dump and compare per partition — only `nvs` should differ.

Stock partition table: `nvs 0x9000 (20K) · otadata 0xE000 · app0 0x10000 (1.9M) · app1 0x1F0000 (1.9M, empty) · spiffs 0x3D0000`.
Stock build: Arduino-ESP32 1.0.6 / ESP-IDF v3.3.5.

> 🔒 The backup contains your **Wi-Fi password** and **YouTube API key** (in NVS) and Ulanzi's proprietary firmware.
> **Never upload it.** `backup/` is in `.gitignore`.

---

## Step 4 — Verify the hardware pin map

Flash the test sketch (it replaces the stock firmware — you have the backup):

```sh
cd firmware/hwtest
../../.venv/bin/pio run -t upload            # add --upload-port $PORT if it isn't auto-detected
../../.venv/bin/pio device monitor           # watch button presses / I2C scan / ADC
```

It lights LED #0 red, #1–2 green, #31 blue, #32 white, #255 yellow and prints buttons, I2C devices and ADC values.
Verified pin map:

| Function | GPIO / I2C | Notes |
|---|---|---|
| LED matrix WS2812B ×256 (GRB) | 32 | serpentine rows from top-left: `idx = y*32 + (y even ? x : 31-x)` |
| Buttons left / middle / right | 26 / 27 / 14 | `INPUT_PULLUP`, active LOW |
| Buzzer | 15 | **drive LOW at boot** or it beeps |
| Light sensor (LDR) | 35 (ADC) | |
| Battery | 34 (ADC) | |
| Temp/humidity SHT3x | I2C `0x44` | SDA 21, SCL 22 (reads warm inside the case — use an offset) |
| RTC (DS1307-compatible) | I2C `0x68` | keeps time without Wi-Fi |

---

## Step 5 — Flash the custom firmware

```sh
cd firmware/tc001
../../.venv/bin/pio run -t upload            # first time over USB (add --upload-port $PORT if needed)
```

- Uses `min_spiffs.csv` (2 × 1.9 MB OTA slots) — after this first USB flash, **every update can go over Wi-Fi**.
- **Wi-Fi:** the clock reuses the Wi-Fi saved by the stock firmware (same NVS area). If it can't connect it opens an
  access point **`TC001-Setup`** for 3 minutes — join it with your phone and pick your network.
- The clock scrolls its IP after connecting. Also reachable at **http://tc001.local/**.

---

## Step 6 — First setup

Open `http://<clock-ip>/` → **Settings**:

- **Time zone** as a POSIX string, e.g. `<+03>-3` (fixed UTC+3), `EET-2EEST,M4.5.5/0,M10.5.4/24` (Cairo with DST),
  `GMT0BST,M3.5.0/1,M10.5.0` (London). 12/24 h, seconds, date format, week start.
- **YouTube:** Channel ID (`UC…`, from your channel URL) and an API key:
  Google Cloud Console → new project → *APIs & Services* → enable **YouTube Data API v3** → *Credentials* →
  **Create API key** → restrict it to YouTube Data API v3. The key is stored on the clock and never sent back to the browser.
- **Sensors:** temperature offset (the internal sensor reads ~8 °C warm), °C/°F, battery calibration.
- **Screens:** choose which screens rotate and for how long.

Note: YouTube rounds public subscriber counts to 3 significant figures (e.g. 10,137 → 10,100).

---

## Step 7 — Dashboard, stock & weather

Web app → **Dashboard** tab.

- **Style:** *Compact* (default; 4-px-tall font, values auto-shorten to fit: `126.51 → 126.5 → 127`, `10100 → 10.1K`)
  or *Ticker* (clock pinned left, scrolling items, battery bar).
- **Stock:** any TradingView symbol `EXCHANGE:SYMBOL` — `EGX:COMI`, `EGX:TMGH`, `NASDAQ:AAPL`.
  Uses TradingView's public scanner endpoint (unofficial, EGX ≈ 15 min delayed). Yahoo Finance had stale EGX data.
- **Weather:** city search (Open-Meteo geocoding), free, no API key.
- **Pin dashboard** keeps it on screen.

---

## Step 8 — Drawings & your logo

Web app → **Draw**: 32×8 editor (pen, eraser, fill, picker, undo, shift), *Live on clock*, *Copy from clock*,
6 slots saved on the device. Add *Drawing* to the rotation if you like.

**Logos:** a straight downscale of a detailed logo to 8 px tall is unreadable. What worked: hand-built pixel art
that keeps the recognisable parts (bold letters, key colours, signature dots). See `tools/mrps_pixelart.py`
(our example) and `tools/make_logo.py` (downscale experiments). Upload a frame:

```sh
curl -X POST -H 'Content-Type: text/plain' "http://<clock-ip>/api/draw?show=1" --data-binary @frame.hex
curl -X POST "http://<clock-ip>/api/draw/save?slot=1"
```

---

## Step 9 — Recording mode

For when the clock is on camera. Web app → **Recording**, or on the clock **long-press the middle button** to start,
**short-press middle** to stop.

- Slides (order/visibility editable): **Subscribers** (refreshes every 60 s), **Bell** (ringing), **Like**,
  **SUBSCRIBE** (flashing white-on-red / red-on-black, < 1 Hz), **Logo** (a drawing slot).
- While recording: rotation stops, **notifications and beeps are muted**, brightness switches to a camera value
  (default 150/255 — dim LEDs flicker/band on video).

---

## Step 10 — Smart home: Tuya IR remotes & automations

We used two **Tuya Wi-Fi IR blasters** ("Smart IR with T&H sensor") that already controlled ACs and TVs in the
Smart Life app. The clock talks to **Tuya's cloud OpenAPI** (the brand IR codes live in the cloud), reads room
temperature/humidity and AC state, and runs your rules.

1. **Find the hubs** (optional, read-only): `python3 tools/ir_discover.py hosts.txt` — Tuya devices have TCP 6668 open
   and broadcast on UDP 6667; `python3 tools/tuya_bcast.py` decrypts the broadcasts (device ID, protocol version).
2. **Create a free Tuya developer project** at [platform.tuya.com](https://platform.tuya.com) → *Cloud* →
   *Create Cloud Project*: Industry *Smart Home*, Development Method *Smart Home*, **Data Center = your app account's
   region** (the free tier allows one; Egypt/Middle East accounts were *Central Europe*).
   In *Authorize API Services* keep the defaults and **add "IR Control Hub Open Service"**.
3. **Link your app:** project → *Devices* → *Link App Account* → *Add App Account* → *Tuya App Account Authorization*
   → scan the QR with **Smart Life → Me → scan icon**. Your devices appear in the list.
4. **(Optional) local keys** for scripts: run the tinytuya wizard yourself in a terminal (it asks for secrets):
   ```sh
   mkdir -p tuya && cd tuya && ../.venv/bin/python -m tinytuya wizard
   # API Key = Access ID, API Secret = Access Secret (project Overview page), any device ID, region eu
   ```
   This writes `tuya/devices.json` (**local keys — private**).
5. **Connect the clock:** web app → **Automations** → *Tuya connection*: paste **Access ID** and **Access Secret**,
   region, the two **hub IDs** and a one-letter label each (e.g. `L`, `B`) → *Save connection* → *Discover remotes*.
   (The clock also auto-discovers when its remote list is empty.)
6. **Use it:**
   - **Devices:** AC on/off/mode/temp/fan; any TV/fan button (lists load from the cloud).
   - **Rules:** *When* (outdoor temp/humidity, rain, room temp/humidity per hub, AC is on) + time window + days →
     *Then* (set AC, press button, clock notification). Fires only on false→true, with a cooldown. *Test* runs the actions now.
   - **Home screen** on the clock: one line per room `L 26° 48% ▮` (bar: blue cool, orange heat, grey off).
   - **Log** shows every action and Tuya's reply.

Caveats: needs internet; the free Tuya IoT Core trial must be extended from time to time on the Tuya site;
anyone on your Wi-Fi can use the clock's web app.

---

## Updating over Wi-Fi (OTA)

```sh
cd firmware/tc001 && ../../.venv/bin/pio run
curl -F "firmware=@.pio/build/tc001/firmware.bin" http://<clock-ip>/update     # → "OK - rebooting"
```

or web app → Settings → *Firmware* → upload `firmware.bin`. If an upload is interrupted the clock simply keeps
running the old firmware — just retry.

## Restoring the stock firmware

```sh
shasum -a 256 -c backup/SHA256SUMS
.venv/bin/esptool.py --port $PORT --baud 460800 write_flash 0 backup/tc001_stock_full.bin
```

This rewrites all 4 MB (bootloader, partition table, firmware, settings incl. Wi-Fi) — the clock is exactly as before.

---

## Buttons

| Button | Normal | Recording mode |
|---|---|---|
| Left / Right (short) | previous / next screen | previous / next slide |
| Middle (short) | pause / resume rotation (red dot while paused) | **stop recording** |
| Middle (hold ~0.7 s) | **start recording** | — |
| Left (hold) | cycle brightness | — |

Any press dismisses a notification.

## HTTP API

Send JSON with `Content-Type: application/json`. Full list in [firmware/tc001/README.md](firmware/tc001/README.md).

```sh
curl http://<clock-ip>/api/status
curl -X POST -H 'Content-Type: application/json' http://<clock-ip>/api/notify \
     -d '{"text":"Hello!","icon":"bell","color":"#FFD60A","duration":8,"beep":true}'
curl "http://<clock-ip>/api/notify?text=Build%20passed&icon=check"
curl -X POST "http://<clock-ip>/api/show?app=dashboard&pin=1"
curl -X POST http://<clock-ip>/api/rec/start
```

---

## Prompts for your Claude

Paste these into Claude Code, opened in this repo folder, one phase at a time. Replace `<IP>`.
Claude should ask before anything that writes to the device — say yes only when you're ready.

**1. Recon (read-only)**
> My Ulanzi TC001 pixel clock is on my network at `<IP>`. Probe it read-only: open ports, its web pages, firmware
> version. Save the pages into `recon/` and summarise what's configurable. Don't change any settings.

**2. Control without flashing**
> Use the stock firmware's "Awtrix Simulator" option. Start `tools/mini_broker.py` on my computer, point the clock at
> it (keep all other settings unchanged), and test the AWTRIX 1 commands in PROTOCOL.md: fill, text, pixel, rect,
> line, circle, bitmap. Ask me what I see on the screen after each test.

**3. Backup**
> The clock is connected over USB. Install esptool in `.venv`, read chip info and eFuses (read-only), then dump the
> full 4 MB flash to `backup/`, hash it, and verify it with a second read compared per partition.
> Tell me whether flash encryption or secure boot is enabled. Do not write anything to the device.

**4. Hardware check**
> Flash `firmware/hwtest` (my backup is in `backup/`). Then read the serial output while I press each button,
> and ask me where the coloured test LEDs appear so we confirm the matrix wiring.

**5. Custom firmware**
> Flash `firmware/tc001` over USB. After it boots, check `/api/status`, then help me fill in Settings
> (time zone `<your TZ>`, YouTube channel ID). I'll paste my YouTube API key into the web app myself.

**6. Dashboard / stock / weather**
> Set the dashboard to Compact, stock symbol `<EXCHANGE:SYMBOL>`, weather city `<city>`. Read `/api/screen` and
> render it as ASCII so we can check the layout pixel by pixel.

**7. Logo**
> Here's my logo `<path>`. Make a 32×8 pixel-art version that stays readable (don't just downscale), show me a 12×
> preview image, then upload it to drawing slot 2.

**8. Smart home**
> Find Tuya devices on my LAN (read-only). Then guide me through creating a Tuya Cloud project and linking my
> Smart Life app — I'll type any secrets myself. Once linked, list my IR remotes from the cloud (read-only), and ask
> before sending any IR command. Then enter the hub IDs in the clock's Automations tab and run Discover.

**9. Before publishing**
> Scan the repo for secrets and personal data (API keys, Tuya credentials/local keys, Wi-Fi name, device IDs,
> backups). Make sure `.gitignore` covers them and nothing secret would be committed. Don't delete files.

---

## Troubleshooting & lessons learned

| Symptom | Cause / fix |
|---|---|
| Clock never connects to the Awtrix server | It only connects at **boot** — power-cycle after saving. It speaks MQTT, so a raw TCP listener sees one CONNECT and nothing else; it needs a broker that answers CONNACK. |
| `read_flash` fails with "serial noise" | Use `--baud 460800` instead of 921600. |
| `verify_flash` mismatch right after a dump | Normal — the firmware rewrote one NVS sector on boot. Compare two dumps per partition. |
| Battery % **drops while plugged into a laptop** | The laptop port didn't supply enough current; with a power bank the battery reading jumped up immediately. Use a ≥1 A charger. Battery % uses rough ADC limits — calibrate in Settings. |
| Internal temperature reads ~34 °C | The SHT3x sits next to the electronics. Set a temperature offset, or use the Tuya hubs' room sensors. |
| `curl` to `/api/settings` returns "bad json" | Add `-H 'Content-Type: application/json'`. |
| Web app button does nothing after an update | Hard-reload the page (⌘⇧R / Ctrl+F5) — the browser had the old page. |
| OTA upload interrupted | The clock keeps the old firmware; retry. Avoid other heavy polling of the clock during uploads. |
| Rule says `unknown remote` | Press *Discover remotes* (Automations → Tuya connection). |
| Text too wide | Font is 3×5 at 4 px per character (`:` `.` `!` are 2 px). 32 px fits 8 characters. |

## Privacy: what never goes on GitHub

`.gitignore` excludes these — keep it that way and check `git status` before every push:

| Path | Contains |
|---|---|
| `backup/` | full flash dumps → **Wi-Fi SSID + password**, YouTube API key, proprietary stock firmware |
| `tuya/`, `tinytuya.json`, `devices.json` | Tuya **Access ID/Secret** and every device's **local key** |
| `recon/` | stock-firmware captures → YouTube API key, channel ID, Wi-Fi name, LAN map |
| `.venv/`, `.pio/` | local environment / build output |

Secrets you enter in the web app (YouTube key, Tuya secret) are stored only on the clock and never returned by its API.
If any of these files ever leaked: regenerate the YouTube API key, reset the Tuya Access Secret, change your Wi-Fi password.

## Project layout

```
firmware/tc001/        custom firmware (PlatformIO): main.cpp, home.h (Tuya + rules), webui.h (web app), fonts, icons
firmware/hwtest/       hardware pin-map test sketch
tools/                 mini_broker.py, awtrix1.py, youtube_display.py (stock-firmware control)
                       ir_discover.py, tuya_bcast.py, tuya_sign_test.py (Tuya)
                       make_logo.py, mrps_pixelart.py, mock_compact.py (pixel-art / layout mockups)
PROTOCOL.md            stock firmware + AWTRIX 1 protocol + verified hardware notes
requirements.txt       pinned Python tools
```

## Credits

[AWTRIX](https://github.com/Blueforcer) (the protocol the stock firmware implements) ·
[FastLED](https://github.com/FastLED/FastLED) · [ArduinoJson](https://arduinojson.org) ·
[WiFiManager](https://github.com/tzapu/WiFiManager) · [tinytuya](https://github.com/jasonacox/tinytuya) ·
[Open-Meteo](https://open-meteo.com) · [esptool](https://github.com/espressif/esptool) ·
[PlatformIO](https://platformio.org). Built with [Claude Code](https://claude.com/claude-code).
Not affiliated with Ulanzi or Tuya.

## License

[MIT](LICENSE) © 2026 Peter Sameh — free to use, modify and share; provided as-is with no warranty.
The MR.PS. name and logo are not covered by the license.
