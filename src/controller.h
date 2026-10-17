#pragma once

// LED and button logic shared by the HTTP API and MQTT.

#include <Arduino.h>
#include <functional>

namespace Controller {

// Looks up a named request parameter. Returns false when it was not given.
using ArgLookup = std::function<bool(const String& name, String& value)>;
// Called for every completed button action (real or simulated); action is "single" ... "hold".
using ActionHandler = void (*)(uint8_t button, const char* action);

void begin(bool fsReady);
void loop();

// Same parameters as the matching HTTP endpoints; message is set for both success and failure.
bool update(const ArgLookup& arg, String& message);      // POST /api/controller
bool press(const ArgLookup& arg, String& message);       // POST /api/controller/press
bool setBrightness(const ArgLookup& arg, String& message);  // POST /api/controller/brightness

// live = true adds uptime and time-since-action fields, which change constantly.
String stateJson(bool live);
// Increases on every change that stateJson(false) would show.
uint32_t stateVersion();
// Static description of what this hardware offers, for /api/info.
String capabilitiesJson();
void onAction(ActionHandler handler);

}  // namespace Controller
