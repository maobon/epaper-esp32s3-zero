#if !defined(ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch requires an ESP32-S3 board."
#endif

#include "AppConfig.h"
#include "EpaperApiClient.h"
#include "EpaperDisplay.h"
#include "ImageInspector.h"
#include "PsramImage.h"
#include "Sht41Sensor.h"
#include "WifiManager.h"

PsramImage downloadedImages[AppConfig::kImageCount];
EpaperApiClient epaperApi;
EpaperDisplay epaperDisplay;
Sht41Sensor sht41Sensor;

size_t currentPageIndex = 0;
uint32_t lastPageChangeMs = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastSlideshowAttemptMs = 0;
bool displayReady = false;
bool slideshowReady = false;
bool sht41Ready = false;

void updateSht41() {
  const uint32_t now = millis();
  if (now - lastSensorReadMs < AppConfig::kSensorReadIntervalMs) {
    return;
  }
  lastSensorReadMs = now;

  if (!sht41Ready) {
    sht41Ready = sht41Sensor.begin();
    if (!sht41Ready) {
      Serial.println("将在 5 秒后重新连接 SHT41");
      return;
    }
  }

  float temperatureCelsius = 0.0F;
  float relativeHumidity = 0.0F;
  if (!sht41Sensor.read(temperatureCelsius, relativeHumidity)) {
    sht41Ready = false;
    Serial.println("将在 5 秒后重新连接 SHT41");
    return;
  }

  Serial.print("SHT41 温度: ");
  Serial.print(temperatureCelsius, 2);
  Serial.print(" °C，湿度: ");
  Serial.print(relativeHumidity, 2);
  Serial.println(" %RH");
}

bool showPage(size_t pageIndex) {
  if (pageIndex >= AppConfig::kImageCount) {
    Serial.println("页面索引超出范围");
    return false;
  }

  Serial.print("正在显示第 ");
  Serial.print(pageIndex + 1);
  Serial.print(" 页: ");
  Serial.println(AppConfig::kImageNames[pageIndex]);

  if (!epaperDisplay.showPng(downloadedImages[pageIndex])) {
    Serial.println("图片显示失败");
    return false;
  }

  currentPageIndex = pageIndex;
  lastPageChangeMs = millis();
  return true;
}

bool initializeSlideshow() {
  if (!displayReady || !connectWifi()) {
    return false;
  }

  if (!epaperApi.authenticate()) {
    Serial.println("接口登录失败");
    return false;
  }

  // 失败重试时丢弃上次未完成的一组图片，避免长期占用和碎片化 PSRAM。
  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    downloadedImages[index].clear();
  }

  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    const char *imageName = AppConfig::kImageNames[index];
    if (!epaperApi.fetchImage(imageName, downloadedImages[index])) {
      Serial.print(imageName);
      Serial.println(" 下载失败，无法启动四页轮播");
      return false;
    }

    Serial.print("正在检测图片: ");
    Serial.println(imageName);
    if (!inspectPsramImage(downloadedImages[index])) {
      Serial.println("PSRAM 图片读取校验失败");
      return false;
    }

    updateSht41();
  }

  Serial.println("4 个界面已全部下载并校验完成，开始循环展示");
  if (!showPage(0)) {
    Serial.println("首页显示失败，无法启动四页轮播");
    return false;
  }

  slideshowReady = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  sht41Ready = sht41Sensor.begin();
  if (sht41Ready) {
    lastSensorReadMs = millis() - AppConfig::kSensorReadIntervalMs;
    updateSht41();
  } else {
    lastSensorReadMs = millis();
  }

  displayReady = epaperDisplay.begin();
  if (!displayReady) {
    Serial.println("墨水屏初始化失败");
    return;
  }

  if (!initializeSlideshow()) {
    Serial.println("轮播初始化失败，将在 60 秒后自动重试");
  }
  lastSlideshowAttemptMs = millis();
}

void loop() {
  updateSht41();

  const uint32_t now = millis();
  if (!slideshowReady) {
    if (displayReady &&
        now - lastSlideshowAttemptMs >=
            AppConfig::kSlideshowRetryIntervalMs) {
      Serial.println("正在重新初始化四页轮播...");
      if (!initializeSlideshow()) {
        Serial.println("轮播初始化仍未成功，将在 60 秒后再次重试");
      }
      lastSlideshowAttemptMs = millis();
    }
    delay(10);
    return;
  }

  if (now - lastPageChangeMs >= AppConfig::kPageDisplayDurationMs) {
    const size_t nextPageIndex =
        (currentPageIndex + 1) % AppConfig::kImageCount;
    if (!showPage(nextPageIndex)) {
      // 显示失败时保留当前页，并在 30 秒后重试，避免频繁刷新。
      lastPageChangeMs = millis();
    }
  }

  delay(10);
}
