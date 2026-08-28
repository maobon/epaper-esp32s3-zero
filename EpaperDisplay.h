#pragma once

#include "PsramImage.h"

class EpaperDisplay {
 public:
  bool begin();
  bool showPng(const PsramImage &pngImage, float temperatureCelsius,
               float relativeHumidity, bool showSensorPanel,
               bool sensorDataValid);

 private:
  bool initialized_ = false;
  PsramImage frameBuffer_;
};
