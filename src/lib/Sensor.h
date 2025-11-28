// lib/Sensor.h
#pragma once
#include <Arduino.h>

class Sensor {
public:
  Sensor();
  void begin();
  // returns true if values valid (and filled), false otherwise
  bool read(float& temperature, float& humidity);
};
