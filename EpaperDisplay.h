#pragma once

#include "PsramImage.h"

class EpaperDisplay {
 public:
  bool begin();
  bool showPng(const PsramImage &pngImage);

 private:
  bool initialized_ = false;
};
