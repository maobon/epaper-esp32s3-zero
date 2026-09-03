#pragma once

#include <Arduino.h>

#include "AppConfig.h"

struct NewsItem {
  String title;
};

struct NewsList {
  NewsItem items[AppConfig::kNewsItemCount];
  size_t count = 0;

  void clear() {
    for (size_t index = 0; index < count; ++index) {
      items[index].title.clear();
    }
    count = 0;
  }
};
