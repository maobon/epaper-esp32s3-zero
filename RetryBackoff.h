#pragma once

#include "SleepSchedule.h"

class RetryBackoff {
 public:
  RetryBackoff(uint32_t initialMs, uint32_t maximumMs)
      : initialMs_(initialMs), maximumMs_(maximumMs) {}

  uint32_t remaining(uint32_t now) const {
    return remainingUntil(now, failedAt_, intervalMs_);
  }

  void failed(uint32_t now) {
    failedAt_ = now;
    intervalMs_ = intervalMs_ == 0 ? initialMs_
        : intervalMs_ > maximumMs_ / 2 ? maximumMs_ : intervalMs_ * 2;
  }

  void succeeded() { intervalMs_ = 0; }

 private:
  uint32_t initialMs_;
  uint32_t maximumMs_;
  uint32_t failedAt_ = 0;
  uint32_t intervalMs_ = 0;
};
