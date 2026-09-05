#pragma once

#include "NewsData.h"
#include "PsramImage.h"

class EpaperDisplay {
 public:
  bool begin();
  bool prepareFrame(const PsramImage &pngImage, PsramImage &destination);
  bool showFrame(const PsramImage &image, float temperatureCelsius,
               float relativeHumidity, bool showSensorPanel,
               bool sensorDataValid);
  bool showNews(const NewsList &news, size_t firstItemIndex,
                size_t maximumItemCount, size_t pageNumber,
                size_t pageCount, bool dataValid);
  bool showChineseNews(const NewsList &news, size_t firstItemIndex,
                       size_t maximumItemCount, size_t pageNumber,
                       size_t pageCount, bool dataValid);

 private:
  bool initialized_ = false;
  PsramImage frameBuffer_;
};
