#include "mqtt.h"

#include <ArduinoJson.h>
#include <BaseCore.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <PubSubClient.h>

#include "controller.h"
#include "settings_file.h"

namespace {

constexpr char kSettingsPath[] = "/mqtt.bin";
constexpr uint32_t kSettingsMagic = 0x4D515401;  // "MQT" + layout version
constexpr uint32_t kRetryMs = 10000;             // between connection attempts
constexpr uint32_t kStateMinIntervalMs = 100;    // rate limit for state publishes
constexpr uint16_t kBufferSize = 1024;           // fits the state JSON and incoming commands
// Topic used as the default by firmware 1.1.0; migrated to the per-device default.
constexpr char kLegacyDefaultTopic[] = "logix3";

// Persisted to LittleFS. Bump kSettingsMagic whenever this layout changes.
struct __attribute__((packed)) Settings {
  uint32_t magic;
  char host[64];  // empty = MQTT disabled
  uint16_t port;
  char user[64];
  char pass[64];
  char topic[64];
  uint32_t checksum;
};

Settings g_settings;
bool g_fsReady = false;
WiFiClient g_net;
PubSubClient g_client(g_net);
bool g_reconnectNow = true;
uint32_t g_lastAttemptAt = 0;
uint32_t g_publishedVersion = 0;
uint32_t g_lastPublishAt = 0;
int g_lastError = 0;            // PubSubClient state after the last failed attempt
String g_statusTopic;           // topic the "online" message was published to

bool elapsed(uint32_t since, uint32_t ms) { return (uint32_t)(millis() - since) >= ms; }

template <size_t N>
void setField(char (&field)[N], const String& value) {
  memset(field, 0, N);
  value.substring(0, N - 1).toCharArray(field, N);
}

template <size_t N>
void terminate(char (&field)[N]) { field[N - 1] = '\0'; }

// <model>/<device id>, e.g. logix3/48e72971e527, so several units never share topics.
String defaultTopic() { return String(BaseCore::config().model) + "/" + BaseCore::deviceId(); }

// Retained identity for MQTT-based discovery: <serviceType>/discovery/<device id>.
String discoveryTopic() {
  const char* prefix = BaseCore::config().serviceType;
  return String(prefix && prefix[0] ? prefix : "logix") + "/discovery/" + BaseCore::deviceId();
}

void setDefaults() {
  memset(&g_settings, 0, sizeof(g_settings));
  g_settings.port = 1883;
  setField(g_settings.topic, defaultTopic());
}

void loadSettings() {
  setDefaults();
  if (!g_fsReady) return;
  File f = LittleFS.open(kSettingsPath, "r");
  if (!f) return;
  Settings loaded;
  if (SettingsFile::read(f, loaded, kSettingsMagic)) {
    terminate(loaded.host);
    terminate(loaded.user);
    terminate(loaded.pass);
    terminate(loaded.topic);
    if (strcmp(loaded.topic, kLegacyDefaultTopic) == 0) setField(loaded.topic, defaultTopic());
    g_settings = loaded;
  } else {
    Serial.println("[mqtt] saved settings invalid, using defaults");
  }
  f.close();
}

String topic(const char* suffix) { return String(g_settings.topic) + "/" + suffix; }

const char* errorText(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT: return "Connection timed out.";
    case MQTT_CONNECTION_LOST: return "Connection lost.";
    case MQTT_CONNECT_FAILED: return "Could not reach the broker.";
    case MQTT_DISCONNECTED: return "Disconnected.";
    case MQTT_CONNECT_BAD_PROTOCOL: return "Broker rejected the protocol version.";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "Broker rejected the client ID.";
    case MQTT_CONNECT_UNAVAILABLE: return "Broker unavailable.";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "Bad username or password.";
    case MQTT_CONNECT_UNAUTHORIZED: return "Not authorized.";
    default: return "";
  }
}

void publishResult(const char* command, bool ok, const String& message) {
  JsonDocument doc;
  doc["command"] = command;
  doc["ok"] = ok;
  doc["message"] = message;
  String out;
  serializeJson(doc, out);
  g_client.publish(topic("result").c_str(), out.c_str());
}

void publishState() {
  String state = Controller::stateJson(false);
  g_client.publish(topic("state").c_str(), state.c_str(), true);
  g_publishedVersion = Controller::stateVersion();
  g_lastPublishAt = millis();
}

// Same JSON as GET /api/info, plus the topic in use, so an integration subscribed to
// <serviceType>/discovery/# can find every device on the broker.
void publishDiscovery() {
  String info = BaseCore::infoJson();
  g_client.publish(discoveryTopic().c_str(), info.c_str(), true);
}

void onAction(uint8_t button, const char* action) {
  if (!g_client.connected()) return;
  String t = topic("button/") + String(button) + "/action";
  g_client.publish(t.c_str(), action);
}

// Commands mirror the HTTP API:
//   <topic>/set             JSON, same keys as POST /api/controller
//   <topic>/press           JSON {"button":1-3,"presses":"single|double|triple|hold"}
//   <topic>/brightness/set  plain 0-255
//   <topic>/get             any payload; republishes <topic>/state
void onMessage(char* rawTopic, byte* payload, unsigned int length) {
  // Copy out first: publishing reuses the client buffer that payload points into.
  String t(rawTopic);
  String body;
  body.reserve(length);
  for (unsigned int i = 0; i < length; i++) body += static_cast<char>(payload[i]);

  String base = String(g_settings.topic) + "/";
  if (!t.startsWith(base)) return;
  String command = t.substring(base.length());

  if (command == "get") {
    publishState();
    return;
  }

  bool ok;
  String message;
  if (command == "brightness/set") {
    Controller::ArgLookup arg = [&body](const String& name, String& value) {
      if (name != "brightness") return false;
      value = body;
      return true;
    };
    ok = Controller::setBrightness(arg, message);
  } else if (command == "set" || command == "press") {
    JsonDocument doc;
    if (deserializeJson(doc, body) || !doc.is<JsonObject>()) {
      publishResult(command.c_str(), false, "Payload must be a JSON object.");
      return;
    }
    JsonObject obj = doc.as<JsonObject>();
    Controller::ArgLookup arg = [&obj](const String& name, String& value) {
      JsonVariant v = obj[name];
      if (v.isNull()) return false;
      value = "";
      if (v.is<const char*>()) value = v.as<const char*>();
      else serializeJson(v, value);  // numbers and booleans as text
      return true;
    };
    ok = command == "set" ? Controller::update(arg, message) : Controller::press(arg, message);
  } else {
    return;
  }
  publishResult(command.c_str(), ok, message);
}

void disconnect() {
  if (g_client.connected()) {
    // A clean disconnect does not trigger the last will, so mark offline ourselves.
    g_client.publish(g_statusTopic.c_str(), "offline", true);
    g_client.disconnect();
  }
  g_net.stop();
}

void connect() {
  g_lastAttemptAt = millis();
  g_reconnectNow = false;
  g_client.setServer(g_settings.host, g_settings.port);

  String clientId = BaseCore::hostname();
  String statusTopic = topic("status");
  const char* user = g_settings.user[0] ? g_settings.user : nullptr;
  const char* pass = g_settings.pass[0] ? g_settings.pass : nullptr;
  Serial.printf("[mqtt] connecting to %s:%u as %s\n", g_settings.host, g_settings.port, clientId.c_str());

  if (!g_client.connect(clientId.c_str(), user, pass, statusTopic.c_str(), 0, true, "offline")) {
    g_lastError = g_client.state();
    Serial.printf("[mqtt] connect failed (%d) %s\n", g_lastError, errorText(g_lastError));
    return;
  }

  g_lastError = 0;
  g_statusTopic = statusTopic;
  g_client.publish(statusTopic.c_str(), "online", true);
  const char* commands[] = {"set", "press", "brightness/set", "get"};
  for (const char* c : commands) g_client.subscribe(topic(c).c_str());
  publishState();
  publishDiscovery();
  Serial.println("[mqtt] connected");
}

const char* statusName() {
  if (!g_settings.host[0]) return "disabled";
  if (g_client.connected()) return "connected";
  if (!BaseCore::isConnected()) return "waiting for WiFi";
  return "disconnected";
}

bool validTopic(const String& t) {
  if (t.isEmpty() || t.length() > 63 || t.startsWith("/") || t.endsWith("/")) return false;
  return t.indexOf('+') < 0 && t.indexOf('#') < 0 && t.indexOf(' ') < 0;
}

void sendJson(int code, const String& body) {
  ESP8266WebServer& server = BaseCore::server();
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendResult(bool ok, const String& message) {
  JsonDocument doc;
  doc["ok"] = ok;
  doc["message"] = message;
  String out;
  serializeJson(doc, out);
  sendJson(ok ? 200 : 400, out);
}

void handleGet() {
  if (!BaseCore::authorize()) return;
  JsonDocument doc;
  doc["host"] = g_settings.host;
  doc["port"] = g_settings.port;
  doc["user"] = g_settings.user;
  doc["passwordSet"] = g_settings.pass[0] != '\0';  // never send the password back
  doc["topic"] = g_settings.topic;
  doc["defaultTopic"] = defaultTopic();
  doc["discoveryTopic"] = discoveryTopic();
  doc["clientId"] = BaseCore::hostname();
  doc["status"] = statusName();
  doc["error"] = g_client.connected() ? "" : errorText(g_lastError);
  String out;
  serializeJson(doc, out);
  sendJson(200, out);
}

// host (blank disables), port, user, pass (blank keeps the saved one), passClear=1, topic.
void handleSave() {
  if (!BaseCore::authorize()) return;
  ESP8266WebServer& server = BaseCore::server();

  String host = server.arg("host");
  String user = server.arg("user");
  String pass = server.arg("pass");
  String topicArg = server.arg("topic");
  String portArg = server.arg("port");
  host.trim();
  user.trim();
  topicArg.trim();
  portArg.trim();

  if (host.length() > 63 || host.indexOf(' ') >= 0) return sendResult(false, "Broker address is not valid.");
  long port = portArg.isEmpty() ? 1883 : portArg.toInt();
  if (port < 1 || port > 65535) return sendResult(false, "Port must be 1-65535.");
  if (user.length() > 63) return sendResult(false, "Username is too long (max 63).");
  if (pass.length() > 63) return sendResult(false, "Password is too long (max 63).");
  if (topicArg.isEmpty()) topicArg = defaultTopic();
  if (!validTopic(topicArg)) {
    return sendResult(false, "Topic must be 1-63 characters without spaces, +, # or a leading/trailing /.");
  }

  disconnect();
  Settings next = g_settings;
  setField(next.host, host);
  next.port = static_cast<uint16_t>(port);
  setField(next.user, user);
  if (server.arg("passClear") == "1") setField(next.pass, "");
  else if (!pass.isEmpty()) setField(next.pass, pass);
  setField(next.topic, topicArg);

  if (!g_fsReady || !SettingsFile::write(kSettingsPath, next, kSettingsMagic)) {
    return sendResult(false, "Could not save MQTT settings to flash.");
  }
  g_settings = next;
  g_lastError = 0;
  g_reconnectNow = true;
  sendResult(true, host.isEmpty() ? "MQTT disabled." : "MQTT settings saved. Connecting…");
}

}  // namespace

namespace Mqtt {

void begin(bool fsReady) {
  g_fsReady = fsReady;
  loadSettings();
  g_net.setTimeout(3000);
  g_client.setBufferSize(kBufferSize);
  g_client.setKeepAlive(30);
  g_client.setSocketTimeout(3);
  g_client.setCallback(onMessage);
  Controller::onAction(onAction);
}

void loop() {
  if (!g_settings.host[0] || BaseCore::isPortalMode() || !BaseCore::isConnected()) {
    if (g_client.connected()) disconnect();
    return;
  }

  if (!g_client.connected()) {
    if (g_reconnectNow || elapsed(g_lastAttemptAt, kRetryMs)) connect();
    return;
  }

  g_client.loop();
  if (Controller::stateVersion() != g_publishedVersion && elapsed(g_lastPublishAt, kStateMinIntervalMs)) {
    publishState();
  }
}

String infoJson() {
  JsonDocument doc;
  doc["enabled"] = g_settings.host[0] != '\0';
  doc["connected"] = g_client.connected();
  doc["topic"] = g_settings.topic;
  String out;
  serializeJson(doc, out);
  return "\"mqtt\":" + out;
}

void registerRoutes() {
  ESP8266WebServer& server = BaseCore::server();
  server.on("/api/mqtt", HTTP_GET, handleGet);
  server.on("/api/mqtt", HTTP_POST, handleSave);
}

}  // namespace Mqtt
