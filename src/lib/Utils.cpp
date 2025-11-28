// lib/Utils.cpp
#include "Utils.h"
#include <ESP8266WiFi.h>
#include "time.h"

Utils::Utils() : ntpInitialized(false), bootEpoch(0), bootMillis(0), lastWifiTry(0) {}

void Utils::begin() {
  bootEpoch = 0;
  bootMillis = millis();
}

void Utils::connectWiFi(const char* ssid, const char* pass) {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if ((now - lastWifiTry) < WIFI_RETRY_INTERVAL) return;
  lastWifiTry = now;
  Serial.printf("Utils: connecting to WiFi %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
}

void Utils::initNTP() {
  // Use system configTime
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  ntpInitialized = true;
}

void Utils::handle() {
  // if not connected attempt reconnect (WiFi reconnect is handled by system but we can try)
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi(NULL, NULL); // will just attempt reconnect
  } else {
    // if connected and ntp not initialized properly, request
    if (!ntpInitialized) {
      initNTP();
    }
  }
}

time_t Utils::getEpoch() {
  time_t now;
  time(&now);
  if (now > 1609459200UL) { // > 2021-01-01
    return now;
  } else {
    // fallback: approximate using bootMillis
    unsigned long deltaMs = millis() - bootMillis;
    time_t approx = bootEpoch + (deltaMs / 1000UL);
    return approx;
  }
}

String Utils::weekNameFromEpoch(time_t t) {
  tm tmstruct;
  gmtime_r(&t, &tmstruct);
  int year = tmstruct.tm_year + 1900;
  int week = (tmstruct.tm_yday / 7) + 1;
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-W%02d.csv", year, week);
  return String(buf);
}
