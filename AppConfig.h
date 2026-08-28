#pragma once

#include <Arduino.h>

namespace AppConfig {

inline constexpr char kLoginUrl[] = "https://api.maobon.site/login";
inline constexpr char kEpaperBaseUrl[] =
    "https://api.maobon.site/api/epaper/";
inline constexpr const char *kImageNames[] = {
    "currency", "landscape", "forecast", "portrait"};
inline constexpr size_t kImageCount =
    sizeof(kImageNames) / sizeof(kImageNames[0]);
inline constexpr uint32_t kWifiConnectTimeoutMs = 20000;
inline constexpr uint32_t kHttpTimeoutMs = 15000;
inline constexpr uint32_t kSlideshowRetryIntervalMs = 60000;
inline constexpr uint32_t kTimeSyncTimeoutMs = 10000;
inline constexpr uint32_t kTimeSyncRetryIntervalMs = 60000;
inline constexpr uint32_t kPageDisplayDurationMs = 30000;
inline constexpr uint32_t kSensorReadIntervalMs = 5000;
inline constexpr char kTimeZone[] = "CST-8";
inline constexpr char kPrimaryNtpServer[] = "ntp.aliyun.com";
inline constexpr char kSecondaryNtpServer[] = "pool.ntp.org";
inline constexpr int8_t kSht41SdaPin = 5;
inline constexpr int8_t kSht41SclPin = 6;
inline constexpr uint8_t kSht41I2cAddress = 0x44;
inline constexpr size_t kInitialImageCapacity = 32 * 1024;
inline constexpr size_t kMaxImageSizeBytes = 2 * 1024 * 1024;

}  // namespace AppConfig
