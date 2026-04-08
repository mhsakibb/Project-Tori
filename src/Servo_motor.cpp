// #include <ESP32Servo.h>
// #include <Adafruit_NeoPixel.h>

// // Define pins and constants
// const int PIN_SG90 = 4;

// // Use the standard macro for the ESP32-S3 built-in LED
// // Note: If this throws an error, replace 'RGB_BUILTIN' with 48
// const int PIN_NEOPIXEL = 48;
// const int NUM_PIXELS = 1;

// // Initialize objects
// Adafruit_NeoPixel LED_RGB(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
// Servo sg90;

// void setup() {
//     // Configure Servo
//     sg90.setPeriodHertz(50);
//     sg90.attach(PIN_SG90, 500, 2400);

//     // Initialize Built-in NeoPixel
//     LED_RGB.begin();
//     LED_RGB.show(); // Turn off on startup
// }

// void loop() {
//     // --- Sweep from 0° to 180° ---
//     LED_RGB.setPixelColor(0, LED_RGB.Color(255, 0, 0)); // Red
//     LED_RGB.show();

//     for (int pos = 0; pos <= 180; pos += 1) {
//         sg90.write(pos);
//         delay(10);
//     }

//     // --- Sweep from 180° to 0° ---
//     LED_RGB.setPixelColor(0, LED_RGB.Color(0, 255, 0)); // Green
//     LED_RGB.show();

//     for (int pos = 180; pos >= 0; pos -= 1) {
//         sg90.write(pos);
//         delay(10);
//     }
// }


// #include <OneWire.h>
// #include <DallasTemperature.h>

// // Data wire is plugged into GPIO 5 on the ESP32-S3
// #define ONE_WIRE_BUS 5

// // Setup a oneWire instance to communicate with any OneWire devices
// OneWire oneWire(ONE_WIRE_BUS);

// // Pass our oneWire reference to Dallas Temperature sensor
// DallasTemperature sensors(&oneWire);

// void setup()
// {
//     // Start serial communication for debugging
//     Serial.begin(115200);
//     Serial.println("ESP32-S3 DS18B20 Temperature Test");

//     // Start up the DallasTemperature library
//     sensors.begin();
// }

// void loop()
// {
//     // Call sensors.requestTemperatures() to issue a global temperature
//     // request to all devices on the bus
//     Serial.print("Requesting temperatures...");
//     sensors.requestTemperatures();
//     Serial.println("DONE");

//     // Read the temperature in Celsius
//     // Index 0 refers to the first sensor on the wire
//     float tempC = sensors.getTempCByIndex(0);

//     // Read the temperature in Fahrenheit (optional)
//     float tempF = sensors.getTempFByIndex(0);

//     // Check if reading was successful
//     if (tempC == DEVICE_DISCONNECTED_C)
//     {
//         Serial.println("Error: Could not read temperature data. Check connections and resistor.");
//     }
//     else
//     {
//         Serial.print("Temperature: ");
//         Serial.print(tempC);
//         Serial.print(" °C  |  ");
//         Serial.print(tempF);
//         Serial.println(" °F");
//     }

//     // Wait 2 seconds before the next reading
//     delay(2000);
// }
