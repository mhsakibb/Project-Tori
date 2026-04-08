#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// --- Motor Driver Pins (Configured for Active-Low Drivers) ---
const int enablePin = 4;
const int dirPin1 = 6;
const int dirPin2 = 5;

// --- Servo Pins ---
const int frontServoPin = 7;
const int backServoPin = 8;
Servo frontServo;
Servo backServo;

AsyncWebServer server(80);

// --- Updated Modern UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Project তরি 🛶</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --bg: #0f172a;
      --glass: rgba(30, 41, 59, 0.7);
      --accent: #00d2ff;
      --accent-alt: #3a7bd5;
      --danger: #ef4444;
      --warning: #f59e0b;
    }
    body {
      font-family: 'Segoe UI', Roboto, sans-serif;
      background: radial-gradient(circle at top, #1e293b, #0f172a);
      color: white; margin: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh;
    }
    .container {
      width: 90%; max-width: 450px; padding: 25px;
      background: var(--glass); backdrop-filter: blur(10px);
      border-radius: 24px; border: 1px solid rgba(255,255,255,0.1);
      box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
    }
    h1 { font-size: 28px; margin-bottom: 20px; letter-spacing: 2px; text-transform: uppercase; color: var(--accent); }

    .grid-btns { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px; }
    .btn {
      padding: 18px; border: none; border-radius: 12px; font-weight: bold; cursor: pointer;
      transition: all 0.3s; text-transform: uppercase; font-size: 14px;
    }
    .btn-drive { background: linear-gradient(135deg, var(--accent), var(--accent-alt)); color: white; }
    .btn-reverse { background: linear-gradient(135deg, var(--warning), #d97706); color: white; }
    .btn:active { transform: translateY(2px); filter: brightness(1.2); }

    .panel {
      background: rgba(255,255,255,0.05); padding: 20px; border-radius: 16px; margin-bottom: 15px;
      text-align: left; border: 1px solid rgba(255,255,255,0.05);
    }
    .label-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
    .val-badge { background: var(--accent); color: #0f172a; padding: 2px 10px; border-radius: 20px; font-weight: 800; font-size: 14px; }

    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
    input[type=range]::-webkit-slider-runnable-track { height: 8px; background: rgba(255,255,255,0.1); border-radius: 4px; }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; height: 24px; width: 24px; border-radius: 50%;
      background: white; cursor: pointer; margin-top: -8px; box-shadow: 0 0 10px var(--accent);
    }

    .stop-btn {
      width: 100%; padding: 20px; background: linear-gradient(135deg, #f87171, var(--danger));
      color: white; border: none; border-radius: 16px; font-size: 20px; font-weight: 800;
      cursor: pointer; box-shadow: 0 10px 15px -3px rgba(239, 68, 68, 0.4);
    }
    .angle-container { display: flex; justify-content: space-between; gap: 10px; margin-top: 15px; }
    .btn-mini { flex: 1; background: rgba(255,255,255,0.1); color: white; border: none; padding: 12px 5px; border-radius: 8px; font-size: 14px; font-weight: bold; cursor: pointer; transition: background 0.2s; }
    .btn-mini:active { background: rgba(255,255,255,0.25); }
  </style>
</head>
<body>
  <div class="container">
    <h1>Project Tori 🛶</h1>

    <div class="grid-btns">
      <button class="btn btn-drive" onclick="setDirection('forward')">Forward</button>
      <button class="btn btn-reverse" onclick="setDirection('reverse')">Reverse</button>
    </div>

    <div class="panel">
      <div class="label-row">
        <span>Main Thruster</span>
        <span class="val-badge" id="speedValue">0</span>
      </div>
      <input type="range" min="0" max="255" value="0" id="speedSlider" oninput="updateSpeed(this.value)">
    </div>

    <div class="panel">
      <div class="label-row">
        <span>Front Fin</span>
        <span class="val-badge" style="background:var(--warning)" id="frontVal">97</span>
      </div>
      <input type="range" min="0" max="180" value="97" id="frontSlider" oninput="updateServo('front', this.value)">
      <div class="angle-container">
        <button class="btn-mini" onclick="quickServo('front', 45)">45°</button>
        <button class="btn-mini" onclick="quickServo('front', 97)">Center</button>
        <button class="btn-mini" onclick="quickServo('front', 135)">135°</button>
      </div>
    </div>

    <div class="panel">
      <div class="label-row">
        <span>Back Fin</span>
        <span class="val-badge" style="background:var(--warning)" id="backVal">97</span>
      </div>
      <input type="range" min="0" max="180" value="97" id="backSlider" oninput="updateServo('back', this.value)">
      <div class="angle-container">
        <button class="btn-mini" onclick="quickServo('back', 45)">45°</button>
        <button class="btn-mini" onclick="quickServo('back', 97)">Center</button>
        <button class="btn-mini" onclick="quickServo('back', 135)">135°</button>
      </div>
    </div>

    <button class="stop-btn" onclick="setDirection('stopped')">STOP SYSTEM</button>
  </div>

  <script>
    function setDirection(dir) {
      fetch('/action?dir=' + dir);
      if (dir == "stopped") {
        document.getElementById('speedSlider').value = 0;
        document.getElementById('speedValue').innerText = "0";
        document.getElementById('frontSlider').value = 97;
        document.getElementById('frontVal').innerText = "97";
        document.getElementById('backSlider').value = 97;
        document.getElementById('backVal').innerText = "97";
      }
    }
    function updateSpeed(val) {
      document.getElementById('speedValue').innerText = val;
      fetch('/speed?val=' + val);
    }
    function updateServo(target, angle) {
      document.getElementById(target + 'Val').innerText = angle;
      fetch('/servo?target=' + target + '&val=' + angle);
    }
    function quickServo(target, angle) {
      document.getElementById(target + 'Val').innerText = angle;
      document.getElementById(target + 'Slider').value = angle;
      fetch('/servo?target=' + target + '&val=' + angle);
    }
  </script>
</body>
</html>
)rawliteral";

// --- Setup and Server Logic ---

void setup()
{
    Serial.begin(115200);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    frontServo.setPeriodHertz(50);
    backServo.setPeriodHertz(50);
    frontServo.attach(frontServoPin, 500, 2400);
    backServo.attach(backServoPin, 500, 2500);

    frontServo.write(97);
    backServo.write(97);

    pinMode(enablePin, OUTPUT);
    pinMode(dirPin1, OUTPUT);
    pinMode(dirPin2, OUTPUT);

    // Initial State: Motors OFF (Active Low = 255)
    digitalWrite(dirPin1, LOW);
    digitalWrite(dirPin2, LOW);
    analogWrite(enablePin, 255);

    WiFi.softAP("Project_Tori", "12345678");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html); });

    server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("val")) {
            int requestedSpeed = request->getParam("val")->value().toInt();
            int invertedPWM = 255 - requestedSpeed; // Active-Low inversion
            analogWrite(enablePin, invertedPWM);
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("target") && request->hasParam("val")) {
            String target = request->getParam("target")->value();
            int angle = request->getParam("val")->value().toInt();
            if (target == "front") frontServo.write(angle);
            else if (target == "back") backServo.write(angle);
        }
        request->send(200, "text/plain", "OK"); });

    server.on("/action", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("dir")) {
            String dir = request->getParam("dir")->value();
            if (dir == "forward") {
                digitalWrite(dirPin1, HIGH);
                digitalWrite(dirPin2, LOW);
            } else if (dir == "reverse") {
                digitalWrite(dirPin1, LOW);
                digitalWrite(dirPin2, HIGH);
            } else if (dir == "stopped") {
                // FIXED STOP LOGIC
                digitalWrite(dirPin1, LOW);
                digitalWrite(dirPin2, LOW);
                analogWrite(enablePin, 255); // 255 is OFF for Active-Low drivers
                frontServo.write(97);
                backServo.write(97);
            }
        }
        request->send(200, "text/plain", "OK"); });

    server.begin();
}

void loop() {}
