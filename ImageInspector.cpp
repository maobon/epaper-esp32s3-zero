#include "ImageInspector.h"

#include <Arduino.h>

namespace {

constexpr size_t kHeaderBytesToPrint = 16;

bool startsWith(const PsramImage &image, const uint8_t *signature,
                size_t signatureSize) {
  if (image.size() < signatureSize) {
    return false;
  }

  for (size_t index = 0; index < signatureSize; ++index) {
    if (image.data()[index] != signature[index]) {
      return false;
    }
  }
  return true;
}

const char *detectImageFormat(const PsramImage &image) {
  static constexpr uint8_t kPngSignature[] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  static constexpr uint8_t kJpegSignature[] = {0xFF, 0xD8, 0xFF};
  static constexpr uint8_t kBmpSignature[] = {'B', 'M'};
  static constexpr uint8_t kGif87aSignature[] = {'G', 'I', 'F', '8', '7', 'a'};
  static constexpr uint8_t kGif89aSignature[] = {'G', 'I', 'F', '8', '9', 'a'};
  static constexpr uint8_t kRiffSignature[] = {'R', 'I', 'F', 'F'};
  static constexpr uint8_t kWebpSignature[] = {'W', 'E', 'B', 'P'};

  if (startsWith(image, kPngSignature, sizeof(kPngSignature))) {
    return "PNG";
  }
  if (startsWith(image, kJpegSignature, sizeof(kJpegSignature))) {
    return "JPEG";
  }
  if (startsWith(image, kBmpSignature, sizeof(kBmpSignature))) {
    return "BMP";
  }
  if (startsWith(image, kGif87aSignature, sizeof(kGif87aSignature)) ||
      startsWith(image, kGif89aSignature, sizeof(kGif89aSignature))) {
    return "GIF";
  }
  if (image.size() >= 12 &&
      startsWith(image, kRiffSignature, sizeof(kRiffSignature)) &&
      memcmp(image.data() + 8, kWebpSignature, sizeof(kWebpSignature)) == 0) {
    return "WebP";
  }
  return "未知";
}

uint32_t calculateFnv1a(const PsramImage &image) {
  uint32_t hash = 2166136261UL;
  for (size_t index = 0; index < image.size(); ++index) {
    hash ^= image.data()[index];
    hash *= 16777619UL;
  }
  return hash;
}

void printHeaderBytes(const PsramImage &image) {
  const size_t byteCount =
      image.size() < kHeaderBytesToPrint ? image.size() : kHeaderBytesToPrint;

  Serial.print("图片头字节: ");
  for (size_t index = 0; index < byteCount; ++index) {
    const uint8_t value = image.data()[index];
    if (value < 0x10) {
      Serial.print('0');
    }
    Serial.print(value, HEX);
    if (index + 1 < byteCount) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

}  // namespace

bool inspectPsramImage(const PsramImage &image) {
  if (image.data() == nullptr || image.size() == 0) {
    Serial.println("PSRAM 图片缓冲区为空");
    return false;
  }

  Serial.println("开始读取 PSRAM 中的图片...");
  Serial.print("图片地址: 0x");
  Serial.println(reinterpret_cast<uintptr_t>(image.data()), HEX);
  Serial.print("图片大小: ");
  Serial.print(image.size());
  Serial.println(" 字节");
  Serial.print("识别格式: ");
  Serial.println(detectImageFormat(image));
  printHeaderBytes(image);

  const uint32_t checksum = calculateFnv1a(image);
  Serial.print("FNV-1a 校验值: 0x");
  Serial.println(checksum, HEX);
  Serial.println("PSRAM 图片读取完成");
  return true;
}
