// lib/Webserver.h
#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "Storage.h"
#include "Utils.h"

// ---- Globals aus Hauptprogramm ----
extern uint32_t g_interval_seconds;
extern String g_wifi_ssid;
extern String g_wifi_pass;
extern String g_http_password;
// ----------------------------------

class WebserverHandler {
public:
  WebserverHandler();
  void begin(Storage* storagePtr, Utils* utilsPtr, const String& httpPassword);
  void handleClient();
  bool isMeasurementActive() const { return measurementActive; }
  void setIntervalChangedCallback(void (*cb)()) { intervalChangedCallback = cb; }
  void setFlushCallback(void (*cb)()) { flushCallback = cb; }
  void updateLastMeasurement(float t, float h, uint32_t ts) {
        lastTemp = t;
        lastHum = h;
        lastTs = ts;
  }

private:
  ESP8266WebServer server;
  Storage* storage;
  Utils* utils;
  String password;
  float lastTemp = 0;
  float lastHum = 0;
  uint32_t lastTs = 0;
  bool measurementActive = true;
  void (*intervalChangedCallback)() = nullptr;
  void (*flushCallback)() = nullptr;

  void setupRoutes();
  // handlers
  void handleRoot();
  void handleGetWeeks();
  void handleGetStorageInfo();
  void handleDownloadWeek();
  void handleDownloadAll(); // returns list only - actual ZIP is client-side
  void handleDeleteAll();
  void handleDeletePrevious();
  void handleGetSettings();
  void handleSetSettings();
  void handleMeasurementStatus();
  void handleToggleMeasurement();
  void handleFlushBuffer();
  void handleSetInterval();
  void handleLastMeasurement();
};
