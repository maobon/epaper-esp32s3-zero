#pragma once

#include <Arduino.h>
#include "PsramImage.h"

class PsramImageWriteStream : public Stream {
 public:
  PsramImageWriteStream(PsramImage &image, size_t maximumSize)
      : image_(image), maximumSize_(maximumSize) {}

  size_t write(uint8_t value) override {
    return write(&value, 1);
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (size == 0) {
      return 0;
    }
    const size_t required = bytesWritten_ + size;
    if (required < bytesWritten_ || required > maximumSize_ ||
        !image_.ensureCapacity(required, maximumSize_)) {
      return 0;
    }

    memcpy(image_.data() + bytesWritten_, buffer, size);
    bytesWritten_ = required;
    return size;
  }

  int available() override {
    return 0;
  }

  int read() override {
    return -1;
  }

  int peek() override {
    return -1;
  }

  void flush() override {}

  size_t bytesWritten() const {
    return bytesWritten_;
  }

 private:
  PsramImage &image_;
  const size_t maximumSize_;
  size_t bytesWritten_ = 0;
};
