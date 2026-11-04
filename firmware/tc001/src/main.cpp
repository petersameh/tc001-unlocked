// TC001 custom firmware — clock, date, YouTube subscribers, sensors, notifications, web app.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <time.h>
#include <vector>
#include <sys/time.h>
#include "esp_sntp.h"
#include "font.h"
#include "font4.h"
#include "icons.h"
#include "webui.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// ---------- Hardware (verified with hwtest) ----------
#define PIN_MATRIX 32
#define PIN_BUZZER 15
#define PIN_BTN_LEFT 26
#define PIN_BTN_MID 27
#define PIN_BTN_RIGHT 14
#define PIN_LDR 35
#define PIN_BAT 34
#define PIN_SDA 21
#define PIN_SCL 22
#define SHT_ADDR 0x44
#define RTC_ADDR 0x68

#define MW 32
#define MH 8
CRGB leds[MW * MH];
CRGB drawBuf[MW * MH];  // drawing screen, row-major (x + y*32), NOT serpentine

static inline int XY(int x, int y) { return y * MW + ((y & 1) ? (MW - 1 - x) : x); }
int clipX0 = 0, clipX1 = MW - 1;  // horizontal clip window for widgets
static inline void px(int x, int y, CRGB c) {
  if (x < clipX0 || x > clipX1 || x < 0 || x >= MW || y < 0 || y >= MH) return;
  leds[XY(x, y)] = c;
}

// ---------- Settings ----------
enum App { APP_DASH, APP_HOME, APP_CLOCK, APP_DATE, APP_YT, APP_TEMP, APP_HUM, APP_BAT, APP_DRAW, APP_COUNT };
static const char *APP_KEYS[APP_COUNT] = {"dashboard", "home", "clock", "date", "youtube", "temp", "humidity", "battery", "drawing"};

struct Settings {
  String tz = "<+03>-3";       // POSIX TZ (stock was GMT+3)
  uint8_t bright = 40;
  bool h24 = true;
  bool secs = true;            // stock: HH:mm:ss
  uint8_t dateFmt = 0;         // 0 = MM/DD (stock), 1 = DD/MM
  bool weekMon = false;        // stock: Sunday first
  uint16_t appDur = 10;        // seconds per screen, 0 = no auto switch
  bool en[APP_COUNT] = {true, true, true, true, true, true, true, true, false};
  uint32_t colClock = 0xFFFFFF, colDate = 0xFFFFFF, colYt = 0xFFFFFF, colTemp = 0xFFFFFF;
  bool tempF = false;
  float tempOffset = 0.0f;
  String ytId, ytKey;
  uint16_t ytEvery = 300;      // seconds
  uint16_t batEmpty = 2100, batFull = 2600;  // raw ADC calibration
  // Dashboard
  uint16_t dashDur = 30;               // seconds the dashboard stays in rotation
  uint8_t dashStyle = 1;               // 0 = ticker, 1 = compact two-line
  String dashOrder = "stock,youtube,weather";  // ticker items, comma separated
  uint8_t tickSpeed = 20;              // ticker px/s
  bool dashSecBar = true;
  uint32_t colDashClock = 0xFFFFFF, colStock = 0x5AC8FA;
  String stockSym = "EGX:COMI";        // TradingView EXCHANGE:SYMBOL
  uint16_t stockEvery = 60;
  float lat = 30.0444f, lon = 31.2357f;
  String city = "Cairo";
  uint16_t wxEvery = 900;
  uint8_t drawSlot = 0;                // active drawing slot
} S;

Preferences prefs;
SemaphoreHandle_t setMux;

void loadSettings() {
  prefs.begin("tc001", true);
  S.tz = prefs.getString("tz", S.tz);
  S.bright = prefs.getUChar("bright", S.bright);
  S.h24 = prefs.getBool("h24", S.h24);
  S.secs = prefs.getBool("secs", S.secs);
  S.dateFmt = prefs.getUChar("dateFmt", S.dateFmt);
  S.weekMon = prefs.getBool("weekMon", S.weekMon);
  S.appDur = prefs.getUShort("appDur", S.appDur);
  for (int i = 0; i < APP_COUNT; i++) S.en[i] = prefs.getBool((String("en_") + APP_KEYS[i]).c_str(), S.en[i]);
  S.colClock = prefs.getUInt("colClock", S.colClock);
  S.colDate = prefs.getUInt("colDate", S.colDate);
  S.colYt = prefs.getUInt("colYt", S.colYt);
  S.colTemp = prefs.getUInt("colTemp", S.colTemp);
  S.tempF = prefs.getBool("tempF", S.tempF);
  S.tempOffset = prefs.getFloat("tempOff", S.tempOffset);
  S.ytId = prefs.getString("ytId", "");
  S.ytKey = prefs.getString("ytKey", "");
  S.ytEvery = prefs.getUShort("ytEvery", S.ytEvery);
  S.batEmpty = prefs.getUShort("batEmpty", S.batEmpty);
  S.batFull = prefs.getUShort("batFull", S.batFull);
  S.dashDur = prefs.getUShort("dashDur", S.dashDur);
  S.dashStyle = prefs.getUChar("dashStyle", S.dashStyle);
  S.dashOrder = prefs.getString("dashOrder", S.dashOrder);
  S.tickSpeed = prefs.getUChar("tickSpeed", S.tickSpeed);
  S.dashSecBar = prefs.getBool("dashSecBar", S.dashSecBar);
  S.colDashClock = prefs.getUInt("colDashClk", S.colDashClock);
  S.colStock = prefs.getUInt("colStock", S.colStock);
  S.stockSym = prefs.getString("stockSym", S.stockSym);
  S.stockEvery = prefs.getUShort("stockEvery", S.stockEvery);
  S.lat = prefs.getFloat("lat", S.lat);
  S.lon = prefs.getFloat("lon", S.lon);
  S.city = prefs.getString("city", S.city);
  S.wxEvery = prefs.getUShort("wxEvery", S.wxEvery);
  S.drawSlot = prefs.getUChar("drawSlot", S.drawSlot);
  prefs.end();
}

void saveSettings() {
  prefs.begin("tc001", false);
  prefs.putString("tz", S.tz);
  prefs.putUChar("bright", S.bright);
  prefs.putBool("h24", S.h24);
  prefs.putBool("secs", S.secs);
  prefs.putUChar("dateFmt", S.dateFmt);
  prefs.putBool("weekMon", S.weekMon);
  prefs.putUShort("appDur", S.appDur);
  for (int i = 0; i < APP_COUNT; i++) prefs.putBool((String("en_") + APP_KEYS[i]).c_str(), S.en[i]);
  prefs.putUInt("colClock", S.colClock);
  prefs.putUInt("colDate", S.colDate);
  prefs.putUInt("colYt", S.colYt);
  prefs.putUInt("colTemp", S.colTemp);
  prefs.putBool("tempF", S.tempF);
  prefs.putFloat("tempOff", S.tempOffset);
  prefs.putString("ytId", S.ytId);
  prefs.putString("ytKey", S.ytKey);
  prefs.putUShort("ytEvery", S.ytEvery);
  prefs.putUShort("batEmpty", S.batEmpty);
  prefs.putUShort("batFull", S.batFull);
  prefs.putUShort("dashDur", S.dashDur);
  prefs.putUChar("dashStyle", S.dashStyle);
  prefs.putString("dashOrder", S.dashOrder);
  prefs.putUChar("tickSpeed", S.tickSpeed);
  prefs.putBool("dashSecBar", S.dashSecBar);
  prefs.putUInt("colDashClk", S.colDashClock);
  prefs.putUInt("colStock", S.colStock);
  prefs.putString("stockSym", S.stockSym);
  prefs.putUShort("stockEvery", S.stockEvery);
  prefs.putFloat("lat", S.lat);
  prefs.putFloat("lon", S.lon);
  prefs.putString("city", S.city);
  prefs.putUShort("wxEvery", S.wxEvery);
  prefs.putUChar("drawSlot", S.drawSlot);
  prefs.end();
}

// ---------- Drawing ----------
int textWidth(const String &s) {
  int w = 0;
  for (size_t i = 0; i < s.length(); i++) w += glyphWidth(s[i]) + 1;
  return w > 0 ? w - 1 : 0;
}

int drawChar(int x, int y, char c, CRGB col) {
  const uint8_t *g = glyphFor(c);
  int w = glyphWidth(c);
  for (int r = 0; r < 5; r++) {
    uint8_t bits = g[r];
    if (glyphNarrow(c)) {
      if (bits & 2) px(x, y + r, col);
    } else if (c != ' ') {
      if (bits & 4) px(x, y + r, col);
      if (bits & 2) px(x + 1, y + r, col);
      if (bits & 1) px(x + 2, y + r, col);
    }
  }
  return w;
}

void drawText(int x, int y, const String &s, CRGB col) {
  for (size_t i = 0; i < s.length(); i++) x += drawChar(x, y, s[i], col) + 1;
}

CRGB paletteColor(char c) {
  switch (c) {
    case 'R': return CRGB(255, 0, 0);
    case 'W': return CRGB(255, 255, 255);
    case 'B': return CRGB(0, 90, 255);
    case 'L': return CRGB(150, 210, 255);
    case 'G': return CRGB(0, 220, 0);
    case 'Y': return CRGB(255, 200, 0);
    case 'O': return CRGB(255, 100, 0);
    case 'g': return CRGB(130, 130, 140);
    default: return CRGB::Black;
  }
}

void drawIcon(int x, int y, const Icon *ic) {
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 7; c++) {
      char ch = ic->rows[r][c];
      if (ch != '.') px(x + c, y + r, paletteColor(ch));
    }
}

// Battery icon drawn procedurally with fill level
void drawBatteryIcon(int x, int y, int pct) {
  CRGB w = CRGB(255, 255, 255);
  for (int i = 0; i < 6; i++) { px(x + i, y + 1, w); px(x + i, y + 5, w); }
  for (int j = 1; j <= 5; j++) { px(x, y + j, w); px(x + 5, y + j, w); }
  for (int j = 2; j <= 4; j++) px(x + 6, y + j, w);
  CRGB f = pct > 50 ? CRGB(0, 220, 0) : pct > 20 ? CRGB(255, 160, 0) : CRGB(255, 0, 0);
  int cols = (pct * 4 + 50) / 100;
  for (int i = 0; i < cols; i++)
    for (int j = 2; j <= 4; j++) px(x + 1 + i, y + j, f);
}

CRGB rgbOf(uint32_t v) { return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF); }

// Icon + text centred as a group (icon 7px, gap 2px). icon may be null.
void drawIconText(const Icon *ic, const String &s, CRGB col, int batPct = -1) {
  bool hasIcon = ic || batPct >= 0;
  int total = (hasIcon ? 9 : 0) + textWidth(s);
  int x0 = (MW - total) / 2;
  if (x0 < 0) x0 = 0;
  if (ic) drawIcon(x0, 0, ic);
  if (batPct >= 0) drawBatteryIcon(x0, 0, batPct);
  drawText(x0 + (hasIcon ? 9 : 0), 1, s, col);
}

void drawCentered(const String &s, CRGB col, int y = 1) {
  int x = (MW - textWidth(s)) / 2;
  drawText(x < 0 ? 0 : x, y, s, col);
}

// ---------- Recording mode (camera overlay) ----------
struct RecCfg {
  String order = "subs,bell,like,subscribe";  // slides: subs, bell, like, subscribe, logo
  uint8_t secs = 4;          // seconds per slide
  uint8_t bright = 150;      // brightness while recording (camera-friendly)
  bool mute = true;          // suppress notifications + beeps while recording
  uint8_t logoSlot = 1;      // drawing slot for the "logo" slide (0-based)
} REC;
bool recActive = false;
int recIdx = 0;
uint32_t recSlideStart = 0;
CRGB recLogo[32 * 8];

void loadRec() {
  Preferences p; p.begin("rec", true);
  REC.order = p.getString("order", REC.order); REC.secs = p.getUChar("secs", REC.secs);
  REC.bright = p.getUChar("bright", REC.bright); REC.mute = p.getBool("mute", REC.mute);
  REC.logoSlot = p.getUChar("logo", REC.logoSlot);
  p.end();
}
void saveRec() {
  Preferences p; p.begin("rec", false);
  p.putString("order", REC.order); p.putUChar("secs", REC.secs); p.putUChar("bright", REC.bright);
  p.putBool("mute", REC.mute); p.putUChar("logo", REC.logoSlot);
  p.end();
}

// ---------- Buzzer ----------
void beep(int ms = 80, int freq = 2200) {
  if (recActive && REC.mute) return;  // never beep on camera
  tone(PIN_BUZZER, freq, ms);
  delay(ms + 5);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
}

// ---------- Sensors ----------
float tempC = NAN, humidity = NAN;
int ldrRaw = 0, batRaw = 0;

bool readSHT(float &t, float &h) {
  Wire.beginTransmission(SHT_ADDR);
  Wire.write(0x24); Wire.write(0x00);  // single shot, high repeatability, no clock stretch
  if (Wire.endTransmission() != 0) return false;
  delay(16);
  if (Wire.requestFrom(SHT_ADDR, 6) != 6) return false;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  uint16_t rt = (b[0] << 8) | b[1], rh = (b[3] << 8) | b[4];
  t = -45.0f + 175.0f * rt / 65535.0f;
  h = 100.0f * rh / 65535.0f;
  return true;
}

int batteryPct() {
  if (S.batFull <= S.batEmpty) return 0;
  long p = (long)(batRaw - S.batEmpty) * 100 / (S.batFull - S.batEmpty);
  return constrain(p, 0, 100);
}

// ---------- RTC (DS1307-compatible, stores UTC) ----------
static uint8_t bcd2bin(uint8_t v) { return (v & 0x0F) + 10 * (v >> 4); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static time_t epochFromUtc(int y, int mo, int d, int h, int mi, int s) {
  // days_from_civil (Howard Hinnant)
  y -= mo <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + (long)doe - 719468;
  return (time_t)days * 86400 + h * 3600 + mi * 60 + s;
}

bool rtcRead() {
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(RTC_ADDR, 7) != 7) return false;
  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = Wire.read();
  if (r[0] & 0x80) return false;  // clock halted -> never set
  int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1]), hr = bcd2bin(r[2] & 0x3F);
  int day = bcd2bin(r[4]), mon = bcd2bin(r[5] & 0x1F), yr = 2000 + bcd2bin(r[6]);
  if (yr < 2024 || mon < 1 || mon > 12 || day < 1 || day > 31) return false;
  struct timeval tv = {epochFromUtc(yr, mon, day, hr, min, sec), 0};
  settimeofday(&tv, nullptr);
  return true;
}

void rtcWrite() {
  time_t now = time(nullptr);
  struct tm u;
  gmtime_r(&now, &u);
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(0);
  Wire.write(bin2bcd(u.tm_sec));
  Wire.write(bin2bcd(u.tm_min));
  Wire.write(bin2bcd(u.tm_hour));  // 24h mode
  Wire.write(bin2bcd(u.tm_wday + 1));
  Wire.write(bin2bcd(u.tm_mday));
  Wire.write(bin2bcd(u.tm_mon + 1));
  Wire.write(bin2bcd(u.tm_year - 100));
  Wire.endTransmission();
}

volatile bool ntpSynced = false;
bool timeValid() { return time(nullptr) > 1700000000; }

// ---------- Data fetching (background task on core 0) ----------
volatile long ytSubs = -1;
volatile bool ytForce = true, stockForce = true, wxForce = true;
String ytErr = "", stockErr = "not fetched", wxErr = "not fetched", stockCur = "";
uint32_t ytLastOk = 0, stockLastOk = 0, wxLastOk = 0;
float stockPrice = NAN, stockChg = NAN, wxTemp = NAN, wxHum = NAN;
int wxCode = -1, wxIsDay = 1;

// HTTPS GET (body == nullptr) or JSON POST. Returns HTTP code, fills out on 200.
int httpsRequest(const String &url, const String *body, String &out) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return -1;
  http.setUserAgent("Mozilla/5.0 (TC001 clock)");
  int code;
  if (body) { http.addHeader("Content-Type", "application/json"); code = http.POST(*body); }
  else code = http.GET();
  if (code == 200) out = http.getString();
  http.end();
  return code;
}

void setErr(String &dst, const String &v) { xSemaphoreTake(setMux, portMAX_DELAY); dst = v; xSemaphoreGive(setMux); }

void fetchYouTube() {
  xSemaphoreTake(setMux, portMAX_DELAY);
  String id = S.ytId, key = S.ytKey;
  xSemaphoreGive(setMux);
  if (!id.length() || !key.length()) { setErr(ytErr, "not configured"); return; }
  String out;
  int code = httpsRequest("https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + id + "&key=" + key, nullptr, out);
  if (code != 200) { setErr(ytErr, "http " + String(code)); return; }
  JsonDocument doc;
  if (deserializeJson(doc, out)) { setErr(ytErr, "bad json"); return; }
  const char *sc = doc["items"][0]["statistics"]["subscriberCount"];
  if (!sc) { setErr(ytErr, "channel not found"); return; }
  ytSubs = atol(sc);
  ytLastOk = millis();
  setErr(ytErr, "");
}

void fetchStock() {
  xSemaphoreTake(setMux, portMAX_DELAY);
  String sym = S.stockSym;
  xSemaphoreGive(setMux);
  if (!sym.length()) { setErr(stockErr, "no symbol"); return; }
  String body = "{\"symbols\":{\"tickers\":[\"" + sym + "\"]},\"columns\":[\"close\",\"change\",\"currency\"]}";
  String out;
  int code = httpsRequest("https://scanner.tradingview.com/global/scan", &body, out);
  if (code != 200) { setErr(stockErr, "http " + String(code)); return; }
  JsonDocument doc;
  if (deserializeJson(doc, out)) { setErr(stockErr, "bad json"); return; }
  JsonArray data = doc["data"];
  if (data.size() == 0) { setErr(stockErr, "symbol not found"); return; }
  JsonArray d = data[0]["d"];
  xSemaphoreTake(setMux, portMAX_DELAY);
  stockPrice = d[0] | NAN;
  stockChg = d[1] | NAN;
  stockCur = d[2] | "";
  stockErr = "";
  xSemaphoreGive(setMux);
  stockLastOk = millis();
}

void fetchWeather() {
  xSemaphoreTake(setMux, portMAX_DELAY);
  float lat = S.lat, lon = S.lon;
  bool f = S.tempF;
  xSemaphoreGive(setMux);
  char url[200];
  snprintf(url, sizeof url, "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code,is_day%s",
           lat, lon, f ? "&temperature_unit=fahrenheit" : "");
  String out;
  int code = httpsRequest(url, nullptr, out);
  if (code != 200) { setErr(wxErr, "http " + String(code)); return; }
  JsonDocument doc;
  if (deserializeJson(doc, out)) { setErr(wxErr, "bad json"); return; }
  JsonObject c = doc["current"];
  if (c.isNull()) { setErr(wxErr, "no data"); return; }
  xSemaphoreTake(setMux, portMAX_DELAY);
  wxTemp = c["temperature_2m"] | NAN;
  wxHum = c["relative_humidity_2m"] | NAN;
  wxCode = c["weather_code"] | -1;
  wxIsDay = c["is_day"] | 1;
  wxErr = "";
  xSemaphoreGive(setMux);
  wxLastOk = millis();
}

void homeTick();
void renderHome();

bool dashWants(const char *item) { return S.en[APP_DASH] && S.dashOrder.indexOf(item) >= 0; }

void dataTask(void *) {
  uint32_t lastYt = 0, lastStock = 0, lastWx = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      uint32_t now = millis();
      if (ytForce || now - lastYt > (uint32_t)(recActive ? 60 : S.ytEvery) * 1000UL) { ytForce = false; lastYt = now; fetchYouTube(); }
      if (stockForce || (dashWants("stock") && now - lastStock > (uint32_t)S.stockEvery * 1000UL)) {
        stockForce = false; lastStock = now; fetchStock();
      }
      if (wxForce || (dashWants("weather") && now - lastWx > (uint32_t)S.wxEvery * 1000UL)) {
        wxForce = false; lastWx = now; fetchWeather();
      }
      homeTick();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

String fmtCount(long n) {
  String s = String(n);
  if (s.length() <= 5) return s;
  struct { long div; const char *suf; } U[] = {{1000000000L, "B"}, {1000000L, "M"}, {1000L, "K"}};
  for (auto &u : U) {
    if (n >= u.div) {
      float v = (float)n / u.div;
      for (int dec = 2; dec >= 0; dec--) {
        String t = String(v, dec);
        if (t.indexOf('.') >= 0) { while (t.endsWith("0")) t.remove(t.length() - 1); if (t.endsWith(".")) t.remove(t.length() - 1); }
        t += u.suf;
        if (t.length() <= 5) return t;
      }
    }
  }
  return s.substring(0, 5);
}

// ---------- Notifications ----------
struct Notification {
  bool active = false;
  String text;
  CRGB color = CRGB::White;
  const Icon *icon = nullptr;
  uint32_t until = 0;
  uint32_t started = 0;
} N;

void notify(const String &text, CRGB color, const String &icon, int seconds, bool doBeep) {
  if (recActive && REC.mute) { Serial.println("[rec] muted notification: " + text); return; }
  N.text = text;
  N.color = color;
  N.icon = iconByName(icon);
  N.started = millis();
  int w = (N.icon ? 9 : 0) + textWidth(text);
  uint32_t minScroll = w > MW ? (uint32_t)(MW + w) * 1000 / 25 + 500 : 0;  // one full pass at 25 px/s
  N.until = millis() + max<uint32_t>((uint32_t)max(seconds, 1) * 1000, minScroll);
  N.active = true;
  if (doBeep) beep();
}

void renderNotification() {
  int w = (N.icon ? 9 : 0) + textWidth(N.text);
  if (w <= MW) {
    drawIconText(N.icon, N.text, N.color);
    return;
  }
  uint32_t t = millis() - N.started;
  int span = MW + w;
  int x = MW - (int)((t * 25 / 1000) % span);
  if (N.icon) drawIcon(x, 0, N.icon);
  drawText(x + (N.icon ? 9 : 0), 1, N.text, N.color);
}

// ---------- Screens ----------
int curApp = APP_DASH;
bool paused = false;
uint32_t lastSwitch = 0;
String overlayMsg;
uint32_t overlayUntil = 0;

void showOverlay(const String &m, uint32_t ms = 1200) { overlayMsg = m; overlayUntil = millis() + ms; }

void nextApp(int dir) {
  for (int i = 0; i < APP_COUNT; i++) {
    curApp = (curApp + dir + APP_COUNT) % APP_COUNT;
    if (S.en[curApp]) break;
  }
  lastSwitch = millis();
}

void drawWeekBar(int wday) {
  int idx = S.weekMon ? (wday + 6) % 7 : wday;
  int x0 = (MW - 27) / 2;
  for (int d = 0; d < 7; d++)
    for (int i = 0; i < 3; i++) px(x0 + d * 4 + i, 7, d == idx ? CRGB(255, 255, 255) : CRGB(50, 50, 50));
}

// ---------- Dashboard: pinned clock | scrolling ticker | battery bar ----------
#define DASH_TICK_X0 18
#define DASH_TICK_X1 29
#define DASH_BAT_X 31

struct Seg { const Icon *icon; String text; CRGB col; int gapAfter; };

int segWidth(const Seg &g) { return (g.icon ? 7 + (g.text.length() ? 2 : 0) : 0) + textWidth(g.text); }

const Icon *weatherIcon(int code, bool day) {
  if (code == 0) return day ? &ICON_SUN : &ICON_MOON;
  if (code == 1 || code == 2) return day ? &ICON_PARTLY : &ICON_CLOUD;
  if (code == 3) return &ICON_CLOUD;
  if (code == 45 || code == 48) return &ICON_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return &ICON_SNOW;
  if (code >= 95) return &ICON_STORM;
  if (code >= 51) return &ICON_RAIN;
  return &ICON_CLOUD;
}
const char *weatherWord(int code) {
  if (code == 0) return "CLEAR";
  if (code == 1 || code == 2) return "FAIR";
  if (code == 3) return "CLOUDY";
  if (code == 45 || code == 48) return "FOG";
  if (code >= 51 && code <= 57) return "DRIZZLE";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "SNOW";
  if (code >= 95) return "STORM";
  if (code >= 61) return "RAIN";
  return "";
}

String fmtPrice(float p) {
  if (isnan(p)) return "--";
  if (p >= 10000) return String((long)lroundf(p));
  if (p >= 1000) return String(p, 1);
  return String(p, 2);
}

void buildTicker(std::vector<Seg> &out) {
  xSemaphoreTake(setMux, portMAX_DELAY);
  String order = S.dashOrder, sym = S.stockSym;
  float price = stockPrice, chg = stockChg, wt = wxTemp;
  int code = wxCode, day = wxIsDay;
  bool f = S.tempF;
  uint32_t cStock = S.colStock, cYt = S.colYt;
  xSemaphoreGive(setMux);
  int from = 0;
  while (from <= (int)order.length()) {
    int comma = order.indexOf(',', from);
    if (comma < 0) comma = order.length();
    String item = order.substring(from, comma);
    item.trim();
    from = comma + 1;
    if (item == "stock") {
      int c = sym.indexOf(':');
      out.push_back({nullptr, c >= 0 ? sym.substring(c + 1) : sym, rgbOf(cStock), 2});
      out.push_back({nullptr, fmtPrice(price), CRGB::White, 2});
      if (!isnan(chg)) {
        bool up = chg >= 0;
        out.push_back({nullptr, String(up ? "{" : "}") + String(fabsf(chg), 2) + "%", up ? CRGB(0, 220, 0) : CRGB(255, 40, 40), 8});
      } else out.back().gapAfter = 8;
    } else if (item == "youtube") {
      out.push_back({&ICON_YOUTUBE, ytSubs >= 0 ? fmtCount(ytSubs) : String("..."), rgbOf(cYt), 8});
    } else if (item == "weather") {
      if (isnan(wt)) out.push_back({&ICON_CLOUD, "--", CRGB::White, 8});
      else {
        out.push_back({weatherIcon(code, day), String((int)lroundf(wt)) + (f ? "`F" : "`"), CRGB::White, 2});
        CRGB wc = code == 0 ? CRGB(255, 200, 0) : code >= 51 ? CRGB(90, 160, 255) : CRGB(170, 170, 180);
        out.push_back({nullptr, weatherWord(code), wc, 8});
      }
    }
  }
}

void drawSeg(int x, const Seg &g) {
  if (g.icon) { drawIcon(x, 0, g.icon); x += 7 + (g.text.length() ? 2 : 0); }
  drawText(x, 1, g.text, g.col);
}


// ---------- Compact dashboard: two lines with the 4-row font ----------
// top:    clock (left) · weather temp (right of centre) · battery icon (top-right)
// bottom: stock price (left, green up / red down) · YouTube subscribers (right)
void drawText4(int x, int y, const String &s, CRGB col) {
  for (size_t i = 0; i < s.length(); i++) {
    const Glyph4 *g = glyph4(s[i]);
    if (!g) continue;
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < g->w; c++)
        if (g->rows[r] & (1 << (g->w - 1 - c))) px(x + c, y + r, col);
    x += g->w + 1;
  }
}

void drawMiniBattery(int x, int y, int pct) {  // 6x4
  CRGB w = CRGB(200, 200, 200);
  for (int i = 0; i < 5; i++) { px(x + i, y, w); px(x + i, y + 3, w); }
  px(x, y + 1, w); px(x, y + 2, w); px(x + 4, y + 1, w); px(x + 4, y + 2, w);
  px(x + 5, y + 1, w); px(x + 5, y + 2, w);
  CRGB f = pct > 50 ? CRGB(0, 220, 0) : pct > 20 ? CRGB(255, 160, 0) : CRGB(255, 0, 0);
  int cols = (pct * 3 + 50) / 100;
  for (int i = 0; i < cols; i++) { px(x + 1 + i, y + 1, f); px(x + 1 + i, y + 2, f); }
}

void subsVariants(long n, String *v, int &cnt) {
  cnt = 0;
  if (n < 0) { v[cnt++] = "-"; return; }
  v[cnt++] = String(n);
  const long divs[] = {1000000000L, 1000000L, 1000L};
  const char *suf[] = {"B", "M", "K"};
  for (int i = 0; i < 3; i++)
    if (n >= divs[i]) {
      float x = (float)n / divs[i];
      for (int dec = 2; dec >= 0; dec--) {
        String t = String(x, dec);
        if (t.indexOf('.') >= 0) { while (t.endsWith("0")) t.remove(t.length() - 1); if (t.endsWith(".")) t.remove(t.length() - 1); }
        t += suf[i];
        if (v[cnt - 1] != t) v[cnt++] = t;
      }
      break;
    }
}

void priceVariants(float p, String *v, int &cnt) {
  cnt = 0;
  if (isnan(p)) { v[cnt++] = "-"; return; }
  for (int dec = 2; dec >= 0; dec--) {
    String t = String(p, dec);
    if (!cnt || v[cnt - 1] != t) v[cnt++] = t;
  }
}

void renderCompact(bool tv, const struct tm &t) {
  xSemaphoreTake(setMux, portMAX_DELAY);
  float price = stockPrice, chg = stockChg, wt = wxTemp;
  int code = wxCode;
  bool f = S.tempF;
  uint32_t cClock = S.colDashClock, cStock = S.colStock, cYt = S.colYt;
  bool wantStock = S.dashOrder.indexOf("stock") >= 0, wantYt = S.dashOrder.indexOf("youtube") >= 0,
       wantWx = S.dashOrder.indexOf("weather") >= 0;
  xSemaphoreGive(setMux);

  // ---- top line ----
  String clk = "-:--";
  if (tv) {
    char b[8];
    int h = t.tm_hour;
    if (!S.h24) { h %= 12; if (h == 0) h = 12; }
    snprintf(b, sizeof b, S.h24 ? "%02d:%02d" : "%d:%02d", h, t.tm_min);
    clk = b;
  }
  drawText4(0, 0, clk, rgbOf(cClock));
  int clkEnd = text4Width(clk) - 1;
  drawMiniBattery(26, 0, batteryPct());
  if (wantWx && !isnan(wt)) {
    String tt = String((int)lroundf(wt)) + "`";
    // right edge at x=24 (1 px before battery), keep >= 2 px gap after the clock
    if (25 - text4Width(tt) < clkEnd + 3) tt.remove(tt.length() - 1);  // drop degree sign if tight
    int tw = text4Width(tt);
    if (25 - tw >= clkEnd + 3) {
      CRGB wc = code == 0 ? CRGB(255, 200, 0) : code >= 51 ? CRGB(90, 160, 255) : CRGB(180, 180, 190);
      drawText4(25 - tw, 0, tt, wc);
    }
  }

  // ---- bottom line: pick the most precise pair that fits with a 2 px gap ----
  String pv[3], yv[5];
  int pc = 0, yc = 0;
  if (wantStock) priceVariants(price, pv, pc);
  if (wantYt) subsVariants(ytSubs, yv, yc);
  String P = pc ? pv[pc - 1] : "", Y = yc ? yv[yc - 1] : "";
  bool found = false;
  for (int yi = 0; yi < max(yc, 1) && !found; yi++)
    for (int pi = 0; pi < max(pc, 1) && !found; pi++) {
      String pp = pc ? pv[pi] : "", yy = yc ? yv[yi] : "";
      int w = text4Width(pp) + text4Width(yy) + (pp.length() && yy.length() ? 2 : 0);
      if (w <= MW) { P = pp; Y = yy; found = true; }
    }
  CRGB pcol = isnan(chg) ? rgbOf(cStock) : chg >= 0 ? CRGB(0, 220, 0) : CRGB(255, 40, 40);
  if (P.length()) drawText4(0, 4, P, pcol);
  if (Y.length()) drawText4(MW - text4Width(Y), 4, Y, rgbOf(cYt));
}

void renderDashboard(bool tv, const struct tm &t) {
  // Pinned clock
  if (tv) {
    char b[8];
    int h = t.tm_hour;
    if (!S.h24) { h %= 12; if (h == 0) h = 12; }
    snprintf(b, sizeof b, S.h24 ? "%02d:%02d" : "%d:%02d", h, t.tm_min);
    String cs = b;
    int cx = (17 - textWidth(cs)) / 2;
    drawText(cx, 1, cs, rgbOf(S.colDashClock));
    if (S.dashSecBar) {
      int n = (t.tm_sec * 17 + 30) / 60;
      for (int i = 0; i < n; i++) px(i, 7, CRGB(45, 45, 55));
    }
  } else drawText(0, 1, "--:--", rgbOf(S.colDashClock));

  // Ticker window
  std::vector<Seg> segs;
  buildTicker(segs);
  int total = 0;
  for (auto &g : segs) total += segWidth(g) + g.gapAfter;
  if (total > 0) {
    clipX0 = DASH_TICK_X0; clipX1 = DASH_TICK_X1;
    int off = (int)((uint64_t)millis() * S.tickSpeed / 1000 % total);
    for (int rep = 0; rep < 2; rep++) {
      int x = DASH_TICK_X0 - off + rep * total;
      for (auto &g : segs) {
        int w = segWidth(g);
        if (x + w >= DASH_TICK_X0 && x <= DASH_TICK_X1) drawSeg(x, g);
        x += w + g.gapAfter;
      }
    }
    clipX0 = 0; clipX1 = MW - 1;
  }

  // Battery bar (bottom-up, 8 steps)
  int pct = batteryPct();
  int lvl = (pct * 8 + 50) / 100;
  CRGB bc = pct > 50 ? CRGB(0, 220, 0) : pct > 20 ? CRGB(255, 160, 0) : CRGB(255, 0, 0);
  for (int y = 0; y < 8; y++) px(DASH_BAT_X, y, (7 - y) < lvl ? bc : CRGB(22, 22, 26));
}

void renderApp() {
  struct tm t;
  bool tv = timeValid();
  if (tv) { time_t now = time(nullptr); localtime_r(&now, &t); }
  switch (curApp) {
    case APP_DASH:
      if (S.dashStyle == 1) renderCompact(tv, t);
      else renderDashboard(tv, t);
      break;
    case APP_HOME:
      renderHome();
      break;
    case APP_DRAW:
      for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) px(x, y, drawBuf[y * MW + x]);
      break;
    case APP_CLOCK: {
      if (!tv) { drawCentered("--:--", rgbOf(S.colClock)); break; }
      char b[12];
      if (S.h24) {
        if (S.secs) snprintf(b, sizeof b, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        else snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
        drawCentered(b, rgbOf(S.colClock));
      } else {
        // 12-hour: H:MM + AM/PM centred as one group (no room for seconds: "10:42:15 PM" = 36 px)
        int h = t.tm_hour % 12;
        if (h == 0) h = 12;
        snprintf(b, sizeof b, "%d:%02d", h, t.tm_min);
        const char *ap = t.tm_hour < 12 ? "AM" : "PM";
        int x0 = (MW - (textWidth(b) + 2 + textWidth(ap))) / 2;
        CRGB c = rgbOf(S.colClock);
        drawText(x0, 1, b, c);
        CRGB dim = c;
        dim.nscale8(170);
        drawText(x0 + textWidth(b) + 2, 1, ap, dim);
      }
      drawWeekBar(t.tm_wday);
      break;
    }
    case APP_DATE: {
      if (!tv) { drawCentered("--/--", rgbOf(S.colDate)); break; }
      char b[8];
      if (S.dateFmt == 0) snprintf(b, sizeof b, "%02d/%02d", t.tm_mon + 1, t.tm_mday);
      else snprintf(b, sizeof b, "%02d/%02d", t.tm_mday, t.tm_mon + 1);
      drawCentered(b, rgbOf(S.colDate));
      drawWeekBar(t.tm_wday);
      break;
    }
    case APP_YT:
      drawIconText(&ICON_YOUTUBE, ytSubs >= 0 ? fmtCount(ytSubs) : String("..."), rgbOf(S.colYt));
      break;
    case APP_TEMP: {
      if (isnan(tempC)) { drawIconText(&ICON_TEMP, "--", rgbOf(S.colTemp)); break; }
      float v = tempC + S.tempOffset;
      if (S.tempF) v = v * 9.0f / 5.0f + 32.0f;
      drawIconText(&ICON_TEMP, String(v, 1) + (S.tempF ? "`F" : "`C"), rgbOf(S.colTemp));
      break;
    }
    case APP_HUM:
      drawIconText(&ICON_DROP, isnan(humidity) ? String("--") : String((int)lroundf(humidity)) + "%", rgbOf(S.colTemp));
      break;
    case APP_BAT: {
      int p = batteryPct();
      drawIconText(nullptr, String(p) + "%", rgbOf(S.colTemp), p);
      break;
    }
  }
  if (paused && curApp != APP_DRAW) px(curApp == APP_DASH ? 30 : MW - 1, 0, CRGB(255, 0, 0));  // tiny pause indicator (not over drawings)
}

// ---------- Recording slides ----------
bool loadDrawingSlot(int slot, CRGB *dst);

std::vector<String> recSlides() {
  std::vector<String> v;
  int from = 0;
  while (from <= (int)REC.order.length()) {
    int c = REC.order.indexOf(',', from);
    if (c < 0) c = REC.order.length();
    String it = REC.order.substring(from, c); it.trim();
    if (it.length()) v.push_back(it);
    from = c + 1;
  }
  if (v.empty()) v.push_back("subs");
  return v;
}

void recStart() {
  recActive = true; recIdx = 0; recSlideStart = millis(); N.active = false;
  loadDrawingSlot(REC.logoSlot, recLogo);
  ytForce = true;
  Serial.println("[rec] start");
}
void recStop() { recActive = false; lastSwitch = millis(); Serial.println("[rec] stop"); }
void recStep(int d) {
  int n = recSlides().size();
  recIdx = (recIdx + d + n) % n; recSlideStart = millis();
}

// "SUBSCRIBE" squeezed to exactly 32 px: narrow I (1 px) and condensed C (2 px)
void drawSubscribe(CRGB col) {
  const char *w = "SUBSCRIBE";
  int x = 0;
  for (int i = 0; w[i]; i++) {
    char c = w[i];
    if (c == 'I') { for (int r = 0; r < 5; r++) px(x, 1 + r, col); x += 2; continue; }
    if (c == 'C') {
      const uint8_t rows[5] = {3, 2, 2, 2, 3};  // 2 px wide: bit1 = left, bit0 = right
      for (int r = 0; r < 5; r++) { if (rows[r] & 2) px(x, 1 + r, col); if (rows[r] & 1) px(x + 1, 1 + r, col); }
      x += 3; continue;
    }
    x += drawChar(x, 1, c, col) + 1;
  }
}

void renderRecording() {
  std::vector<String> sl = recSlides();
  if (recIdx >= (int)sl.size()) recIdx = 0;
  if (millis() - recSlideStart > (uint32_t)max<int>(REC.secs, 1) * 1000UL) recStep(+1);
  String s = sl[recIdx];
  uint32_t t = millis() - recSlideStart;
  if (s == "subs") {
    drawIconText(&ICON_YOUTUBE, ytSubs >= 0 ? fmtCount(ytSubs) : String("..."), CRGB::White);
  } else if (s == "bell") {
    int wob = ((t / 120) % 6 < 4) ? (((t / 120) % 2) ? 1 : -1) : 0;  // ring, ring, pause
    int total = 9 + textWidth("BELL");
    int x0 = (MW - total) / 2;
    drawIcon(x0 + wob, 0, &ICON_BELL);
    drawText(x0 + 9, 1, "BELL", CRGB::White);
  } else if (s == "like") {
    drawIconText(&ICON_LIKE, "LIKE", CRGB::White);
  } else if (s == "subscribe") {
    bool on = (t / 600) % 2 == 0;  // ~0.8 Hz: gentle on camera
    if (on) { for (int i = 0; i < MW * MH; i++) leds[i] = CRGB(230, 0, 0); drawSubscribe(CRGB::White); }
    else drawSubscribe(CRGB(230, 0, 0));
  } else if (s == "logo") {
    for (int y = 0; y < MH; y++) for (int x = 0; x < MW; x++) px(x, y, recLogo[y * MW + x]);
  }
}

void render() {
  FastLED.clear();
  if (recActive) {
    renderRecording();
    FastLED.setBrightness(REC.bright);
    FastLED.show();
    return;
  }
  if (millis() < overlayUntil) drawCentered(overlayMsg, CRGB(0, 200, 255));
  else if (N.active) {
    if ((int32_t)(millis() - N.until) >= 0) N.active = false;
    else renderNotification();
  }
  if (millis() >= overlayUntil && !N.active) renderApp();
  FastLED.setBrightness(S.bright);
  FastLED.show();
}

// ---------- Buttons ----------
struct Btn {
  int pin; bool down = false; uint32_t since = 0; bool longFired = false;
  explicit Btn(int p) : pin(p) {}
};
Btn btnL(PIN_BTN_LEFT), btnM(PIN_BTN_MID), btnR(PIN_BTN_RIGHT);
const uint8_t BRIGHT_STEPS[] = {8, 25, 60, 120, 255};

void cycleBrightness() {
  int i = 0;
  while (i < (int)sizeof(BRIGHT_STEPS) && BRIGHT_STEPS[i] <= S.bright) i++;
  S.bright = BRIGHT_STEPS[i % sizeof(BRIGHT_STEPS)];
  saveSettings();
  showOverlay("B " + String(S.bright * 100 / 255) + "%");
}

// returns 1 on short press release, 2 on long press (fires once while held)
int pollBtn(Btn &b) {
  bool d = digitalRead(b.pin) == LOW;
  uint32_t now = millis();
  int ev = 0;
  if (d && !b.down) { b.down = true; b.since = now; b.longFired = false; }
  else if (d && b.down && !b.longFired && now - b.since > 700) { b.longFired = true; ev = 2; }
  else if (!d && b.down) {
    if (!b.longFired && now - b.since > 30) ev = 1;
    b.down = false;
  }
  return ev;
}

void handleButtons() {
  int l = pollBtn(btnL), m = pollBtn(btnM), r = pollBtn(btnR);
  if (recActive) {  // recording: middle press stops, left/right step slides
    if (m == 1) recStop();
    if (l == 1) recStep(-1);
    if (r == 1) recStep(+1);
    return;
  }
  if (m == 2) { recStart(); return; }  // long-press middle starts recording mode
  if ((l || m || r) && N.active) { N.active = false; return; }  // any press dismisses a notification
  if (l == 1) nextApp(-1);
  if (r == 1) nextApp(+1);
  if (m == 1) { paused = !paused; showOverlay(paused ? "PAUSE" : "PLAY", 800); lastSwitch = millis(); }
  if (l == 2) cycleBrightness();  // long-press left: brightness
}

// ---------- Drawing (pixel art) ----------
#define DRAW_SLOTS 6

String slotKey(int slot) { return "s" + String(slot); }

bool loadDrawingSlot(int slot, CRGB *dst) {
  Preferences p;
  p.begin("draw", true);
  size_t n = p.getBytes(slotKey(slot).c_str(), dst, sizeof(CRGB) * MW * MH);
  p.end();
  if (n != sizeof(CRGB) * MW * MH) { memset(dst, 0, sizeof(CRGB) * MW * MH); return false; }
  return true;
}

void saveDrawingSlot(int slot) {
  Preferences p;
  p.begin("draw", false);
  p.putBytes(slotKey(slot).c_str(), drawBuf, sizeof drawBuf);
  p.end();
}

String framesToHex(const CRGB *buf) {
  String out;
  out.reserve(MW * MH * 6);
  char b[7];
  for (int i = 0; i < MW * MH; i++) { snprintf(b, sizeof b, "%02x%02x%02x", buf[i].r, buf[i].g, buf[i].b); out += b; }
  return out;
}

bool hexToFrame(const String &h, CRGB *dst) {
  if (h.length() < MW * MH * 6) return false;
  char b[3] = {0, 0, 0};
  for (int i = 0; i < MW * MH; i++) {
    uint8_t v[3];
    for (int k = 0; k < 3; k++) { b[0] = h[i * 6 + k * 2]; b[1] = h[i * 6 + k * 2 + 1]; v[k] = strtoul(b, nullptr, 16); }
    dst[i] = CRGB(v[0], v[1], v[2]);
  }
  return true;
}

#include "home.h"

// ---------- Web server ----------
WebServer server(80);

String hexColor(uint32_t v) { char b[8]; snprintf(b, sizeof b, "#%06X", (unsigned)(v & 0xFFFFFF)); return b; }
uint32_t parseColor(const char *s, uint32_t def) {
  if (!s) return def;
  if (*s == '#') s++;
  if (strlen(s) != 6) return def;
  return strtoul(s, nullptr, 16) & 0xFFFFFF;
}

void sendJson(JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

void apiStatus() {
  JsonDocument d;
  d["fw"] = FW_VERSION;
  d["ip"] = WiFi.localIP().toString();
  d["rssi"] = WiFi.RSSI();
  d["ssid"] = WiFi.SSID();
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["timeValid"] = timeValid();
  if (timeValid()) {
    time_t now = time(nullptr); struct tm t; localtime_r(&now, &t);
    char b[24]; strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &t); d["time"] = b;
  }
  if (!isnan(tempC)) d["tempC"] = roundf((tempC + S.tempOffset) * 10) / 10;
  if (!isnan(humidity)) d["humidity"] = roundf(humidity * 10) / 10;
  d["ldrRaw"] = ldrRaw;
  d["batRaw"] = batRaw;
  d["batPct"] = batteryPct();
  d["ytSubs"] = (long)ytSubs;
  d["ytAgeS"] = ytLastOk ? (long)((millis() - ytLastOk) / 1000) : -1L;
  xSemaphoreTake(setMux, portMAX_DELAY);
  JsonObject st = d["stock"].to<JsonObject>();
  st["symbol"] = S.stockSym;
  if (!isnan(stockPrice)) st["price"] = stockPrice;
  if (!isnan(stockChg)) st["changePct"] = roundf(stockChg * 100) / 100;
  st["currency"] = stockCur;
  st["err"] = stockErr;
  st["ageS"] = stockLastOk ? (long)((millis() - stockLastOk) / 1000) : -1L;
  JsonObject wx = d["weather"].to<JsonObject>();
  wx["city"] = S.city;
  if (!isnan(wxTemp)) wx["temp"] = wxTemp;
  if (!isnan(wxHum)) wx["hum"] = wxHum;
  wx["code"] = wxCode;
  wx["desc"] = weatherWord(wxCode);
  wx["isDay"] = wxIsDay;
  wx["err"] = wxErr;
  wx["ageS"] = wxLastOk ? (long)((millis() - wxLastOk) / 1000) : -1L;
  d["ytErr"] = ytErr;
  xSemaphoreGive(setMux);
  d["recording"] = recActive;
  d["app"] = recActive ? "recording" : APP_KEYS[curApp];
  d["paused"] = paused;
  sendJson(d);
}

void apiGetSettings() {
  JsonDocument d;
  xSemaphoreTake(setMux, portMAX_DELAY);
  d["tz"] = S.tz; d["bright"] = S.bright; d["h24"] = S.h24; d["secs"] = S.secs;
  d["dateFmt"] = S.dateFmt; d["weekMon"] = S.weekMon; d["appDur"] = S.appDur;
  JsonObject en = d["apps"].to<JsonObject>();
  for (int i = 0; i < APP_COUNT; i++) en[APP_KEYS[i]] = S.en[i];
  d["colClock"] = hexColor(S.colClock); d["colDate"] = hexColor(S.colDate);
  d["colYt"] = hexColor(S.colYt); d["colTemp"] = hexColor(S.colTemp);
  d["tempF"] = S.tempF; d["tempOffset"] = S.tempOffset;
  d["ytId"] = S.ytId; d["ytKeySet"] = S.ytKey.length() > 0; d["ytEvery"] = S.ytEvery;
  d["batEmpty"] = S.batEmpty; d["batFull"] = S.batFull;
  d["dashStyle"] = S.dashStyle; d["dashDur"] = S.dashDur; d["dashOrder"] = S.dashOrder; d["tickSpeed"] = S.tickSpeed; d["dashSecBar"] = S.dashSecBar;
  d["colDashClock"] = hexColor(S.colDashClock); d["colStock"] = hexColor(S.colStock);
  d["stockSym"] = S.stockSym; d["stockEvery"] = S.stockEvery;
  d["lat"] = S.lat; d["lon"] = S.lon; d["city"] = S.city; d["wxEvery"] = S.wxEvery;
  xSemaphoreGive(setMux);
  sendJson(d);
}

void apiPostSettings() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
  xSemaphoreTake(setMux, portMAX_DELAY);
  bool tzChanged = false, ytChanged = false;
  if (d["tz"].is<const char *>()) { S.tz = d["tz"].as<String>(); tzChanged = true; }
  if (d["bright"].is<int>()) S.bright = constrain(d["bright"].as<int>(), 1, 255);
  if (d["h24"].is<bool>()) S.h24 = d["h24"];
  if (d["secs"].is<bool>()) S.secs = d["secs"];
  if (d["dateFmt"].is<int>()) S.dateFmt = d["dateFmt"].as<int>() ? 1 : 0;
  if (d["weekMon"].is<bool>()) S.weekMon = d["weekMon"];
  if (d["appDur"].is<int>()) S.appDur = constrain(d["appDur"].as<int>(), 0, 3600);
  if (d["apps"].is<JsonObject>())
    for (int i = 0; i < APP_COUNT; i++)
      if (d["apps"][APP_KEYS[i]].is<bool>()) S.en[i] = d["apps"][APP_KEYS[i]];
  S.colClock = parseColor(d["colClock"], S.colClock);
  S.colDate = parseColor(d["colDate"], S.colDate);
  S.colYt = parseColor(d["colYt"], S.colYt);
  S.colTemp = parseColor(d["colTemp"], S.colTemp);
  if (d["tempF"].is<bool>()) S.tempF = d["tempF"];
  if (d["tempOffset"].is<float>()) S.tempOffset = d["tempOffset"];
  if (d["ytId"].is<const char *>()) { S.ytId = d["ytId"].as<String>(); S.ytId.trim(); ytChanged = true; }
  if (d["ytKey"].is<const char *>() && strlen(d["ytKey"]) > 0) { S.ytKey = d["ytKey"].as<String>(); S.ytKey.trim(); ytChanged = true; }
  if (d["ytEvery"].is<int>()) S.ytEvery = constrain(d["ytEvery"].as<int>(), 60, 86400);
  if (d["batEmpty"].is<int>()) S.batEmpty = d["batEmpty"];
  if (d["batFull"].is<int>()) S.batFull = d["batFull"];
  bool stockChanged = false, wxChanged = false;
  if (d["dashStyle"].is<int>()) S.dashStyle = d["dashStyle"].as<int>() ? 1 : 0;
  if (d["dashDur"].is<int>()) S.dashDur = constrain(d["dashDur"].as<int>(), 0, 3600);
  if (d["dashOrder"].is<const char *>()) S.dashOrder = d["dashOrder"].as<String>();
  if (d["tickSpeed"].is<int>()) S.tickSpeed = constrain(d["tickSpeed"].as<int>(), 5, 60);
  if (d["dashSecBar"].is<bool>()) S.dashSecBar = d["dashSecBar"];
  S.colDashClock = parseColor(d["colDashClock"], S.colDashClock);
  S.colStock = parseColor(d["colStock"], S.colStock);
  if (d["stockSym"].is<const char *>()) {
    String v = d["stockSym"].as<String>(); v.trim(); v.toUpperCase(); v.replace("\"", "");
    if (v != S.stockSym) { S.stockSym = v; stockChanged = true; }
  }
  if (d["stockEvery"].is<int>()) S.stockEvery = constrain(d["stockEvery"].as<int>(), 30, 3600);
  if (d["lat"].is<float>()) { S.lat = d["lat"]; wxChanged = true; }
  if (d["lon"].is<float>()) { S.lon = d["lon"]; wxChanged = true; }
  if (d["city"].is<const char *>()) S.city = d["city"].as<String>();
  if (d["wxEvery"].is<int>()) S.wxEvery = constrain(d["wxEvery"].as<int>(), 300, 86400);
  if (d["tempF"].is<bool>()) wxChanged = true;
  saveSettings();
  xSemaphoreGive(setMux);
  if (tzChanged) { setenv("TZ", S.tz.c_str(), 1); tzset(); }
  if (ytChanged) ytForce = true;
  if (stockChanged) { stockPrice = NAN; stockChg = NAN; stockForce = true; }
  if (wxChanged) wxForce = true;
  if (!S.en[curApp]) nextApp(+1);
  server.send(200, "application/json", "{\"ok\":true}");
}

void apiNotify() {
  String text, icon, color = "#FFFFFF";
  int dur = 8;
  bool bp = false;
  if (server.hasArg("plain") && server.arg("plain").length()) {
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
    text = d["text"] | "";
    icon = d["icon"] | "";
    color = d["color"] | "#FFFFFF";
    dur = d["duration"] | 8;
    bp = d["beep"] | false;
  } else {
    text = server.arg("text");
    icon = server.arg("icon");
    if (server.hasArg("color")) color = server.arg("color");
    if (server.hasArg("duration")) dur = server.arg("duration").toInt();
    bp = server.arg("beep") == "1" || server.arg("beep") == "true";
  }
  if (!text.length()) { server.send(400, "text/plain", "text required"); return; }
  notify(text, rgbOf(parseColor(color.c_str(), 0xFFFFFF)), icon, dur, bp);
  server.send(200, "application/json", "{\"ok\":true}");
}

void apiScreen() {
  String out;
  out.reserve(MW * MH * 6 + 2);
  char b[7];
  for (int y = 0; y < MH; y++)
    for (int x = 0; x < MW; x++) {
      CRGB c = leds[XY(x, y)];
      snprintf(b, sizeof b, "%02x%02x%02x", c.r, c.g, c.b);
      out += b;
    }
  server.send(200, "text/plain", out);
}

void setupWeb() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store, must-revalidate");
    server.send_P(200, "text/html", WEB_UI);
  });
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/settings", HTTP_GET, apiGetSettings);
  server.on("/api/settings", HTTP_POST, apiPostSettings);
  server.on("/api/notify", HTTP_ANY, apiNotify);
  server.on("/api/screen", HTTP_GET, apiScreen);
  server.on("/api/next", HTTP_POST, [] { nextApp(+1); server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/prev", HTTP_POST, [] { nextApp(-1); server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/pause", HTTP_POST, [] { paused = !paused; lastSwitch = millis(); server.send(200, "application/json", paused ? "{\"paused\":true}" : "{\"paused\":false}"); });
  server.on("/api/show", HTTP_POST, [] {
    String a = server.arg("app");
    for (int i = 0; i < APP_COUNT; i++)
      if (a == APP_KEYS[i]) { curApp = i; lastSwitch = millis(); N.active = false; if (server.arg("pin") == "1") paused = true; }
    server.send(200, "application/json", "{\"ok\":true}");
  });
  // --- drawing ---
  server.on("/api/draw", HTTP_GET, [] { server.send(200, "text/plain", framesToHex(drawBuf)); });
  server.on("/api/draw", HTTP_POST, [] {  // body: 1536 hex chars (rrggbb x 256, row-major)
    if (!hexToFrame(server.arg("plain"), drawBuf)) { server.send(400, "text/plain", "need 1536 hex chars"); return; }
    if (server.arg("show") == "1") { curApp = APP_DRAW; paused = true; N.active = false; lastSwitch = millis(); }
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/draw/save", HTTP_POST, [] {
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= DRAW_SLOTS) { server.send(400, "text/plain", "bad slot"); return; }
    saveDrawingSlot(slot);
    S.drawSlot = slot; saveSettings();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/draw/load", HTTP_POST, [] {
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= DRAW_SLOTS) { server.send(400, "text/plain", "bad slot"); return; }
    loadDrawingSlot(slot, drawBuf);
    S.drawSlot = slot; saveSettings();
    server.send(200, "text/plain", framesToHex(drawBuf));
  });
  server.on("/api/draw/slots", HTTP_GET, [] {  // JSON array: hex frame or null per slot
    static CRGB tmp[MW * MH];
    String out = "{\"active\":" + String(S.drawSlot) + ",\"slots\":[";
    for (int i = 0; i < DRAW_SLOTS; i++) {
      if (i) out += ",";
      out += loadDrawingSlot(i, tmp) ? "\"" + framesToHex(tmp) + "\"" : String("null");
    }
    out += "]}";
    server.send(200, "application/json", out);
  });
  server.on("/api/dashboard/refresh", HTTP_POST, [] { stockForce = wxForce = ytForce = true; server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/youtube/refresh", HTTP_POST, [] { ytForce = true; server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/beep", HTTP_POST, [] { beep(); server.send(200, "application/json", "{\"ok\":true}"); });
  server.on("/api/reboot", HTTP_POST, [] { server.send(200, "application/json", "{\"ok\":true}"); delay(300); ESP.restart(); });
  server.on("/update", HTTP_POST,
    [] {
      bool ok = !Update.hasError();
      server.send(ok ? 200 : 500, "text/plain", ok ? "OK - rebooting" : "Update failed");
      delay(500);
      if (ok) ESP.restart();
    },
    [] {
      HTTPUpload &up = server.upload();
      if (up.status == UPLOAD_FILE_START) { showOverlay("OTA", 60000); Update.begin(UPDATE_SIZE_UNKNOWN); }
      else if (up.status == UPLOAD_FILE_WRITE) Update.write(up.buf, up.currentSize);
      else if (up.status == UPLOAD_FILE_END) Update.end(true);
    });
  homeSetupRoutes(server);
  server.on("/api/rec", HTTP_GET, [] {
    JsonDocument d;
    d["active"] = recActive; d["order"] = REC.order; d["secs"] = REC.secs; d["bright"] = REC.bright;
    d["mute"] = REC.mute; d["logoSlot"] = REC.logoSlot;
    std::vector<String> sl = recSlides();
    d["slide"] = recActive && recIdx < (int)sl.size() ? sl[recIdx] : "";
    String out; serializeJson(d, out);
    server.send(200, "application/json", out);
  });
  server.on("/api/rec", HTTP_POST, [] {
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
    if (d["order"].is<const char *>()) REC.order = d["order"].as<String>();
    if (d["secs"].is<int>()) REC.secs = constrain(d["secs"].as<int>(), 1, 60);
    if (d["bright"].is<int>()) REC.bright = constrain(d["bright"].as<int>(), 5, 255);
    if (d["mute"].is<bool>()) REC.mute = d["mute"];
    if (d["logoSlot"].is<int>()) { REC.logoSlot = constrain(d["logoSlot"].as<int>(), 0, 5); if (recActive) loadDrawingSlot(REC.logoSlot, recLogo); }
    saveRec();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/rec/start", HTTP_POST, [] { recStart(); server.send(200, "application/json", "{\"active\":true}"); });
  server.on("/api/rec/stop", HTTP_POST, [] { recStop(); server.send(200, "application/json", "{\"active\":false}"); });
  server.on("/api/rec/next", HTTP_POST, [] { if (recActive) recStep(+1); server.send(200, "application/json", "{\"ok\":true}"); });
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
}

// ---------- Setup / loop ----------
void showNow(const String &s, CRGB c = CRGB(0, 200, 255)) {
  FastLED.clear();
  drawCentered(s, c);
  FastLED.show();
}

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.begin(115200);
  Serial.printf("\nTC001 custom fw %s\n", FW_VERSION);

  setMux = xSemaphoreCreateMutex();
  loadSettings();
  loadDrawingSlot(S.drawSlot, drawBuf);
  homeLoad();
  loadRec();

  FastLED.addLeds<WS2812B, PIN_MATRIX, GRB>(leds, MW * MH);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
  FastLED.setBrightness(S.bright);
  showNow("HELLO");

  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_MID, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  setenv("TZ", S.tz.c_str(), 1);
  tzset();
  Serial.printf("RTC: %s\n", rtcRead() ? "time loaded" : "not set");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("tc001");
  WiFiManager wm;
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback([](WiFiManager *) { showNow("WIFI?", CRGB(255, 160, 0)); });
  showNow("WIFI");
  bool ok = wm.autoConnect("TC001-Setup");
  Serial.printf("WiFi: %s %s\n", ok ? "connected" : "offline", WiFi.localIP().toString().c_str());

  sntp_set_time_sync_notification_cb([](struct timeval *) { ntpSynced = true; });
  configTzTime(S.tz.c_str(), "pool.ntp.org", "time.google.com");

  if (ok) {
    MDNS.begin("tc001");
    MDNS.addService("http", "tcp", 80);
    setupWeb();
    notify(WiFi.localIP().toString(), CRGB(0, 200, 255), "", 4, false);
  }

  xTaskCreatePinnedToCore(dataTask, "data", 12288, nullptr, 1, nullptr, 0);
  if (!S.en[curApp]) nextApp(+1);
  lastSwitch = millis();
}

uint32_t lastSensor = 0, lastFrame = 0;

void loop() {
  server.handleClient();
  handleButtons();

  if (homePend) {
    xSemaphoreTake(setMux, portMAX_DELAY);
    String txt = homePendText; bool bp = homePendBeep; homePend = false;
    xSemaphoreGive(setMux);
    notify(txt, CRGB(0, 200, 255), "bell", 10, bp);
  }
  if (ntpSynced) { ntpSynced = false; rtcWrite(); Serial.println("NTP synced -> RTC updated"); }

  if (millis() - lastSensor > 5000 || lastSensor == 0) {
    lastSensor = millis();
    float t, h;
    if (readSHT(t, h)) { tempC = t; humidity = h; }
    ldrRaw = analogRead(PIN_LDR);
    batRaw = analogRead(PIN_BAT);
  }

  uint16_t dur = curApp == APP_DASH ? S.dashDur : S.appDur;
  if (!recActive && S.appDur > 0 && dur > 0 && !paused && !N.active && millis() - lastSwitch > (uint32_t)dur * 1000UL) nextApp(+1);

  if (millis() - lastFrame >= 40) { lastFrame = millis(); render(); }
  delay(2);
}
