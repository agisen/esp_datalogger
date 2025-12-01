#include "Storage.h"
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>

Storage::Storage() {}

void Storage::begin() {
  // LittleFS already mounted in sketch, but we can check
  if (!LittleFS.begin()) {
    Serial.println(F("Storage: LittleFS.begin() failed"));
  } else {
    Serial.println(F("Storage: LittleFS ready"));
  }

    debugListFiles();
}

FsUsage Storage::getFsUsage() {
  FSInfo info;
  LittleFS.info(info);
  return { info.usedBytes, info.totalBytes };
}

bool Storage::loadSettings(uint32_t &intervalSeconds, String &ssid, String &pass, String &httpPassword) {
  if (!LittleFS.exists("/settings.json")) {
    Serial.println(F("Storage: settings.json does not exist"));
    return false;
  }

  File f = LittleFS.open("/settings.json", "r");
  if (!f) {
    Serial.println(F("Storage: failed to open settings.json"));
    return false;
  }

  size_t size = f.size();
  std::unique_ptr<char[]> buf(new char[size + 1]);
  f.readBytes(buf.get(), size);
  buf[size] = 0;
  f.close();

  DynamicJsonDocument doc(512);
  auto err = deserializeJson(doc, buf.get());
  if (err) {
    Serial.println(F("Storage: settings.json parse error"));
    return false;
  }
  if (doc["interval"].is<uint32_t>())
    intervalSeconds = doc["interval"].as<uint32_t>();
  if (doc["wifi_ssid"].is<const char*>())
    ssid = doc["wifi_ssid"].as<const char*>();
  if (doc["wifi_pass"].is<const char*>())
    pass = doc["wifi_pass"].as<const char*>();
  if (doc["http_password"].is<const char*>())
    httpPassword = doc["http_password"].as<const char*>();

  Serial.println(F("Storage: Settings loaded from settings.json"));
  return true;
}

bool Storage::saveSettings(uint32_t intervalSeconds, const String &ssid, const String &pass, const String &httpPassword) {

  DynamicJsonDocument doc(512);

  // exakt dieselben Keys wie loadSettings()
  doc["interval"] = intervalSeconds;
  doc["wifi_ssid"] = ssid;
  doc["wifi_pass"] = pass;
  doc["http_password"] = httpPassword;

  File f = LittleFS.open("/settings.json", "w");
  if (!f) {
    Serial.println(F("Storage: failed to open settings.json for writing"));
    return false;
  }

  if (serializeJson(doc, f) == 0) {
    Serial.println(F("Storage: failed to write settings.json"));
    f.close();
    return false;
  }

  f.close();
  Serial.println(F("Storage: settings.json saved"));

  return true;
}

static String weekNameFromTime(time_t t) {
  // simple week number (year-week) using day-of-year/7 (not strict ISO)
  tm tmstruct;
  gmtime_r(&t, &tmstruct);
  unsigned int year = tmstruct.tm_year + 1900;
  unsigned int week = (tmstruct.tm_yday / 7) + 1; // 1..53
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-W%02d", year, week);
  return String(buf);
}

bool Storage::saveBatch(Measurement *arr, uint8_t len) {
  if (len == 0) return true;

  // Estimate bytes needed: roughly len * 40 (timestamp;temp;hum\n)
  uint32_t estimated = (uint32_t)len * 40;

  // Check 85%-rule
  FsUsage fs = getFsUsage();
  uint64_t used  = fs.used;
  uint64_t total = fs.total;
  uint64_t threshold = (total * 85) / 100;
  Serial.printf("Storage: used=%llu total=%llu threshold=%llu need=%u\n", (unsigned long long)used, (unsigned long long)total, (unsigned long long)threshold, estimated);

  // while writing would exceed threshold, delete oldest file(s)
  while ((used + estimated) > threshold) {
    bool deleted = deleteOldestWeek();
    if (!deleted) {
      Serial.println(F("Storage: cannot free more space"));
      return false;
    }
    fs = getFsUsage();
    used = fs.used;
  }

  // Determine current week file name (use time of first measurement)
  String week = weekNameFromTime((time_t)arr[0].ts);
  String path = "/" + week + ".csv";

  // Open file for append
  File f = LittleFS.open(path, "a");
  if (!f) {
    Serial.printf("Storage: failed to open %s for append\n", path.c_str());
    return false;
  }

  // Write all entries
  for (uint8_t i = 0; i < len; i++) {
    char line[32];
    snprintf(line, sizeof(line), "%lu;%.1f;%.1f\n", (unsigned long)arr[i].ts, arr[i].temp, arr[i].hum);
    f.print(line);
  }
  f.close();
  return true;
}

// -----------------------------------------

void Storage::listWeeks(std::vector<String> &outWeeks) {
  outWeeks.clear();
  Dir dir = LittleFS.openDir("/");
  Serial.println("Listing week files in LittleFS:");
  while (dir.next()) {
    String name = dir.fileName();
    if (name.endsWith(".csv")) {
      outWeeks.push_back(name);
      Serial.println("  Found week file: " + name);
    }
  }
}

bool Storage::deleteOldestWeek() {
  std::vector<String> weeks;
  listWeeks(weeks);
  if (weeks.empty()) return false;
  std::sort(weeks.begin(), weeks.end());
  String oldest = weeks.front();
  String path = "/" + oldest;
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
    Serial.printf("Storage: deleted oldest file %s\n", oldest.c_str());
    return true;
  }
  return false;
}

void Storage::deleteAllWeeks() {
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName();
    if (name.endsWith(".csv")) {
      LittleFS.remove(name);
    }
  }
}

void Storage::deleteWeeksBefore(const String &currentWeek) {
  std::vector<String> weeks;
  listWeeks(weeks);
  for (String &w : weeks) {
    if (w < currentWeek) {
      LittleFS.remove("/" + w);
      Serial.printf("Storage: deleted %s\n", w.c_str());
    }
  }
}

bool Storage::readWeekCSV(const String &weekName, String &outContent) {
  String path = "/" + weekName;
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  outContent = "";
  while (f.available()) {
    outContent += (char)f.read();
  }
  f.close();
  return true;
}

void Storage::debugListFiles() {
  Dir dir = LittleFS.openDir("/");
  Serial.println("Listing all files in LittleFS:");
  while (dir.next()) {
    Serial.printf("  %s  %u\n", dir.fileName().c_str(), dir.fileSize());
  }
}
