# TC001 custom firmware

Standalone firmware for the Ulanzi TC001 (ESP32, 32×8 WS2812B). No cloud, no phone app.

**Web app:** `http://<clock-ip>/` (or http://tc001.local/)

## Screens
**Dashboard** · **Drawing** (pixel art, off in rotation by default) · Clock (with weekday bar) · Date · YouTube subscribers · Temperature · Humidity · Battery.

### Dashboard styles (32×8) — choose in the Dashboard tab (`dashStyle`)
**Compact (1, default)** — two lines, 4-row proportional font (`src/font4.h`), nothing scrolls:
```
top    (rows 0–3): clock HH:MM · weather temp (yellow sun / blue rain / grey cloud) · battery icon x26–31
bottom (rows 4–7): stock price (green up / red down) · YouTube subscribers (right-aligned)
```
Values shorten automatically to fit with a 2 px gap (126.51 → 126.5 → 127, 10100 → 10.1K → 10K).

**Ticker (0)**:
```
x 0–16   pinned clock HH:MM (+ dim seconds bar on row 7)
x 18–29  scrolling ticker: stock → YouTube → weather (order/visibility configurable)
x 31     battery bar, 8 steps bottom-up (green > 50 %, orange > 20 %, red)
```
Data sources: stock = TradingView scanner (`EXCHANGE:SYMBOL`, EGX ≈ 15 min delayed, unofficial endpoint);
weather = Open-Meteo (free, no key); YouTube = Data API v3. Configure in the web app's **Dashboard** tab.
Each can be enabled/disabled; rotation every N seconds (0 = manual only).

## Recording mode (camera overlay)
Cycles slides for when the clock is on camera: **subs** (YouTube icon + live count, refreshed every 60 s),
**bell** (ringing), **like** (thumbs up), **subscribe** (flashing white-on-red / red-on-black, < 1 Hz), **logo** (a drawing slot).
While active: normal rotation stops, notifications/beeps are muted (if `mute`), brightness switches to the recording value.
Start/stop: web app **Recording** tab, `POST /api/rec/start|stop`, **long-press middle** to start, **short-press middle** to stop. Left/right = previous/next slide.
Settings (`/api/rec`): `order` (comma list), `secs`, `bright`, `mute`, `logoSlot` (0-based).

## Buttons
| Button | Short press | Long press |
|---|---|---|
| Left | previous screen | cycle brightness |
| Middle | pause / resume rotation (red dot top-right while paused); **stops recording mode** | **start recording mode** |
| Right | next screen | — |
Any button dismisses a notification.

## Build & flash
```sh
cd firmware/tc001
../../.venv/bin/pio run                       # build
../../.venv/bin/pio run -t upload             # flash over USB
curl -F "firmware=@.pio/build/tc001/firmware.bin" http://<clock-ip>/update   # flash over Wi-Fi
```
Partition layout: `min_spiffs.csv` (2 × 1.9 MB OTA app slots).
Wi-Fi: reuses saved credentials; if it can't connect it opens AP **TC001-Setup** for 3 min.
Restore stock firmware: see [Restoring the stock firmware](../../README.md#restoring-the-stock-firmware) in the main README.

## HTTP API
Send JSON bodies with `Content-Type: application/json`.

| Method | Path | What |
|---|---|---|
| GET | `/api/status` | time, sensors, battery, subscribers, Wi-Fi, current screen |
| GET | `/api/settings` | all settings (API key is never returned, only `ytKeySet`) |
| POST | `/api/settings` | partial update, e.g. `{"bright":60,"apps":{"battery":false}}` |
| POST/GET | `/api/notify` | `{"text":"Hi","icon":"bell","color":"#FFD60A","duration":8,"beep":true}` or `?text=Hi&icon=heart` |
| GET | `/api/screen` | current frame: 256 × `rrggbb` hex, row-major from top-left |
| POST | `/api/next`, `/api/prev`, `/api/pause` | navigation |
| POST | `/api/show?app=dashboard[&pin=1]` | jump to a screen (pin = pause there) |
| POST | `/api/dashboard/refresh` | refetch stock, weather and YouTube now |
| POST | `/api/youtube/refresh` | fetch subscriber count now |
| GET | `/api/draw` | current drawing: 256 × `rrggbb`, row-major from top-left |
| POST | `/api/draw[?show=1]` | set drawing (body = 1536 hex chars, `text/plain`); `show=1` jumps to it and pauses |
| POST | `/api/draw/save?slot=0-5` | store current drawing in a slot (NVS, survives reboot) |
| POST | `/api/draw/load?slot=0-5` | load a slot into the drawing screen (returns the frame) |
| GET | `/api/draw/slots` | `{"active":n,"slots":[hex or null ×6]}` |
| POST | `/api/beep`, `/api/reboot` | |
| POST | `/update` | multipart firmware upload (OTA) |

Icons: `bell`, `heart`, `check`, `youtube`, `temp`, `drop`.
Text longer than the screen scrolls automatically. Font is a custom 3×5 (4 px/char, `:` `.` `!` are 2 px); `` ` `` renders as °.

## Settings keys
`tz` (POSIX TZ string), `bright` 1–255, `h24`, `secs`, `dateFmt` (0 MM/DD, 1 DD/MM), `weekMon`,
`appDur`, `apps{clock,date,youtube,temp,humidity,battery}`, `colClock`, `colDate`, `colYt`, `colTemp`,
`tempF`, `tempOffset`, `ytId`, `ytKey`, `ytEvery` (s, ≥60), `batEmpty`, `batFull` (raw ADC).
Dashboard: `dashDur`, `dashOrder` (e.g. `"stock,youtube,weather"`), `tickSpeed` (px/s), `dashSecBar`,
`colDashClock`, `colStock`, `stockSym`, `stockEvery`, `lat`, `lon`, `city`, `wxEvery`.

## Known caveats
- The SHT3x sits inside the case and reads warm (≈34 °C at room temp). Set `tempOffset`.
- Battery % uses guessed raw ADC limits (2100 → 2600); calibrate `batEmpty`/`batFull`.
- YouTube rounds public subscriber counts to 3 significant figures.
- HTTPS to the YouTube API skips certificate validation (`setInsecure`).
