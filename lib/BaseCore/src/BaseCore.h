#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>

struct BaseConfig {
  const char* title = "ESP-12F Base Firmware";   // web page title / heading
  const char* fwVersion = "1.0.0";
  const char* apSsidPrefix = "ESP12F-";          // + last 4 hex of MAC
  const char* apPassword = "";                   // "" = open AP, else 8-63 chars
  const char* hostnamePrefix = "esp12f-";        // default hostname + MAC suffix
  uint32_t staConnectTimeoutMs = 20000;          // per connection attempt
  uint32_t staRetryIntervalMs = 300000;          // portal-mode retry / lost-link fallback
  int resetButtonPin = 0;                        // FLASH / GPIO0 button, active low; -1 disables
  uint32_t resetHoldMs = 5000;                   // hold to erase WiFi credentials

  // Device identity, used by GET /api/info and mDNS so integrations (e.g. Home Assistant)
  // can discover the device and pick model-specific behaviour.
  const char* manufacturer = "Logix";
  const char* model = "base";                    // machine-readable model id, stable across firmware versions
  const char* serviceType = "logix";             // advertised as _<serviceType>._tcp; "" disables
  uint8_t apiVersion = 1;                        // bump on breaking API changes
  // Optional extra members for /api/info, without braces, e.g. "\"capabilities\":{...}".
  String (*infoExtra)() = nullptr;
};

namespace BaseCore {

void begin(const BaseConfig& cfg = BaseConfig());
void loop();

// The shared web server (port 80). Application code may register extra routes.
ESP8266WebServer& server();

// Call at the top of a route handler; returns false (and has already sent a
// 401 challenge) when an admin password is set and the request lacks it.
bool authorize();

bool isPortalMode();
bool isConnected();
String hostname();
// Lowercase MAC without separators (e.g. "48e72971e527"); stable unique id for this unit.
String deviceId();
// Identity JSON served at GET /api/info (also usable for MQTT discovery).
String infoJson();
const BaseConfig& config();
void scheduleRestart(uint32_t delayMs = 1000);

}  // namespace BaseCore
