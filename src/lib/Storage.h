#pragma once
#include <Arduino.h>
#include <vector>

struct Measurement {
  uint32_t ts;
  float temp;
  float hum;
};

class Storage {
public:
  Storage();
  void begin();

  // Save a batch of measurements (array of Measurement of length len)
  // returns true on success
  bool saveBatch(Measurement *arr, uint8_t len);

  // Load settings from /config/settings.json (if exists). Returns true if loaded
  bool loadSettings(uint32_t &intervalSeconds, String &ssid, String &pass, String &httpPassword);

  // Storage info
  uint64_t usedBytes();
  uint64_t totalBytes();

  // Get list of weeks (filenames without leading '/')
  void listWeeks(std::vector<String> &outWeeks);

  // Delete oldest week file (returns true if a file was deleted)
  bool deleteOldestWeek();

  // Delete all weeks
  void deleteAllWeeks();

  // Delete all weeks before the given week (e.g., "2025-W03")
  void deleteWeeksBefore(const String &currentWeek);

  // Read a week's CSV content into a provided String (returns true on success)
  bool readWeekCSV(const String &weekName, String &outContent);

  // Diagnostic helper
  void debugListFiles();
};
