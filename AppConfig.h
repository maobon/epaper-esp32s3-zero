#pragma once

#include <Arduino.h>

namespace AppConfig {

inline constexpr char kLoginUrl[] = "https://api.maobon.site/login";
inline constexpr char kEpaperBaseUrl[] =
    "https://api.maobon.site/api/epaper/";
inline constexpr char kNewsUrl[] = "https://api.maobon.site/api/news";
inline constexpr char kChineseNewsUrl[] =
    "https://api.maobon.site/api/news-audio";
inline constexpr const char *kImageNames[] = {
    "currency", "landscape", "forecast", "portrait"};
inline constexpr size_t kImageCount =
    sizeof(kImageNames) / sizeof(kImageNames[0]);
inline constexpr size_t kNewsPageIndex = kImageCount;
inline constexpr size_t kNewsPageCount = 4;
inline constexpr size_t kNewsItemsPerPage = 3;
inline constexpr size_t kNewsItemCount =
    kNewsPageCount * kNewsItemsPerPage;
inline constexpr size_t kChineseNewsPageIndex =
    kNewsPageIndex + kNewsPageCount;
inline constexpr size_t kChineseNewsPageCount = 2;
inline constexpr size_t kChineseNewsItemsPerPage = 5;
inline constexpr size_t kChineseNewsItemCount = 10;
inline constexpr size_t kNewsStorageCount =
    kNewsItemCount > kChineseNewsItemCount ? kNewsItemCount
                                           : kChineseNewsItemCount;
inline constexpr size_t kInterfaceCount = kImageCount + 2;
inline constexpr size_t kPageCount =
    kImageCount + kNewsPageCount + kChineseNewsPageCount;
static_assert(kNewsPageCount > 0 && kNewsItemsPerPage > 0,
              "News pagination must not be empty");
static_assert(kNewsItemCount <= 100,
              "News API accepts at most 100 items per request");
static_assert(kChineseNewsPageCount > 0 && kChineseNewsItemsPerPage > 0,
              "Chinese news pagination must not be empty");
static_assert(kChineseNewsItemCount <=
                  kChineseNewsPageCount * kChineseNewsItemsPerPage,
              "Chinese news pages must hold every requested item");
static_assert(kChineseNewsItemCount <= 100,
              "Chinese news API accepts at most 100 items per request");
inline constexpr size_t kForecastPageIndex = 2;
static_assert(kForecastPageIndex < kImageCount,
              "Forecast page index must refer to an image");
inline constexpr uint32_t kWifiConnectTimeoutMs = 20000;
inline constexpr uint8_t kNetworkResumeHour = 8;
static_assert(kNetworkResumeHour > 0 && kNetworkResumeHour < 24);
inline constexpr uint32_t kWifiRetryInitialMs = 60000;
inline constexpr uint32_t kWifiRetryMaximumMs = 15UL * 60UL * 1000UL;
// Disable for continuous USB serial debugging. Wi-Fi remains on-demand.
inline constexpr bool kLightSleepEnabled = true;
// Waveshare ESP32-S3-Zero: WS2812 DIN has a 1 kOhm pull-up to 3.3 V.
// Set to -1 when porting to a board without this LED circuit.
inline constexpr int8_t kBoardRgbLedPin = 21;
inline constexpr uint32_t kMinimumSleepMs = 100;
// Optional: disable the ten-page startup preview to save panel refresh energy.
inline constexpr bool kInitialPreviewEnabled = true;
inline constexpr uint32_t kHttpTimeoutMs = 15000;
inline constexpr uint32_t kHttpConnectTimeoutMs = 10000;
inline constexpr uint32_t kTlsHandshakeTimeoutSeconds = 15;
inline constexpr size_t kMaxLoginResponseBytes = 8 * 1024;
inline constexpr size_t kMaxNewsResponseBytes = 32 * 1024;
inline constexpr uint32_t kSlideshowRetryIntervalMs = 60000;
inline constexpr uint32_t kContentRefreshRetryIntervalMs = 60000;
inline constexpr uint32_t kTimeSyncTimeoutMs = 10000;
inline constexpr uint32_t kTimeSyncRetryIntervalMs = 60000;
inline constexpr uint32_t kContentRefreshIntervalMs =
    2UL * 60UL * 60UL * 1000UL;
inline constexpr uint32_t kInitialPreviewPageDurationMs = 10UL * 1000UL;
inline constexpr uint8_t kSlowPageStartHour = 2;
inline constexpr uint8_t kSlowPageEndHour = 8;
inline constexpr uint32_t kSlowPageDurationMs = 30UL * 60UL * 1000UL;
static_assert(kSlowPageStartHour < kSlowPageEndHour && kSlowPageEndHour < 24);
inline constexpr uint32_t kImagePageDisplayDurationMs =
    10UL * 60UL * 1000UL;
inline constexpr uint32_t kNewsPageDisplayDurationMs =
    5UL * 60UL * 1000UL;
inline constexpr uint32_t kChineseNewsPageDisplayDurationMs =
    5UL * 60UL * 1000UL;
inline constexpr uint32_t kPageDisplayRetryIntervalMs = 30UL * 1000UL;
inline constexpr bool kSht41Enabled = false;
inline constexpr uint32_t kSensorReadIntervalMs = 30000;
inline constexpr uint32_t kSensorRetryIntervalMs = 30000;
inline constexpr float kSht41TemperatureOffsetCelsius = -1.65F;
inline constexpr char kTimeZone[] = "CST-8";
inline constexpr char kPrimaryNtpServer[] = "ntp.aliyun.com";
inline constexpr char kSecondaryNtpServer[] = "pool.ntp.org";
inline constexpr int8_t kSht41SdaPin = 5;
inline constexpr int8_t kSht41SclPin = 6;
inline constexpr uint8_t kSht41I2cAddress = 0x44;
inline constexpr size_t kInitialImageCapacity = 32 * 1024;
inline constexpr size_t kMaxImageSizeBytes = 2 * 1024 * 1024;

}  // namespace AppConfig
