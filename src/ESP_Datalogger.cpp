/* Datalogger.ino
  Haupt-Sketch: Setup, Loop, Timer-Logik, RAM-Puffer
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "lib/Sensor.h"
#include "lib/Storage.h"
#include "lib/Utils.h"
#include "lib/Webserver.h"

// === Konfiguration (falls settings.json fehlt, werden diese Defaults genutzt) ===
#define DEFAULT_INTERVAL_SECONDS 300   // 5 min default
#define DEFAULT_WIFI_SSID "DEIN_WLAN"
#define DEFAULT_WIFI_PASS "DEIN_PASSWORT"
#define DEFAULT_HTTP_PASSWORD "admin" // für Lösch-APIs (falls genutzt)

// RAM-Puffergröße (Anzahl Measurements vor Batch-Write)
#define BUFFER_SIZE 10

// Globale Objekte
Sensor sensor;
Storage storage;
Utils utils;
WebserverHandler webserver;

////////////////////
// IDEE: BUFFER SIZE AN MESSINTERVALL ANPASSEN => KONSTANTE ZAHL VON SCHREIBZYKLEN PRO ZEIT
////////////////////
// RAM-Puffer
Measurement buffer[BUFFER_SIZE];
uint8_t bufferCount = 0;

// Timer
unsigned long lastMeasureMillis = 0;
unsigned long measureIntervalMs = DEFAULT_INTERVAL_SECONDS * 1000UL;

// Settings (werden aus settings.json geladen, falls vorhanden)
String g_wifi_ssid = DEFAULT_WIFI_SSID;
String g_wifi_pass = DEFAULT_WIFI_PASS;
uint32_t g_interval_seconds = DEFAULT_INTERVAL_SECONDS;
String g_http_password = DEFAULT_HTTP_PASSWORD;

// Strict mode flag
bool strictModeEnabled = true; // falls true: kein Logging wenn Jahr < 2020

// Forward declaration
void flushBuffer();
void performMeasurement();
void blinkLed(unsigned long duration);

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("=== Datalogger starting ==="));
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); // LED will be on during setup

  // // LittleFS mounten --> Doppelt, erfolgt auch in storage.begin()
  // if (!LittleFS.begin()) {
  //   Serial.println(F("ERROR: LittleFS mount failed"));
  // }

  // Storage init
  storage.begin();

  // Utils init (WiFi & NTP)
  utils.begin();

  // Load settings from LittleFS (settings.json)
  if (!storage.loadSettings(g_interval_seconds, g_wifi_ssid, g_wifi_pass, g_http_password)) {
    Serial.println(F("Error in Storage.loadSettings, using defaults."));
  }

  // Apply interval
  measureIntervalMs = (unsigned long)g_interval_seconds * 1000UL;

  // Connect WiFi (non-blocking attempt inside utils)
  utils.connectWiFi(g_wifi_ssid.c_str(), g_wifi_pass.c_str());

  // init NTP (will be attempted in utils)
  utils.initNTP();

  // Sensor init
  sensor.begin();

  // Webserver init (serves files from LittleFS/data)
  webserver.begin(&storage, &utils, g_http_password);
  webserver.setFlushCallback(flushBuffer);

  // Start measure timer immediately (first measurement after interval)
  lastMeasureMillis = millis();

  digitalWrite(LED_BUILTIN, HIGH); // Ensure LED starts off after setup
  Serial.println(F("Setup complete."));
  delay(300);
  blinkLed(300);
  blinkLed(300);
  blinkLed(300);
}

void loop() {
  // Periodic tasks from utils (NTP check, reconnection attempts)
  utils.handle();

  // Handle web server
  webserver.handleClient();

  if (webserver.isMeasurementActive()) {
    // Measurement (non-blocking)
    unsigned long nowMs = millis();
    if ((nowMs - lastMeasureMillis) >= measureIntervalMs) {
      lastMeasureMillis = nowMs;
      performMeasurement();
    }

    // Optionally: flush buffer periodically if not full for graceful shutdown safeguards
    // (e.g., every minute) - optional
    // (skipped here to minimize flash writes)
  }
}

// Perform a measurement and push into buffer (then flush when buffer full)
void performMeasurement() {
  // Update current timestamp (NTP-backed if available)
  time_t ts = utils.getEpoch();
  tm timeinfo;
  gmtime_r(&ts, &timeinfo);
  int year = timeinfo.tm_year + 1900;

  // Strict-Mode: ignore measurements if time year < 2020
  if (strictModeEnabled && year < 2020) {
    Serial.println(F("Strict mode active: time invalid - skipping measurement"));
    return;
  }

  // Read sensor (the Sensor class handles retries & NaN filtering)
  float t = NAN, h = NAN;
  bool ok = sensor.read(t, h);

  if (!ok || isnan(t) || isnan(h)) {
    Serial.println(F("Sensor read failed or NaN - measurement discarded"));
    return;
  }

  // Print with ts if available, else without ts
  if (ts) {
    Serial.printf("Measured: %.2f C, %.2f %% at %lu\n", t, h, (unsigned long)ts);
  } else {
    Serial.printf("Measured: %.2f C, %.2f %%\n", t, h);
  }
  // Serial.printf("Measured: %.2f C, %.2f %%\n", t, h);
  blinkLed(500);

  // Push to buffer
  buffer[bufferCount].ts = (uint32_t)ts;
  buffer[bufferCount].temp = t;
  buffer[bufferCount].hum = h;
  bufferCount++;

  if (bufferCount >= BUFFER_SIZE) {
    flushBuffer();
  }
}

// Flush RAM buffer to LittleFS (writes batch)
void flushBuffer() {
  if (bufferCount == 0) {
    Serial.println(F("Buffer is empty. Nothing to flush to storage"));
    return;
  }

  // Build array to write
  Measurement tmp[BUFFER_SIZE];
  memcpy(tmp, buffer, sizeof(Measurement)*bufferCount);

  // Attempt to save; Storage will check 85% rule and delete oldest files if necessary
  bool saved = storage.saveBatch(tmp, bufferCount);

  if (saved) {
    Serial.printf("Flushed %u entries to storage\n", bufferCount);
    bufferCount = 0;
  } else {
    Serial.println(F("ERROR: Failed to flush buffer to storage"));
    // Keep buffer (to retry later) - but risk of data loss if reboot
  }
}

// Turn the global LED ON for a specified duration, default duration is 500 ms
void blinkLed(unsigned long duration = 500) {
  // Serial.println("Blink!");
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
                                    // but actually the LED is on; this is because
                                    // it is active low on the ESP-01)
  delay(duration);                  // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(duration);
}