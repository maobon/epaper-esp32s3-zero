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
NewsList latestNews;
NewsList latestChineseNews;

size_t currentPageIndex = 0;
uint32_t lastPageChangeMs = 0;
uint32_t lastPageDisplayAttemptMs = 0;
uint32_t lastSensorReadMs = 0;
uint32_t lastSlideshowAttemptMs = 0;
uint32_t lastContentRefreshMs = 0;
uint32_t lastContentRefreshAttemptMs = 0;
uint32_t lastTimeSyncAttemptMs = 0;
bool displayReady = false;
bool slideshowReady = false;
bool initialPreviewActive = false;
size_t initialPreviewPagesShown = 0;
bool pageDisplayRetryPending = false;
bool sht41Ready = false;
bool networkTimeReady = false;
bool sensorDataValid = false;
bool newsDataValid = false;
bool chineseNewsDataValid = false;
bool pendingNewsRefresh = false;
bool pendingChineseNewsRefresh = false;
bool pendingImageRefresh[AppConfig::kImageCount] = {};
float latestTemperatureCelsius = 0.0F;
float latestRelativeHumidity = 0.0F;

void markAllContentForRefresh() {
  pendingNewsRefresh = true;
  pendingChineseNewsRefresh = true;
  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    pendingImageRefresh[index] = true;
  }
}

bool hasPendingContentRefresh() {
  if (pendingNewsRefresh || pendingChineseNewsRefresh) {
    return true;
  }
  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    if (pendingImageRefresh[index]) {
      return true;
    }
  }
  return false;
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
  if (!AppConfig::kSht41Enabled) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t sensorIntervalMs =
      sht41Ready ? AppConfig::kSensorReadIntervalMs
                 : AppConfig::kSensorRetryIntervalMs;
  if (now - lastSensorReadMs < sensorIntervalMs) {
    return;
  }
  lastSensorReadMs = now;

  if (!sht41Ready) {
    sht41Ready = sht41Sensor.begin();
    if (!sht41Ready) {
      sensorDataValid = false;
      Serial.println("将在 30 秒后重新连接 SHT41");
      return;
    }
  }

  float temperatureCelsius = 0.0F;
  float relativeHumidity = 0.0F;
  if (!sht41Sensor.read(temperatureCelsius, relativeHumidity)) {
    sht41Ready = false;
    sensorDataValid = false;
    Serial.println("将在 30 秒后重新连接 SHT41");
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
  if (pageIndex >= AppConfig::kPageCount) {
    Serial.println("页面索引超出范围");
    return false;
  }

  Serial.print("正在显示第 ");
  Serial.print(pageIndex + 1);
  Serial.print(" 页: ");
  const bool isNewsPage =
      pageIndex >= AppConfig::kNewsPageIndex &&
      pageIndex < AppConfig::kNewsPageIndex + AppConfig::kNewsPageCount;
  const bool isChineseNewsPage =
      pageIndex >= AppConfig::kChineseNewsPageIndex &&
      pageIndex < AppConfig::kChineseNewsPageIndex +
                      AppConfig::kChineseNewsPageCount;
  if (isNewsPage) {
    Serial.print("news ");
    Serial.print(pageIndex - AppConfig::kNewsPageIndex + 1);
    Serial.print('/');
    Serial.println(AppConfig::kNewsPageCount);
  } else if (isChineseNewsPage) {
    Serial.print("chinese news ");
    Serial.print(pageIndex - AppConfig::kChineseNewsPageIndex + 1);
    Serial.print('/');
    Serial.println(AppConfig::kChineseNewsPageCount);
  } else {
    Serial.println(AppConfig::kImageNames[pageIndex]);
  }
  lastPageDisplayAttemptMs = millis();

  bool displaySucceeded = false;
  if (isNewsPage) {
    displaySucceeded = epaperDisplay.showNews(
        latestNews,
        (pageIndex - AppConfig::kNewsPageIndex) *
            AppConfig::kNewsItemsPerPage,
        AppConfig::kNewsItemsPerPage,
        pageIndex - AppConfig::kNewsPageIndex + 1,
        AppConfig::kNewsPageCount, newsDataValid);
  } else if (isChineseNewsPage) {
    displaySucceeded = epaperDisplay.showChineseNews(
        latestChineseNews,
        (pageIndex - AppConfig::kChineseNewsPageIndex) *
            AppConfig::kChineseNewsItemsPerPage,
        AppConfig::kChineseNewsItemsPerPage,
        pageIndex - AppConfig::kChineseNewsPageIndex + 1,
        AppConfig::kChineseNewsPageCount, chineseNewsDataValid);
  } else {
    displaySucceeded = epaperDisplay.showPng(
        downloadedImages[pageIndex], latestTemperatureCelsius,
        latestRelativeHumidity,
        pageIndex == AppConfig::kForecastPageIndex, sensorDataValid);
  }
  if (!displaySucceeded) {
    Serial.println("页面显示失败");
    pageDisplayRetryPending = true;
    return false;
  }

  currentPageIndex = pageIndex;
  lastPageChangeMs = millis();
  pageDisplayRetryPending = false;
  return true;
}

bool isPageDisplayDue(uint32_t now, uint32_t pageDurationMs,
                      uint32_t retryIntervalMs) {
  if (pageDisplayRetryPending) {
    return now - lastPageDisplayAttemptMs >= retryIntervalMs;
  }
  return now - lastPageChangeMs >= pageDurationMs;
}

uint32_t currentPageDisplayDurationMs() {
  const bool isNewsPage =
      currentPageIndex >= AppConfig::kNewsPageIndex &&
      currentPageIndex <
          AppConfig::kNewsPageIndex + AppConfig::kNewsPageCount;
  if (isNewsPage) {
    return AppConfig::kNewsPageDisplayDurationMs;
  }
  const bool isChineseNewsPage =
      currentPageIndex >= AppConfig::kChineseNewsPageIndex &&
      currentPageIndex < AppConfig::kChineseNewsPageIndex +
                             AppConfig::kChineseNewsPageCount;
  return isChineseNewsPage ? AppConfig::kChineseNewsPageDisplayDurationMs
                           : AppConfig::kInterfaceDisplayDurationMs;
}

bool refreshContent() {
  if (!hasPendingContentRefresh()) {
    return true;
  }
  if (!connectWifi()) {
    return false;
  }

  if (!epaperApi.authenticate()) {
    Serial.println("接口登录失败");
    return false;
  }

  // 新闻页由设备本地绘制，启动时优先取得 JSON 数据，避免首次轮播
  // 进入新闻页时只能显示“等待刷新”的占位内容。
  if (pendingNewsRefresh) {
    if (epaperApi.fetchNews(latestNews)) {
      newsDataValid = true;
      pendingNewsRefresh = false;
    } else {
      Serial.println("新闻更新失败，继续保留旧数据");
    }
  }

  if (pendingChineseNewsRefresh) {
    if (epaperApi.fetchChineseNews(latestChineseNews)) {
      chineseNewsDataValid = true;
      pendingChineseNewsRefresh = false;
    } else {
      Serial.println("中文新闻更新失败，继续保留旧数据");
    }
  }

  for (size_t index = 0; index < AppConfig::kImageCount; ++index) {
    if (!pendingImageRefresh[index]) {
      continue;
    }
    const char *imageName = AppConfig::kImageNames[index];
    PsramImage refreshedImage;
    if (!epaperApi.fetchImage(imageName, refreshedImage)) {
      Serial.print(imageName);
      Serial.println(" 下载失败，继续保留旧图片");
      continue;
    }

    Serial.print("正在检测图片: ");
    Serial.println(imageName);
    if (!inspectPsramImage(refreshedImage)) {
      Serial.println("PSRAM 图片读取校验失败，继续保留旧图片");
      continue;
    }

    downloadedImages[index].swap(refreshedImage);
    pendingImageRefresh[index] = false;
    updateSht41();
  }

  if (!hasPendingContentRefresh()) {
    Serial.print("中英文新闻与 ");
    Serial.print(AppConfig::kImageCount);
    Serial.println(" 张图片已全部更新");
  }

  return !hasPendingContentRefresh();
}

bool initializeSlideshow() {
  if (!displayReady || !refreshContent()) {
    return false;
  }
  lastContentRefreshMs = millis();

  Serial.print(AppConfig::kInterfaceCount);
  Serial.println(" 个界面的数据已准备完成，开始循环展示");
  if (!showPage(0)) {
    Serial.println("首页显示失败，无法启动图片轮播");
    return false;
  }

  initialPreviewActive = true;
  initialPreviewPagesShown = 1;
  Serial.println("开始首次快速预览，每个页面显示 10 秒");
  slideshowReady = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (AppConfig::kSht41Enabled) {
    sht41Ready = sht41Sensor.begin();
    if (sht41Ready) {
      lastSensorReadMs = millis() - AppConfig::kSensorReadIntervalMs;
      updateSht41();
    } else {
      lastSensorReadMs = millis();
    }
  } else {
    Serial.println("SHT41 已通过配置关闭");
  }

  displayReady = epaperDisplay.begin();
  markAllContentForRefresh();
  if (!displayReady) {
    Serial.println("墨水屏初始化失败，将在 60 秒后自动重试");
  } else if (!initializeSlideshow()) {
    Serial.println("轮播初始化失败，将在 60 秒后自动重试");
  }
  lastSlideshowAttemptMs = millis();
  lastContentRefreshAttemptMs = lastSlideshowAttemptMs;

  syncNetworkTime();
}

void loop() {
  updateSht41();

  uint32_t now = millis();
  if (!networkTimeReady &&
      now - lastTimeSyncAttemptMs >= AppConfig::kTimeSyncRetryIntervalMs) {
    syncNetworkTime();
    now = millis();
  }

  if (!slideshowReady) {
    if (now - lastSlideshowAttemptMs >=
        AppConfig::kSlideshowRetryIntervalMs) {
      if (!displayReady) {
        Serial.println("正在重新初始化墨水屏...");
        displayReady = epaperDisplay.begin();
        if (!displayReady) {
          Serial.println("墨水屏初始化仍未成功，将在 60 秒后再次重试");
        }
      }

      if (displayReady) {
        Serial.println("正在重新初始化图片轮播...");
        if (!initializeSlideshow()) {
          Serial.println("轮播初始化仍未成功，将在 60 秒后再次重试");
        }
      }
      lastSlideshowAttemptMs = millis();
    }
    delay(10);
    return;
  }

  const bool contentRefreshDue =
      now - lastContentRefreshMs >= AppConfig::kContentRefreshIntervalMs;
  const bool refreshRetryDue =
      now - lastContentRefreshAttemptMs >=
      AppConfig::kContentRefreshRetryIntervalMs;
  if (contentRefreshDue && refreshRetryDue) {
    Serial.print("已到 3 小时内容更新时间，正在重新请求 ");
    Serial.print(AppConfig::kInterfaceCount);
    Serial.println(" 个页面的数据...");
    if (!hasPendingContentRefresh()) {
      markAllContentForRefresh();
    }
    if (refreshContent()) {
      lastContentRefreshMs = millis();
    } else {
      Serial.println("部分或全部页面数据更新失败，将在 60 秒后重试");
    }
    lastContentRefreshAttemptMs = millis();
  }

  now = millis();
  if (initialPreviewActive &&
      isPageDisplayDue(now, AppConfig::kInitialPreviewPageDurationMs,
                       AppConfig::kInitialPreviewPageDurationMs)) {
    if (initialPreviewPagesShown < AppConfig::kPageCount) {
      const size_t nextPageIndex =
          (currentPageIndex + 1) % AppConfig::kPageCount;
      if (showPage(nextPageIndex)) {
        ++initialPreviewPagesShown;
      }
    } else if (showPage(0)) {
      initialPreviewActive = false;
      Serial.println(
          "首次快速预览完成，开始正常轮播：每个界面显示 8 分钟");
    }
  } else if (!initialPreviewActive &&
             isPageDisplayDue(now, currentPageDisplayDurationMs(),
                              AppConfig::kPageDisplayRetryIntervalMs)) {
    const size_t nextPageIndex =
        (currentPageIndex + 1) % AppConfig::kPageCount;
    // 显示失败时保留当前页，并在 30 秒后重试，避免频繁刷新。
    showPage(nextPageIndex);
  }

  delay(10);
}
