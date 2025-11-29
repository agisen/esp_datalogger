// lib/Sensor.cpp
#include "Sensor.h"
#include <DHT.h>

// Pin und Typ anpassen falls nötig
#ifndef DHTPIN
  #define DHTPIN 4
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
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
    return true;
  }

  return false;
}
