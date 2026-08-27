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
inline constexpr uint32_t kImageCheckDelayMs = 2000;
inline constexpr uint32_t kNextImageDelayMs = 2000;
inline constexpr size_t kInitialImageCapacity = 32 * 1024;

}  // namespace AppConfig
