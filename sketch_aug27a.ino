#if !defined(ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch requires an ESP32-S3 board."
#endif

#include "AppConfig.h"
#include "EpaperApiClient.h"
#include "EpaperDisplay.h"
#include "PsramImage.h"
#include "Sht41Sensor.h"
#include "WifiManager.h"

#include <time.h>
#include <esp_sleep.h>
#include <esp32-hal-rgb-led.h>
#include <esp32-hal-rmt.h>
#include "SleepSchedule.h"
#include "PageSelection.h"
#include "PageHours.h"

PsramImage cachedFrames[AppConfig::kImageCount];
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

bool isPageReady(size_t page) {
  if (page < AppConfig::kImageCount) {
    return cachedFrames[page].size() != 0;
  }
  if (page < AppConfig::kChineseNewsPageIndex) {
    return newsDataValid &&
        (page - AppConfig::kNewsPageIndex) * AppConfig::kNewsItemsPerPage <
            latestNews.count;
  }
  return page < AppConfig::kPageCount && chineseNewsDataValid &&
      (page - AppConfig::kChineseNewsPageIndex) *
          AppConfig::kChineseNewsItemsPerPage < latestChineseNews.count;
}

uint32_t nextContentAttemptWaitMs(uint32_t now) {
  return contentWaitMs(now, lastContentRefreshMs, lastContentRefreshAttemptMs,
      AppConfig::kContentRefreshIntervalMs,
      AppConfig::kContentRefreshRetryIntervalMs, hasPendingContentRefresh(),
      wifiRetryRemainingMs(now));
}

uint32_t nextSlideshowAttemptWaitMs(uint32_t now) {
  const uint32_t waitMs = remainingUntil(now, lastSlideshowAttemptMs,
                                        AppConfig::kSlideshowRetryIntervalMs);
  return displayReady ? max(waitMs, wifiRetryRemainingMs(now)) : waitMs;
}

bool syncNetworkTime() {
  WifiSession wifiSession;
  lastTimeSyncAttemptMs = millis();
  if (!connectWifi(true)) {
    finishTimeSync();
    lastTimeSyncAttemptMs = millis();
    return false;
  }

  configTzTime(AppConfig::kTimeZone, AppConfig::kPrimaryNtpServer,
               AppConfig::kSecondaryNtpServer);

  tm localTime = {};
  const bool synced = getLocalTime(&localTime, AppConfig::kTimeSyncTimeoutMs);
  finishTimeSync();
  lastTimeSyncAttemptMs = millis();
  if (!synced) {
    Serial.println("NTP 校时失败，继续执行禁网策略；无有效时间时需重启再校时");
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
    displaySucceeded = epaperDisplay.showFrame(
        cachedFrames[pageIndex], latestTemperatureCelsius,
        latestRelativeHumidity,
        AppConfig::kSht41Enabled && pageIndex == AppConfig::kForecastPageIndex,
        sensorDataValid);
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
  if (initialPreviewActive) {
    return AppConfig::kInitialPreviewPageDurationMs;
  }
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
                           : AppConfig::kImagePageDisplayDurationMs;
}

PageTiming currentPageTiming() {
  const uint32_t normalMs = currentPageDisplayDurationMs();
  const time_t clockNow = time(nullptr);
  tm local = {};
  if (clockNow < 1704067200 || localtime_r(&clockNow, &local) == nullptr) {
    return {normalMs, UINT32_MAX};
  }
  return pageTimingAt(local.tm_hour * 3600U + local.tm_min * 60U + local.tm_sec,
      normalMs, AppConfig::kSlowPageStartHour, AppConfig::kSlowPageEndHour,
      AppConfig::kSlowPageDurationMs);
}

bool refreshContent() {
  if (!hasPendingContentRefresh()) {
    return true;
  }
  WifiSession wifiSession;
  // A cold boot may connect once for NTP, before any application requests.
  if (!networkTimeReady) {
    syncNetworkTime();
  }
  if (!networkRequestsAllowed() || !connectWifi()) {
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
    if (!epaperDisplay.prepareFrame(refreshedImage, cachedFrames[index])) {
      Serial.println("图片解码或 CRC 校验失败，继续保留旧图片");
      continue;
    }

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
  if (!displayReady) {
    return false;
  }
  if (refreshContent()) {
    lastContentRefreshMs = millis();
  }
  lastContentRefreshAttemptMs = millis();

  const size_t firstPage =
      findReadyPage(0, AppConfig::kPageCount, false, isPageReady);
  if (firstPage == SIZE_MAX || !showPage(firstPage)) {
    Serial.println("暂无可显示页面，将稍后重试");
    return false;
  }
  Serial.println("开始展示已准备好的页面，缺失内容将在后续重试");

  initialPreviewActive = AppConfig::kInitialPreviewEnabled;
  if (initialPreviewActive) {
    Serial.println("开始首次预览：通常每页 10 秒，凌晨 02:00–08:00 每页 30 分钟");
  }
  slideshowReady = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  if (!initializeWifiPolicy()) {
    Serial.println("禁网监控初始化失败，禁止联网");
  }
  if (AppConfig::kBoardRgbLedPin >= 0) {
    const uint8_t ledPin = AppConfig::kBoardRgbLedPin;
    rgbLedWrite(ledPin, 0, 0, 0);
    rmtDeinit(ledPin);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    delay(1);  // Latch the all-zero WS2812 frame before parking DIN high.
    // R1 is 1 kOhm to 3.3 V: leaving DIN low would waste about 3.3 mA.
    digitalWrite(ledPin, HIGH);
  }
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

}

void sleepUntilNextTask() {
  // All network requests and panel refreshes are synchronous and complete here.
  if (!disconnectWifi()) {
    delay(1000);
    return;
  }
  if (!AppConfig::kLightSleepEnabled) {
    delay(10);
    return;
  }

  const uint32_t now = millis();
  uint32_t waitMs = UINT32_MAX;
  if (AppConfig::kSht41Enabled) {
    waitMs = min(waitMs, remainingUntil(now, lastSensorReadMs,
        sht41Ready ? AppConfig::kSensorReadIntervalMs
                   : AppConfig::kSensorRetryIntervalMs));
  }
  if (!networkTimeReady) {
    waitMs = min(waitMs, max(wifiRetryRemainingMs(now),
        remainingUntil(now, lastTimeSyncAttemptMs,
                       AppConfig::kTimeSyncRetryIntervalMs)));
  }
  if (!slideshowReady) {
    waitMs = min(waitMs, nextSlideshowAttemptWaitMs(now));
  } else {
    waitMs = min(waitMs, nextContentAttemptWaitMs(now));
    const PageTiming timing = currentPageTiming();
    waitMs = min(waitMs, timing.boundaryWaitMs);
    const uint32_t pageInterval = pageDisplayRetryPending
        ? (initialPreviewActive ? AppConfig::kInitialPreviewPageDurationMs
                                : AppConfig::kPageDisplayRetryIntervalMs)
        : timing.durationMs;
    waitMs = min(waitMs, remainingUntil(now,
        pageDisplayRetryPending ? lastPageDisplayAttemptMs : lastPageChangeMs,
        pageInterval));
  }

  if (waitMs < AppConfig::kMinimumSleepMs) {
    delay(1);
    return;
  }
  // Arduino-ESP32 millis() uses esp_timer, compensated across light sleep.
  // Keep default memory power domains: the slideshow cache lives in PSRAM.
  esp_err_t result = esp_sleep_enable_timer_wakeup(uint64_t(waitMs) * 1000ULL);
  if (result == ESP_OK) {
    Serial.flush();
    result = esp_light_sleep_start();
  }
  if (result != ESP_OK) {
    Serial.printf("浅睡眠失败: %s\n", esp_err_to_name(result));
    delay(1000);  // Avoid a busy loop if hardware/SDK rejects sleep.
  }
}

void loop() {
  updateSht41();

  uint32_t now = millis();
  if (!networkTimeReady &&
      wifiRetryRemainingMs(now) == 0 &&
      now - lastTimeSyncAttemptMs >= AppConfig::kTimeSyncRetryIntervalMs) {
    syncNetworkTime();
    now = millis();
  }

  if (!slideshowReady) {
    if (nextSlideshowAttemptWaitMs(now) == 0) {
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
    sleepUntilNextTask();
    return;
  }

  if (nextContentAttemptWaitMs(now) == 0) {
    Serial.print("正在更新待获取的内容，界面数量: ");
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
  const PageTiming timing = currentPageTiming();
  if (initialPreviewActive &&
      isPageDisplayDue(now, timing.durationMs,
                       AppConfig::kInitialPreviewPageDurationMs)) {
    const size_t nextPageIndex = findReadyPage(
        currentPageIndex + 1, AppConfig::kPageCount, false, isPageReady);
    if (nextPageIndex != SIZE_MAX) {
      showPage(nextPageIndex);
    } else {
      const size_t firstPage =
          findReadyPage(0, AppConfig::kPageCount, false, isPageReady);
      if (firstPage == SIZE_MAX) {
        initialPreviewActive = false;
        slideshowReady = false;
        pageDisplayRetryPending = false;
        lastSlideshowAttemptMs = millis();
        sleepUntilNextTask();
        return;
      }
      if (firstPage != currentPageIndex && !showPage(firstPage)) {
        sleepUntilNextTask();
        return;
      }
      lastPageChangeMs = millis();
      initialPreviewActive = false;
      Serial.println(
          "预览完成：图片每页 10 分钟，新闻每页 5 分钟，凌晨 02:00–08:00 每页 30 分钟");
    }
  } else if (!initialPreviewActive &&
             isPageDisplayDue(now, timing.durationMs,
                              AppConfig::kPageDisplayRetryIntervalMs)) {
    const size_t nextPageIndex = findReadyPage(
        currentPageIndex + 1, AppConfig::kPageCount, true, isPageReady);
    // 显示失败时保留当前页，并在 30 秒后重试，避免频繁刷新。
    if (nextPageIndex != SIZE_MAX) {
      showPage(nextPageIndex);
    } else {
      slideshowReady = false;
      pageDisplayRetryPending = false;
      lastSlideshowAttemptMs = millis();
    }
  }

  sleepUntilNextTask();
}
