#include "controller.h"

#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>

#include "settings_file.h"

namespace {

constexpr uint8_t kButtonCount = 3;
constexpr uint8_t kButtonPins[kButtonCount] = {13, 5, 4};
constexpr uint8_t kLedDataPin = 12;  // single WS2812 chain: pixel 0/1/2 = LED 1/2/3
constexpr bool kButtonsActiveHigh = false;  // buttons pull to GND; internal pull-ups hold them high
constexpr uint32_t kButtonDebounceMs = 25;
constexpr uint8_t kMaxPresses = 3;              // single, double, triple press
constexpr uint8_t kModeHold = 4;                // LED mode selected by a long hold
constexpr uint8_t kColorModes = 4;              // single, double, triple, hold
constexpr uint32_t kMultiPressWindowMs = 400;   // max gap between presses of one sequence
constexpr uint32_t kHoldMs = 3000;              // hold this long for the hold color
constexpr uint32_t kSaveDelayMs = 2000;         // coalesce flash writes (button presses, brightness drags)

constexpr char kSettingsPath[] = "/leds.bin";
constexpr uint32_t kSettingsMagicV2 = 0x4C583302;  // "LX3" + layout version
constexpr uint32_t kSettingsMagicV3 = 0x4C583303;
constexpr uint32_t kSettingsMagic = 0x4C583304;

struct Rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct LedConfig {
  Rgb colors[kColorModes];  // color for a single / double / triple press and a hold
  Rgb off;
};

// Persisted to LittleFS. Bump kSettingsMagic whenever this layout changes.
// Packed so no uninitialised padding bytes end up in the checksum.
struct __attribute__((packed)) Settings {
  uint32_t magic;
  uint8_t brightness;
  LedConfig leds[kButtonCount];
  uint8_t mode[kButtonCount];  // 0 = off, 1-3 = press color, 4 = hold color
  uint8_t oldSwitchPattern;    // 1 = old board wiring: LED 1 and LED 3 pixels swapped
  uint32_t checksum;
};

// Layout written by firmware 1.2.0 (no old switch pattern option).
struct __attribute__((packed)) SettingsV3 {
  uint32_t magic;
  uint8_t brightness;
  LedConfig leds[kButtonCount];
  uint8_t mode[kButtonCount];
  uint32_t checksum;
};

// Layout written by firmware 1.0.6 (no hold color, no saved LED state).
struct __attribute__((packed)) SettingsV2 {
  uint32_t magic;
  uint8_t brightness;
  struct {
    Rgb press[kMaxPresses];
    Rgb off;
  } leds[kButtonCount];
  uint32_t checksum;
};

struct ButtonState {
  bool rawPressed;
  bool pressed;
  uint32_t rawChangedAt;
  uint32_t pressCount;
  uint8_t sequenceCount;   // presses in the sequence being collected
  uint32_t pressedAt;
  uint32_t releasedAt;
  bool holdFired;          // current press already selected the hold color
  uint8_t lastAction;      // mode of the last completed action (0 = none yet)
  uint32_t actionCount;    // completed actions since boot, so API pollers can spot new events
  uint32_t lastActionAt;
};

// Index = LED mode / button action. Used by the API in both directions.
const char* const kModeNames[] = {"off", "single", "double", "triple", "hold"};

const Settings kDefaultSettings = {
    kSettingsMagic,
    255,
    {
        {{{0xFF, 0x40, 0x40}, {0xFF, 0xA0, 0x00}, {0xFF, 0xFF, 0xFF}, {0x80, 0x00, 0xFF}}, {0x00, 0x00, 0x00}},
        {{{0x00, 0xFF, 0x88}, {0xFF, 0xA0, 0x00}, {0xFF, 0xFF, 0xFF}, {0x80, 0x00, 0xFF}}, {0x00, 0x00, 0x00}},
        {{{0x09, 0x8C, 0xFF}, {0xFF, 0xA0, 0x00}, {0xFF, 0xFF, 0xFF}, {0x80, 0x00, 0xFF}}, {0x00, 0x00, 0x00}},
    },
    {0, 0, 0},
    0,
    0,
};

Settings g_settings = kDefaultSettings;
ButtonState g_buttonStates[kButtonCount] = {};
bool g_fsReady = false;
bool g_saveQueued = false;
uint32_t g_saveQueuedAt = 0;
uint32_t g_stateVersion = 1;
Controller::ActionHandler g_actionHandler = nullptr;

Adafruit_NeoPixel g_strip(kButtonCount, kLedDataPin, NEO_GRB + NEO_KHZ800);

bool elapsed(uint32_t since, uint32_t ms) { return (uint32_t)(millis() - since) >= ms; }

void loadSettings() {
  g_settings = kDefaultSettings;
  if (!g_fsReady) {
    Serial.println("[leds] no filesystem, using defaults");
    return;
  }

  File f = LittleFS.open(kSettingsPath, "r");
  if (!f) {
    Serial.println("[leds] no saved settings, using defaults");
    return;
  }

  Settings loaded;
  SettingsV3 v3;
  SettingsV2 old;
  if (SettingsFile::read(f, loaded, kSettingsMagic)) {
    for (uint8_t i = 0; i < kButtonCount; i++) {
      if (loaded.mode[i] > kColorModes) loaded.mode[i] = 0;
    }
    loaded.oldSwitchPattern = loaded.oldSwitchPattern ? 1 : 0;
    g_settings = loaded;
    Serial.println("[leds] settings loaded");
  } else if (SettingsFile::read(f, v3, kSettingsMagicV3)) {
    // Same layout minus the old switch pattern option, which defaults to off.
    g_settings.brightness = v3.brightness;
    memcpy(g_settings.leds, v3.leds, sizeof(v3.leds));
    for (uint8_t i = 0; i < kButtonCount; i++) g_settings.mode[i] = v3.mode[i] > kColorModes ? 0 : v3.mode[i];
    Serial.println("[leds] migrated settings from previous layout");
  } else if (SettingsFile::read(f, old, kSettingsMagicV2)) {
    // Keep colors saved by the previous firmware; the hold color and LED state use defaults.
    g_settings.brightness = old.brightness;
    for (uint8_t i = 0; i < kButtonCount; i++) {
      memcpy(g_settings.leds[i].colors, old.leds[i].press, sizeof(old.leds[i].press));
      g_settings.leds[i].off = old.leds[i].off;
    }
    Serial.println("[leds] migrated settings from previous layout");
  } else {
    Serial.println("[leds] saved settings invalid, using defaults");
  }
  f.close();
}

bool saveSettings() {
  if (!g_fsReady) return false;
  if (!SettingsFile::write(kSettingsPath, g_settings, kSettingsMagic)) {
    Serial.println("[leds] failed to save settings");
    return false;
  }
  Serial.println("[leds] settings saved");
  return true;
}

void queueSave() {
  g_saveQueued = true;
  g_saveQueuedAt = millis();
}

void pollSave() {
  if (g_saveQueued && elapsed(g_saveQueuedAt, kSaveDelayMs)) {
    g_saveQueued = false;
    saveSettings();
  }
}

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

String colorToHex(const Rgb& color) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", color.red, color.green, color.blue);
  return String(buf);
}

bool parseHexColor(String value, Rgb& color) {
  value.trim();
  if (value.length() != 7 || value[0] != '#') return false;

  int digits[6];
  for (uint8_t i = 0; i < 6; i++) {
    digits[i] = hexNibble(value[i + 1]);
    if (digits[i] < 0) return false;
  }

  color.red = (digits[0] << 4) | digits[1];
  color.green = (digits[2] << 4) | digits[3];
  color.blue = (digits[4] << 4) | digits[5];
  return true;
}

bool parseBrightness(String value, uint8_t& brightness) {
  value.trim();
  if (value.isEmpty()) return false;
  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }
  long n = value.toInt();
  if (n < 0 || n > 255) return false;
  brightness = static_cast<uint8_t>(n);
  return true;
}

// Accepts a mode name ("off", "single", ... "hold") or its number (0-4).
bool parseMode(String value, uint8_t& mode) {
  value.trim();
  value.toLowerCase();
  for (uint8_t m = 0; m <= kColorModes; m++) {
    if (value == kModeNames[m] || value == String(m)) {
      mode = m;
      return true;
    }
  }
  return false;
}

bool parseBool(String value, bool& out) {
  value.trim();
  value.toLowerCase();
  if (value == "1" || value == "true" || value == "on") out = true;
  else if (value == "0" || value == "false" || value == "off") out = false;
  else return false;
  return true;
}

bool readButtonRaw(uint8_t index) {
  int level = digitalRead(kButtonPins[index]);
  return kButtonsActiveHigh ? level == HIGH : level == LOW;
}

void setPixel(uint8_t index) {
  const LedConfig& led = g_settings.leds[index];
  uint8_t mode = g_settings.mode[index];
  const Rgb& color = mode > 0 ? led.colors[mode - 1] : led.off;
  // The old board chained the pixels in the opposite order, so LED 1 and LED 3 trade places.
  uint8_t pixel = g_settings.oldSwitchPattern ? kButtonCount - 1 - index : index;
  g_strip.setPixelColor(pixel, Adafruit_NeoPixel::Color(color.red, color.green, color.blue));
}

void applyAllLeds() {
  // setBrightness() is lossy on stored pixels, so always rewrite them afterwards.
  g_strip.setBrightness(g_settings.brightness);
  for (uint8_t i = 0; i < kButtonCount; i++) setPixel(i);
  g_strip.show();
  g_stateVersion++;
}

void setLedMode(uint8_t index, uint8_t mode) {
  g_settings.mode[index] = mode;
  setPixel(index);
  g_strip.show();
  g_stateVersion++;
  queueSave();
}

// A button action: mode 1-3 comes from a press sequence, 4 from a hold. Selecting the
// mode the LED is already showing turns it off.
void selectMode(uint8_t index, uint8_t mode) {
  ButtonState& b = g_buttonStates[index];
  b.lastAction = mode;
  b.actionCount++;
  b.lastActionAt = millis();
  setLedMode(index, g_settings.mode[index] == mode ? 0 : mode);
  Serial.printf("[controller] button %u %s -> LED %u %s\n", index + 1, kModeNames[mode], index + 1,
                kModeNames[g_settings.mode[index]]);
  if (g_actionHandler) g_actionHandler(index + 1, kModeNames[mode]);
}

void pollButtons() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < kButtonCount; i++) {
    ButtonState& b = g_buttonStates[i];
    bool rawPressed = readButtonRaw(i);
    if (rawPressed != b.rawPressed) {
      b.rawPressed = rawPressed;
      b.rawChangedAt = now;
    } else if (rawPressed != b.pressed && elapsed(b.rawChangedAt, kButtonDebounceMs)) {
      b.pressed = rawPressed;
      g_stateVersion++;
      if (rawPressed) {
        b.pressCount++;
        b.pressedAt = now;
        b.holdFired = false;
        if (b.sequenceCount < kMaxPresses) b.sequenceCount++;
      } else if (!b.holdFired) {
        b.releasedAt = now;
        // Nothing beyond a triple press, so finish the sequence without waiting.
        if (b.sequenceCount >= kMaxPresses) {
          selectMode(i, b.sequenceCount);
          b.sequenceCount = 0;
        }
      }
    }

    // A long hold replaces any presses collected so far and fires without waiting for release.
    if (b.pressed && !b.holdFired && elapsed(b.pressedAt, kHoldMs)) {
      b.holdFired = true;
      b.sequenceCount = 0;
      selectMode(i, kModeHold);
    }

    if (b.sequenceCount > 0 && !b.pressed && elapsed(b.releasedAt, kMultiPressWindowMs)) {
      selectMode(i, b.sequenceCount);
      b.sequenceCount = 0;
    }
  }
}

void initButtons() {
  for (uint8_t i = 0; i < kButtonCount; i++) {
    ButtonState& b = g_buttonStates[i];
    pinMode(kButtonPins[i], kButtonsActiveHigh ? INPUT : INPUT_PULLUP);
    b.rawPressed = readButtonRaw(i);
    b.pressed = b.rawPressed;
    b.rawChangedAt = millis();
    // A button already down at boot must not count as a hold.
    b.holdFired = b.pressed;
  }
}

}  // namespace

namespace Controller {

void begin(bool fsReady) {
  g_fsReady = fsReady;
  initButtons();
  loadSettings();
  g_strip.begin();
  applyAllLeds();
}

void loop() {
  pollButtons();
  pollSave();
}

// Every field is optional, so clients can change a single color or LED:
//   ledNColor1-4 (single/double/triple/hold), ledNOffColor, ledNState, brightness,
//   oldSwitchPattern.
bool update(const ArgLookup& arg, String& message) {
  Settings next = g_settings;
  uint8_t fields = 0;
  String value;

  for (uint8_t i = 0; i < kButtonCount; i++) {
    String prefix = "led" + String(i + 1);
    for (uint8_t m = 0; m < kColorModes; m++) {
      String name = prefix + "Color" + String(m + 1);
      if (!arg(name, value)) continue;
      if (!parseHexColor(value, next.leds[i].colors[m])) {
        message = name + " must use #RRGGBB format.";
        return false;
      }
      fields++;
    }
    if (arg(prefix + "OffColor", value)) {
      if (!parseHexColor(value, next.leds[i].off)) {
        message = prefix + "OffColor must use #RRGGBB format.";
        return false;
      }
      fields++;
    }
    if (arg(prefix + "State", value)) {
      if (!parseMode(value, next.mode[i])) {
        message = prefix + "State must be off, single, double, triple or hold.";
        return false;
      }
      fields++;
    }
  }

  if (arg("brightness", value)) {
    if (!parseBrightness(value, next.brightness)) {
      message = "Brightness must be 0-255.";
      return false;
    }
    fields++;
  }
  if (arg("oldSwitchPattern", value)) {
    bool old;
    if (!parseBool(value, old)) {
      message = "oldSwitchPattern must be true or false.";
      return false;
    }
    next.oldSwitchPattern = old ? 1 : 0;
    fields++;
  }
  if (fields == 0) {
    message = "No LED settings given.";
    return false;
  }

  g_settings = next;
  applyAllLeds();
  g_saveQueued = false;
  if (!saveSettings()) {
    message = "LED settings applied but could not be saved to flash.";
    return false;
  }
  message = "LED settings saved.";
  return true;
}

// Simulates a button action: presses=1-3 (or single/double/triple) or presses=hold.
bool press(const ArgLookup& arg, String& message) {
  String value;
  int button = arg("button", value) ? value.toInt() : 0;
  if (button < 1 || button > kButtonCount) {
    message = "Button number must be 1-3.";
    return false;
  }
  uint8_t mode = 1;
  if (arg("presses", value) && (!parseMode(value, mode) || mode == 0)) {
    message = "Presses must be 1-3, single, double, triple or hold.";
    return false;
  }

  selectMode(static_cast<uint8_t>(button - 1), mode);
  message = g_settings.mode[button - 1] ? "LED on." : "LED off.";
  return true;
}

bool setBrightness(const ArgLookup& arg, String& message) {
  String value;
  uint8_t brightness;
  if (!arg("brightness", value) || !parseBrightness(value, brightness)) {
    message = "Brightness must be 0-255.";
    return false;
  }

  g_settings.brightness = brightness;
  applyAllLeds();
  queueSave();
  message = "Brightness set.";
  return true;
}

String stateJson(bool live) {
  uint32_t now = millis();
  String body = "{";
  if (live) body += "\"uptimeMs\":" + String(now) + ",";
  body += "\"buttons\":[";
  for (uint8_t i = 0; i < kButtonCount; i++) {
    const ButtonState& b = g_buttonStates[i];
    if (i > 0) body += ",";
    body += String("{\"pressed\":") + (b.pressed ? "true" : "false") +
            ",\"pressCount\":" + String(b.pressCount) +
            ",\"lastAction\":\"" + (b.actionCount ? kModeNames[b.lastAction] : "none") + "\"" +
            ",\"actionCount\":" + String(b.actionCount);
    if (live) body += ",\"lastActionMsAgo\":" + (b.actionCount ? String(now - b.lastActionAt) : String("null"));
    body += "}";
  }
  body += "],\"leds\":[";
  for (uint8_t i = 0; i < kButtonCount; i++) {
    const LedConfig& led = g_settings.leds[i];
    if (i > 0) body += ",";
    body += "{\"mode\":" + String(g_settings.mode[i]) + ",\"state\":\"" + kModeNames[g_settings.mode[i]] +
            "\",\"colors\":[";
    for (uint8_t m = 0; m < kColorModes; m++) {
      if (m > 0) body += ",";
      body += "\"" + colorToHex(led.colors[m]) + "\"";
    }
    body += "],\"offColor\":\"" + colorToHex(led.off) + "\"}";
  }
  body += "],\"brightness\":" + String(g_settings.brightness) +
          ",\"oldSwitchPattern\":" + (g_settings.oldSwitchPattern ? "true" : "false") +
          ",\"multiPressMs\":" + String(kMultiPressWindowMs) + ",\"holdMs\":" + String(kHoldMs) + "}";
  return body;
}

uint32_t stateVersion() { return g_stateVersion; }

String capabilitiesJson() {
  String j = "{\"buttons\":" + String(kButtonCount) + ",\"leds\":" + String(kButtonCount) +
             ",\"ledType\":\"rgb\",\"brightness\":true,\"actions\":[";
  for (uint8_t m = 1; m <= kColorModes; m++) {
    if (m > 1) j += ",";
    j += "\"" + String(kModeNames[m]) + "\"";
  }
  j += "],\"ledStates\":[";
  for (uint8_t m = 0; m <= kColorModes; m++) {
    if (m > 0) j += ",";
    j += "\"" + String(kModeNames[m]) + "\"";
  }
  j += "],\"multiPressMs\":" + String(kMultiPressWindowMs) + ",\"holdMs\":" + String(kHoldMs) + "}";
  return j;
}

void onAction(ActionHandler handler) { g_actionHandler = handler; }

}  // namespace Controller
