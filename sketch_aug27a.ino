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

#include <time.h>

PsramImage downloadedImages[AppConfig::kImageCount];
EpaperApiClient epaperApi;
EpaperDisplay epaperDisplay;
Sht41Sensor sht41Sensor;

size_t currentPageIndex = 0;
uint32_t lastPageChangeMs = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastSlideshowAttemptMs = 0;
uint32_t lastImageRefreshAttemptMs = 0;
uint32_t lastTimeSyncAttemptMs = 0;
int32_t lastImageRefreshHourKey = -1;
int32_t lastImageRefreshAttemptHourKey = -1;
bool displayReady = false;
bool slideshowReady = false;
bool sht41Ready = false;
bool networkTimeReady = false;
bool sensorDataValid = false;
float latestTemperatureCelsius = 0.0F;
float latestRelativeHumidity = 0.0F;

bool getCurrentHourKey(int32_t &hourKey) {
  tm localTime = {};
  if (!getLocalTime(&localTime, 0)) {
    return false;
  }

  hourKey = (localTime.tm_year + 1900) * 100000L +
            localTime.tm_yday * 100L + localTime.tm_hour;
  return true;
}

void markCurrentHourRefreshed() {
  int32_t currentHourKey = -1;
  if (networkTimeReady && getCurrentHourKey(currentHourKey)) {
    lastImageRefreshHourKey = currentHourKey;
    lastImageRefreshAttemptHourKey = currentHourKey;
  }
}

bool syncNetworkTime() {
  lastTimeSyncAttemptMs = millis();
  if (!connectWifi()) {
    return false;
  }

  configTzTime(AppConfig::kTimeZone, AppConfig::kPrimaryNtpServer,
               AppConfig::kSecondaryNtpServer);

  tm localTime = {};
  if (!getLocalTime(&localTime, AppConfig::kTimeSyncTimeoutMs)) {
    Serial.println("NTP 网络校时失败，将在 60 秒后重试");
    return false;
  }

  networkTimeReady = true;
  Serial.print("NTP 校时成功，当前时间: ");
  char formattedTime[24] = {};
  strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
           &localTime);
  Serial.println(formattedTime);
  return true;
}

void updateSht41() {
  const uint32_t now = millis();
  if (now - lastSensorReadMs < AppConfig::kSensorReadIntervalMs) {
    return;
  }
  lastSensorReadMs = now;

  if (!sht41Ready) {
    sht41Ready = sht41Sensor.begin();
    if (!sht41Ready) {
      sensorDataValid = false;
      Serial.println("将在 5 秒后重新连接 SHT41");
      return;
    }
  }

  float temperatureCelsius = 0.0F;
  float relativeHumidity = 0.0F;
  if (!sht41Sensor.read(temperatureCelsius, relativeHumidity)) {
    sht41Ready = false;
    sensorDataValid = false;
    Serial.println("将在 5 秒后重新连接 SHT41");
    return;
  }

  latestTemperatureCelsius = temperatureCelsius;
  latestRelativeHumidity = relativeHumidity;
  sensorDataValid = true;

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

  if (!epaperDisplay.showPng(downloadedImages[pageIndex],
                             latestTemperatureCelsius,
                             latestRelativeHumidity,
                             pageIndex == AppConfig::kForecastPageIndex,
                             sensorDataValid)) {
    Serial.println("图片显示失败");
    return false;
  }

  currentPageIndex = pageIndex;
  lastPageChangeMs = millis();
  return true;
}

bool refreshImages() {
  if (!connectWifi()) {
    return false;
  }

  if (!epaperApi.authenticate()) {
    Serial.println("接口登录失败");
    return false;
  }

  bool allImagesUpdated = true;
  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    const char *imageName = AppConfig::kImageNames[index];
    PsramImage refreshedImage;
    if (!epaperApi.fetchImage(imageName, refreshedImage)) {
      Serial.print(imageName);
      Serial.println(" 下载失败，继续保留旧图片");
      allImagesUpdated = false;
      continue;
    }

    Serial.print("正在检测图片: ");
    Serial.println(imageName);
    if (!inspectPsramImage(refreshedImage)) {
      Serial.println("PSRAM 图片读取校验失败，继续保留旧图片");
      allImagesUpdated = false;
      continue;
    }

    downloadedImages[index].swap(refreshedImage);
    updateSht41();
  }

  if (allImagesUpdated) {
    Serial.println("4 张图片已全部更新");
  }
  return allImagesUpdated;
}

bool initializeSlideshow() {
  if (!displayReady || !refreshImages()) {
    return false;
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
  lastImageRefreshAttemptMs = lastSlideshowAttemptMs;

  if (syncNetworkTime() && slideshowReady) {
    // 启动时的下载视为当前小时已更新，等到下一个整点再请求。
    markCurrentHourRefreshed();
  }
}

void loop() {
  updateSht41();

  uint32_t now = millis();
  if (!networkTimeReady &&
      now - lastTimeSyncAttemptMs >= AppConfig::kTimeSyncRetryIntervalMs) {
    if (syncNetworkTime() && slideshowReady) {
      markCurrentHourRefreshed();
    }
    now = millis();
  }

  if (!slideshowReady) {
    if (displayReady &&
        now - lastSlideshowAttemptMs >=
            AppConfig::kSlideshowRetryIntervalMs) {
      Serial.println("正在重新初始化四页轮播...");
      if (!initializeSlideshow()) {
        Serial.println("轮播初始化仍未成功，将在 60 秒后再次重试");
      } else {
        markCurrentHourRefreshed();
      }
      lastSlideshowAttemptMs = millis();
    }
    delay(10);
    return;
  }

  int32_t currentHourKey = -1;
  const bool hasCurrentTime = getCurrentHourKey(currentHourKey);
  if (networkTimeReady && !hasCurrentTime) {
    networkTimeReady = false;
    Serial.println("网络时间不可用，暂停整点图片更新并重新校时");
  }

  const bool hourlyRefreshDue =
      networkTimeReady && hasCurrentTime &&
      currentHourKey != lastImageRefreshHourKey;
  const bool firstAttemptThisHour =
      currentHourKey != lastImageRefreshAttemptHourKey;
  const bool refreshRetryDue =
      now - lastImageRefreshAttemptMs >=
      AppConfig::kSlideshowRetryIntervalMs;
  if (hourlyRefreshDue && (firstAttemptThisHour || refreshRetryDue)) {
    Serial.println("已到整点更新时间，正在重新请求 4 张图片...");
    lastImageRefreshAttemptHourKey = currentHourKey;
    if (refreshImages()) {
      lastImageRefreshHourKey = currentHourKey;
    } else {
      Serial.println("部分或全部图片更新失败，将在 60 秒后重试");
    }
    lastImageRefreshAttemptMs = millis();
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
