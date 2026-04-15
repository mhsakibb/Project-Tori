#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// --- Configuration ---
const char* ssid = "House_9A🛜";
const char* password = "oikireoikire";

// --- Hardware Pins ---
const int enablePin = 4;
const int dirPin1 = 6;
const int dirPin2 = 5;
const int frontServoPin = 7;
const int backServoPin = 8;
const int rgbPin = 48;

// --- PWM Constants ---
const int pwmFreq = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

// --- Objects ---
Servo frontServo;
Servo backServo;
AsyncWebServer server(80);
Adafruit_NeoPixel LED_RGB(1, rgbPin, NEO_GRB + NEO_KHZ800);

// --- Global States ---
int currentSpeed = 0;
bool isStopped = true;
unsigned long previousRGBTime = 0;
const long rgbInterval = 5;
uint16_t rainbowHue = 0;

// --- Logic: Motor & LED Control ---
void applyMotorLogic() {
    // Active-Low: 255 is off, 0 is full power
    ledcWrite(pwmChannel, 255 - currentSpeed);

    // LED Feedback
    if (isStopped || currentSpeed == 0) {
        LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); // Red
    } else if (currentSpeed < 255) {
        LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0)); // Green
    }
    LED_RGB.show();
}

void emergencyStop() {
    isStopped = true;
    currentSpeed = 0;
    digitalWrite(dirPin1, LOW);
    digitalWrite(dirPin2, LOW);
    frontServo.write(97);
    backServo.write(97);
    applyMotorLogic();
    Serial.println("SYSTEM_HALTED");
}

// --- HTML UI (Stored in Flash) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Project Tori 🛶</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0f172a; --glass: rgba(30, 41, 59, 0.7); --accent: #00d2ff; --danger: #ef4444; }
    body { font-family: sans-serif; background: var(--bg); color: white; display: flex; justify-content: center; padding: 20px; }
    .container { width: 100%; max-width: 400px; background: var(--glass); padding: 20px; border-radius: 20px; text-align: center; backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.1); }
    .btn { width: 48%; padding: 15px; margin: 5px 0; border-radius: 10px; border: none; font-weight: bold; cursor: pointer; }
    .stop-btn { width: 100%; background: var(--danger); color: white; margin-top: 20px; padding: 20px; font-size: 1.2rem; }
    input[type=range] { width: 100%; margin: 20px 0; }
    .label { display: flex; justify-content: space-between; }
  </style>
</head>
<body>
  <div class="container">
    <h1>TORI CONTROL 🛶</h1>
    <div style="display:flex; justify-content: space-between;">
        <button class="btn" style="background:#3a7bd5; color:white;" onclick="fetch('/action?dir=forward')">FORWARD</button>
        <button class="btn" style="background:#f59e0b; color:white;" onclick="fetch('/action?dir=reverse')">REVERSE</button>
    </div>
    <div class="label"><span>Thruster</span><span id="sv">0</span></div>
    <input type="range" min="0" max="255" value="0" oninput="document.getElementById('sv').innerText=this.value; fetch('/speed?val='+this.value)">

    <div class="label"><span>Front Fin</span></div>
    <input type="range" min="0" max="180" value="97" oninput="fetch('/servo?target=front&val='+this.value)">

    <button class="stop-btn" onclick="location.reload(); fetch('/action?dir=stopped')">EMERGENCY STOP</button>
  </div>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200); // USB Control Port

    // Hardware Init
    LED_RGB.begin();
    LED_RGB.setBrightness(50);

    ledcSetup(pwmChannel, pwmFreq, pwmResolution);
    ledcAttachPin(enablePin, pwmChannel);

    pinMode(dirPin1, OUTPUT);
    pinMode(dirPin2, OUTPUT);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    frontServo.attach(frontServoPin, 500, 2400);
    backServo.attach(backServoPin, 500, 2500);

    emergencyStop();

    // WiFi Setup (Station Mode for Domain/Port Forwarding)
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());

    // --- Web Routes ---
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("val")) {
            currentSpeed = request->getParam("val")->value().toInt();
            applyMotorLogic();
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("target") && request->hasParam("val")) {
            String target = request->getParam("target")->value();
            int angle = request->getParam("val")->value().toInt();
            if (target == "front") frontServo.write(angle);
            else if (target == "back") backServo.write(angle);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/action", HTTP_GET, [](AsyncWebServerRequest *request){
        String dir = request->getParam("dir")->value();
        if (dir == "forward") { digitalWrite(dirPin1, HIGH); digitalWrite(dirPin2, LOW); isStopped = false; }
        else if (dir == "reverse") { digitalWrite(dirPin1, LOW); digitalWrite(dirPin2, HIGH); isStopped = false; }
        else if (dir == "stopped") { emergencyStop(); }
        applyMotorLogic();
        request->send(200, "text/plain", "OK");
    });

    server.begin();
}

void loop() {
    // 1. Desktop USB Control (Parsing Serial Commands)
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.startsWith("SPD:")) {
            currentSpeed = cmd.substring(4).toInt();
            applyMotorLogic();
        } else if (cmd == "STOP") {
            emergencyStop();
        } else if (cmd.startsWith("F_SRV:")) {
            frontServo.write(cmd.substring(6).toInt());
        }
    }

    // 2. Rainbow Animation for Max Speed
    if (!isStopped && currentSpeed == 255) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousRGBTime >= rgbInterval) {
            previousRGBTime = currentMillis;
            rainbowHue += 256;
            LED_RGB.setPixelColor(0, LED_RGB.ColorHSV(rainbowHue, 255, 255));
            LED_RGB.show();
        }
    }
}
