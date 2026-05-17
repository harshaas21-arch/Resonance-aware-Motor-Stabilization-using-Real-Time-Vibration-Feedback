#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

// ======================================================
// WIFI CREDENTIALS  ← CHANGE THESE
// ======================================================
const char* ssid     = "Pixel_8a";
const char* password = "gautham009";

// ======================================================
// PIN DEFINITIONS
// ======================================================
#define EN   25   // PWM enable pin → L293D EN
#define IN1  26   // Direction pin 1 → L293D IN1
#define IN2  27   // Direction pin 2 → L293D IN2
#define POT  34   // Potentiometer wiper → ADC1_CH6

// ======================================================
// HARDWARE OBJECTS
// ======================================================
WebServer            server(80);
WebSocketsServer     webSocket(81);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// ======================================================
// CONTROL STATE
// ======================================================
float filteredVibration = 0.0f;   // EMA-smoothed 3-axis vibration
float smoothPWM         = 0.0f;   // EMA-smoothed PWM output (avoids jerky steps)

float vibrationThreshold = 1.2f;  // m/s² — above this, damping kicks in
int   dampingStrength    = 50;    // 0–100 — aggressiveness of reduction

// speedMode:
//   "POT"  — desired PWM comes from the physical potentiometer
//   "UI"   — desired PWM comes from the browser slider
String speedMode     = "POT";
int    uiSpeedTarget = 128;        // stored value from browser slider

bool motorStopped    = false;

unsigned long lastBroadcast = 0;

// ======================================================
// HELPER: read the potentiometer with simple oversampling
// ======================================================
int readPot()
{
    // Average 4 samples to reduce ADC noise
    long sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += analogRead(POT);
        delayMicroseconds(200);
    }
    return sum / 4;
}

// ======================================================
// HTML PAGE  (stored in flash, served via HTTP GET /)
// ======================================================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>MOTOCTRL PRO</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@400;600;700&display=swap');

  :root {
    --bg:        #080c12;
    --panel:     #0d1420;
    --border:    #1a2535;
    --accent:    #00ffae;
    --accent2:   #5da9ff;
    --warn:      #ffb347;
    --danger:    #ff5f6d;
    --text:      #ccd6f6;
    --muted:     #4a5878;
    --radius:    16px;
  }

  * { margin:0; padding:0; box-sizing:border-box; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Rajdhani', sans-serif;
    font-size: 15px;
    min-height: 100vh;
    padding: 16px;
    max-width: 520px;
    margin: 0 auto;
  }

  /* ── HEADER ── */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 20px;
    padding-bottom: 14px;
    border-bottom: 1px solid var(--border);
  }
  .logo { font-family: 'Share Tech Mono', monospace; font-size: 22px; letter-spacing: 2px; }
  .logo span { color: var(--accent); }
  .conn-dot {
    width: 10px; height: 10px; border-radius: 50%;
    background: var(--muted);
    box-shadow: none;
    transition: all .4s;
    display: inline-block; margin-right: 6px;
  }
  .conn-dot.live {
    background: var(--accent);
    box-shadow: 0 0 10px var(--accent);
    animation: pulse 1.8s infinite;
  }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.5} }
  .conn-label { font-family:'Share Tech Mono',monospace; font-size:11px; color:var(--muted); }

  /* ── METRIC CARDS ── */
  .cards {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-bottom: 14px;
  }
  .card {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 14px 16px;
    position: relative;
    overflow: hidden;
  }
  .card::before {
    content:''; position:absolute; top:0; left:0; right:0; height:2px;
    border-radius:2px 2px 0 0;
  }
  .card.c-green::before { background: var(--accent); }
  .card.c-blue::before  { background: var(--accent2); }
  .card.c-warn::before  { background: var(--warn); }
  .card.c-danger::before{ background: var(--danger); }

  .card-label { font-size:10px; letter-spacing:2px; text-transform:uppercase; color:var(--muted); margin-bottom:6px; }
  .card-val   { font-family:'Share Tech Mono',monospace; font-size:30px; line-height:1; }
  .card-unit  { font-size:11px; color:var(--muted); margin-top:3px; }

  .card.c-green .card-val { color: var(--accent); }
  .card.c-blue  .card-val { color: var(--accent2); }
  .card.c-warn  .card-val { color: var(--warn); }
  .card.c-danger.card-val { color: var(--danger); }

  .mini-bar {
    height:3px; background:var(--border); border-radius:2px;
    margin-top:10px; overflow:hidden;
  }
  .mini-fill {
    height:100%; width:0%; border-radius:2px;
    transition: width .2s, background .3s;
  }

  /* ── MODE PILL ── */
  .mode-row {
    display: flex; align-items: center; justify-content: space-between;
    background: var(--panel); border:1px solid var(--border);
    border-radius: var(--radius); padding: 10px 16px;
    margin-bottom: 14px;
  }
  .mode-label { font-size:11px; letter-spacing:2px; text-transform:uppercase; color:var(--muted); }
  .toggle-wrap { display:flex; background:var(--bg); border-radius:20px; padding:3px; gap:3px; }
  .toggle-btn {
    border:none; border-radius:16px; padding:6px 18px;
    font-family:'Rajdhani',sans-serif; font-weight:600; font-size:13px;
    cursor:pointer; transition:.2s; background:transparent; color:var(--muted);
  }
  .toggle-btn.active { background:var(--accent); color:#000; }

  /* ── ALERT BANNER ── */
  .alert {
    display:none; background:#1a0608; border:1px solid #ff5f6d55;
    border-radius:10px; padding:10px 14px; color:#ff9fa3;
    font-size:12px; letter-spacing:.5px; margin-bottom:14px;
  }
  .alert.show { display:block; }

  /* ── PANELS ── */
  .panel {
    background: var(--panel); border:1px solid var(--border);
    border-radius: var(--radius); padding:16px; margin-bottom:14px;
  }
  .panel-title {
    font-size:10px; letter-spacing:2px; text-transform:uppercase;
    color:var(--muted); margin-bottom:14px;
    padding-bottom:8px; border-bottom:1px solid var(--border);
  }

  /* ── GRAPHS ── */
  canvas {
    width:100%!important; height:100%!important;
    display:block;
  }
  .chart-wrap { width:100%; height:90px; margin-bottom:10px; }
  .chart-wrap:last-child { margin-bottom:0; }
  .chart-label { font-size:10px; letter-spacing:1px; color:var(--muted); margin-bottom:4px; }

  /* ── SLIDERS ── */
  .slider-row { margin-bottom:14px; }
  .slider-top {
    display:flex; justify-content:space-between; align-items:center;
    margin-bottom:8px;
  }
  .slider-name { font-size:13px; font-weight:600; letter-spacing:.5px; }
  .slider-value {
    font-family:'Share Tech Mono',monospace; font-size:13px;
    color:var(--accent); min-width:36px; text-align:right;
  }
  input[type=range] {
    width:100%; appearance:none; height:5px;
    border-radius:4px; background:var(--border); outline:none;
    cursor:pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    appearance:none; width:20px; height:20px; border-radius:50%;
    background:var(--accent); cursor:pointer;
    box-shadow:0 0 8px rgba(0,255,174,.6);
    transition:.15s;
  }
  input[type=range]::-webkit-slider-thumb:hover {
    box-shadow:0 0 16px rgba(0,255,174,.9);
    transform:scale(1.15);
  }
  input[type=range]:disabled { opacity:.35; cursor:not-allowed; }

  /* ── CONTROL BUTTONS ── */
  .btn-row { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-top:4px; }
  .btn {
    border:none; border-radius:12px; padding:16px 8px;
    font-family:'Rajdhani',sans-serif; font-weight:700; font-size:15px;
    letter-spacing:1px; cursor:pointer; transition:.18s;
    display:flex; flex-direction:column; align-items:center; gap:5px;
    -webkit-tap-highlight-color:transparent;
  }
  .btn:hover { transform:translateY(-2px); }
  .btn:active { transform:scale(.97); }
  .btn-fwd  { background:var(--accent); color:#000; }
  .btn-stop { background:var(--danger); color:#fff; }
  .btn svg  { width:20px; height:20px; stroke:currentColor; fill:none; stroke-width:2.2; stroke-linecap:round; stroke-linejoin:round; }
  .btn-stop.halted { box-shadow:0 0 18px rgba(255,95,109,.5); }

  /* ── STAT ROW ── */
  .stat-row {
    display:flex; justify-content:space-between;
    padding:7px 0; border-bottom:1px solid var(--border);
    font-size:13px;
  }
  .stat-row:last-child { border:none; }
  .stat-key  { color:var(--muted); }
  .stat-val  { font-family:'Share Tech Mono',monospace; color:var(--text); }

</style>
</head>
<body>

<!-- HEADER -->
<div class="header">
  <div class="logo">MOTO<span>CTRL</span> PRO</div>
  <div style="display:flex;align-items:center;gap:6px">
    <span class="conn-dot" id="dot"></span>
    <span class="conn-label" id="connLabel">OFFLINE</span>
  </div>
</div>

<!-- METRIC CARDS -->
<div class="cards">
  <div class="card c-green">
    <div class="card-label">Vibration</div>
    <div class="card-val" id="mVib">--</div>
    <div class="card-unit">m/s²</div>
    <div class="mini-bar"><div class="mini-fill" id="vibFill" style="background:var(--accent)"></div></div>
  </div>
  <div class="card c-blue">
    <div class="card-label">Final PWM</div>
    <div class="card-val" id="mFin">--</div>
    <div class="card-unit">/ 255</div>
    <div class="mini-bar"><div class="mini-fill" id="pwmFill" style="background:var(--accent2)"></div></div>
  </div>
  <div class="card c-warn">
    <div class="card-label">Desired PWM</div>
    <div class="card-val" id="mDes">--</div>
    <div class="card-unit">setpoint</div>
  </div>
  <div class="card c-danger">
    <div class="card-label">Pot ADC</div>
    <div class="card-val" id="mPot">--</div>
    <div class="card-unit">0 – 4095</div>
  </div>
</div>

<!-- SPEED SOURCE TOGGLE -->
<div class="mode-row">
  <span class="mode-label">Speed Source</span>
  <div class="toggle-wrap">
    <button class="toggle-btn active" id="tPot" onclick="setMode('POT')">Potentiometer</button>
    <button class="toggle-btn"        id="tUI"  onclick="setMode('UI')">UI Slider</button>
  </div>
</div>

<!-- VIBRATION ALERT -->
<div class="alert" id="alertBox">⚠ High vibration — damping active</div>

<!-- LIVE GRAPHS -->
<div class="panel">
  <div class="panel-title">Live Graphs</div>

  <div class="chart-label">Vibration  (m/s²)</div>
  <div class="chart-wrap"><canvas id="vibChart"></canvas></div>

  <div class="chart-label" style="margin-top:10px">PWM  — <span style="color:var(--accent2)">desired</span> vs <span style="color:var(--accent)">final</span></div>
  <div class="chart-wrap"><canvas id="pwmChart"></canvas></div>
</div>

<!-- CONTROLS -->
<div class="panel">
  <div class="panel-title">Controls</div>

  <!-- Speed slider — only active in UI mode -->
  <div class="slider-row">
    <div class="slider-top">
      <span class="slider-name">Speed (UI)</span>
      <span class="slider-value" id="speedDisp">128</span>
    </div>
    <input type="range" id="speedSlider" min="0" max="255" value="128" disabled
           oninput="onSpeed(this)">
  </div>

  <!-- Threshold slider -->
  <div class="slider-row">
    <div class="slider-top">
      <span class="slider-name">Vib Threshold</span>
      <span class="slider-value" id="thrDisp">1.2</span>
    </div>
    <input type="range" id="thrSlider" min="0.1" max="5" step="0.1" value="1.2"
           oninput="onThresh(this)">
  </div>

  <!-- Damping slider -->
  <div class="slider-row" style="margin-bottom:18px">
    <div class="slider-top">
      <span class="slider-name">Damping Strength</span>
      <span class="slider-value" id="dampDisp">50</span>
    </div>
    <input type="range" id="dampSlider" min="0" max="100" value="50"
           oninput="onDamp(this)">
  </div>

  <!-- Forward / Stop buttons -->
  <div class="btn-row">
    <button class="btn btn-fwd" onclick="setDir()">
      <svg viewBox="0 0 24 24"><polyline points="5 12 19 12"/><polyline points="13 6 19 12 13 18"/></svg>
      FORWARD
    </button>
    <button class="btn btn-stop" id="stopBtn" onclick="doStop()">
      <svg viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>
      STOP
    </button>
  </div>
</div>

<!-- SYSTEM STATS -->
<div class="panel">
  <div class="panel-title">System Stats</div>
  <div class="stat-row"><span class="stat-key">Damping active</span><span class="stat-val" id="sDamp">—</span></div>
  <div class="stat-row"><span class="stat-key">Reduction applied</span><span class="stat-val" id="sRed">—</span></div>
  <div class="stat-row"><span class="stat-key">Speed source</span><span class="stat-val" id="sSrc">—</span></div>
  <div class="stat-row"><span class="stat-key">Motor state</span><span class="stat-val" id="sState">—</span></div>
</div>

<script>
// ── CONFIG ──────────────────────────────────────────────
const GRAPH_LEN   = 80;   // points on each chart
const BROADCAST_MS = 100; // matches firmware

// ── STATE ────────────────────────────────────────────────
let ws;
let mode = "POT";          // "POT" or "UI"
let stopped = false;

// Graph buffers
const vibBuf  = new Array(GRAPH_LEN).fill(0);
const desBuf  = new Array(GRAPH_LEN).fill(0);
const finBuf  = new Array(GRAPH_LEN).fill(0);

// ── GRAPH DRAWING ────────────────────────────────────────
function resizeCanvas(c) {
  const r = c.parentElement.getBoundingClientRect();
  c.width  = r.width  * devicePixelRatio;
  c.height = r.height * devicePixelRatio;
}

function drawGraph(canvas, series, maxY, colors, fill) {
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);

  // grid lines
  ctx.strokeStyle = 'rgba(26,37,53,.9)';
  ctx.lineWidth = 1;
  for (let i = 1; i < 4; i++) {
    const y = H * (1 - i/4);
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }

  const step = W / (GRAPH_LEN - 1);

  series.forEach((data, ci) => {
    const col = colors[ci];
    ctx.beginPath();
    data.forEach((v, i) => {
      const x = i * step;
      const y = H - Math.min(Math.max(v / maxY, 0), 1) * H * .92;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    });

    ctx.strokeStyle = col;
    ctx.lineWidth = 1.8;
    ctx.stroke();

    // fill under last series only
    if (fill && ci === series.length - 1) {
      ctx.lineTo((GRAPH_LEN - 1) * step, H);
      ctx.lineTo(0, H);
      ctx.closePath();
      ctx.fillStyle = col + '18';
      ctx.fill();
    }
  });
}

const cVib = document.getElementById('vibChart');
const cPwm = document.getElementById('pwmChart');

function render() {
  resizeCanvas(cVib); resizeCanvas(cPwm);
  drawGraph(cVib, [vibBuf],          5,   ['#00ffae'], true);
  drawGraph(cPwm, [desBuf, finBuf],  260, ['#5da9ff','#00ffae'], true);
}

// ── WEBSOCKET ────────────────────────────────────────────
function connect() {
  ws = new WebSocket('ws://' + location.hostname + ':81');

  ws.onopen = () => {
    document.getElementById('dot').classList.add('live');
    document.getElementById('connLabel').textContent = 'LIVE';
  };

  ws.onclose = () => {
    document.getElementById('dot').classList.remove('live');
    document.getElementById('connLabel').textContent = 'OFFLINE';
    setTimeout(connect, 2000);
  };

  ws.onmessage = (e) => {
    let d;
    try { d = JSON.parse(e.data); } catch { return; }

    const vib = parseFloat(d.vibration) || 0;
    const des = parseInt(d.desiredPWM)  || 0;
    const fin = parseInt(d.finalPWM)    || 0;
    const pot = parseInt(d.potValue)    || 0;

    // ── Cards ──
    const vibColor = vib > 2 ? '#ff5f6d' : vib > 1.2 ? '#ffb347' : '#00ffae';
    const mVib = document.getElementById('mVib');
    mVib.textContent = vib.toFixed(2);
    mVib.style.color = vibColor;

    document.getElementById('mFin').textContent = fin;
    document.getElementById('mDes').textContent = des;
    document.getElementById('mPot').textContent = pot;

    // bars
    document.getElementById('vibFill').style.width    = Math.min(vib/5*100,100).toFixed(1)+'%';
    document.getElementById('vibFill').style.background = vibColor;
    document.getElementById('pwmFill').style.width    = (fin/255*100).toFixed(1)+'%';

    // ── Alert ──
    const dampActive = vib > parseFloat(document.getElementById('thrSlider').value);
    document.getElementById('alertBox').classList.toggle('show', dampActive);

    // ── Sync pot-mode speed slider display ──
    // When in POT mode, keep the speed slider thumb tracking current desired PWM
    // (read-only visual feedback — slider stays disabled)
    if (mode === 'POT') {
      document.getElementById('speedSlider').value = des;
      document.getElementById('speedDisp').textContent = des;
    }

    // ── Stats ──
    document.getElementById('sDamp').textContent  = dampActive ? 'YES' : 'NO';
    document.getElementById('sRed').textContent   = Math.max(des - fin, 0) + ' counts';
    document.getElementById('sSrc').textContent   = mode;
    document.getElementById('sState').textContent = stopped ? 'STOPPED' : 'RUNNING';

    // ── Graphs ──
    vibBuf.push(vib); vibBuf.shift();
    desBuf.push(des); desBuf.shift();
    finBuf.push(fin); finBuf.shift();
    render();
  };

  ws.onerror = () => {};
}

// ── MODE TOGGLE (POT ↔ UI) ──────────────────────────────
function setMode(m) {
  mode = m;
  document.getElementById('tPot').classList.toggle('active', m === 'POT');
  document.getElementById('tUI' ).classList.toggle('active', m === 'UI');
  const sl = document.getElementById('speedSlider');

  if (m === 'POT') {
    sl.disabled = true;
    send('AUTO');            // firmware: use potentiometer
  } else {
    sl.disabled = false;
    // Immediately send current slider value so UI takes control
    send('SPEED:' + sl.value);
    stopped = false;
    document.getElementById('stopBtn').classList.remove('halted');
  }
}

// ── SLIDER HANDLERS ─────────────────────────────────────
function onSpeed(el) {
  document.getElementById('speedDisp').textContent = el.value;
  if (mode === 'UI') { send('SPEED:' + el.value); stopped = false; }
}

function onThresh(el) {
  document.getElementById('thrDisp').textContent = parseFloat(el.value).toFixed(1);
  send('THRESH:' + el.value);
}

function onDamp(el) {
  document.getElementById('dampDisp').textContent = el.value;
  send('DAMP:' + el.value);
}

// ── DIRECTION / STOP ─────────────────────────────────────
function setDir() {
  stopped = false;
  document.getElementById('stopBtn').classList.remove('halted');
  send('DIR:FWD');
  if (mode === 'UI') send('SPEED:' + document.getElementById('speedSlider').value);
  else               send('AUTO');
}

function doStop() {
  stopped = true;
  document.getElementById('stopBtn').classList.add('halted');
  send('STOP');
}

// ── UTILS ────────────────────────────────────────────────
function send(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(msg);
}

window.addEventListener('resize', render);
connect();
render();
</script>
</body>
</html>
)rawliteral";

// ======================================================
// WEBSOCKET EVENT HANDLER
// ======================================================
void webSocketEvent(uint8_t num, WStype_t type,
                    uint8_t* payload, size_t length)
{
    if (type != WStype_TEXT) return;

    String msg = String((char*)payload);
    Serial.println("[WS] " + msg);

    if (msg.startsWith("SPEED:")) {
        uiSpeedTarget = msg.substring(6).toInt();
        uiSpeedTarget = constrain(uiSpeedTarget, 0, 255);
        speedMode    = "UI";
        motorStopped = false;
    }
    else if (msg == "AUTO") {
        speedMode    = "POT";   // hand control back to pot
        motorStopped = false;
    }
    else if (msg == "STOP") {
        motorStopped = true;
    }
    else if (msg == "DIR:FWD") {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        motorStopped = false;
    }
    else if (msg.startsWith("THRESH:")) {
        vibrationThreshold = msg.substring(7).toFloat();
    }
    else if (msg.startsWith("DAMP:")) {
        dampingStrength = constrain(msg.substring(5).toInt(), 0, 100);
    }
}

// ======================================================
// SETUP
// ======================================================
void setup()
{
    Serial.begin(115200);

    // I2C for ADXL345
    Wire.begin(21, 22);

    if (!accel.begin()) {
        Serial.println("[ERR] ADXL345 not found – halting");
        while (1) delay(1000);
    }
    accel.setRange(ADXL345_RANGE_16_G); // wide range for high-speed motors

    // Motor driver pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    digitalWrite(IN1, HIGH);  // default: forward
    digitalWrite(IN2, LOW);

    // ledc PWM: 5 kHz, 8-bit (0–255)
    ledcAttach(EN, 5000, 8);
    ledcWrite(EN, 0);          // start with motor off

    // WiFi
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println();
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());

    // HTTP server – serve dashboard
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", webpage);
    });
    server.begin();

    // WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.println("[OK] System ready");
}

// ======================================================
// LOOP
// ======================================================
void loop()
{
    server.handleClient();
    webSocket.loop();

    // ── Potentiometer (oversampled) ──────────────────────
    int potValue  = readPot();
    int desiredPWM = (speedMode == "POT")
                         ? map(potValue, 0, 4095, 0, 255)
                         : uiSpeedTarget;

    // ── Accelerometer ────────────────────────────────────
    sensors_event_t event;
    accel.getEvent(&event);

    // 3-axis vector magnitude
    float ax = event.acceleration.x;
    float ay = event.acceleration.y;
    float az = event.acceleration.z;
    float magnitude = sqrtf(ax*ax + ay*ay + az*az);

    // Remove static gravity component
    float rawVibration = fabsf(magnitude - 9.81f);

    // ── EMA low-pass filter (α = 0.08 → τ ≈ 11.5 samples) ──
    filteredVibration = 0.92f * filteredVibration
                      + 0.08f * rawVibration;

    // ── Proportional damping control law ─────────────────
    int finalPWM;

    if (motorStopped) {
        finalPWM  = 0;
        smoothPWM = 0;
    }
    else {
        // How far above threshold are we?
        float excess = filteredVibration - vibrationThreshold;
        if (excess < 0) excess = 0;

        // Scale reduction: excess × strength × gain
        // Max reduction capped at 85% of desired so motor never stalls
        float reduction = excess * dampingStrength * 12.0f;
        reduction = fminf(reduction, desiredPWM * 0.85f);

        float target = desiredPWM - reduction;

        // Hard lower bound: 100 PWM keeps motor spinning
        if (target > 0 && target < 100) target = 100;

        // Smooth the PWM transitions (avoids electrical transients)
        smoothPWM = 0.90f * smoothPWM + 0.10f * target;

        finalPWM = (int)smoothPWM;
    }

    finalPWM = constrain(finalPWM, 0, 255);

    ledcWrite(EN, finalPWM);

    // ── Broadcast telemetry at 10 Hz ─────────────────────
    if (millis() - lastBroadcast >= 100) {
        lastBroadcast = millis();

        StaticJsonDocument<256> doc;
        doc["potValue"]   = potValue;
        doc["desiredPWM"] = desiredPWM;
        doc["finalPWM"]   = finalPWM;
        doc["vibration"]  = filteredVibration;

        String json;
        serializeJson(doc, json);
        webSocket.broadcastTXT(json);

        Serial.printf("[TELEM] Pot:%4d  Des:%3d  Vib:%.3f  Fin:%3d  Mode:%s\n",
                      potValue, desiredPWM,
                      filteredVibration, finalPWM,
                      speedMode.c_str());
    }

    delay(10); // ~100 Hz control loop
}
