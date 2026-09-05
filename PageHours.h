#pragma once

#include <stdint.h>

struct PageTiming {
  uint32_t durationMs;
  uint32_t boundaryWaitMs;
};

// Boundary wakeups ensure an already sleeping page adopts the new interval.
inline PageTiming pageTimingAt(uint32_t secondOfDay, uint32_t normalMs,
                                uint8_t startHour, uint8_t endHour,
                                uint32_t slowMs) {
  const uint32_t start = startHour * 3600U;
  const uint32_t end = endHour * 3600U;
  const bool slow = secondOfDay >= start && secondOfDay < end;
  const uint32_t untilBoundary = secondOfDay < start ? start - secondOfDay
      : slow ? end - secondOfDay : 86400U - secondOfDay + start;
  return {slow ? slowMs : normalMs, untilBoundary * 1000U};
}
