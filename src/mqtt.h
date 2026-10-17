#pragma once

// MQTT link: mirrors the controller HTTP API onto <topic>/... (see API.md).

#include <Arduino.h>

namespace Mqtt {

// Call after BaseCore::begin(): the default topic uses the model and device id.
void begin(bool fsReady);
void loop();
// GET/POST /api/mqtt for the settings page.
void registerRoutes();
// "mqtt":{enabled,connected,topic} member for /api/info.
String infoJson();

}  // namespace Mqtt
