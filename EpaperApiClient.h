#pragma once

#include <Arduino.h>

#include "NewsData.h"
#include "PsramImage.h"

class EpaperApiClient {
 public:
  bool authenticate();
  bool fetchImage(const char *imageName, PsramImage &destination);
  bool fetchNews(NewsList &destination);

 private:
  bool login();
  bool downloadImage(const char *imageName, PsramImage &destination);
  bool downloadNews(NewsList &destination);

  String accessToken_;
};
