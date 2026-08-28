#pragma once

#include <Arduino.h>

class Sht41Sensor {
 public:
  bool begin();
  bool read(float &temperatureCelsius, float &relativeHumidity);

 private:
  static bool hasValidCrc(const uint8_t *data, uint8_t expectedCrc);
};
