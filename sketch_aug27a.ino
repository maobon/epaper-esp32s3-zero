#if !defined(ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch requires an ESP32-S3 board."
#endif

#include "AppConfig.h"
#include "EpaperApiClient.h"
#include "EpaperDisplay.h"
#include "ImageInspector.h"
#include "PsramImage.h"
#include "WifiManager.h"

PsramImage downloadedImages[AppConfig::kImageCount];
EpaperApiClient epaperApi;
EpaperDisplay epaperDisplay;

size_t currentPageIndex = 0;
uint32_t lastPageChangeMs = 0;
bool slideshowReady = false;

bool showPage(size_t pageIndex) {
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!connectWifi()) {
    return;
  }

  if (!epaperDisplay.begin()) {
    Serial.println("墨水屏初始化失败");
    return;
  }

  if (!epaperApi.authenticate()) {
    Serial.println("接口登录失败");
    return;
  }

  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    const char *imageName = AppConfig::kImageNames[index];
    if (!epaperApi.fetchImage(imageName, downloadedImages[index])) {
      Serial.print(imageName);
      Serial.println(" 下载失败，无法启动四页轮播");
      return;
    }

    Serial.println("图片保存成功，2 秒后开始读取校验...");
    delay(AppConfig::kImageCheckDelayMs);

    Serial.print("正在检测图片: ");
    Serial.println(imageName);
    if (!inspectPsramImage(downloadedImages[index])) {
      Serial.println("PSRAM 图片读取校验失败");
      return;
    }
  }

  Serial.println("4 个界面已全部下载并校验完成，开始循环展示");
  if (!showPage(0)) {
    Serial.println("首页显示失败，无法启动四页轮播");
    return;
  }
  slideshowReady = true;
}

void loop() {
  if (!slideshowReady ||
      millis() - lastPageChangeMs < AppConfig::kPageDisplayDurationMs) {
    return;
  }

  const size_t nextPageIndex =
      (currentPageIndex + 1) % AppConfig::kImageCount;
  if (!showPage(nextPageIndex)) {
    // 显示失败时保留当前页，并在 30 秒后重试下一页，避免频繁刷新。
    lastPageChangeMs = millis();
  }
}
