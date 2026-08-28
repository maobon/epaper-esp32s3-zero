#pragma once

#include <Arduino.h>

class PsramImage {
 public:
  PsramImage() = default;
  ~PsramImage();

  PsramImage(const PsramImage &) = delete;
  PsramImage &operator=(const PsramImage &) = delete;

  bool allocate(size_t initialCapacity);
  bool ensureCapacity(size_t requiredCapacity,
                      size_t maximumCapacity = SIZE_MAX);
  void setSize(size_t size);
  void clear();
  void swap(PsramImage &other);

  uint8_t *data();
  const uint8_t *data() const;
  size_t size() const;
  size_t capacity() const;

 private:
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
};
