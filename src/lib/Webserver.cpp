// lib/Webserver/Webserver.cpp
#include "Webserver.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

WebserverHandler::WebserverHandler() : server(80), storage(nullptr), utils(nullptr) {}

void WebserverHandler::begin(Storage* storagePtr, Utils* utilsPtr, const String& httpPassword) {
  storage = storagePtr;
  utils = utilsPtr;
  password = httpPassword;
  setupRoutes();
  server.begin();
  Serial.println(F("Webserver started on port 80"));
}

void WebserverHandler::handleClient() {
  server.handleClient();
}

void WebserverHandler::setupRoutes() {
  server.on("/", HTTP_GET, std::bind(&WebserverHandler::handleRoot, this));
  server.on("/api/weeks", HTTP_GET, std::bind(&WebserverHandler::handleGetWeeks, this));
  server.on("/api/storageinfo", HTTP_GET, std::bind(&WebserverHandler::handleGetStorageInfo, this));
  server.on("/api/download_week", HTTP_GET, std::bind(&WebserverHandler::handleDownloadWeek, this));
  server.on("/api/download_all", HTTP_GET, std::bind(&WebserverHandler::handleDownloadAll, this));
  server.on("/api/delete_all", HTTP_POST, std::bind(&WebserverHandler::handleDeleteAll, this));
  server.on("/api/delete_prev", HTTP_POST, std::bind(&WebserverHandler::handleDeletePrevious, this));
  server.on("/api/get_settings", HTTP_GET, std::bind(&WebserverHandler::handleGetSettings, this));
  server.on("/api/set_settings", HTTP_POST, std::bind(&WebserverHandler::handleSetSettings, this));
  server.onNotFound([](){
    // try to serve static files from LittleFS
    String path = server.uri();
    if (path == "/") path = "/index.html";
    if (LittleFS.exists(path)) {
      File f = LittleFS.open(path, "r");
      server.streamFile(f, "text/html");
      f.close();
      return;
    }
    server.send(404, "text/plain", "Not found");
  });
}

void WebserverHandler::handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(200, "text/plain", "Index missing");
  }
}

void WebserverHandler::handleGetWeeks() {
  std::vector<String> weeks;
  storage->listWeeks(weeks);
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (auto &w : weeks) arr.add(w);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebserverHandler::handleGetStorageInfo() {
  DynamicJsonDocument doc(512);
  uint64_t used = storage->usedBytes();
  uint64_t total = storage->totalBytes();
  int percent = (total>0)?(int)((used*100)/total):0;
  doc["used_bytes"] = (uint32_t)used;
  doc["total_bytes"] = (uint32_t)total;
  doc["percent"] = percent;

  // compute weeks possible per interval (1,5,10,15,20,30,60)
  JsonObject d = doc.createNestedObject("weeks_possible_for_interval");
  const int intervals[] = {1,5,10,15,20,30,60};
  for (int i=0;i<7;i++) {
    int T = intervals[i];
    // measurements/week = 10080 / T
    float measured = 10080.0f / (float)T;
    float bytesPerWeek = measured * 30.0f;
    if (bytesPerWeek <= 0.001f) bytesPerWeek = 1;
    int weeks = (int)( (float)total / bytesPerWeek );
    d[String(T)] = weeks;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebserverHandler::handleDownloadWeek() {
  if (!server.hasArg("week")) {
    server.send(400, "text/plain", "week query param required");
    return;
  }
  String week = server.arg("week"); // e.g. "2025-W03.csv" or "2025-W03"
  if (!week.endsWith(".csv")) week += ".csv";
  String path = "/" + week;
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "week not found");
    return;
  }
  File f = LittleFS.open(path, "r");
  server.setContentLength(f.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + week + "\"");
  server.streamFile(f, "text/csv");
  f.close();
}

void WebserverHandler::handleDownloadAll() {
  // we don't zip server-side. Return list of files as JSON so client can fetch and zip client-side
  std::vector<String> weeks;
  storage->listWeeks(weeks);
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (auto &w : weeks) arr.add(w);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void WebserverHandler::handleDeleteAll() {
  // Basic protection: require header "X-Auth: <password>"
  if (!server.hasHeader("X-Auth")) {
    server.send(401, "text/plain", "missing auth");
    return;
  }
  String val = server.header("X-Auth");
  if (val != password) {
    server.send(403, "text/plain", "forbidden");
    return;
  }
  storage->deleteAllWeeks();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebserverHandler::handleDeletePrevious() {
  if (!server.hasHeader("X-Auth")) {
    server.send(401, "text/plain", "missing auth");
    return;
  }
  String val = server.header("X-Auth");
  if (val != password) {
    server.send(403, "text/plain", "forbidden");
    return;
  }
  // Need current week param
  if (!server.hasArg("current")) {
    server.send(400, "text/plain", "current query param required");
    return;
  }
  String cur = server.arg("current");
  if (!cur.endsWith(".csv")) cur += ".csv";
  storage->deleteWeeksBefore(cur);
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebserverHandler::handleGetSettings() {
  DynamicJsonDocument doc(256);
  // For simplicity, read settings.json
  if (LittleFS.exists("/config/settings.json")) {
    File f = LittleFS.open("/config/settings.json", "r");
    String s;
    while (f.available()) s += (char)f.read();
    f.close();
    server.send(200, "application/json", s);
  } else {
    doc["interval"] = 300;
    doc["wifi_ssid"] = "";
    doc["wifi_pass"] = "";
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  }
}

void WebserverHandler::handleSetSettings() {
  if (!server.hasHeader("X-Auth")) {
    server.send(401, "text/plain", "missing auth");
    return;
  }
  String val = server.header("X-Auth");
  if (val != password) {
    server.send(403, "text/plain", "forbidden");
    return;
  }
  // Expect JSON body with interval, wifi_ssid, wifi_pass
  String body = server.arg("plain");
  DynamicJsonDocument doc(512);
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "text/plain", "invalid json");
    return;
  }
  // write settings.json
  if (!LittleFS.exists("/config")) {
    LittleFS.mkdir("/config");
  }
  File f = LittleFS.open("/config/settings.json", "w");
  if (!f) {
    server.send(500, "text/plain", "cannot save settings");
    return;
  }
  serializeJson(doc, f);
  f.close();

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}
