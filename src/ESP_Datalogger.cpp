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

// Forward
void flushBuffer();
void performMeasurement();

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("=== Datalogger starting ==="));

  // LittleFS mounten
  if (!LittleFS.begin()) {
    Serial.println(F("ERROR: LittleFS mount failed"));
    // trotzdem versuchen weiterzumachen - aber Storage-Init wird fehlschlagen
  }

  // Storage init
  storage.begin();

  // Utils init (WiFi & NTP)
  utils.begin();

  // Load settings from LittleFS (settings.json)
  if (!storage.loadSettings(g_interval_seconds, g_wifi_ssid, g_wifi_pass, g_http_password)) {
    Serial.println(F("No settings.json found, using defaults."));
  } else {
    Serial.println(F("Settings loaded from settings.json"));
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

  // Start measure timer immediately (first measurement after interval)
  lastMeasureMillis = millis();

  Serial.println(F("Setup complete."));
}

void loop() {
  // Handle web server
  webserver.handleClient();

  // Periodic tasks from utils (NTP check, reconnection attempts)
  utils.handle();

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

// Perform a measurement and push into buffer (then flush when buffer full)
void performMeasurement() {
  Update current timestamp (NTP-backed if available)
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

  Serial.printf("Measured: %.2f C, %.2f %% at %lu\n", t, h, (unsigned long)ts);

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
  if (bufferCount == 0) return;

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
