#include "BaseCore.h"

#include <DNSServer.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <StreamString.h>
#include <Updater.h>

#include "BasePage.h"

namespace {

const char* ADMIN_USER = "admin";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);
const uint32_t SETTINGS_MAGIC = 0x45535036UL;

struct StoredSettings {
  uint32_t magic;
  char ssid[33];
  char pass[64];
  char host[33];
  char admin[65];
};

BaseConfig g_cfg;
ESP8266WebServer g_server(80);
DNSServer g_dns;
StoredSettings g_store = {};

String g_ssid, g_pass, g_host, g_admin;
String g_apSsid;
String g_macSuffix;

bool g_portal = false;
bool g_restartPending = false;
uint32_t g_restartAt = 0;

uint32_t g_lastRetry = 0;
bool g_retrying = false;
uint32_t g_linkLostAt = 0;
uint32_t g_btnDownAt = 0;

bool g_updateStarted = false;
bool g_updateAuthOk = false;
bool g_updateFirstChunk = true;
String g_updateError;

bool elapsed(uint32_t since, uint32_t ms) { return (uint32_t)(millis() - since) >= ms; }

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

String jsonStr(const String& s) { return "\"" + jsonEscape(s) + "\""; }

void sendJson(int code, const String& body) {
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(code, "application/json", body);
}

void sendResult(bool ok, const String& msg, bool restart = false) {
  sendJson(ok ? 200 : 400, String("{\"ok\":") + (ok ? "true" : "false") +
                               ",\"message\":" + jsonStr(msg) +
                               ",\"restart\":" + (restart ? "true" : "false") + "}");
}

bool validHostname(const String& h) {
  if (h.length() < 1 || h.length() > 32) return false;
  if (h[0] == '-' || h[h.length() - 1] == '-') return false;
  for (size_t i = 0; i < h.length(); i++) {
    char c = h[i];
    if (!(isdigit(c) || (c >= 'a' && c <= 'z') || c == '-')) return false;
  }
  return true;
}

template <size_t N>
void setField(char (&field)[N], const String& value) {
  memset(field, 0, sizeof(field));
  value.substring(0, N - 1).toCharArray(field, N);
}

template <size_t N>
void sanitizeField(char (&field)[N]) {
  field[N - 1] = '\0';
  if (memchr(field, '\0', N) == nullptr) memset(field, 0, sizeof(field));
}

void syncRuntimeFromStore() {
  sanitizeField(g_store.ssid);
  sanitizeField(g_store.pass);
  sanitizeField(g_store.host);
  sanitizeField(g_store.admin);
  g_ssid = g_store.ssid;
  g_pass = g_store.pass;
  g_host = g_store.host;
  g_admin = g_store.admin;
}

void syncStoreFromRuntime() {
  g_store.magic = SETTINGS_MAGIC;
  setField(g_store.ssid, g_ssid);
  setField(g_store.pass, g_pass);
  setField(g_store.host, g_host);
  setField(g_store.admin, g_admin);
}

bool commitSettings() {
  syncStoreFromRuntime();
  EEPROM.put(0, g_store);
  if (EEPROM.commit()) return true;
  Serial.println("[storage] EEPROM commit failed");
  return false;
}

String updaterError() {
  StreamString stream;
  Update.printError(stream);
  return stream.length() ? String(stream.c_str()) : String("Unknown error");
}

String flashModeName() {
  switch (ESP.getFlashChipMode()) {
    case FM_QIO: return "QIO";
    case FM_QOUT: return "QOUT";
    case FM_DIO: return "DIO";
    case FM_DOUT: return "DOUT";
    default: return "?";
  }
}

bool networkIsSecure(int index) { return WiFi.encryptionType(index) != ENC_TYPE_NONE; }

void loadSettings() {
  memset(&g_store, 0, sizeof(g_store));
  EEPROM.begin(sizeof(g_store));
  if (EEPROM.length() != sizeof(g_store)) {
    Serial.println("[storage] EEPROM init failed");
  } else {
    EEPROM.get(0, g_store);
    if (g_store.magic != SETTINGS_MAGIC) memset(&g_store, 0, sizeof(g_store));
  }

  syncRuntimeFromStore();
  g_macSuffix = WiFi.macAddress();
  g_macSuffix.replace(":", "");
  if (g_macSuffix.length() >= 4) {
    g_macSuffix = g_macSuffix.substring(g_macSuffix.length() - 4);
  } else {
    g_macSuffix = "0000";
  }
  g_macSuffix.toUpperCase();

  if (!validHostname(g_host)) {
    g_host = String(g_cfg.hostnamePrefix) + g_macSuffix;
    g_host.toLowerCase();
    commitSettings();
  }
  g_apSsid = String(g_cfg.apSsidPrefix) + g_macSuffix;
}

bool forgetWifi() {
  g_ssid = "";
  g_pass = "";
  if (!commitSettings()) return false;
  WiFi.disconnect(false);
  return true;
}

// Advertises the web server plus a vendor service whose TXT records identify the device
// without an HTTP request: model, id, fw, api.
void startMdns() {
  if (!MDNS.begin(g_host.c_str())) return;
  MDNS.addService("http", "tcp", 80);
  if (!g_cfg.serviceType || !g_cfg.serviceType[0]) return;
  MDNS.addService(g_cfg.serviceType, "tcp", 80);
  MDNS.addServiceTxt(g_cfg.serviceType, "tcp", "model", g_cfg.model);
  MDNS.addServiceTxt(g_cfg.serviceType, "tcp", "id", BaseCore::deviceId().c_str());
  MDNS.addServiceTxt(g_cfg.serviceType, "tcp", "fw", g_cfg.fwVersion);
  MDNS.addServiceTxt(g_cfg.serviceType, "tcp", "api", String(g_cfg.apiVersion).c_str());
  MDNS.addServiceTxt(g_cfg.serviceType, "tcp", "manufacturer", g_cfg.manufacturer);
}

bool connectStation() {
  if (g_ssid.isEmpty()) return false;

  Serial.printf("[wifi] connecting to \"%s\"\n", g_ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.hostname(g_host);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_ssid.c_str(), g_pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && !elapsed(start, g_cfg.staConnectTimeoutMs)) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wifi] connection failed");
    WiFi.disconnect(false);
    return false;
  }

  Serial.printf("[wifi] connected, IP %s, http://%s.local/\n",
                WiFi.localIP().toString().c_str(), g_host.c_str());
  startMdns();
  return true;
}

void startPortal() {
  Serial.printf("[portal] starting AP \"%s\" at %s\n", g_apSsid.c_str(),
                AP_IP.toString().c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(g_host);
  WiFi.setAutoReconnect(false);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  const char* pass = (g_cfg.apPassword && strlen(g_cfg.apPassword) >= 8) ? g_cfg.apPassword : nullptr;
  WiFi.softAP(g_apSsid.c_str(), pass);

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", AP_IP);

  g_portal = true;
  g_retrying = false;
  g_lastRetry = millis();
}

void portalRetryTick() {
  if (g_ssid.isEmpty()) return;

  if (g_retrying) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[portal] saved network is back, restarting into station mode");
      BaseCore::scheduleRestart(500);
      g_retrying = false;
    } else if (elapsed(g_lastRetry, g_cfg.staConnectTimeoutMs)) {
      WiFi.disconnect(false);
      g_retrying = false;
      g_lastRetry = millis();
    }
    return;
  }

  if (elapsed(g_lastRetry, g_cfg.staRetryIntervalMs) && WiFi.softAPgetStationNum() == 0) {
    Serial.println("[portal] retrying saved network");
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
    g_retrying = true;
    g_lastRetry = millis();
  }
}

void stationWatchTick() {
  if (WiFi.status() == WL_CONNECTED) {
    g_linkLostAt = 0;
    return;
  }
  if (g_linkLostAt == 0) {
    g_linkLostAt = millis() | 1;
    Serial.println("[wifi] link lost, auto-reconnecting");
  } else if (elapsed(g_linkLostAt, g_cfg.staRetryIntervalMs)) {
    Serial.println("[wifi] link down too long, falling back to setup portal");
    g_linkLostAt = 0;
    MDNS.close();
    startPortal();
  }
}

void resetButtonTick() {
  if (g_cfg.resetButtonPin < 0) return;
  if (digitalRead(g_cfg.resetButtonPin) == LOW) {
    if (g_btnDownAt == 0) {
      g_btnDownAt = millis() | 1;
    } else if (elapsed(g_btnDownAt, g_cfg.resetHoldMs)) {
      Serial.println("[reset] button held, erasing WiFi credentials");
      if (!forgetWifi()) {
        Serial.println("[reset] failed to erase WiFi credentials");
        g_btnDownAt = 0;
        return;
      }
      ESP.restart();
    }
  } else {
    g_btnDownAt = 0;
  }
}

bool isOwnHost() {
  String h = g_server.hostHeader();
  int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  h.toLowerCase();
  return h.isEmpty() || h == AP_IP.toString() || h == WiFi.softAPIP().toString() ||
         h == WiFi.localIP().toString() || h == g_host || h == g_host + ".local";
}

bool captiveRedirect() {
  if (!g_portal || isOwnHost()) return false;
  g_server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
  g_server.send(302, "text/plain", "");
  return true;
}

void sendPage(bool settingsView) {
  if (captiveRedirect()) return;
  if (!BaseCore::authorize()) return;
  String page = FPSTR(BASE_PAGE_HTML);
  page.replace("%TITLE%", g_cfg.title);
  page.replace("%VIEW%", settingsView ? "settings" : "main");
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "text/html", page);
}

// In setup AP mode the root shows settings so WiFi can be configured.
void handleRoot() { sendPage(g_portal); }
void handleSettings() { sendPage(true); }

void handleStatus() {
  if (!BaseCore::authorize()) return;
  bool connected = WiFi.status() == WL_CONNECTED;

  String j = "{";
  j += "\"title\":" + jsonStr(g_cfg.title);
  j += ",\"version\":" + jsonStr(g_cfg.fwVersion);
  j += ",\"build\":" + jsonStr(String(__DATE__) + " " + __TIME__);
  j += ",\"mode\":" + jsonStr(g_portal ? "portal" : "station");
  j += ",\"savedSsid\":" + jsonStr(g_ssid);
  j += ",\"connected\":" + String(connected ? "true" : "false");
  j += ",\"ip\":" + jsonStr(connected ? WiFi.localIP().toString() : "");
  j += ",\"rssi\":" + String(connected ? WiFi.RSSI() : 0);
  j += ",\"apSsid\":" + jsonStr(g_portal ? g_apSsid : "");
  j += ",\"apIp\":" + jsonStr(g_portal ? AP_IP.toString() : "");
  j += ",\"apClients\":" + String(g_portal ? WiFi.softAPgetStationNum() : 0);
  j += ",\"hostname\":" + jsonStr(g_host);
  j += ",\"mac\":" + jsonStr(WiFi.macAddress());
  j += ",\"uptime\":" + String(millis() / 1000);
  j += ",\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"chip\":" + jsonStr("ESP8266");
  j += ",\"flash\":" + String(ESP.getFlashChipRealSize());
  j += ",\"flashMode\":" + jsonStr(flashModeName());
  j += ",\"sketch\":" + String(ESP.getSketchSize());
  j += ",\"otaSpace\":" + String(ESP.getFreeSketchSpace());
  j += ",\"sdk\":" + jsonStr(ESP.getSdkVersion());
  j += ",\"authEnabled\":" + String(g_admin.isEmpty() ? "false" : "true");
  j += "}";
  sendJson(200, j);
}

// Identity for integrations. Deliberately public (no admin password) so discovery can read
// the model before asking for credentials; authRequired says whether other calls need them.
void handleInfo() { sendJson(200, BaseCore::infoJson()); }

void handleScan() {
  if (!BaseCore::authorize()) return;
  int n = WiFi.scanComplete();
  if (g_server.hasArg("fresh") && n >= 0) {
    WiFi.scanDelete();
    n = WIFI_SCAN_FAILED;
  }
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    sendJson(200, "{\"scanning\":true}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    sendJson(200, "{\"scanning\":true}");
    return;
  }

  String j = "{\"scanning\":false,\"networks\":[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool dup = false;
    for (int k = 0; k < n && !dup; k++) {
      if (k != i && WiFi.SSID(k) == ssid &&
          (WiFi.RSSI(k) > WiFi.RSSI(i) || (WiFi.RSSI(k) == WiFi.RSSI(i) && k < i))) {
        dup = true;
      }
    }
    if (dup) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"ssid\":" + jsonStr(ssid) + ",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"secure\":" + (networkIsSecure(i) ? "true" : "false") + "}";
  }
  j += "]}";
  WiFi.scanDelete();
  sendJson(200, j);
}

void handleWifiSave() {
  if (!BaseCore::authorize()) return;
  String ssid = g_server.arg("ssid");
  String pass = g_server.arg("pass");
  ssid.trim();
  if (ssid.isEmpty() || ssid.length() > 32) return sendResult(false, "SSID must be 1-32 characters.");
  if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63))
    return sendResult(false, "Password must be empty (open network) or 8-63 characters.");

  g_ssid = ssid;
  g_pass = pass;
  if (!commitSettings()) return sendResult(false, "Failed to save WiFi credentials to flash.");

  sendResult(true, "Saved. Restarting to connect to \"" + ssid + "\". If it can't connect, the setup AP \"" +
                       g_apSsid + "\" will come back.", true);
  BaseCore::scheduleRestart(1500);
}

void handleDeviceSave() {
  if (!BaseCore::authorize()) return;
  bool restart = false;
  String nextHost = g_host;
  String nextAdmin = g_admin;

  if (g_server.hasArg("hostname")) {
    String h = g_server.arg("hostname");
    h.trim();
    h.toLowerCase();
    if (!validHostname(h))
      return sendResult(false, "Hostname: 1-32 chars, a-z, 0-9 and '-', not starting/ending with '-'.");
    if (h != g_host) {
      nextHost = h;
      restart = true;
    }
  }

  if (g_server.arg("adminClear") == "1") {
    nextAdmin = "";
  } else if (g_server.arg("admin").length() > 0) {
    String a = g_server.arg("admin");
    if (a.length() < 4 || a.length() > 64) return sendResult(false, "Admin password must be 4-64 characters.");
    nextAdmin = a;
  }

  g_host = nextHost;
  g_admin = nextAdmin;
  if (!commitSettings()) return sendResult(false, "Failed to save device settings to flash.");

  sendResult(true, restart ? "Saved. Restarting to apply the new hostname." : "Saved.", restart);
  if (restart) BaseCore::scheduleRestart(1500);
}

void handleForget() {
  if (!BaseCore::authorize()) return;
  if (!forgetWifi()) return sendResult(false, "Failed to erase WiFi credentials from flash.");
  sendResult(true, "WiFi credentials erased. Restarting into setup AP \"" + g_apSsid + "\".", true);
  BaseCore::scheduleRestart(1500);
}

void handleReboot() {
  if (!BaseCore::authorize()) return;
  sendResult(true, "Restarting.", true);
  BaseCore::scheduleRestart(1000);
}

void handleUpdateUpload() {
  HTTPUpload& up = g_server.upload();

  if (up.status == UPLOAD_FILE_START) {
    g_updateStarted = true;
    g_updateFirstChunk = true;
    g_updateError = "";
    g_updateAuthOk = g_admin.isEmpty() || g_server.authenticate(ADMIN_USER, g_admin.c_str());
    if (!g_updateAuthOk) {
      g_updateError = "Unauthorized";
      return;
    }
    Serial.printf("[ota] receiving %s\n", up.filename.c_str());
    size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (maxSketchSpace == 0 || !Update.begin(maxSketchSpace)) g_updateError = updaterError();
    return;
  }

  if (!g_updateAuthOk || !g_updateError.isEmpty()) {
    if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
      if (Update.isRunning()) Update.end();
    }
    return;
  }

  if (up.status == UPLOAD_FILE_WRITE) {
    if (g_updateFirstChunk) {
      g_updateFirstChunk = false;
      if (up.currentSize < 16 || up.buf[0] != 0xE9) {
        g_updateError = "Not an ESP8266 application image (.bin).";
        Update.end();
        return;
      }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      g_updateError = updaterError();
      if (Update.isRunning()) Update.end();
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) {
      g_updateError = updaterError();
    } else {
      Serial.printf("[ota] success, %u bytes\n", (unsigned)up.totalSize);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (Update.isRunning()) Update.end();
    g_updateError = "Upload aborted.";
  }
}

void handleUpdateDone() {
  g_server.sendHeader("Connection", "close");
  if (!g_updateStarted) {
    if (!BaseCore::authorize()) return;
    return sendResult(false, "No firmware file received.");
  }
  g_updateStarted = false;
  if (!g_updateAuthOk) {
    g_server.requestAuthentication();
    return;
  }
  if (!g_updateError.isEmpty()) {
    Serial.printf("[ota] failed: %s\n", g_updateError.c_str());
    return sendResult(false, "Update failed: " + g_updateError);
  }
  sendResult(true, "Update successful. Restarting into the new firmware.", true);
  BaseCore::scheduleRestart(1500);
}

void handleNotFound() {
  if (captiveRedirect()) return;
  g_server.send(404, "text/plain", "Not found");
}

void registerRoutes() {
  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/settings", HTTP_GET, handleSettings);
  g_server.on("/api/status", HTTP_GET, handleStatus);
  g_server.on("/api/info", HTTP_GET, handleInfo);
  g_server.on("/api/scan", HTTP_GET, handleScan);
  g_server.on("/api/wifi", HTTP_POST, handleWifiSave);
  g_server.on("/api/device", HTTP_POST, handleDeviceSave);
  g_server.on("/api/forget", HTTP_POST, handleForget);
  g_server.on("/api/reboot", HTTP_POST, handleReboot);
  g_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  g_server.onNotFound(handleNotFound);
}

}  // namespace

namespace BaseCore {

void begin(const BaseConfig& cfg) {
  g_cfg = cfg;
  if (g_cfg.resetButtonPin >= 0) pinMode(g_cfg.resetButtonPin, INPUT_PULLUP);

  WiFi.persistent(false);
  loadSettings();
  Serial.printf("\n%s v%s (%s)\n", g_cfg.title, g_cfg.fwVersion, g_host.c_str());

  if (!connectStation()) startPortal();

  registerRoutes();
  g_server.begin();
}

void loop() {
  if (g_portal) {
    g_dns.processNextRequest();
    portalRetryTick();
  } else {
    stationWatchTick();
    if (WiFi.status() == WL_CONNECTED) MDNS.update();
  }
  g_server.handleClient();
  resetButtonTick();

  if (g_restartPending && (int32_t)(millis() - g_restartAt) >= 0) {
    Serial.println("[sys] restarting");
    delay(100);
    ESP.restart();
  }
}

ESP8266WebServer& server() { return g_server; }

bool authorize() {
  if (g_admin.isEmpty() || g_server.authenticate(ADMIN_USER, g_admin.c_str())) return true;
  g_server.requestAuthentication(BASIC_AUTH, g_cfg.title);
  return false;
}

bool isPortalMode() { return g_portal; }
bool isConnected() { return WiFi.status() == WL_CONNECTED; }
String hostname() { return g_host; }

const BaseConfig& config() { return g_cfg; }

String infoJson() {
  String j = "{";
  j += "\"manufacturer\":" + jsonStr(g_cfg.manufacturer);
  j += ",\"model\":" + jsonStr(g_cfg.model);
  j += ",\"name\":" + jsonStr(g_cfg.title);
  j += ",\"id\":" + jsonStr(deviceId());
  j += ",\"mac\":" + jsonStr(WiFi.macAddress());
  j += ",\"hostname\":" + jsonStr(g_host);
  j += ",\"ip\":" + jsonStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String());
  j += ",\"fw\":" + jsonStr(g_cfg.fwVersion);
  j += ",\"api\":" + String(g_cfg.apiVersion);
  j += ",\"authRequired\":" + String(g_admin.isEmpty() ? "false" : "true");
  if (g_cfg.infoExtra) {
    String extra = g_cfg.infoExtra();
    if (!extra.isEmpty()) j += "," + extra;
  }
  j += "}";
  return j;
}

String deviceId() {
  String id = WiFi.macAddress();
  id.replace(":", "");
  id.toLowerCase();
  return id;
}

void scheduleRestart(uint32_t delayMs) {
  g_restartPending = true;
  g_restartAt = millis() + delayMs;
}

}  // namespace BaseCore
