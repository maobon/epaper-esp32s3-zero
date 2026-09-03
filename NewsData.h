#pragma once

#include <Arduino.h>

#include "AppConfig.h"

struct NewsItem {
  String title;
  String summary;
  String duration;
};

struct NewsList {
  NewsItem items[AppConfig::kNewsStorageCount];
  size_t count = 0;

  void clear() {
    for (size_t index = 0; index < count; ++index) {
      items[index].title.clear();
      items[index].summary.clear();
      items[index].duration.clear();
    }
    count = 0;
  }
};
