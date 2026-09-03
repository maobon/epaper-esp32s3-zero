#pragma once

#include <Arduino.h>

namespace AppConfig {

inline constexpr char kLoginUrl[] = "https://api.maobon.site/login";
inline constexpr char kEpaperBaseUrl[] =
    "https://api.maobon.site/api/epaper/";
inline constexpr char kNewsUrl[] = "https://api.maobon.site/api/news";
inline constexpr const char *kImageNames[] = {
    "currency", "landscape", "forecast", "portrait"};
inline constexpr size_t kImageCount =
    sizeof(kImageNames) / sizeof(kImageNames[0]);
inline constexpr size_t kNewsPageIndex = kImageCount;
inline constexpr size_t kNewsPageCount = 2;
inline constexpr size_t kNewsItemsPerPage = 5;
inline constexpr size_t kNewsItemCount =
    kNewsPageCount * kNewsItemsPerPage;
inline constexpr size_t kPageCount = kImageCount + kNewsPageCount;
static_assert(kNewsPageCount > 0 && kNewsItemsPerPage > 0,
              "News pagination must not be empty");
static_assert(kNewsItemCount <= 100,
              "News API accepts at most 100 items per request");
inline constexpr size_t kForecastPageIndex = 2;
static_assert(kForecastPageIndex < kImageCount,
              "Forecast page index must refer to an image");
inline constexpr uint32_t kWifiConnectTimeoutMs = 20000;
inline constexpr uint32_t kHttpTimeoutMs = 15000;
inline constexpr size_t kMaxNewsResponseBytes = 32 * 1024;
inline constexpr uint32_t kSlideshowRetryIntervalMs = 60000;
inline constexpr uint32_t kContentRefreshRetryIntervalMs = 60000;
inline constexpr uint32_t kTimeSyncTimeoutMs = 10000;
inline constexpr uint32_t kTimeSyncRetryIntervalMs = 60000;
inline constexpr uint32_t kContentRefreshIntervalMs =
    3UL * 60UL * 60UL * 1000UL;
inline constexpr uint32_t kInitialPreviewPageDurationMs = 10UL * 1000UL;
inline constexpr uint32_t kPageDisplayDurationMs = 10UL * 60UL * 1000UL;
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
