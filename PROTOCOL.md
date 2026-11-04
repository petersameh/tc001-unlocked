# Ulanzi TC001 — stock firmware network control (reverse-engineered)

## Device
- ESP32 (Espressif MAC prefix `88:57:21`), stock Ulanzi firmware **1.7.6**
- Only TCP/80 open: unauthenticated web config UI
  - `/` general settings (POST form, `page=sys_setting`)
  - `/app_switch` enabled apps + YouTube/Bilibili/etc + **Awtrix Simulator** (POST form, `page=app_switch`)
  - `/info` about page, `/update` firmware update
- Saved page snapshots in `recon/`

## "Awtrix Simulator" = AWTRIX 1 MQTT client
Enable in `/app_switch`: `isAwtrixSimulator=on`, `awtrixServer=<host>`, `awtrixPort=7001`.
The clock connects **to us** as an MQTT 3.1.1 client:

| Step | Detail |
|---|---|
| CONNECT | client id `AWTRIXController-xxxx`, keepalive 15 s, clean session, no auth |
| SUBSCRIBE | `awtrixmatrix/#` QoS 0 |
| PUBLISH | topic `matrixClient`, payload `connected` |

Any MQTT broker works (we use `tools/mini_broker.py`). Commands are published to `awtrixmatrix/`
as **binary payloads**; byte 0 = command id. Multi-byte ints are big-endian.

## Commands (✅ = verified on device)
| Id | Command | Payload after id | Status |
|---|---|---|---|
| `00` | draw text | x(2) y(2) r g b text… | ✅ y=1 → glyph rows 1–5; 4 px/char, "1234" at x=0 spans cols 0–15 |
| `01` | draw bitmap | x(2) y(2) w(1) h(1) then w·h pixels as RGB565 (2 B each, row-major) | ✅ |
| `02` | circle outline | x(2) y(2) r(1) r g b | ✅ |
| `03` | filled circle | x(2) y(2) r(1) r g b | ✅ |
| `04` | pixel | x(2) y(2) r g b | ✅ (0,0) top-left, (31,7) bottom-right |
| `05` | rectangle outline | x(2) y(2) **w(1) h(1)** r g b | ✅ (w/h are 1 byte, not 2) |
| `06` | line | x0(2) y0(2) x1(2) y1(2) r g b | ✅ |
| `07` | fill matrix | r g b | ✅ |
| `08` | show (push buffer to LEDs) | — | ✅ |
| `09` | clear buffer | — | ✅ |

Drawing goes to a buffer; send `08` to display.

## Tools
- `tools/mini_broker.py [port] [log] [cmdfile]` — tiny MQTT broker; logs all packets;
  append lines `<topic> <hexpayload>` to the cmd file to publish to the clock.
- `tools/sniff_server.py` — raw TCP logger (first-contact recon).

## Hardware / firmware (from USB, 2026-11-04)
- ESP32-D0WD rev1.1, 40 MHz crystal, **4 MB flash**; no flash encryption, no secure boot, JTAG enabled
- Stock fw: Arduino-ESP32 1.0.6 / ESP-IDF v3.3.5, runs from `app0`; `app1` empty
- USB serial: CH340-style, `/dev/cu.usbserial-210`; 921600 baud unreliable, **460800 works**
- Full backup + restore steps: see the main [README](README.md#step-3--back-up-the-stock-firmware-usb)

## Verified pin map & peripherals (hwtest firmware, 2026-11-04)
| Function | GPIO / addr | Notes |
|---|---|---|
| LED matrix (WS2812B ×256, GRB) | 32 | serpentine rows from top-left: `idx = y*32 + (y even ? x : 31-x)` |
| Button left | 26 | INPUT_PULLUP, active LOW |
| Button middle | 27 | INPUT_PULLUP, active LOW |
| Button right | 14 | INPUT_PULLUP, active LOW |
| Buzzer | 15 | drive LOW at boot or it beeps |
| Light sensor (LDR) | 35 (ADC) | ~1075 in room light |
| Battery | 34 (ADC) | ~2530 on USB power |
| Temp/humidity SHT3x | I2C 0x44 | SDA 21, SCL 22 |
| RTC (DS1307-compatible) | I2C 0x68 | |
