#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>

// --- Configuration ---
const char *ssid = "tori";
const char *password = "12345678";

// --- Hardware Pins ---
const int enablePin = 4;
const int dirPin1 = 6;
const int dirPin2 = 5;
const int frontServoPin = 7;
const int backServoPin = 8;
const int rgbPin = 48;
const int tempPin = 11;
const int gpsRxPin = 38;
const int gpsTxPin = 40;

// --- PWM Constants ---
const int pwmFreq = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

// --- Objects ---
Servo frontServo;
Servo backServo;
AsyncWebServer server(80);
Adafruit_NeoPixel LED_RGB(1, rgbPin, NEO_GRB + NEO_KHZ800);
OneWire oneWire(tempPin);
DallasTemperature tempSensor(&oneWire);
HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;

// --- Global States & Thread Safety Flags ---
int currentSpeed = 0;
int targetServoAngle = 97;
bool isStopped = true;
bool hardwareUpdateRequired = false;
bool servoUpdateRequired = false;

// NEW: Global variable to store the latest temperature
float currentTemp = 0.0;
float currentLat = 0.0;
float currentLng = 0.0;

unsigned long previousRGBTime = 0;
const long rgbInterval = 5;
uint16_t rainbowHue = 0;

unsigned long previousTempTime = 0;
const long tempInterval = 2000;

// --- Logic Functions (Called only from Loop for thread safety) ---
void applyMotorLogic() {
  ledcWrite(pwmChannel, 255 - currentSpeed);

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
  targetServoAngle = 97;
  digitalWrite(dirPin1, LOW);
  digitalWrite(dirPin2, LOW);
  frontServo.write(97);
  backServo.write(97);
  applyMotorLogic();
  Serial.println("EVENT:HALTED");
}

void handleTemperature() {
  if (millis() - previousTempTime >= tempInterval) {
    previousTempTime = millis();
    tempSensor.requestTemperatures();
    float tempC = tempSensor.getTempCByIndex(0);

    if (tempC != DEVICE_DISCONNECTED_C) {
      currentTemp = tempC; // NEW: Update the global variable
      Serial.print("TEMP OF THE MAIN BOARD: ");
      Serial.print(currentTemp);
      Serial.println(" °C");
    } else {
      Serial.println("TEMP: SENSOR ERROR (Check Wiring)");
    }
  }
}

void handleGPS() {
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // Send satellite count every 2 seconds (same interval as temp)
  static unsigned long lastGpsTime = 0;
  if (millis() - lastGpsTime >= 2000) {
    lastGpsTime = millis();

    if (gps.charsProcessed() < 10) {
      Serial.println("GPS: WIRING_ERROR");
    } else {
      Serial.print("GPS_SAT: ");
      Serial.println(gps.satellites.value());

      if (gps.location.isValid()) {
        currentLat = gps.location.lat();
        currentLng = gps.location.lng();

        Serial.print("GPS: ");
        Serial.print(currentLat, 6);
        Serial.print(",");
        Serial.println(currentLng, 6);
      }
    }
  }
}

// --- HTML UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Project Tori 🛶</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root { --bg: #0f172a; --glass: rgba(30, 41, 59, 0.7); --accent: #00d2ff; --danger: #ef4444; }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: white; display: flex; justify-content: center; padding: 20px; }
    .container { width: 100%; max-width: 400px; background: var(--glass); padding: 30px; border-radius: 24px; text-align: center; backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.1); }
    .btn { width: 48%; padding: 15px; margin: 5px 0; border-radius: 12px; border: none; font-weight: bold; cursor: pointer; transition: 0.2s; }
    .stop-btn { width: 100%; background: var(--danger); color: white; margin-top: 20px; padding: 20px; font-size: 1.2rem; border-radius: 12px; font-weight: 800; border: none; cursor: pointer; }
    input[type=range] { width: 100%; margin: 20px 0; accent-color: var(--accent); }
    .label { display: flex; justify-content: space-between; font-weight: bold; margin-top: 10px;}
    .hint { font-size: 0.9rem; color: #00d2ff; margin-top: 20px; background: rgba(0,210,255,0.1); padding: 10px; border-radius: 8px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>TORI COMMAND 🛶</h1>
    <div style="display:flex; justify-content: space-between;">
        <button class="btn" style="background:#3a7bd5; color:white;" onclick="sendAction('forward')">FORWARD</button>
        <button class="btn" style="background:#f59e0b; color:white;" onclick="sendAction('reverse')">REVERSE</button>
    </div>
    <div class="label"><span>Thruster Power</span><span id="sv">0</span></div>
    <input type="range" id="speedIdx" min="0" max="255" value="0" oninput="updateSpeed(this.value)">
    <div class="label"><span>Fin Steering</span><span id="fv">97</span></div>
    <input type="range" id="frontIdx" min="40" max="150" value="97" oninput="updateServo('front', this.value)">
    <button class="stop-btn" onclick="sendAction('stopped')">EMERGENCY STOP</button>
    <div class="hint" id="keyHint">Use ↑ ↓ ← → and Spacebar</div>
  </div>
  <script>
    function sendAction(dir) {
        fetch('/action?dir=' + dir);
        if(dir === 'stopped') {
            document.getElementById('speedIdx').value = 0; document.getElementById('sv').innerText = 0;
            document.getElementById('frontIdx').value = 97; document.getElementById('fv').innerText = 97;
        }
    }
    function updateSpeed(val) { document.getElementById('sv').innerText = val; fetch('/speed?val=' + val); }
    function updateServo(target, val) { document.getElementById('fv').innerText = val; fetch('/servo?target=' + target + '&val=' + val); }

    window.addEventListener('keydown', function(e) {
      let sInput = document.getElementById('speedIdx');
      let fInput = document.getElementById('frontIdx');
      switch(e.key) {
        case 'ArrowUp': sendAction('forward'); let sUp = Math.min(parseInt(sInput.value) + 25, 255); updateSpeed(sUp); sInput.value = sUp; break;
        case 'ArrowDown': sendAction('reverse'); let sDown = Math.min(parseInt(sInput.value) + 25, 255); updateSpeed(sDown); sInput.value = sDown; break;
        case 'ArrowLeft': let a = Math.max(parseInt(fInput.value) - 15, 40); updateServo('front', a); fInput.value = a; break;
        case 'ArrowRight': let d = Math.min(parseInt(fInput.value) + 15, 150); updateServo('front', d); fInput.value = d; break;
        case ' ': sendAction('stopped'); break;
      }
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  GPS_Serial.begin(9600, SERIAL_8N1, gpsRxPin, gpsTxPin);

  LED_RGB.begin();
  LED_RGB.setBrightness(50);

  tempSensor.begin();

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(enablePin, pwmChannel);
  pinMode(dirPin1, OUTPUT);
  pinMode(dirPin2, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  frontServo.attach(frontServoPin, 500, 2400);
  backServo.attach(backServoPin, 500, 2500);

  emergencyStop();

  WiFi.begin(ssid, password);

  // Wait max 5 seconds for WiFi, then continue anyway!
  int wifiWait = 0;
  while (WiFi.status() != WL_CONNECTED && wifiWait < 10) {
    delay(500);
    Serial.print(".");
    wifiWait++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Ready! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi Failed! Proceeding with USB only.");
  }

  // --- Web Handlers ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });

  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
    String tempString = String(currentTemp);
    AsyncWebServerResponse *response =
        request->beginResponse(200, "text/plain", tempString);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("val")) {
      currentSpeed = request->getParam("val")->value().toInt();
      hardwareUpdateRequired = true;
    }
    request->send(200);
  });

  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("target") && request->hasParam("val")) {
      int angle = request->getParam("val")->value().toInt();
      String target = request->getParam("target")->value();

      if (target == "front") {
        frontServo.write(angle);
      } else if (target == "back") {
        backServo.write(angle);
      }
    }
    AsyncWebServerResponse *response =
        request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.on("/action", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("dir")) {
      String dir = request->getParam("dir")->value();
      if (dir == "forward") {
        digitalWrite(dirPin1, HIGH);
        digitalWrite(dirPin2, LOW);
        isStopped = false;
      } else if (dir == "reverse") {
        digitalWrite(dirPin1, LOW);
        digitalWrite(dirPin2, HIGH);
        isStopped = false;
      } else {
        isStopped = true;
        currentSpeed = 0;
      }
      hardwareUpdateRequired = true;
    }
    request->send(200);
  });

  server.begin();
}

void loop() {
  if (hardwareUpdateRequired) {
    applyMotorLogic();
    hardwareUpdateRequired = false;
  }

  if (servoUpdateRequired) {
    frontServo.write(targetServoAngle);
    servoUpdateRequired = false;
  }

  handleTemperature();
  handleGPS();

  // ==========================================
  // UPDATED: Added DIR:FWD and DIR:REV parsing
  // ==========================================
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    Serial.println("ACK: " + cmd);

    if (cmd == "DIR:FWD") {
      digitalWrite(dirPin1, HIGH);
      digitalWrite(dirPin2, LOW);
      isStopped = false;
      hardwareUpdateRequired = true;
    } else if (cmd == "DIR:REV") {
      digitalWrite(dirPin1, LOW);
      digitalWrite(dirPin2, HIGH);
      isStopped = false;
      hardwareUpdateRequired = true;
    } else if (cmd.startsWith("SPD:")) {
      currentSpeed = cmd.substring(4).toInt();
      applyMotorLogic();
    } else if (cmd.startsWith("F_SRV:")) {
      frontServo.write(cmd.substring(6).toInt());
    } else if (cmd.startsWith("B_SRV:")) {
      backServo.write(cmd.substring(6).toInt());
    } else if (cmd == "STOP") {
      emergencyStop();
    }
  }

  if (!isStopped && currentSpeed == 255) {
    if (millis() - previousRGBTime >= rgbInterval) {
      previousRGBTime = millis();
      rainbowHue += 256;
      LED_RGB.setPixelColor(0, LED_RGB.ColorHSV(rainbowHue, 255, 255));
      LED_RGB.show();
    }
  }

  yield();
}
