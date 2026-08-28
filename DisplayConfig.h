#pragma once

#include <Arduino.h>

namespace DisplayConfig {

inline constexpr int8_t kBusyPin = 7;
inline constexpr int8_t kResetPin = 8;
inline constexpr int8_t kDataCommandPin = 9;
inline constexpr int8_t kChipSelectPin = 10;
inline constexpr int8_t kMosiPin = 11;
inline constexpr int8_t kClockPin = 12;
inline constexpr int8_t kMisoPin = -1;

inline constexpr uint16_t kWidth = 800;
inline constexpr uint16_t kHeight = 480;
// 提高阈值可保留抗锯齿文字和图标中的中灰色像素，使细节更清晰。
inline constexpr uint8_t kBlackThreshold = 190;
inline constexpr size_t kFrameBufferSize =
    static_cast<size_t>(kWidth) * kHeight / 8;

}  // namespace DisplayConfig
