#pragma once

#include <stdint.h>

// Unsigned subtraction also handles the ~49.7-day millis() rollover.
inline uint32_t remainingUntil(uint32_t now, uint32_t previous,
                               uint32_t interval) {
  const uint32_t elapsed = now - previous;
  return elapsed >= interval ? 0 : interval - elapsed;
}

inline uint32_t contentWaitMs(uint32_t now, uint32_t lastSuccess,
                              uint32_t lastAttempt, uint32_t refreshInterval,
                              uint32_t retryInterval, bool pending,
                              uint32_t networkWait) {
  const uint32_t refreshWait =
      pending ? 0 : remainingUntil(now, lastSuccess, refreshInterval);
  const uint32_t retryWait = remainingUntil(now, lastAttempt, retryInterval);
  const uint32_t taskWait = refreshWait > retryWait ? refreshWait : retryWait;
  return taskWait > networkWait ? taskWait : networkWait;
}
