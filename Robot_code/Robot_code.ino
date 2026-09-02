#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ========== WiFi Credentials ==========
const char* ssid = "TRONIC_BEAST";
const char* password = "12345678";

// ========== Motor Pins (L298N) ==========
#define IN1 16
#define IN2 17
#define IN3 18
#define IN4 19
#define ENA 21
#define ENB 22

// ========== Buzzer Pin ==========
#define BUZZER 23

// ========== WebSocket ==========
WebSocketsServer webSocket = WebSocketsServer(81);

// ========== Web Server ==========
WiFiServer server(80);

// ========== Motor Speed Control ==========
int leftSpeed = 0;
int rightSpeed = 0;

// ========== HTML Page ==========
String htmlPage;

// ========== Motor Control Functions ==========
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  leftSpeed = rightSpeed = 0;
}

void setMotors(int left, int right) {
  leftSpeed = constrain(left, -255, 255);
  rightSpeed = constrain(right, -255, 255);

  // Left motor
  if (leftSpeed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (leftSpeed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  // Right motor
  if (rightSpeed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (rightSpeed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }

  analogWrite(ENA, abs(leftSpeed));
  analogWrite(ENB, abs(rightSpeed));
}

// ========== Buzzer ==========
void playTone(int frequency, int duration) {
  tone(BUZZER, frequency, duration);
}

// ========== WebSocket Event ==========
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String json = String((char*)payload);
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (!error) {
      int joyX = doc["x"];
      int joyY = doc["y"];
      int buzzerAction = doc["buzzer"];

      // Tank drive from joystick Y (forward/back) and X (turn)
      int left = joyY + joyX;
      int right = joyY - joyX;

      // Normalize to -255..255
      left = constrain(left, -255, 255);
      right = constrain(right, -255, 255);

      setMotors(left, right);

      if (buzzerAction == 1) {
        playTone(1000, 200);
      }
    }
  }
}

// ========== Build HTML Page ==========
void buildHTML() {
  htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>JOY BEAST · 4WD WiFi Robot</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
      user-select: none;
    }
    body {
      background: linear-gradient(145deg, #0a0f1e 0%, #0c1222 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      font-family: 'Segoe UI', 'Poppins', system-ui, sans-serif;
      padding: 20px;
    }
    .dashboard {
      max-width: 650px;
      width: 100%;
      background: rgba(18, 25, 45, 0.65);
      backdrop-filter: blur(12px);
      border-radius: 64px;
      padding: 28px 24px 38px;
      box-shadow: 0 25px 45px rgba(0,0,0,0.5), inset 0 1px 0 rgba(255,255,255,0.08);
      border: 1px solid rgba(255,215,0,0.2);
    }
    .header {
      text-align: center;
      margin-bottom: 32px;
    }
    .joystick-glow {
      font-size: 54px;
      font-weight: 800;
      background: linear-gradient(135deg, #FFD966, #FFB347);
      -webkit-background-clip: text;
      background-clip: text;
      color: transparent;
      text-shadow: 0 0 12px rgba(255,180,70,0.4);
    }
    .badge {
      font-size: 14px;
      background: #1e2a3a;
      display: inline-block;
      padding: 6px 16px;
      border-radius: 60px;
      color: #FFD966;
      font-weight: 500;
      margin-top: 12px;
    }
    .joystick-container {
      display: flex;
      justify-content: center;
      margin: 40px 0;
    }
    .joystick-base {
      width: 220px;
      height: 220px;
      background: #101826;
      border-radius: 50%;
      box-shadow: inset 0 0 12px #00000055, 0 18px 28px rgba(0,0,0,0.4);
      display: flex;
      justify-content: center;
      align-items: center;
      border: 1px solid #ffd96633;
    }
    .joystick-thumb {
      width: 110px;
      height: 110px;
      background: radial-gradient(circle at 30% 30%, #FFE6B0, #FFB347);
      border-radius: 50%;
      box-shadow: 0 8px 20px black;
      cursor: grab;
      transition: 0.02s linear;
      border: 2px solid rgba(255,245,200,0.8);
    }
    .joystick-thumb:active { cursor: grabbing; }
    .info-panel {
      background: #0B0F17CC;
      border-radius: 48px;
      padding: 20px;
      margin: 28px 0;
      text-align: center;
    }
    .motor-values {
      display: flex;
      justify-content: space-between;
      gap: 16px;
      font-weight: 600;
    }
    .card {
      background: #00000033;
      flex: 1;
      padding: 12px;
      border-radius: 32px;
      color: #FFD966;
      font-size: 18px;
    }
    .buzzer-btn {
      background: linear-gradient(95deg, #2c1e2f, #1a1020);
      border: none;
      color: #ffb347;
      font-size: 28px;
      padding: 14px 28px;
      border-radius: 60px;
      font-weight: bold;
      width: 80%;
      margin-top: 16px;
      cursor: pointer;
      transition: 0.1s ease;
      box-shadow: 0 6px 0 #4a2a1a;
    }
    .buzzer-btn:active {
      transform: translateY(3px);
      box-shadow: 0 2px 0 #4a2a1a;
    }
    .status {
      color: #7f8c9a;
      font-size: 13px;
      margin-top: 20px;
    }
    footer {
      text-align: center;
      font-size: 12px;
      color: #ffd96666;
      margin-top: 25px;
    }
  </style>
</head>
<body>
<div class="dashboard">
  <div class="header">
    <div class="joystick-glow">⚡ JOY DRIVE ⚡</div>
    <div class="badge">4WD • WIFI TANK • LOW LATENCY</div>
  </div>

  <div class="joystick-container">
    <div class="joystick-base" id="joystickBase">
      <div class="joystick-thumb" id="joystickThumb"></div>
    </div>
  </div>

  <div class="info-panel">
    <div class="motor-values">
      <div class="card">🎮 LEFT: <span id="leftVal">0</span></div>
      <div class="card">🎮 RIGHT: <span id="rightVal">0</span></div>
    </div>
    <button class="buzzer-btn" id="buzzerBtn">🔊 HORN / BUZZER</button>
  </div>
  <div class="status">✅ LIVE • DRIVER: JOY</div>
  <footer>virtual joystick · tank steering · esp32 beast</footer>
</div>

<script>
  let ws;
  let active = false;
  let centerX = 0, centerY = 0, thumb = null, baseRect = null;
  let currentX = 0, currentY = 0;

  function connectWS() {
    ws = new WebSocket(`ws://${window.location.hostname}:81`);
    ws.onopen = () => console.log("WebSocket connected");
    ws.onerror = (e) => console.log("WS error", e);
  }
  connectWS();

  function sendJoystick(x, y) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    let data = { x: Math.round(x), y: Math.round(y), buzzer: 0 };
    ws.send(JSON.stringify(data));
    document.getElementById('leftVal').innerText = Math.round(y + x);
    document.getElementById('rightVal').innerText = Math.round(y - x);
  }

  function sendBuzzer() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ws.send(JSON.stringify({ x: currentX, y: currentY, buzzer: 1 }));
    setTimeout(() => {
      if (ws && ws.readyState === WebSocket.OPEN) 
        ws.send(JSON.stringify({ x: currentX, y: currentY, buzzer: 0 }));
    }, 200);
  }

  window.onload = () => {
    thumb = document.getElementById('joystickThumb');
    const base = document.getElementById('joystickBase');
    baseRect = base.getBoundingClientRect();
    centerX = baseRect.left + baseRect.width/2;
    centerY = baseRect.top + baseRect.height/2;
    const maxDist = baseRect.width/2 - thumb.offsetWidth/2;

    function updatePos(clientX, clientY) {
      let dx = clientX - centerX;
      let dy = clientY - centerY;
      let dist = Math.hypot(dx, dy);
      if (dist > maxDist) {
        dx = dx * maxDist / dist;
        dy = dy * maxDist / dist;
        dist = maxDist;
      }
      let normX = (dx / maxDist) * 255;
      let normY = (dy / maxDist) * 255;
      normY = -normY;
      currentX = Math.min(255, Math.max(-255, normX));
      currentY = Math.min(255, Math.max(-255, normY));
      thumb.style.transform = `translate(${dx}px, ${dy}px)`;
      sendJoystick(currentX, currentY);
    }

    function handleStart(e) {
      active = true;
      e.preventDefault();
      const point = e.touches ? e.touches[0] : e;
      updatePos(point.clientX, point.clientY);
    }

    function handleMove(e) {
      if (!active) return;
      e.preventDefault();
      const point = e.touches ? e.touches[0] : e;
      updatePos(point.clientX, point.clientY);
    }

    function handleEnd() {
      active = false;
      thumb.style.transform = `translate(0px, 0px)`;
      currentX = 0; currentY = 0;
      sendJoystick(0, 0);
    }

    thumb.addEventListener('mousedown', handleStart);
    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleEnd);
    thumb.addEventListener('touchstart', handleStart);
    window.addEventListener('touchmove', handleMove);
    window.addEventListener('touchend', handleEnd);

    document.getElementById('buzzerBtn').addEventListener('click', sendBuzzer);
    setInterval(() => {
      if (ws && ws.readyState !== WebSocket.OPEN) connectWS();
    }, 3000);
  };
</script>
</body>
</html>
)rawliteral";
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // Stop motors initially
  stopMotors();

  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  // Start WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Build HTML page
  buildHTML();

  // Start web server
  server.begin();
}

// ========== Main Loop ==========
void loop() {
  webSocket.loop();
  
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();
    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
    client.print(htmlPage);
    client.stop();
  }
  
  delay(10);
}