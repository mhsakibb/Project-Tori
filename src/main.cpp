#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// --- Hardware Pins ---
const int enablePin = 4; // PWM Pin (Driver Enable)
const int dirPin1 = 6;   // Direction Pin 1
const int dirPin2 = 5;   // Direction Pin 2
const int frontServoPin = 7;
const int backServoPin = 8;
const int rgbPin = 48; // S3 Built-in RGB LED

// --- PWM Constants for High-Power Motor ---
const int pwmFreq = 5000;    // 5kHz frequency
const int pwmChannel = 0;    // ESP32 LEDC Channel 0
const int pwmResolution = 8; // 8-bit resolution (0-255)

// --- Objects ---
Servo frontServo;
Servo backServo;
AsyncWebServer server(80);
Adafruit_NeoPixel LED_RGB(1, rgbPin, NEO_GRB + NEO_KHZ800);

// --- Global States ---
int currentSpeed = 0;
bool isStopped = true;

// --- Rainbow Animation Variables ---
unsigned long previousRGBTime = 0;
const long rgbInterval = 5; // Speed of the color dance (lower is faster)
uint16_t rainbowHue = 0;    // Tracks the current color (0 to 65535)

// --- Logic: LED Feedback ---
void updateLED()
{
    // 1. Signal Flash (Blue pulse to acknowledge command)
    LED_RGB.setPixelColor(0, LED_RGB.Color(0, 0, 255));
    LED_RGB.show();
    delay(20);

    // 2. Base State Color
    if (isStopped || currentSpeed == 0)
    {
        LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); // Red (Stopped)
    }
    else if (currentSpeed > 0 && currentSpeed < 255)
    {
        LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0)); // Green (Cruising)
    }
    // Note: We don't set a color here if speed is 255, the loop() takes over!

    LED_RGB.show();
}

// --- HTML UI (Glassmorphism Design with RPM) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Project Tori 🛶</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0f172a; --glass: rgba(30, 41, 59, 0.7); --accent: #00d2ff; --danger: #ef4444; --warning: #f59e0b; }
    body { font-family: 'Segoe UI', sans-serif; background: radial-gradient(circle at top, #1e293b, #0f172a); color: white; margin: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
    .container { width: 90%; max-width: 450px; padding: 25px; background: var(--glass); backdrop-filter: blur(10px); border-radius: 24px; border: 1px solid rgba(255,255,255,0.1); box-shadow: 0 25px 50px rgba(0,0,0,0.5); text-align: center;}
    h1 { font-size: 26px; letter-spacing: 2px; color: var(--accent); margin-bottom: 20px; }
    .grid-btns { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px; }
    .btn { padding: 18px; border: none; border-radius: 12px; font-weight: bold; cursor: pointer; transition: 0.3s; text-transform: uppercase; }
    .btn-drive { background: linear-gradient(135deg, var(--accent), #3a7bd5); color: white; }
    .btn-reverse { background: linear-gradient(135deg, var(--warning), #d97706); color: white; }
    .btn:active { transform: scale(0.95); filter: brightness(1.2); }
    .panel { background: rgba(255,255,255,0.05); padding: 15px; border-radius: 16px; margin-bottom: 15px; text-align: left; border: 1px solid rgba(255,255,255,0.05); }
    .label-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
    .val-badge { background: var(--accent); color: #0f172a; padding: 4px 10px; border-radius: 20px; font-weight: 800; font-size: 14px; }
    .rpm-badge { background: var(--danger); color: white; margin-left: 5px; }
    input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
    input[type=range]::-webkit-slider-runnable-track { height: 8px; background: rgba(255,255,255,0.1); border-radius: 4px; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 24px; width: 24px; border-radius: 50%; background: white; cursor: pointer; margin-top: -8px; box-shadow: 0 0 10px var(--accent); }
    .stop-btn { width: 100%; padding: 20px; background: linear-gradient(135deg, #f87171, var(--danger)); color: white; border: none; border-radius: 16px; font-size: 20px; font-weight: 800; cursor: pointer; }
    .btn-mini { flex: 1; background: rgba(255,255,255,0.1); color: white; border: none; padding: 10px 5px; border-radius: 8px; font-size: 12px; cursor: pointer; }
    .angle-container { display: flex; justify-content: space-between; gap: 5px; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>PROJECT TORI 🛶</h1>

    <div class="grid-btns">
      <button class="btn btn-drive" onclick="setDirection('forward')">Forward</button>
      <button class="btn btn-reverse" onclick="setDirection('reverse')">Reverse</button>
    </div>

    <div class="panel">
      <div class="label-row">
        <span>Thruster Power</span>
        <div>
          <span class="val-badge" id="speedValue">0 / 255</span>
          <span class="val-badge rpm-badge" id="rpmValue">0 RPM</span>
        </div>
      </div>
      <input type="range" min="0" max="255" value="0" id="speedSlider" oninput="updateSpeed(this.value)">
    </div>

    <div class="panel">
      <div class="label-row"><span>Front Fin</span><span class="val-badge" id="frontVal">97</span></div>
      <input type="range" min="0" max="180" value="97" id="frontSlider" oninput="updateServo('front', this.value)">
      <div class="angle-container">
        <button class="btn-mini" onclick="quickServo('front', 45)">45°</button>
        <button class="btn-mini" onclick="quickServo('front', 97)">Center</button>
        <button class="btn-mini" onclick="quickServo('front', 135)">135°</button>
      </div>
    </div>

    <div class="panel">
      <div class="label-row"><span>Back Fin</span><span class="val-badge" id="backVal">97</span></div>
      <input type="range" min="0" max="180" value="97" id="backSlider" oninput="updateServo('back', this.value)">
      <div class="angle-container">
        <button class="btn-mini" onclick="quickServo('back', 45)">45°</button>
        <button class="btn-mini" onclick="quickServo('back', 97)">Center</button>
        <button class="btn-mini" onclick="quickServo('back', 135)">135°</button>
      </div>
    </div>

    <button class="stop-btn" onclick="setDirection('stopped')">EMERGENCY STOP</button>
  </div>

  <script>
    const MAX_RPM = 21000;

    function setDirection(dir) {
      fetch('/action?dir=' + dir);
      if (dir == "stopped") {
        document.getElementById('speedSlider').value = 0;
        document.getElementById('speedValue').innerText = "0 / 255";
        document.getElementById('rpmValue').innerText = "0 RPM";

        document.getElementById('frontSlider').value = 97;
        document.getElementById('frontVal').innerText = "97";
        document.getElementById('backSlider').value = 97;
        document.getElementById('backVal').innerText = "97";
      }
    }
    function updateSpeed(val) {
      document.getElementById('speedValue').innerText = val + " / 255";
      let estimatedRpm = Math.round((val / 255) * MAX_RPM);
      document.getElementById('rpmValue').innerText = estimatedRpm.toLocaleString() + " RPM";
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

// --- Main Setup ---
void setup()
{
    Serial.begin(115200);

    // LED Init
    LED_RGB.begin();
    LED_RGB.setBrightness(50);
    LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); // Start Red
    LED_RGB.show();

    // PWM Hardware Config (Active-Low: 255 is off, 0 is full speed)
    ledcSetup(pwmChannel, pwmFreq, pwmResolution);
    ledcAttachPin(enablePin, pwmChannel);
    ledcWrite(pwmChannel, 255);

    // Servo Init
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    frontServo.setPeriodHertz(50);
    backServo.setPeriodHertz(50);
    frontServo.attach(frontServoPin, 500, 2400);
    backServo.attach(backServoPin, 500, 2500);
    frontServo.write(97);
    backServo.write(97);

    // Direction Pins Init
    pinMode(dirPin1, OUTPUT);
    pinMode(dirPin2, OUTPUT);
    digitalWrite(dirPin1, LOW);
    digitalWrite(dirPin2, LOW);

    // WiFi Access Point
    WiFi.softAP("Project_Tori", "12345678");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // --- Web Routes ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html); });

    server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("val")) {
      currentSpeed = request->getParam("val")->value().toInt();

      long estimatedRpm = map(currentSpeed, 0, 255, 0, 21000);
      Serial.print("Slider: ");
      Serial.print(currentSpeed);
      Serial.print(" | Est. RPM: ");
      Serial.println(estimatedRpm);

      // Apply Active-Low logic to hardware PWM
      ledcWrite(pwmChannel, 255 - currentSpeed);
      updateLED();
    }
    request->send(200, "text/plain", "OK"); });

    server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("target") && request->hasParam("val")) {
      String target = request->getParam("target")->value();
      int angle = request->getParam("val")->value().toInt();
      if (target == "front") frontServo.write(angle);
      else if (target == "back") backServo.write(angle);
      updateLED();
    }
    request->send(200, "text/plain", "OK"); });

    server.on("/action", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("dir")) {
      String dir = request->getParam("dir")->value();
      if (dir == "forward") {
        digitalWrite(dirPin1, HIGH);
        digitalWrite(dirPin2, LOW);
        isStopped = false;
    } else if (dir == "reverse") {
        digitalWrite(dirPin1, LOW);
        digitalWrite(dirPin2, LOW);
        isStopped = false;
    } else if (dir == "stopped") {
        digitalWrite(dirPin1, LOW);
        digitalWrite(dirPin2, LOW);
        ledcWrite(pwmChannel, 255); // Force Stop
        currentSpeed = 0;
        isStopped = true;
        frontServo.write(97);
        backServo.write(97);
      }
      updateLED();
    }
    request->send(200, "text/plain", "OK"); });

    server.begin();
}

// --- Main Loop (Rainbow Animation) ---
void loop()
{
    // Only dance if we are NOT stopped AND the slider is maxed out at 255
    if (!isStopped && currentSpeed == 255)
    {

        unsigned long currentMillis = millis();

        // Check if it's time to change the color
        if (currentMillis - previousRGBTime >= rgbInterval)
        {
            previousRGBTime = currentMillis;

            // Advance the color hue (Adafruit NeoPixel hue goes from 0 to 65535)
            rainbowHue += 256;

            // Set the color (Hue, Saturation, Brightness)
            LED_RGB.setPixelColor(0, LED_RGB.ColorHSV(rainbowHue, 255, 255));
            LED_RGB.show();
        }
    }
}
