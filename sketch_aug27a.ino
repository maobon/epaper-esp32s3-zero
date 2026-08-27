#if !defined(ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch requires an ESP32-S3 board."
#endif

#include "AppConfig.h"
#include "EpaperApiClient.h"
#include "EpaperDisplay.h"
#include "ImageInspector.h"
#include "PsramImage.h"
#include "WifiManager.h"

PsramImage currentImage;
EpaperApiClient epaperApi;
EpaperDisplay epaperDisplay;

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
    if (!epaperApi.fetchImage(imageName, currentImage)) {
      Serial.print(imageName);
      Serial.println(" 下载失败，继续请求下一张");
      continue;
    }

    Serial.println("图片保存成功，2 秒后开始读取校验...");
    delay(AppConfig::kImageCheckDelayMs);

    Serial.print("正在检测图片: ");
    Serial.println(imageName);
    if (!inspectPsramImage(currentImage)) {
      Serial.println("PSRAM 图片读取校验失败");
      continue;
    }

    Serial.print("正在显示图片: ");
    Serial.println(imageName);
    if (!epaperDisplay.showPng(currentImage)) {
      Serial.println("图片显示失败");
      continue;
    }

    if (index + 1 < AppConfig::kImageCount) {
      Serial.println("显示成功，2 秒后请求下一张图片...");
      delay(AppConfig::kNextImageDelayMs);
    }
  }
}

void loop() {
  // 登录、下载、校验和显示仅在启动后执行一次。
}
