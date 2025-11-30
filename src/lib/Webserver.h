// lib/Webserver.h
#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "Storage.h"
#include "Utils.h"

class WebserverHandler {
public:
  WebserverHandler();
  void begin(Storage* storagePtr, Utils* utilsPtr, const String& httpPassword);
  void handleClient();
  bool isMeasurementActive() const { return measurementActive; }
  void setFlushCallback(void (*cb)()) { flushCallback = cb; }

private:
  ESP8266WebServer server;
  Storage* storage;
  Utils* utils;
  String password;
  bool measurementActive = true;
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
};
