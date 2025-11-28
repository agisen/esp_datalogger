// lib/Sensor.cpp
#include "Sensor.h"
#include <DHT.h>

// Pin und Typ anpassen falls nötig
#ifndef DHTPIN
  #define DHTPIN D4
#endif
#ifndef DHTTYPE
  #define DHTTYPE DHT22
#endif

static DHT dht(DHTPIN, DHTTYPE);

Sensor::Sensor() { }

void Sensor::begin() {
  dht.begin();
}

bool Sensor::read(float& temperature, float& humidity) {
  // Non-blocking note: DHT library uses delays internally; keep interval large enough
  // Retry up to 3 times if reading invalid
  const int MAX_TRIES = 3;
  for (int i=0;i<MAX_TRIES;i++) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
      return true;
    }
    delay(200); // small pause between tries
  }
  return false;
}
