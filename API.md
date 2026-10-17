# Logix 3 HTTP API

All endpoints are on port 80 of the device (e.g. `http://192.168.22.235` or `http://<hostname>.local`).

- **Requests:** `POST` parameters are sent form-encoded (`application/x-www-form-urlencoded`) or as a query string.
- **Responses:** JSON.
- **Colors:** `#RRGGBB`. URL-encode the `#` as `%23`.
- **Authentication:** none by default. Once an admin password is set on the Device Settings page, every endpoint requires HTTP Basic auth with user `admin` (for curl, add `-u admin:<password>`).

Actions return `{"ok":true,"message":"..."}` with HTTP 200. Errors return `{"ok":false,"message":"..."}` with HTTP 400.

# Identifying the device

Every Logix device identifies itself in the same three ways, so one integration can support several models. Use `model` to choose model-specific behaviour, and use `id` (the MAC address) as the unique ID.

## mDNS / zeroconf

The device advertises `_logix._tcp.local.` on port 80, with these TXT records:

| Key | Example | Meaning |
|---|---|---|
| `model` | `logix3` | machine-readable model; stays the same across firmware versions |
| `id` | `48e72971e527` | lowercase MAC without separators; stable unique ID |
| `fw` | `1.2.0` | firmware version |
| `api` | `1` | API contract version; increases only on breaking changes |
| `manufacturer` | `Logix` | |

For Home Assistant, add `"zeroconf": ["_logix._tcp.local."]` to `manifest.json` and read `discovery_info.properties["model"]` in `async_step_zeroconf`. The device also still advertises `_http._tcp`.

## GET /api/info

This endpoint never requires a password, even when an admin password is set, so discovery can always read the model. `authRequired` says whether the other endpoints need credentials.

```json
{
  "manufacturer": "Logix", "model": "logix3", "name": "Logix 3",
  "id": "48e72971e527", "mac": "48:E7:29:71:E5:27", "hostname": "esp12f-e527", "ip": "192.168.22.235",
  "fw": "1.2.0", "api": 1, "authRequired": false,
  "capabilities": {
    "buttons": 3, "leds": 3, "ledType": "rgb", "brightness": true,
    "actions": ["single", "double", "triple", "hold"],
    "ledStates": ["off", "single", "double", "triple", "hold"],
    "multiPressMs": 400, "holdMs": 3000
  },
  "mqtt": {"enabled": true, "connected": true, "topic": "logix3/48e72971e527"}
}
```

The fields up to `authRequired` are the same on every Logix model. `capabilities` and `mqtt` depend on the model.

## MQTT discovery

When MQTT is enabled, the device publishes the same JSON, retained, to `logix/discovery/<id>`. An integration can subscribe to `logix/discovery/#` (in Home Assistant, `"mqtt": ["logix/discovery/#"]` in `manifest.json`) and read the device's command topic from `mqtt.topic`.

## LED states

Each LED is always in one of five states. The state names, or their numbers, are used everywhere a state or action is given.

| State | Number | LED shows |
|---|---|---|
| `off` | 0 | its off color |
| `single` | 1 | its single press color |
| `double` | 2 | its double press color |
| `triple` | 3 | its triple press color |
| `hold` | 4 | its hold (3 s) color |

Button and LED numbers are 1-3, top to bottom.

## Read everything

`GET /api/controller`

```json
{
  "uptimeMs": 31946,
  "buttons": [
    {"pressed": false, "pressCount": 4, "lastAction": "double", "actionCount": 2, "lastActionMsAgo": 5120}
  ],
  "leds": [
    {"mode": 2, "state": "double", "colors": ["#ff4040", "#ffa000", "#ffffff", "#8000ff"], "offColor": "#000000"}
  ],
  "brightness": 255,
  "multiPressMs": 400,
  "holdMs": 3000
}
```

The example shows one entry in each array; the real response always has three, one per button and LED.

`buttons[n]`:
- `pressed`: the button is held down right now.
- `pressCount`: raw presses since boot.
- `lastAction`: the last completed action: `single`, `double`, `triple`, `hold`, or `none` if there hasn't been one since boot.
- `actionCount`: completed actions since boot. When polling, a change in this value means a new action happened.
- `lastActionMsAgo`: milliseconds since the last action, or `null` if there hasn't been one since boot.

Actions triggered through `/api/controller/press` are counted too.

`leds[n]`:
- `state` and `mode`: the current state, as a name and as a number.
- `colors`: the single, double, triple and hold colors, in that order.
- `offColor`: the color shown when the LED is off.

## Set colors, LED states, brightness

`POST /api/controller`

Every parameter is optional; send only the ones you want to change. Changes apply immediately and are saved to flash.

| Parameter | Value |
|---|---|
| `led1Color1` … `led3Color4` | color for single (1), double (2), triple (3), hold (4) |
| `led1OffColor` … `led3OffColor` | off color |
| `led1State` … `led3State` | `off`, `single`, `double`, `triple`, `hold` (or 0-4) |
| `brightness` | 0-255, applies to all LEDs |

Setting a state does not toggle: setting `led2State=double` on an LED that is already `double` leaves it on.

```sh
# LED 1 hold color to orange
curl -X POST -d "led1Color4=%23ff8000" http://192.168.22.235/api/controller

# LED 2 on in its double press color, LED 3 off, 50% brightness
curl -X POST -d "led2State=double&led3State=off&brightness=128" http://192.168.22.235/api/controller
```

## Simulate a button action

`POST /api/controller/press`

| Parameter | Value |
|---|---|
| `button` | 1-3 |
| `presses` | `1`-`3`, `single`, `double`, `triple` or `hold` (default `1`) |

This behaves exactly like the physical button. The LED switches to that action's color, or turns off if it was already showing it. The action also appears in the button's `lastAction` and `actionCount`.

```sh
curl -X POST -d "button=3&presses=hold" http://192.168.22.235/api/controller/press
# {"ok":true,"message":"LED on."}
```

## Brightness only

`POST /api/controller/brightness` with `brightness=0-255`.

Use this for frequent changes, such as from a slider. It is saved to flash 2 s after the last change, instead of on every call.

## Device

| Endpoint | Purpose |
|---|---|
| `GET /api/info` | device identity and capabilities (no password required); see above |
| `GET /api/status` | firmware version, network, uptime, heap |
| `GET /api/mqtt` | MQTT settings and connection status (the password is never returned) |
| `POST /api/mqtt` | save MQTT settings: `host` (blank disables MQTT), `port`, `user`, `pass` (blank keeps the saved one), `passClear=1`, `topic` |
| `POST /api/reboot` | restart the device |
| `POST /update` | firmware upload (multipart field `firmware`) |

# MQTT

Set the broker on the Device Settings page, in the **MQTT** card. The device connects as client ID `<hostname>` (e.g. `esp12f-e527`) and retries every 10 s if the broker is unreachable. Every topic below sits under the base topic `T`. The default is `<model>/<id>` (e.g. `logix3/48e72971e527`), so several devices never share topics. All messages are QoS 0.

The MQTT commands take the same parameters as the HTTP endpoints above and behave the same way.

## Published by the device

| Topic | Retained | Payload |
|---|---|---|
| `T/status` | yes | `online`, or `offline` (last will, also sent on a clean disconnect) |
| `T/state` | yes | same JSON as `GET /api/controller`, without `uptimeMs` and `lastActionMsAgo`; republished on every change (at most every 100 ms) |
| `T/button/N/action` | no | `single`, `double`, `triple` or `hold`: one message per completed button action, physical or simulated |
| `T/result` | no | reply to each command: `{"command":"set","ok":true,"message":"LED settings saved."}` |

## Commands

| Topic | Payload | HTTP equivalent |
|---|---|---|
| `T/set` | JSON object with any of the `POST /api/controller` keys | `POST /api/controller` |
| `T/press` | `{"button":1-3,"presses":"single"\|"double"\|"triple"\|"hold"}` | `POST /api/controller/press` |
| `T/brightness/set` | `0`-`255` as plain text | `POST /api/controller/brightness` |
| `T/get` | anything | republishes `T/state` |

```sh
mosquitto_pub -h <broker> -t logix3/48e72971e527/set -m '{"led2State":"double","led3Color4":"#ff8000","brightness":128}'
mosquitto_pub -h <broker> -t logix3/48e72971e527/press -m '{"button":3,"presses":"hold"}'
mosquitto_pub -h <broker> -t logix3/48e72971e527/brightness/set -m 200
mosquitto_sub -h <broker> -t 'logix3/48e72971e527/#' -v
```
