#pragma once
// Smart-home integration: Tuya cloud client (IR hubs, room sensors, AC state), rule engine, home screen.
// Included from main.cpp after notify()/settings/weather globals are defined.
#include <mbedtls/md.h>

#define HOME_HUBS 2
#define HOME_LOG 30

// ---------- State ----------
struct HomeHub { String id, label; float temp = NAN, hum = NAN; uint32_t ts = 0; String err; };
struct HomeRemote { String id, name, hub; int cat = 0, brand = 0; int power = -1, mode = 0, temp = 24, wind = 0; };

struct HomeCfg {
  String cid, secret, region = "eu";
  uint16_t every = 300;  // poll seconds
} HC;
HomeHub hubs[HOME_HUBS];
std::vector<HomeRemote> remotes;
JsonDocument rulesDoc;  // array of rule objects (see README)
std::vector<int8_t> ruleState;   // -1 unknown, 0 false, 1 true
std::vector<uint32_t> ruleFired; // millis of last fire, 0 = never
String homeLog[HOME_LOG];
int homeLogN = 0, homeLogHead = 0;
String homeLastErr;
volatile bool homePollForce = true;
SemaphoreHandle_t tuyaMux;  // serialises cloud calls + token
std::vector<String> actionQueue;  // JSON actions waiting for the data task
String homePendText; bool homePendBeep = false; volatile bool homePend = false;

void homeLogAdd(const String &m) {
  String line;
  if (timeValid()) {
    time_t now = time(nullptr); struct tm t; localtime_r(&now, &t);
    char b[8]; snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min); line = b;
  } else line = "--:--";
  line += " " + m;
  xSemaphoreTake(setMux, portMAX_DELAY);
  homeLog[homeLogHead] = line;
  homeLogHead = (homeLogHead + 1) % HOME_LOG;
  if (homeLogN < HOME_LOG) homeLogN++;
  xSemaphoreGive(setMux);
  Serial.println("[home] " + line);
}

// ---------- Persistence ----------
void homeSaveRemotes() {
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (auto &r : remotes) {
    JsonObject o = a.add<JsonObject>();
    o["id"] = r.id; o["name"] = r.name; o["hub"] = r.hub; o["cat"] = r.cat; o["brand"] = r.brand;
  }
  String s; serializeJson(d, s);
  Preferences p; p.begin("home", false); p.putString("remotes", s); p.end();
}

void homeSaveCfg() {
  Preferences p; p.begin("home", false);
  p.putString("cid", HC.cid); p.putString("secret", HC.secret); p.putString("region", HC.region);
  p.putUShort("every", HC.every);
  for (int i = 0; i < HOME_HUBS; i++) { p.putString(("hub" + String(i)).c_str(), hubs[i].id); p.putString(("lab" + String(i)).c_str(), hubs[i].label); }
  p.end();
}

void homeResetRuleState() {
  size_t n = rulesDoc.as<JsonArray>().size();
  ruleState.assign(n, -1);
  ruleFired.assign(n, 0);
}

bool homeSaveRules(const String &json) {
  JsonDocument d;
  if (deserializeJson(d, json) || !d.is<JsonArray>()) return false;
  String s; serializeJson(d, s);
  Preferences p; p.begin("home", false);
  bool ok = p.putBytes("rules", s.c_str(), s.length()) == s.length();
  p.end();
  if (!ok) return false;
  xSemaphoreTake(setMux, portMAX_DELAY);
  rulesDoc = d;
  homeResetRuleState();
  xSemaphoreGive(setMux);
  return true;
}

const char *DEFAULT_RULES = R"JSON([
 {"name":"Hot afternoon","en":false,"cooldown":120,"from":"12:00","to":"23:00","days":127,
  "cond":[{"src":"outTemp","op":">=","val":32},{"src":"temp0","op":">=","val":27}],
  "act":[{"type":"ac","remote":"","power":1,"mode":0,"temp":24,"wind":0},
         {"type":"notify","text":"HOT - LIVING AC ON 24","beep":false}]},
 {"name":"Cooled down","en":false,"cooldown":60,"from":"","to":"","days":127,
  "cond":[{"src":"temp0","op":"<=","val":23},{"src":"ac:","op":"==","val":1}],
  "act":[{"type":"ac","remote":"","power":0,"mode":0,"temp":24,"wind":0}]},
 {"name":"Rain alert","en":false,"cooldown":180,"from":"","to":"","days":127,
  "cond":[{"src":"rain","op":"==","val":1}],
  "act":[{"type":"notify","text":"RAIN - CLOSE THE WINDOWS","beep":true}]}
])JSON";

void homeLoad() {
  tuyaMux = xSemaphoreCreateMutex();
  Preferences p; p.begin("home", true);
  HC.cid = p.getString("cid", "");
  HC.secret = p.getString("secret", "");
  HC.region = p.getString("region", "eu");
  HC.every = p.getUShort("every", 300);
  for (int i = 0; i < HOME_HUBS; i++) {
    hubs[i].id = p.getString(("hub" + String(i)).c_str(), "");
    hubs[i].label = p.getString(("lab" + String(i)).c_str(), i == 0 ? "L" : "B");
  }
  String rs = p.getString("remotes", "[]");
  size_t rl = p.getBytesLength("rules");
  String rules;
  if (rl > 0) { std::vector<char> buf(rl + 1, 0); p.getBytes("rules", buf.data(), rl); rules = buf.data(); }
  p.end();
  JsonDocument d;
  if (!deserializeJson(d, rs))
    for (JsonObject o : d.as<JsonArray>()) {
      HomeRemote r; r.id = o["id"].as<String>(); r.name = o["name"].as<String>(); r.hub = o["hub"].as<String>();
      r.cat = o["cat"] | 0; r.brand = o["brand"] | 0; remotes.push_back(r);
    }
  if (!rules.length() || deserializeJson(rulesDoc, rules) || !rulesDoc.is<JsonArray>()) deserializeJson(rulesDoc, DEFAULT_RULES);
  homeResetRuleState();
}

HomeRemote *remoteById(const String &id) {
  for (auto &r : remotes) if (r.id == id) return &r;
  return nullptr;
}
int hubIndex(const String &id) {
  for (int i = 0; i < HOME_HUBS; i++) if (hubs[i].id == id) return i;
  return -1;
}

// ---------- Tuya OpenAPI client (HMAC-SHA256 signature) ----------
String tuyaToken;
uint32_t tuyaTokenExp = 0;

String tuyaHost() {
  if (HC.region == "us") return "openapi.tuyaus.com";
  if (HC.region == "us-e") return "openapi-ueaz.tuyaus.com";
  if (HC.region == "eu-w") return "openapi-weaz.tuyaeu.com";
  if (HC.region == "cn") return "openapi.tuyacn.com";
  if (HC.region == "in") return "openapi.tuyain.com";
  return "openapi.tuyaeu.com";
}

static String toHex(const uint8_t *b, size_t n, bool upper) {
  static const char *lo = "0123456789abcdef", *up = "0123456789ABCDEF";
  const char *h = upper ? up : lo;
  String s; s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { s += h[b[i] >> 4]; s += h[b[i] & 15]; }
  return s;
}
static String sha256Hex(const String &data) {
  uint8_t out[32];
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t *)data.c_str(), data.length(), out);
  return toHex(out, 32, false);
}
static String hmacUpper(const String &key, const String &msg) {
  uint8_t out[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t *)key.c_str(), key.length(),
                  (const uint8_t *)msg.c_str(), msg.length(), out);
  return toHex(out, 32, true);
}
static String nowMs() {
  struct timeval tv; gettimeofday(&tv, nullptr);
  char b[20]; snprintf(b, sizeof b, "%lld", (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000);
  return b;
}

// raw signed request; returns HTTP code, body in out
static int tuyaRaw(const char *method, const String &path, const String &body, const String &tok, String &out) {
  String t = nowMs();
  String sts = String(method) + "\n" + sha256Hex(body) + "\n\n" + path;
  String sign = hmacUpper(HC.secret, HC.cid + tok + t + sts);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, "https://" + tuyaHost() + path)) return -1;
  http.addHeader("client_id", HC.cid);
  http.addHeader("sign", sign);
  http.addHeader("t", t);
  http.addHeader("sign_method", "HMAC-SHA256");
  if (tok.length()) http.addHeader("access_token", tok);
  if (body.length()) http.addHeader("Content-Type", "application/json");
  int code = http.sendRequest(method, body);
  out = code > 0 ? http.getString() : "";
  http.end();
  return code;
}

bool tuyaEnsureToken(String &err) {
  if (tuyaToken.length() && (int32_t)(tuyaTokenExp - millis()) > 60000) return true;
  String out;
  int code = tuyaRaw("GET", "/v1.0/token?grant_type=1", "", "", out);
  JsonDocument d;
  if (code != 200 || deserializeJson(d, out)) { err = "token http " + String(code); return false; }
  if (!(d["success"] | false)) { err = "token: " + d["msg"].as<String>(); return false; }
  tuyaToken = d["result"]["access_token"].as<String>();
  tuyaTokenExp = millis() + (uint32_t)(d["result"]["expire_time"] | 3600) * 1000UL;
  return true;
}

// Signed API call with token; result JSON in doc. Returns true if Tuya says success.
bool tuyaCall(const char *method, const String &path, const String &body, JsonDocument &doc, String &err) {
  if (!HC.cid.length() || !HC.secret.length()) { err = "Tuya not configured"; return false; }
  if (!timeValid()) { err = "clock time not set"; return false; }
  if (WiFi.status() != WL_CONNECTED) { err = "no Wi-Fi"; return false; }
  xSemaphoreTake(tuyaMux, portMAX_DELAY);
  bool ok = false;
  for (int attempt = 0; attempt < 2 && !ok; attempt++) {
    if (!tuyaEnsureToken(err)) break;
    String out;
    int code = tuyaRaw(method, path, body, tuyaToken, out);
    doc.clear();
    if (code != 200 || deserializeJson(doc, out)) { err = "http " + String(code); break; }
    if (doc["success"] | false) { ok = true; err = ""; break; }
    int c = doc["code"] | 0;
    err = String(c) + " " + doc["msg"].as<String>();
    if (c == 1010 || c == 1011) { tuyaToken = ""; continue; }  // token invalid/expired -> retry once
    break;
  }
  xSemaphoreGive(tuyaMux);
  return ok;
}

// ---------- Polling ----------
void homePollHubs() {
  for (int i = 0; i < HOME_HUBS; i++) {
    if (!hubs[i].id.length()) continue;
    JsonDocument d; String err;
    if (tuyaCall("GET", "/v1.0/devices/" + hubs[i].id + "/status", "", d, err)) {
      float t = NAN, h = NAN;
      for (JsonObject o : d["result"].as<JsonArray>()) {
        String c = o["code"].as<String>();
        if (c == "va_temperature") t = (o["value"] | 0) / 10.0f;
        if (c == "va_humidity") h = o["value"] | 0;
      }
      xSemaphoreTake(setMux, portMAX_DELAY);
      hubs[i].temp = t; hubs[i].hum = h; hubs[i].ts = millis(); hubs[i].err = "";
      xSemaphoreGive(setMux);
    } else { setErr(hubs[i].err, err); homeLastErr = err; }
  }
  for (auto &r : remotes) {
    if (r.cat != 5) continue;  // air conditioners only
    JsonDocument d; String err;
    if (tuyaCall("GET", "/v2.0/infrareds/" + r.hub + "/remotes/" + r.id + "/ac/status", "", d, err)) {
      JsonObject s = d["result"];
      r.power = atoi(s["power"] | "0"); r.mode = atoi(s["mode"] | "0");
      r.temp = atoi(s["temp"] | "24"); r.wind = atoi(s["wind"] | "0");
    }
  }
}

bool homeDiscover(String &err) {
  std::vector<HomeRemote> found;
  for (int i = 0; i < HOME_HUBS; i++) {
    if (!hubs[i].id.length()) continue;
    JsonDocument d;
    if (!tuyaCall("GET", "/v2.0/infrareds/" + hubs[i].id + "/remotes", "", d, err)) return false;
    for (JsonObject o : d["result"].as<JsonArray>()) {
      HomeRemote r; r.id = o["remote_id"].as<String>(); r.name = o["remote_name"].as<String>(); r.hub = hubs[i].id;
      r.cat = o["category_id"] | 0; r.brand = o["brand_id"] | 0;
      HomeRemote *old = remoteById(r.id);
      if (old) { r.power = old->power; r.mode = old->mode; r.temp = old->temp; r.wind = old->wind; }
      found.push_back(r);
    }
  }
  remotes = found;
  homeSaveRemotes();
  homeLogAdd("found " + String(remotes.size()) + " remotes");
  return true;
}

// ---------- Actions ----------
static const char *MODE_NAMES[] = {"COOL", "HEAT", "AUTO", "FAN", "DRY"};

bool homeRunAction(JsonObjectConst a, String &desc) {
  String type = a["type"] | "";
  String err;
  if (type == "notify") {
    xSemaphoreTake(setMux, portMAX_DELAY);
    homePendText = a["text"] | "!"; homePendBeep = a["beep"] | false; homePend = true;
    xSemaphoreGive(setMux);
    desc = "notify \"" + String((const char *)(a["text"] | "")) + "\"";
    return true;
  }
  String rid = a["remote"] | "";
  if (!rid.length() && type == "ac")  // empty remote in an AC action = first air conditioner found
    for (auto &x : remotes) if (x.cat == 5) { rid = x.id; break; }
  HomeRemote *r = remoteById(rid);
  if (!r) { desc = "unknown remote " + String((const char *)(a["remote"] | "")) + " (press Discover remotes)"; return false; }
  JsonDocument d;
  if (type == "ac") {
    int pw = a["power"] | 1, md = a["mode"] | 0, tp = a["temp"] | 24, wd = a["wind"] | 0;
    String body = "{\"power\":" + String(pw) + ",\"mode\":" + String(md) + ",\"temp\":" + String(tp) + ",\"wind\":" + String(wd) + "}";
    bool ok = tuyaCall("POST", "/v2.0/infrareds/" + r->hub + "/air-conditioners/" + r->id + "/scenes/command", body, d, err);
    desc = r->name + (pw ? " ON " + String(MODE_NAMES[constrain(md, 0, 4)]) + " " + String(tp) : String(" OFF"));
    if (ok) { r->power = pw; r->mode = md; r->temp = tp; r->wind = wd; }
    else desc += " FAILED: " + err;
    return ok;
  }
  if (type == "key") {
    String key = a["key"] | "";
    String body = "{\"category_id\":" + String(a["cat"] | r->cat) + ",\"key_id\":" + String(a["key_id"] | 0) + ",\"key\":\"" + key + "\"}";
    bool ok = tuyaCall("POST", "/v2.0/infrareds/" + r->hub + "/remotes/" + r->id + "/raw/command", body, d, err);
    desc = r->name + " [" + key + "]" + (ok ? "" : " FAILED: " + err);
    return ok;
  }
  desc = "unknown action type";
  return false;
}

void homeQueueAction(const String &json) {
  xSemaphoreTake(setMux, portMAX_DELAY);
  if (actionQueue.size() < 20) actionQueue.push_back(json);
  xSemaphoreGive(setMux);
}

// ---------- Rules ----------
static int hmToMin(const char *s) {
  if (!s || !*s) return -1;
  int h = 0, m = 0;
  if (sscanf(s, "%d:%d", &h, &m) != 2) return -1;
  return h * 60 + m;
}

// returns 1 true, 0 false, -1 unknown (sensor data missing)
int homeCondValue(const String &src, float &v) {
  if (src == "outTemp") { if (isnan(wxTemp)) return -1; v = wxTemp; return 1; }
  if (src == "outHum") { if (isnan(wxHum)) return -1; v = wxHum; return 1; }
  if (src == "rain") { if (wxCode < 0) return -1; v = ((wxCode >= 51 && wxCode <= 67) || (wxCode >= 80 && wxCode <= 82) || wxCode >= 95) ? 1 : 0; return 1; }
  if (src == "roomTempClock") { if (isnan(tempC)) return -1; v = tempC + S.tempOffset; return 1; }
  for (int i = 0; i < HOME_HUBS; i++) {
    if (src == "temp" + String(i)) { if (isnan(hubs[i].temp)) return -1; v = hubs[i].temp; return 1; }
    if (src == "hum" + String(i)) { if (isnan(hubs[i].hum)) return -1; v = hubs[i].hum; return 1; }
  }
  if (src.startsWith("ac:")) {
    String id = src.substring(3);
    if (!id.length()) for (auto &x : remotes) if (x.cat == 5) { id = x.id; break; }  // "ac:" = first AC
    HomeRemote *r = remoteById(id); if (!r || r->power < 0) return -1; v = r->power; return 1; }
  return -1;
}

int homeEvalRule(JsonObjectConst rule) {
  if (!timeValid()) return -1;
  time_t now = time(nullptr); struct tm t; localtime_r(&now, &t);
  int days = rule["days"] | 127;
  if (!(days & (1 << t.tm_wday))) return 0;
  int from = hmToMin(rule["from"] | ""), to = hmToMin(rule["to"] | "");
  int cur = t.tm_hour * 60 + t.tm_min;
  if (from >= 0 && to >= 0) {
    bool in = from <= to ? (cur >= from && cur < to) : (cur >= from || cur < to);  // window may wrap midnight
    if (!in) return 0;
  }
  bool any = false;
  for (JsonObjectConst c : rule["cond"].as<JsonArrayConst>()) {
    float v;
    int k = homeCondValue(c["src"] | "", v);
    if (k < 0) return -1;
    float target = c["val"] | 0.0f;
    String op = c["op"] | ">=";
    bool ok = op == ">=" ? v >= target : op == "<=" ? v <= target : fabsf(v - target) < 0.01f;
    if (!ok) return 0;
    any = true;
  }
  return any ? 1 : 0;
}

void homeFireRule(int i, JsonObjectConst rule, const char *why) {
  homeLogAdd(String(why) + " \"" + String((const char *)(rule["name"] | "rule")) + "\"");
  for (JsonObjectConst a : rule["act"].as<JsonArrayConst>()) {
    String s; serializeJson(a, s);
    homeQueueAction(s);
  }
  if (i >= 0 && i < (int)ruleFired.size()) ruleFired[i] = millis() | 1;
}

void homeEvalRules() {
  xSemaphoreTake(setMux, portMAX_DELAY);
  JsonDocument copy = rulesDoc;  // evaluate on a snapshot
  xSemaphoreGive(setMux);
  JsonArrayConst arr = copy.as<JsonArrayConst>();
  if (ruleState.size() != arr.size()) homeResetRuleState();
  int i = 0;
  for (JsonObjectConst rule : arr) {
    int st = homeEvalRule(rule);
    if (!(rule["en"] | false)) { ruleState[i] = st; i++; continue; }
    if (st >= 0) {
      bool rising = st == 1 && ruleState[i] == 0;  // only fire on false -> true (never on first evaluation after boot)
      uint32_t cd = (uint32_t)(rule["cooldown"] | 30) * 60000UL;
      bool cooled = ruleFired[i] == 0 || millis() - ruleFired[i] > cd;
      if (rising && cooled) homeFireRule(i, rule, "rule");
      ruleState[i] = st;
    }
    i++;
  }
}

// called from dataTask (core 0) every ~0.5 s
void homeTick() {
  static uint32_t lastPoll = 0, lastEval = 0;
  // run queued actions
  for (;;) {
    xSemaphoreTake(setMux, portMAX_DELAY);
    if (actionQueue.empty()) { xSemaphoreGive(setMux); break; }
    String js = actionQueue.front(); actionQueue.erase(actionQueue.begin());
    xSemaphoreGive(setMux);
    JsonDocument d; deserializeJson(d, js);
    String desc;
    bool ok = homeRunAction(d.as<JsonObjectConst>(), desc);
    homeLogAdd((ok ? "OK " : "ERR ") + desc);
  }
  if (!HC.cid.length() || !HC.secret.length()) return;
  static uint32_t lastDiscover = 0;
  if (remotes.empty() && (lastDiscover == 0 || millis() - lastDiscover > 600000UL)) {  // auto-discover, retry every 10 min
    lastDiscover = millis() | 1;
    String err;
    if (!homeDiscover(err)) homeLogAdd("ERR discover: " + err);
  }
  if (homePollForce || millis() - lastPoll > (uint32_t)HC.every * 1000UL) {
    homePollForce = false; lastPoll = millis();
    homePollHubs();
  }
  if (millis() - lastEval > 30000) { lastEval = millis(); homeEvalRules(); }
}

// ---------- Home screen: one line per room: label · temp · humidity · AC bar ----------
CRGB acColor(const HomeRemote *r) {
  if (!r || r->power < 0) return CRGB::Black;
  if (r->power == 0) return CRGB(30, 30, 36);
  switch (r->mode) {
    case 0: return CRGB(0, 120, 255);   // cool
    case 1: return CRGB(255, 110, 0);   // heat
    case 2: return CRGB(0, 200, 80);    // auto
    case 3: return CRGB(220, 220, 220); // fan
    default: return CRGB(0, 200, 200);  // dry
  }
}

void renderHome() {
  for (int i = 0; i < HOME_HUBS; i++) {
    int y = i * 4;
    HomeHub &h = hubs[i];
    String lab = h.label.length() ? h.label.substring(0, 1) : String(i == 0 ? "L" : "B");
    lab.toUpperCase();
    drawText4(0, y, lab, CRGB(90, 110, 140));
    String t = isnan(h.temp) ? String("--") : String((int)lroundf(h.temp)) + "`";
    drawText4(4, y, t, CRGB::White);
    String hm = isnan(h.hum) ? String("--") : String((int)lroundf(h.hum)) + "%";
    drawText4(27 - text4Width(hm), y, hm, CRGB(120, 190, 255));
    const HomeRemote *ac = nullptr;
    for (auto &r : remotes) if (r.cat == 5 && r.hub == h.id) { ac = &r; break; }
    CRGB c = acColor(ac);
    for (int yy = y; yy < y + 3; yy++)
      for (int xx = 29; xx < 32; xx++) px(xx, yy, c);
  }
}

// ---------- Web API ----------
void homeSetupRoutes(WebServer &srv) {
  srv.on("/api/home", HTTP_GET, [&srv] {
    JsonDocument d;
    d["idSet"] = HC.cid.length() > 0;
    d["secretSet"] = HC.secret.length() > 0;
    d["region"] = HC.region;
    d["every"] = HC.every;
    d["lastErr"] = homeLastErr;
    xSemaphoreTake(setMux, portMAX_DELAY);
    JsonArray hs = d["hubs"].to<JsonArray>();
    for (int i = 0; i < HOME_HUBS; i++) {
      JsonObject o = hs.add<JsonObject>();
      o["id"] = hubs[i].id; o["label"] = hubs[i].label; o["err"] = hubs[i].err;
      if (!isnan(hubs[i].temp)) o["temp"] = hubs[i].temp;
      if (!isnan(hubs[i].hum)) o["hum"] = hubs[i].hum;
      o["ageS"] = hubs[i].ts ? (long)((millis() - hubs[i].ts) / 1000) : -1L;
    }
    JsonArray rs = d["remotes"].to<JsonArray>();
    for (auto &r : remotes) {
      JsonObject o = rs.add<JsonObject>();
      o["id"] = r.id; o["name"] = r.name; o["hub"] = r.hub; o["cat"] = r.cat;
      if (r.cat == 5) { o["power"] = r.power; o["mode"] = r.mode; o["temp"] = r.temp; o["wind"] = r.wind; }
    }
    d["rules"] = rulesDoc;
    JsonArray st = d["ruleState"].to<JsonArray>();
    for (size_t i = 0; i < ruleState.size(); i++) st.add(ruleState[i]);
    JsonArray lg = d["log"].to<JsonArray>();
    for (int i = 0; i < homeLogN; i++) lg.add(homeLog[(homeLogHead - 1 - i + HOME_LOG) % HOME_LOG]);  // newest first
    xSemaphoreGive(setMux);
    if (!isnan(wxTemp)) d["outTemp"] = wxTemp;
    if (!isnan(wxHum)) d["outHum"] = wxHum;
    String out; serializeJson(d, out);
    srv.send(200, "application/json", out);
  });

  srv.on("/api/home/config", HTTP_POST, [&srv] {
    JsonDocument d;
    if (deserializeJson(d, srv.arg("plain"))) { srv.send(400, "text/plain", "bad json"); return; }
    bool credChanged = false;
    if (d["cid"].is<const char *>() && strlen(d["cid"]) > 0) { HC.cid = d["cid"].as<String>(); HC.cid.trim(); credChanged = true; }
    if (d["secret"].is<const char *>() && strlen(d["secret"]) > 0) { HC.secret = d["secret"].as<String>(); HC.secret.trim(); credChanged = true; }
    if (d["region"].is<const char *>()) { HC.region = d["region"].as<String>(); credChanged = true; }
    if (d["every"].is<int>()) HC.every = constrain(d["every"].as<int>(), 60, 3600);
    if (d["hubs"].is<JsonArray>()) {
      int i = 0;
      for (JsonObject h : d["hubs"].as<JsonArray>()) {
        if (i >= HOME_HUBS) break;
        if (h["id"].is<const char *>()) { hubs[i].id = h["id"].as<String>(); hubs[i].id.trim(); }
        if (h["label"].is<const char *>()) hubs[i].label = h["label"].as<String>().substring(0, 1);
        i++;
      }
    }
    homeSaveCfg();
    if (credChanged) { xSemaphoreTake(tuyaMux, portMAX_DELAY); tuyaToken = ""; xSemaphoreGive(tuyaMux); }
    homePollForce = true;
    srv.send(200, "application/json", "{\"ok\":true}");
  });

  srv.on("/api/home/discover", HTTP_POST, [&srv] {
    String err;
    if (homeDiscover(err)) { homePollForce = true; srv.send(200, "application/json", "{\"ok\":true,\"count\":" + String(remotes.size()) + "}"); }
    else srv.send(502, "application/json", "{\"ok\":false,\"err\":\"" + err + "\"}");
  });

  srv.on("/api/home/keys", HTTP_GET, [&srv] {
    HomeRemote *r = remoteById(srv.arg("remote"));
    if (!r) { srv.send(404, "text/plain", "unknown remote"); return; }
    JsonDocument d; String err;
    if (!tuyaCall("GET", "/v2.0/infrareds/" + r->hub + "/remotes/" + r->id + "/keys", "", d, err)) {
      srv.send(502, "application/json", "{\"ok\":false,\"err\":\"" + err + "\"}"); return;
    }
    JsonDocument o;
    o["cat"] = d["result"]["category_id"] | r->cat;
    JsonArray ks = o["keys"].to<JsonArray>();
    for (JsonObject k : d["result"]["key_list"].as<JsonArray>()) {
      JsonObject x = ks.add<JsonObject>();
      x["key"] = k["key"]; x["key_id"] = k["key_id"]; x["name"] = k["key_name"];
    }
    String out; serializeJson(o, out);
    srv.send(200, "application/json", out);
  });

  srv.on("/api/home/action", HTTP_POST, [&srv] {
    JsonDocument d;
    if (deserializeJson(d, srv.arg("plain")) || !d.is<JsonObject>()) { srv.send(400, "text/plain", "bad json"); return; }
    String s; serializeJson(d, s);
    homeQueueAction(s);
    srv.send(200, "application/json", "{\"ok\":true,\"queued\":true}");
  });

  srv.on("/api/home/rules", HTTP_POST, [&srv] {
    if (homeSaveRules(srv.arg("plain"))) srv.send(200, "application/json", "{\"ok\":true}");
    else srv.send(400, "text/plain", "rules must be a JSON array (max ~4 KB)");
  });

  srv.on("/api/home/rules/test", HTTP_POST, [&srv] {
    int i = srv.arg("i").toInt();
    xSemaphoreTake(setMux, portMAX_DELAY);
    JsonDocument copy = rulesDoc;
    xSemaphoreGive(setMux);
    JsonArrayConst arr = copy.as<JsonArrayConst>();
    if (i < 0 || i >= (int)arr.size()) { srv.send(400, "text/plain", "bad index"); return; }
    homeFireRule(-1, arr[i], "test");
    srv.send(200, "application/json", "{\"ok\":true}");
  });

  srv.on("/api/home/refresh", HTTP_POST, [&srv] { homePollForce = true; srv.send(200, "application/json", "{\"ok\":true}"); });
}
