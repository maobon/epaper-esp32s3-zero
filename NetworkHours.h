#pragma once

#include <stdint.h>

// Local wall-clock time, [00:00:00, resumeHour:00:00) is offline.
inline uint32_t quietNetworkWaitMs(int hour, int minute, int second,
                                    int resumeHour) {
  const uint32_t seconds = hour * 3600U + minute * 60U + second;
  const uint32_t resumeSeconds = resumeHour * 3600U;
  return seconds < resumeSeconds ? (resumeSeconds - seconds) * 1000U : 0;
}
