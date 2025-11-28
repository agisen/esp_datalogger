// lib/Utils.h
#pragma once
#include <Arduino.h>

class Utils {
public:
  Utils();
  void begin();

  // WiFi connect (non-blocking attempt)
  void connectWiFi(const char* ssid, const char* pass);

  // Init NTP (configTime)
  void initNTP();

  // handle periodics (reconnect attempts, NTP checks)
  void handle();

  // Return current epoch time (NTP-backed if available, else millis-based)
  time_t getEpoch();

  // Formatting
  String weekNameFromEpoch(time_t t);

private:
  bool ntpInitialized;
  time_t bootEpoch; // approximate epoch at boot (if NTP unavailable)
  unsigned long bootMillis;
  const int WIFI_RETRY_INTERVAL = 30000; // try reconnect every 30s
  unsigned long lastWifiTry = 0;
};
