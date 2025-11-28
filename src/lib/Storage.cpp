#include "Storage.h"
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>

Storage::Storage() {}

void Storage::begin() {
  // LittleFS already mounted in sketch, but we can check
  if (!LittleFS.begin()) {
    Serial.println(F("Storage: LittleFS begin failed"));
  } else {
    Serial.println(F("Storage: LittleFS ready"));
  }
}

uint64_t Storage::usedBytes() {
  return LittleFS.usedBytes();
}
uint64_t Storage::totalBytes() {
  return LittleFS.totalBytes();
}

bool Storage::loadSettings(uint32_t &intervalSeconds, String &ssid, String &pass, String &httpPassword) {
  if (!LittleFS.exists("/settings.json")) return false;
  File f = LittleFS.open("/settings.json", "r");
  if (!f) return false;
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
  if (doc.containsKey("interval")) intervalSeconds = doc["interval"];
  if (doc.containsKey("wifi_ssid")) ssid = String((const char*)doc["wifi_ssid"]);
  if (doc.containsKey("wifi_pass")) pass = String((const char*)doc["wifi_pass"]);
  if (doc.containsKey("http_password")) httpPassword = String((const char*)doc["http_password"]);
  return true;
}

static String weekNameFromTime(time_t t) {
  // simple week number (year-week) using day-of-year/7 (not strict ISO)
  tm tm;
  gmtime_r(&t, &tm);
  int year = tm.tm_year + 1900;
  int week = (tm.tm_yday / 7) + 1; // 1..53
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-W%02d", year, week);
  return String(buf);
}

// ---------- REPARIERTE saveBatch ----------
bool Storage::saveBatch(Measurement *arr, uint8_t len) {
  if (len == 0) return true;

  // Estimate bytes needed: roughly len * 40 (timestamp;temp;hum\n)
  uint32_t estimated = (uint32_t)len * 40;

  // Check 85%-rule
  uint64_t used = usedBytes();
  uint64_t total = totalBytes();
  uint64_t threshold = (total * 85) / 100;
  Serial.printf("Storage: used=%llu total=%llu threshold=%llu need=%u\n", (unsigned long long)used, (unsigned long long)total, (unsigned long long)threshold, estimated);

  // while writing would exceed threshold, delete oldest file(s)
  while ((used + estimated) > threshold) {
    bool deleted = deleteOldestWeek();
    if (!deleted) {
      Serial.println(F("Storage: cannot free more space"));
      return false;
    }
    used = usedBytes();
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
    char line[64];
    snprintf(line, sizeof(line), "%lu;%.2f;%.2f\n", (unsigned long)arr[i].ts, arr[i].temp, arr[i].hum);
    f.print(line);
  }
  f.close();
  return true;
}

// -----------------------------------------

void Storage::listWeeks(std::vector<String> &outWeeks) {
  outWeeks.clear();
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String name = dir.fileName(); // starts with "/"
    if (name.endsWith(".csv")) {
      name = name.substring(1); // strip '/'
      outWeeks.push_back(name);
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
  Serial.println(F("Storage files:"));
  while (dir.next()) {
    Serial.printf("  %s  %u\n", dir.fileName().c_str(), dir.fileSize());
  }
}
