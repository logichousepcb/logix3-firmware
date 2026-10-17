#include <Arduino.h>
#include <BaseCore.h>
#include <LittleFS.h>

#include "controller.h"
#include "mqtt.h"

namespace {

void sendJson(const String& body, int code = 200) {
  ESP8266WebServer& server = BaseCore::server();
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendResult(bool ok, const String& message) {
  sendJson(String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":\"" + message + "\"}", ok ? 200 : 400);
}

bool serverArg(const String& name, String& value) {
  ESP8266WebServer& server = BaseCore::server();
  if (!server.hasArg(name)) return false;
  value = server.arg(name);
  return true;
}

using ControllerAction = bool (*)(const Controller::ArgLookup&, String&);

void runAction(ControllerAction action) {
  if (!BaseCore::authorize()) return;
  String message;
  bool ok = action(serverArg, message);
  sendResult(ok, message);
}

void handleControllerStatus() {
  if (!BaseCore::authorize()) return;
  sendJson(Controller::stateJson(true));
}

// Model-specific members of GET /api/info.
String infoExtra() { return "\"capabilities\":" + Controller::capabilitiesJson() + "," + Mqtt::infoJson(); }

void registerRoutes() {
  ESP8266WebServer& server = BaseCore::server();
  server.on("/api/controller", HTTP_GET, handleControllerStatus);
  server.on("/api/controller", HTTP_POST, [] { runAction(Controller::update); });
  server.on("/api/controller/press", HTTP_POST, [] { runAction(Controller::press); });
  server.on("/api/controller/brightness", HTTP_POST, [] { runAction(Controller::setBrightness); });
  Mqtt::registerRoutes();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  bool fsReady = LittleFS.begin();  // formats the partition on first use
  if (!fsReady) Serial.println("[fs] LittleFS mount failed; settings will not be saved");
  Controller::begin(fsReady);

  BaseConfig cfg;
  cfg.title = "Logix 3";
  cfg.fwVersion = "1.2.0";
  cfg.manufacturer = "Logix";
  cfg.model = "logix3";
  cfg.serviceType = "logix";
  cfg.apiVersion = 1;
  cfg.infoExtra = infoExtra;
  BaseCore::begin(cfg);
  Mqtt::begin(fsReady);

  registerRoutes();
}

void loop() {
  BaseCore::loop();
  Controller::loop();
  Mqtt::loop();
}
