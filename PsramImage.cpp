#include "PsramImage.h"

#include <esp32-hal-psram.h>

PsramImage::~PsramImage() {
  clear();
}

bool PsramImage::allocate(size_t initialCapacity) {
  clear();
  if (initialCapacity == 0) {
    return false;
  }

  data_ = static_cast<uint8_t *>(ps_malloc(initialCapacity));
  if (data_ == nullptr) {
    return false;
  }

  capacity_ = initialCapacity;
  return true;
}

bool PsramImage::ensureCapacity(size_t requiredCapacity,
                                size_t maximumCapacity) {
  if (requiredCapacity > maximumCapacity) {
    return false;
  }
  if (requiredCapacity <= capacity_) {
    return true;
  }

  size_t newCapacity = capacity_;
  while (newCapacity < requiredCapacity) {
    if (newCapacity == 0) {
      newCapacity = requiredCapacity;
    } else if (newCapacity > maximumCapacity / 2) {
      newCapacity = maximumCapacity;
    } else {
      newCapacity *= 2;
    }
  }

  uint8_t *newData = static_cast<uint8_t *>(ps_realloc(data_, newCapacity));
  if (newData == nullptr) {
    return false;
  }

  data_ = newData;
  capacity_ = newCapacity;
  return true;
}

void PsramImage::setSize(size_t size) {
  size_ = size <= capacity_ ? size : capacity_;
}

void PsramImage::clear() {
  free(data_);
  data_ = nullptr;
  size_ = 0;
  capacity_ = 0;
}

void PsramImage::swap(PsramImage &other) {
  uint8_t *otherData = other.data_;
  const size_t otherSize = other.size_;
  const size_t otherCapacity = other.capacity_;

  other.data_ = data_;
  other.size_ = size_;
  other.capacity_ = capacity_;

  data_ = otherData;
  size_ = otherSize;
  capacity_ = otherCapacity;
}

uint8_t *PsramImage::data() {
  return data_;
}

const uint8_t *PsramImage::data() const {
  return data_;
}

size_t PsramImage::size() const {
  return size_;
}

size_t PsramImage::capacity() const {
  return capacity_;
}
