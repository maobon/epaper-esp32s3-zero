#pragma once

#include <Arduino.h>

#include "PsramImage.h"

class EpaperApiClient {
 public:
  bool authenticate();
  bool fetchImage(const char *imageName, PsramImage &destination);

 private:
  bool login();
  bool downloadImage(const char *imageName, PsramImage &destination);

  String accessToken_;
};
