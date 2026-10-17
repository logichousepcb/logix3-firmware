#pragma once

#include <Arduino.h>

// %TITLE% is substituted at request time with BaseConfig::title.
// %VIEW% is "main" (controller only) or "settings" (status, WiFi, firmware, device, maintenance).
static const char BASE_PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%TITLE%</title>
<style>
:root{--bg:#f3f5f8;--card:#fff;--fg:#1d2430;--muted:#667085;--line:#d9dee7;--accent:#1f6feb;--ok:#1a7f37;--bad:#cf222e;--warn:#9a6700}
@media (prefers-color-scheme:dark){:root{--bg:#0f1419;--card:#181f27;--fg:#e6edf3;--muted:#8b98a5;--line:#2b3440;--accent:#4493f8;--ok:#3fb950;--bad:#f85149;--warn:#d29922}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{background:var(--card);border-bottom:1px solid var(--line);padding:14px 16px}
header .wrap{display:flex;flex-wrap:wrap;align-items:center;gap:8px 12px}
h1{font-size:19px;margin:0;flex:1 1 auto}
main.wrap{padding:16px}
.wrap{max-width:760px;margin:0 auto}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:16px}
h2{font-size:15px;margin:0 0 12px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted)}
.pill{font-size:12px;padding:2px 9px;border-radius:999px;border:1px solid var(--line);white-space:nowrap}
.pill.ok{color:var(--ok);border-color:var(--ok)}.pill.warn{color:var(--warn);border-color:var(--warn)}
dl{display:grid;grid-template-columns:max-content 1fr;gap:6px 16px;margin:0}
dt{color:var(--muted)}dd{margin:0;word-break:break-all}
label{display:block;font-size:13px;color:var(--muted);margin:10px 0 4px}
input[type=text],input[type=password],input[type=number]{width:100%;padding:9px 10px;border:1px solid var(--line);border-radius:7px;background:var(--bg);color:var(--fg);font:inherit}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:12px}
button,.filebtn{font:inherit;padding:8px 14px;border-radius:7px;border:1px solid var(--line);background:var(--bg);color:var(--fg);cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.danger{color:var(--bad);border-color:var(--bad)}
button:disabled{opacity:.5;cursor:default}
.nets{list-style:none;margin:10px 0 0;padding:0;border:1px solid var(--line);border-radius:7px;max-height:220px;overflow:auto}
.nets:empty{display:none}
.nets li{display:flex;justify-content:space-between;gap:8px;padding:8px 10px;border-bottom:1px solid var(--line);cursor:pointer}
.nets li:last-child{border-bottom:0}.nets li:hover{background:var(--bg)}
.nets span.s{color:var(--muted);font-size:13px;white-space:nowrap}
.msg{margin-top:10px;font-size:14px;min-height:1em}
.msg.ok{color:var(--ok)}.msg.bad{color:var(--bad)}
.bar{height:10px;border-radius:5px;background:var(--bg);border:1px solid var(--line);overflow:hidden;margin-top:12px;display:none}
.bar>div{height:100%;width:0;background:var(--accent);transition:width .2s}
.hint{font-size:13px;color:var(--muted);margin:6px 0 0}
input[type=file]{max-width:100%}
.chk{display:flex;align-items:center;gap:6px;font-size:13px;color:var(--muted);margin-top:8px}
.controller-wrap{display:flex;gap:20px;flex-wrap:wrap;align-items:flex-start}
.controller-svg{width:270px;max-width:100%;height:auto;flex:0 0 auto}
.controller-panel{flex:1 1 230px}
.device-button{cursor:pointer;outline:none;touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
.key{transition:transform .1s;transform-box:fill-box;transform-origin:center}
.device-button.pressed .key,.device-button:active .key{transform:translateY(3px) scale(.975)}
.device-button:focus-visible .rim{stroke:#2089ff;stroke-width:3}
.led-fill,.halo{fill:var(--led,#098cff);transition:fill .3s,opacity .3s}
.halo{opacity:.48}
.device-button.off .halo{opacity:0}
.device-button.off .led-fill{fill:#bcc6cd}
.device-button.dim .halo{opacity:.15}
.led-grid{display:grid;grid-template-columns:1fr repeat(5,40px);gap:10px 6px;align-items:center;margin:14px 0}
.led-grid label{margin:0;font-size:14px;color:var(--fg)}
.led-grid .col{font-size:12px;line-height:1.2;color:var(--muted);text-align:center}
.led-grid input[type=color]{width:40px;height:34px;padding:0;border:1px solid var(--line);border-radius:7px;background:var(--bg)}
.bright-row{display:flex;align-items:center;gap:12px;margin:14px 0}
.bright-row label{margin:0;font-size:14px;color:var(--fg)}
.bright-row input[type=range]{flex:1 1 auto;min-width:0;accent-color:var(--accent)}
.bright-row output{font-size:14px;min-width:3.2em;text-align:right;font-variant-numeric:tabular-nums}
.navlink{font-size:14px;color:var(--accent);text-decoration:none;white-space:nowrap}
.navlink:hover{text-decoration:underline}
body[data-view=main] .settings-only,body[data-view=settings] .main-only{display:none}
</style>
</head>
<body data-view="%VIEW%">
<header><div class="wrap">
  <h1>%TITLE%</h1>
  <span class="pill" id="ver">v-</span>
  <span class="pill" id="mode">-</span>
  <a class="navlink main-only" href="/settings">Device Settings &rsaquo;</a>
  <a class="navlink settings-only" href="/">&lsaquo; Controller</a>
</div></header>
<main class="wrap">

<section class="card settings-only">
  <h2>Status</h2>
  <dl id="status"><dt>Loading</dt><dd>…</dd></dl>
</section>

<section class="card main-only">
  <h2>Controller</h2>
  <div class="controller-wrap">
    <svg class="controller-svg" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 470" role="img" aria-labelledby="controllerTitle">
      <title id="controllerTitle">Logix 3 controller</title>
      <defs>
        <linearGradient id="plate" x2="1" y2="1"><stop stop-color="#fff"/><stop offset=".5" stop-color="#f2f1ed"/><stop offset="1" stop-color="#d5d4cf"/></linearGradient>
        <linearGradient id="insert" x2="1" y2="1"><stop stop-color="#fafaf8"/><stop offset="1" stop-color="#e5e5e0"/></linearGradient>
        <linearGradient id="lens" x2="0" y2="1"><stop stop-color="white" stop-opacity=".65"/><stop offset=".5" stop-color="white" stop-opacity=".08"/><stop offset="1" stop-color="black" stop-opacity=".18"/></linearGradient>
        <radialGradient id="shine"><stop stop-color="white" stop-opacity=".7"/><stop offset="1" stop-color="white" stop-opacity="0"/></radialGradient>
        <filter id="shadow" x="-30%" y="-30%" width="160%" height="160%"><feDropShadow dx="0" dy="5" stdDeviation="4" flood-opacity=".2"/></filter>
        <filter id="glow" x="-60%" y="-100%" width="220%" height="300%"><feGaussianBlur stdDeviation="7"/></filter>
      </defs>
      <rect x="20" y="12" width="260" height="442" rx="13" fill="url(#plate)" stroke="#c9cac6" filter="url(#shadow)"/>
      <rect x="28" y="20" width="244" height="425" rx="10" fill="none" stroke="white" stroke-opacity=".6"/>
      <rect x="83" y="104" width="134" height="265" rx="8" fill="#9a9c97"/>
      <rect x="85" y="106" width="130" height="261" rx="7" fill="url(#insert)" stroke="white"/>
      <g transform="translate(150 47)"><circle r="12" fill="#c3c5bf" stroke="#a7aaa3"/><path d="M-8 1H8" stroke="#656b64" stroke-width="2.6"/><path d="M-8 -1H8" stroke="white" stroke-opacity=".65"/></g>
      <g transform="translate(150 419)"><circle r="12" fill="#c3c5bf" stroke="#a7aaa3"/><path d="M-8 1H8" stroke="#656b64" stroke-width="2.6"/><path d="M-8 -1H8" stroke="white" stroke-opacity=".65"/></g>
      <g id="button-1" class="device-button" role="button" tabindex="0" aria-label="Button 1" aria-pressed="false" style="--led:#098cff" data-button="1">
        <rect x="98" y="164" width="104" height="37" rx="5" fill="#a2a6a5"/>
        <g class="key"><rect class="halo" x="101" y="160" width="98" height="32" rx="6" filter="url(#glow)"/>
          <rect class="rim" x="99" y="160" width="102" height="34" rx="5" fill="#d1dce4" stroke="#8da7bc"/>
          <rect id="led-1" class="led-fill" x="102" y="162" width="96" height="29" rx="4"/>
          <rect x="102" y="162" width="96" height="29" rx="4" fill="url(#lens)"/>
          <ellipse cx="150" cy="175" rx="20" ry="13" fill="url(#shine)"/>
          <path d="M108 165H192" stroke="white" stroke-opacity=".55" stroke-linecap="round"/>
        </g>
      </g>
      <g id="button-2" class="device-button" role="button" tabindex="0" aria-label="Button 2" aria-pressed="false" style="--led:#098cff" data-button="2">
        <rect x="98" y="226" width="104" height="37" rx="5" fill="#a2a6a5"/>
        <g class="key"><rect class="halo" x="101" y="222" width="98" height="32" rx="6" filter="url(#glow)"/>
          <rect class="rim" x="99" y="222" width="102" height="34" rx="5" fill="#d1dce4" stroke="#8da7bc"/>
          <rect id="led-2" class="led-fill" x="102" y="224" width="96" height="29" rx="4"/>
          <rect x="102" y="224" width="96" height="29" rx="4" fill="url(#lens)"/>
          <ellipse cx="150" cy="237" rx="20" ry="13" fill="url(#shine)"/>
          <path d="M108 227H192" stroke="white" stroke-opacity=".55" stroke-linecap="round"/>
        </g>
      </g>
      <g id="button-3" class="device-button" role="button" tabindex="0" aria-label="Button 3" aria-pressed="false" style="--led:#098cff" data-button="3">
        <rect x="98" y="288" width="104" height="37" rx="5" fill="#a2a6a5"/>
        <g class="key"><rect class="halo" x="101" y="284" width="98" height="32" rx="6" filter="url(#glow)"/>
          <rect class="rim" x="99" y="284" width="102" height="34" rx="5" fill="#d1dce4" stroke="#8da7bc"/>
          <rect id="led-3" class="led-fill" x="102" y="286" width="96" height="29" rx="4"/>
          <rect x="102" y="286" width="96" height="29" rx="4" fill="url(#lens)"/>
          <ellipse cx="150" cy="299" rx="20" ry="13" fill="url(#shine)"/>
          <path d="M108 289H192" stroke="white" stroke-opacity=".55" stroke-linecap="round"/>
        </g>
      </g>
      <circle cx="194" cy="327" r="2.4" fill="#555b59"/>
      <circle cx="194" cy="337" r="2.4" fill="#555b59"/>
    </svg>
    <div class="controller-panel">
      <p class="hint" style="margin-top:0">Press a button (on screen or on the device) once, twice or three times quickly, or hold it for 3 seconds, to light its LED in that color. Repeat the same action to turn it off. LED states are restored after a power cut. Brightness applies immediately; colors are saved when you click Save.</p>
      <div class="bright-row">
        <label for="brightness">Brightness</label>
        <input id="brightness" type="range" min="0" max="255" step="1" value="255">
        <output id="brightnessVal" for="brightness">100%</output>
      </div>
      <form id="controllerForm">
        <div class="led-grid" id="ledGrid">
          <span></span><span class="col">Single press</span><span class="col">Double press</span><span class="col">Triple press</span><span class="col">Hold 3&nbsp;s</span><span class="col">Off</span>
        </div>
        <div class="row">
          <button class="primary" type="submit">Save LED colors</button>
        </div>
        <div class="msg" id="controllerMsg"></div>
      </form>
    </div>
  </div>
</section>

<section class="card settings-only">
  <h2>WiFi Setup</h2>
  <div class="row" style="margin-top:0">
    <button id="scanBtn" type="button">Scan for networks</button>
    <span class="hint" id="scanMsg"></span>
  </div>
  <ul class="nets" id="nets"></ul>
  <form id="wifiForm">
    <label for="ssid">Network name (SSID)</label>
    <input type="text" id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
    <label for="pass">Password</label>
    <input type="password" id="pass" name="pass" maxlength="63" autocomplete="off">
    <label class="chk"><input type="checkbox" id="showPass"> Show password</label>
    <div class="row"><button class="primary" type="submit">Save &amp; Connect</button></div>
    <div class="msg" id="wifiMsg"></div>
  </form>
</section>

<section class="card settings-only">
  <h2>MQTT <span class="pill" id="mqttState" style="text-transform:none;letter-spacing:0;margin-left:6px">-</span></h2>
  <form id="mqttForm">
    <label for="mqttHost">Broker IP or hostname (leave blank to disable MQTT)</label>
    <input type="text" id="mqttHost" maxlength="63" autocomplete="off" placeholder="e.g. 192.168.1.10">
    <label for="mqttPort">Port</label>
    <input type="number" id="mqttPort" min="1" max="65535" placeholder="1883">
    <label for="mqttUser">Username</label>
    <input type="text" id="mqttUser" maxlength="63" autocomplete="off" placeholder="optional">
    <label for="mqttPass">Password</label>
    <input type="password" id="mqttPass" maxlength="63" autocomplete="new-password" placeholder="leave blank to keep current">
    <label class="chk"><input type="checkbox" id="mqttPassClear"> Remove saved password</label>
    <label for="mqttTopic">Base topic</label>
    <input type="text" id="mqttTopic" maxlength="63" autocomplete="off" placeholder="logix3">
    <p class="hint" id="mqttHint"></p>
    <div class="row"><button class="primary" type="submit">Save MQTT settings</button></div>
    <div class="msg" id="mqttMsg"></div>
  </form>
</section>

<section class="card settings-only">
  <h2>Firmware Update</h2>
  <p class="hint" style="margin:0 0 10px">Upload an ESP8266 application image (<code>firmware.bin</code> from <code>.pio/build/&lt;env&gt;/</code>) built for the same ESP-12F target. Do not power off while the upload runs.</p>
  <form id="fwForm">
    <input type="file" id="fwFile" accept=".bin,application/octet-stream" required>
    <div class="row"><button class="primary" type="submit" id="fwBtn">Upload &amp; Install</button></div>
    <div class="bar" id="fwBar"><div></div></div>
    <div class="msg" id="fwMsg"></div>
  </form>
</section>

<section class="card settings-only">
  <h2>Hostname &amp; Security</h2>
  <form id="devForm">
    <label for="hostname">Hostname (reachable as <i>hostname</i>.local)</label>
    <input type="text" id="hostname" name="hostname" maxlength="32" autocomplete="off">
    <label for="admin">New admin password (user: <b>admin</b>)</label>
    <input type="password" id="admin" name="admin" maxlength="64" autocomplete="new-password" placeholder="leave blank to keep current">
    <label class="chk"><input type="checkbox" id="adminClear" name="adminClear" value="1"> Remove admin password</label>
    <p class="hint" id="authHint"></p>
    <div class="row"><button class="primary" type="submit">Save</button></div>
    <div class="msg" id="devMsg"></div>
  </form>
</section>

<section class="card settings-only">
  <h2>Maintenance</h2>
  <div class="row" style="margin-top:0">
    <button type="button" id="rebootBtn">Reboot</button>
    <button type="button" class="danger" id="forgetBtn">Forget WiFi</button>
  </div>
  <p class="hint">Use <b>Forget WiFi</b> to erase saved credentials. Holding the FLASH/BOOT button (GPIO0) for 5 seconds also erases WiFi credentials.</p>
  <div class="msg" id="mntMsg"></div>
</section>

</main>
<script>
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let rebooting = false;
const controllerButtons = [1, 2, 3].map(n => $('button-' + n));

function setMsg(el, text, ok) { el.textContent = text; el.className = 'msg' + (ok === undefined ? '' : ok ? ' ok' : ' bad'); }
// color is the lit color, or null when the LED is showing its off color.
function setLED(n, color, offColor) {
  const el = controllerButtons[n - 1];
  if (!el) return;
  // An off color of black means the LED is dark, so draw the unlit lens.
  const dark = !color && offColor.toLowerCase() === '#000000';
  el.classList.toggle('off', dark);
  el.classList.toggle('dim', !color && !dark);
  if (!dark) el.style.setProperty('--led', color || offColor);
}
function setPressed(n, pressed) {
  const el = controllerButtons[n - 1];
  if (!el) return;
  el.classList.toggle('pressed', !!pressed);
  el.setAttribute('aria-pressed', String(!!pressed));
}

function fmtBytes(n) { return n >= 1048576 ? (n / 1048576).toFixed(2) + ' MB' : (n / 1024).toFixed(1) + ' KB'; }
function fmtUptime(s) {
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  return (d ? d + 'd ' : '') + h + 'h ' + m + 'm ' + (s % 60) + 's';
}

async function loadStatus() {
  if (rebooting) return;
  try {
    const r = await fetch('/api/status', {cache: 'no-store'});
    if (!r.ok) return;
    const s = await r.json();
    $('ver').textContent = 'v' + s.version;
    const m = $('mode');
    m.textContent = s.mode === 'portal' ? 'Setup AP' : (s.connected ? 'Connected' : 'Reconnecting');
    m.className = 'pill ' + (s.mode === 'portal' ? 'warn' : s.connected ? 'ok' : 'warn');
    const rows = [];
    if (s.mode === 'portal') {
      rows.push(['Setup AP', s.apSsid + ' (' + s.apIp + ')'], ['AP clients', s.apClients]);
      rows.push(['Saved network', s.savedSsid || '(none)']);
    } else {
      rows.push(['Network', s.savedSsid], ['IP address', s.ip || '-'], ['Signal', s.connected ? s.rssi + ' dBm' : '-']);
    }
    rows.push(['Hostname', s.hostname + '.local'], ['MAC', s.mac], ['Uptime', fmtUptime(s.uptime)],
      ['Firmware', 'v' + s.version + ' (' + s.build + ')'],
      ['Sketch / update space', fmtBytes(s.sketch) + ' / ' + fmtBytes(s.otaSpace)],
      ['Chip', s.chip + ', ' + fmtBytes(s.flash) + ' flash'], ['Flash mode', s.flashMode],
      ['Free heap', fmtBytes(s.heap)], ['SDK', s.sdk]);
    $('status').innerHTML = rows.map(([k, v]) => '<dt>' + esc(k) + '</dt><dd>' + esc(v) + '</dd>').join('');
    if (document.activeElement !== $('hostname') && !$('hostname').dataset.touched) $('hostname').value = s.hostname;
    $('authHint').textContent = s.authEnabled ? 'An admin password is set; this page requires login.'
      : 'No admin password set: anyone on this network can change settings or upload firmware.';
    if (!$('ssid').value && s.savedSsid) $('ssid').value = s.savedSsid;
  } catch (e) {}
}

async function post(url, data) {
  const r = await fetch(url, {method: 'POST', body: new URLSearchParams(data)});
  let j; try { j = await r.json(); } catch (e) { j = {ok: false, message: 'HTTP ' + r.status}; }
  return j;
}

function waitForReboot(msgEl, text) {
  rebooting = true;
  setMsg(msgEl, text, true);
  let tries = 0;
  setTimeout(function poll() {
    tries++;
    fetch('/api/status', {cache: 'no-store'}).then(r => { if (r.ok) location.reload(); else throw 0; })
      .catch(() => {
        if (tries < 40) setTimeout(poll, 2000);
        else setMsg(msgEl, text + ' The device did not come back at this address. It may have joined your network; look for it at its hostname.local or in your router.', true);
      });
  }, 4000);
}

async function handleResult(j, msgEl) {
  setMsg(msgEl, j.message, j.ok);
  if (j.ok && j.restart) waitForReboot(msgEl, j.message);
}

async function loadController() {
  if (rebooting) return;
  try {
    const r = await fetch('/api/controller', {cache: 'no-store'});
    if (!r.ok) return;
    const j = await r.json();
    j.buttons.forEach((button, i) => {
      setPressed(i + 1, button.pressed);
    });
    if (j.multiPressMs) multiPressMs = j.multiPressMs;
    if (j.holdMs) holdMs = j.holdMs;
    j.leds.forEach((led, i) => {
      const n = i + 1;
      // Leave the pickers alone while the user has unsaved edits.
      if (!colorsDirty) {
        led.colors.forEach((c, m) => { $('ledColor' + n + '_' + (m + 1)).value = c; });
        $('ledOffColor' + n).value = led.offColor;
      }
      setLED(n, led.mode > 0 ? led.colors[led.mode - 1] : null, led.offColor);
    });
    if (typeof j.brightness === 'number' && Date.now() > brightnessHoldUntil && document.activeElement !== $('brightness')) {
      $('brightness').value = j.brightness;
      showBrightness();
    }
  } catch (e) {}
}

// Brightness slider: send while dragging (throttled) and always send the final value.
let brightnessHoldUntil = 0, brightnessTimer = null, brightnessSending = false, brightnessPending = false;
function showBrightness() { $('brightnessVal').textContent = Math.round($('brightness').value / 255 * 100) + '%'; }
async function sendBrightness() {
  if (brightnessSending) { brightnessPending = true; return; }
  brightnessSending = true;
  try {
    const j = await post('/api/controller/brightness', {brightness: $('brightness').value});
    if (!j.ok) setMsg($('controllerMsg'), j.message, false);
  } catch (e) {
    setMsg($('controllerMsg'), 'Request failed.', false);
  }
  brightnessSending = false;
  if (brightnessPending) { brightnessPending = false; sendBrightness(); }
}
$('brightness').addEventListener('input', () => {
  brightnessHoldUntil = Date.now() + 1500;
  showBrightness();
  if (!brightnessTimer) brightnessTimer = setTimeout(() => { brightnessTimer = null; sendBrightness(); }, 120);
});
$('brightness').addEventListener('change', () => {
  brightnessHoldUntil = Date.now() + 1500;
  if (brightnessTimer) { clearTimeout(brightnessTimer); brightnessTimer = null; }
  sendBrightness();
});

// LED color pickers: one row per LED with single/double/triple press, hold and off colors.
let colorsDirty = false;
const pressNames = ['single press', 'double press', 'triple press', 'hold'];
[1, 2, 3].forEach(n => {
  const grid = $('ledGrid');
  const label = document.createElement('label');
  label.htmlFor = 'ledColor' + n + '_1';
  label.textContent = 'LED ' + n;
  grid.appendChild(label);
  const pickers = pressNames.map((name, m) => ['ledColor' + n + '_' + (m + 1), 'LED ' + n + ' ' + name + ' color'])
    .concat([['ledOffColor' + n, 'LED ' + n + ' off color']]);
  pickers.forEach(([id, aria]) => {
    const input = document.createElement('input');
    input.type = 'color';
    input.id = id;
    input.setAttribute('aria-label', aria);
    grid.appendChild(input);
  });
});
$('controllerForm').addEventListener('input', () => { colorsDirty = true; });

// Count quick taps and long holds on an on-screen button the same way the hardware does.
let multiPressMs = 400, holdMs = 3000;
const tapCounts = [0, 0, 0], tapTimers = [null, null, null], holdTimers = [null, null, null], holdFired = [false, false, false];
async function sendPress(button, presses) {
  try {
    const j = await post('/api/controller/press', {button: String(button), presses: String(presses)});
    if (!j.ok) setMsg($('controllerMsg'), j.message, false);
    loadController();
  } catch (e) {
    setMsg($('controllerMsg'), 'Request failed.', false);
  }
}
function controllerTap(button) {
  const i = button - 1;
  clearTimeout(tapTimers[i]);
  tapCounts[i] = Math.min(tapCounts[i] + 1, 3);
  const finish = () => { const c = tapCounts[i]; tapCounts[i] = 0; tapTimers[i] = null; sendPress(button, c); };
  if (tapCounts[i] >= 3) finish();
  else tapTimers[i] = setTimeout(finish, multiPressMs);
}
function controllerDown(button) {
  const i = button - 1;
  holdFired[i] = false;
  clearTimeout(holdTimers[i]);
  holdTimers[i] = setTimeout(() => {
    // A hold replaces any taps collected so far.
    holdTimers[i] = null;
    holdFired[i] = true;
    clearTimeout(tapTimers[i]);
    tapTimers[i] = null;
    tapCounts[i] = 0;
    sendPress(button, 'hold');
  }, holdMs);
}
function controllerUp(button, cancelled) {
  const i = button - 1;
  clearTimeout(holdTimers[i]);
  holdTimers[i] = null;
  if (!cancelled && !holdFired[i]) controllerTap(button);
  holdFired[i] = false;
}

function bindControllerButtons() {
  controllerButtons.forEach((el, i) => {
    const n = i + 1;
    function press(v) {
      setPressed(n, v);
      el.dispatchEvent(new CustomEvent('controller-button', {bubbles: true, detail: {button: n, pressed: v}}));
    }
    let down = false;
    function start() { if (down) return; down = true; press(true); controllerDown(n); }
    function end(cancelled) { if (!down) return; down = false; press(false); controllerUp(n, cancelled); }
    el.addEventListener('pointerdown', e => { el.setPointerCapture(e.pointerId); start(); });
    el.addEventListener('pointerup', () => end(false));
    el.addEventListener('pointercancel', () => end(true));
    el.addEventListener('lostpointercapture', () => end(true));
    el.addEventListener('contextmenu', e => e.preventDefault());
    el.addEventListener('keydown', e => {
      if ([' ', 'Enter'].includes(e.key)) {
        e.preventDefault();
        if (!e.repeat) start();
      }
    });
    el.addEventListener('keyup', e => {
      if ([' ', 'Enter'].includes(e.key)) {
        e.preventDefault();
        end(false);
      }
    });
    el.addEventListener('blur', () => end(true));
  });
}

$('controllerForm').onsubmit = async e => {
  e.preventDefault();
  const data = {};
  [1, 2, 3].forEach(n => {
    [1, 2, 3, 4].forEach(m => { data['led' + n + 'Color' + m] = $('ledColor' + n + '_' + m).value; });
    data['led' + n + 'OffColor'] = $('ledOffColor' + n).value;
  });
  setMsg($('controllerMsg'), 'Saving LED colors…');
  try {
    const j = await post('/api/controller', data);
    await handleResult(j, $('controllerMsg'));
    if (j.ok) { colorsDirty = false; loadController(); }
  } catch (e) {
    setMsg($('controllerMsg'), 'Request failed.', false);
  }
};

// WiFi scan
$('scanBtn').onclick = async () => {
  const btn = $('scanBtn'); btn.disabled = true; $('scanMsg').textContent = 'Scanning…'; $('nets').innerHTML = '';
  let url = '/api/scan?fresh=1';
  for (let i = 0; i < 20; i++) {
    try {
      const j = await (await fetch(url, {cache: 'no-store'})).json();
      if (!j.scanning) {
        const nets = j.networks.sort((a, b) => b.rssi - a.rssi);
        $('nets').innerHTML = nets.map(n => '<li data-ssid="' + esc(n.ssid) + '"><span>' + (n.secure ? '&#128274; ' : '') + esc(n.ssid) +
          '</span><span class="s">' + n.rssi + ' dBm</span></li>').join('');
        $('scanMsg').textContent = nets.length ? nets.length + ' network(s) found. Tap one to select.' : 'No networks found.';
        btn.disabled = false; return;
      }
    } catch (e) {}
    url = '/api/scan';
    await new Promise(r => setTimeout(r, 1500));
  }
  $('scanMsg').textContent = 'Scan timed out.'; btn.disabled = false;
};
$('nets').onclick = e => {
  const li = e.target.closest('li'); if (!li) return;
  $('ssid').value = li.dataset.ssid; $('pass').value = ''; $('pass').focus();
};
$('showPass').onchange = e => { $('pass').type = e.target.checked ? 'text' : 'password'; };

$('wifiForm').onsubmit = async e => {
  e.preventDefault();
  setMsg($('wifiMsg'), 'Saving…');
  try { await handleResult(await post('/api/wifi', {ssid: $('ssid').value, pass: $('pass').value}), $('wifiMsg')); }
  catch (err) { setMsg($('wifiMsg'), 'Request failed.', false); }
};

// Device settings
$('hostname').oninput = () => { $('hostname').dataset.touched = '1'; };
$('devForm').onsubmit = async e => {
  e.preventDefault();
  const d = {hostname: $('hostname').value};
  if ($('adminClear').checked) d.adminClear = '1'; else if ($('admin').value) d.admin = $('admin').value;
  setMsg($('devMsg'), 'Saving…');
  try {
    const j = await post('/api/device', d);
    $('admin').value = ''; $('adminClear').checked = false; delete $('hostname').dataset.touched;
    await handleResult(j, $('devMsg'));
    if (j.ok && !j.restart) loadStatus();
  } catch (err) { setMsg($('devMsg'), 'Request failed.', false); }
};

// Firmware upload
$('fwForm').onsubmit = e => {
  e.preventDefault();
  const f = $('fwFile').files[0];
  if (!f) return;
  if (!/\.bin$/i.test(f.name) && !confirm('"' + f.name + '" does not end in .bin. Upload anyway?')) return;
  const fd = new FormData(); fd.append('firmware', f, f.name);
  const xhr = new XMLHttpRequest();
  const bar = $('fwBar'), fill = bar.firstElementChild;
  bar.style.display = 'block'; fill.style.width = '0'; $('fwBtn').disabled = true;
  setMsg($('fwMsg'), 'Uploading ' + f.name + ' (' + fmtBytes(f.size) + ')…');
  xhr.upload.onprogress = ev => {
    if (!ev.lengthComputable) return;
    const p = Math.round(ev.loaded / ev.total * 100);
    fill.style.width = p + '%';
    setMsg($('fwMsg'), p < 100 ? 'Uploading… ' + p + '%' : 'Finishing and verifying…');
  };
  xhr.onload = () => {
    let j; try { j = JSON.parse(xhr.responseText); } catch (err) { j = {ok: false, message: 'HTTP ' + xhr.status}; }
    $('fwBtn').disabled = false;
    if (!j.ok) bar.style.display = 'none';
    handleResult(j, $('fwMsg'));
  };
  xhr.onerror = () => { $('fwBtn').disabled = false; bar.style.display = 'none'; setMsg($('fwMsg'), 'Upload failed (connection lost).', false); };
  xhr.open('POST', '/update');
  xhr.send(fd);
};

// Maintenance
$('rebootBtn').onclick = async () => {
  if (!confirm('Reboot the device?')) return;
  try { await handleResult(await post('/api/reboot', {}), $('mntMsg')); } catch (e) { setMsg($('mntMsg'), 'Request failed.', false); }
};
$('forgetBtn').onclick = async () => {
  if (!confirm('Erase the saved WiFi credentials? The device will restart into setup AP mode.')) return;
  try { await handleResult(await post('/api/forget', {}), $('mntMsg')); } catch (e) { setMsg($('mntMsg'), 'Request failed.', false); }
};

// MQTT settings: fields load once (and after a save), the status pill refreshes.
async function loadMqtt(fillFields) {
  if (rebooting) return;
  try {
    const r = await fetch('/api/mqtt', {cache: 'no-store'});
    if (!r.ok) return;
    const m = await r.json();
    if (fillFields) {
      $('mqttHost').value = m.host;
      $('mqttPort').value = m.port;
      $('mqttUser').value = m.user;
      $('mqttTopic').value = m.topic;
      $('mqttTopic').placeholder = m.defaultTopic;
      $('mqttPass').value = '';
      $('mqttPassClear').checked = false;
      $('mqttPass').placeholder = m.passwordSet ? 'leave blank to keep current' : 'optional';
    }
    const pill = $('mqttState');
    pill.textContent = m.status.charAt(0).toUpperCase() + m.status.slice(1);
    pill.className = 'pill' + (m.status === 'connected' ? ' ok' : m.status === 'disabled' ? '' : ' warn');
    $('mqttHint').textContent = m.status === 'disabled'
      ? 'Commands: ' + m.topic + '/set, /press, /brightness/set, /get. State: ' + m.topic + '/state.'
      : 'Client ID ' + m.clientId + '. Commands: ' + m.topic + '/set, /press, /brightness/set, /get. State: ' +
        m.topic + '/state.' + (m.error ? ' Last error: ' + m.error : '');
  } catch (e) {}
}

$('mqttForm').onsubmit = async e => {
  e.preventDefault();
  const d = {host: $('mqttHost').value, port: $('mqttPort').value, user: $('mqttUser').value, topic: $('mqttTopic').value};
  if ($('mqttPassClear').checked) d.passClear = '1'; else if ($('mqttPass').value) d.pass = $('mqttPass').value;
  setMsg($('mqttMsg'), 'Saving…');
  try {
    const j = await post('/api/mqtt', d);
    setMsg($('mqttMsg'), j.message, j.ok);
    if (j.ok) { loadMqtt(true); setTimeout(() => loadMqtt(false), 2500); }
  } catch (err) { setMsg($('mqttMsg'), 'Request failed.', false); }
};

bindControllerButtons();
if (document.body.dataset.view === 'main') {
  loadController();
  setInterval(loadController, 250);
} else {
  loadMqtt(true);
  setInterval(() => loadMqtt(false), 5000);
}
loadStatus();
setInterval(loadStatus, 5000);
</script>
</body>
</html>
)rawliteral";
