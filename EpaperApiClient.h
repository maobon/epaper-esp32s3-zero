#pragma once

#include <Arduino.h>

#include "NewsData.h"
#include "PsramImage.h"

class EpaperApiClient {
 public:
  bool authenticate();
  bool fetchImage(const char *imageName, PsramImage &destination);
  bool fetchNews(NewsList &destination);
  bool fetchChineseNews(NewsList &destination);

 private:
  bool login();
  bool downloadImage(const char *imageName, PsramImage &destination);
  bool downloadNews(const char *url, const char *responseKey,
                    size_t maximumItemCount, bool includeDuration,
                    NewsList &destination);

  String accessToken_;
};
