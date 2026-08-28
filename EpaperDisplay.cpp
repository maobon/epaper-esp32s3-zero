#include "EpaperDisplay.h"

#include <PNGdec.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <stdio.h>

#include "DisplayConfig.h"

namespace {

GxEPD2_426_GDEQ0426T82 displayDriver(
    DisplayConfig::kChipSelectPin, DisplayConfig::kDataCommandPin,
    DisplayConfig::kResetPin, DisplayConfig::kBusyPin);
PNG pngDecoder;

// Sensor panel coordinates are expressed in the 480x800 portrait image space.
// The display buffer itself is 800x480, so pixels are rotated when written.
constexpr int kSensorPanelX = 320;
constexpr int kSensorPanelY = 14;
constexpr int kSensorPanelWidth = 155;
constexpr int kSensorPanelHeight = 36;
constexpr int kSensorTextScale = 2;
static_assert(kSensorPanelX + kSensorPanelWidth <= DisplayConfig::kHeight);
static_assert(kSensorPanelY + kSensorPanelHeight <= DisplayConfig::kWidth);

struct DecodeContext {
  PsramImage *frameBuffer;
  int sourceWidth;
  int sourceHeight;
  bool rotateClockwise;
  uint16_t rgbLine[DisplayConfig::kWidth];
};

const uint8_t *glyphFor(char character) {
  static constexpr uint8_t kSpace[] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr uint8_t kDash[] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static constexpr uint8_t kDot[] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static constexpr uint8_t kColon[] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static constexpr uint8_t kPercent[] = {0x63, 0x13, 0x08, 0x64, 0x63};
  static constexpr uint8_t kC[] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static constexpr uint8_t kH[] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static constexpr uint8_t kT[] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static constexpr uint8_t kDigits[][5] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E},
      {0x00, 0x42, 0x7F, 0x40, 0x00},
      {0x42, 0x61, 0x51, 0x49, 0x46},
      {0x21, 0x41, 0x45, 0x4B, 0x31},
      {0x18, 0x14, 0x12, 0x7F, 0x10},
      {0x27, 0x45, 0x45, 0x45, 0x39},
      {0x3C, 0x4A, 0x49, 0x49, 0x30},
      {0x01, 0x71, 0x09, 0x05, 0x03},
      {0x36, 0x49, 0x49, 0x49, 0x36},
      {0x06, 0x49, 0x49, 0x29, 0x1E},
  };

  if (character >= '0' && character <= '9') {
    return kDigits[character - '0'];
  }
  switch (character) {
    case '-':
      return kDash;
    case '.':
      return kDot;
    case ':':
      return kColon;
    case '%':
      return kPercent;
    case 'C':
      return kC;
    case 'H':
      return kH;
    case 'T':
      return kT;
    default:
      return kSpace;
  }
}

void setFramePixel(PsramImage &frameBuffer, int x, int y, bool black) {
  if (x < 0 || x >= DisplayConfig::kWidth || y < 0 ||
      y >= DisplayConfig::kHeight) {
    return;
  }

  const size_t byteOffset =
      static_cast<size_t>(y) * DisplayConfig::kWidth / 8 +
      static_cast<size_t>(x) / 8;
  const uint8_t mask = 0x80U >> (x & 7);
  if (black) {
    frameBuffer.data()[byteOffset] &= static_cast<uint8_t>(~mask);
  } else {
    frameBuffer.data()[byteOffset] |= mask;
  }
}

void setPortraitPixel(PsramImage &frameBuffer, int x, int y, bool black) {
  setFramePixel(frameBuffer, DisplayConfig::kWidth - 1 - y, x, black);
}

void fillPortraitRectangle(PsramImage &frameBuffer, int x, int y, int width,
                           int height, bool black) {
  for (int row = y; row < y + height; ++row) {
    for (int column = x; column < x + width; ++column) {
      setPortraitPixel(frameBuffer, column, row, black);
    }
  }
}

void drawPortraitText(PsramImage &frameBuffer, int x, int y, const char *text,
                      int scale) {
  while (*text != '\0') {
    const uint8_t *glyph = glyphFor(*text++);
    for (int column = 0; column < 5; ++column) {
      for (int row = 0; row < 7; ++row) {
        if ((glyph[column] & (1U << row)) == 0) {
          continue;
        }
        fillPortraitRectangle(frameBuffer, x + column * scale,
                              y + row * scale, scale, scale, true);
      }
    }
    x += 6 * scale;
  }
}

void drawSensorPanel(PsramImage &frameBuffer, float temperatureCelsius,
                     float relativeHumidity, bool sensorDataValid) {
  fillPortraitRectangle(frameBuffer, kSensorPanelX, kSensorPanelY,
                        kSensorPanelWidth, kSensorPanelHeight, false);
  fillPortraitRectangle(frameBuffer, kSensorPanelX, kSensorPanelY,
                        kSensorPanelWidth, 2, true);
  fillPortraitRectangle(frameBuffer, kSensorPanelX,
                        kSensorPanelY + kSensorPanelHeight - 2,
                        kSensorPanelWidth, 2, true);
  fillPortraitRectangle(frameBuffer, kSensorPanelX, kSensorPanelY, 2,
                        kSensorPanelHeight, true);
  fillPortraitRectangle(frameBuffer,
                        kSensorPanelX + kSensorPanelWidth - 2, kSensorPanelY,
                        2, kSensorPanelHeight, true);

  char sensorText[24] = "--.-C --.-%";
  if (sensorDataValid) {
    snprintf(sensorText, sizeof(sensorText), "%.1fC %.1f%%",
             temperatureCelsius, relativeHumidity);
  }

  drawPortraitText(frameBuffer, kSensorPanelX + 10, kSensorPanelY + 11,
                   sensorText, kSensorTextScale);
}

uint8_t rgb565Luminance(uint16_t color) {
  const uint16_t red = ((color >> 11) & 0x1F) * 255 / 31;
  const uint16_t green = ((color >> 5) & 0x3F) * 255 / 63;
  const uint16_t blue = (color & 0x1F) * 255 / 31;
  return static_cast<uint8_t>((red * 299UL + green * 587UL +
                               blue * 114UL) /
                              1000UL);
}

int decodePngLine(PNGDRAW *draw) {
  DecodeContext *context = static_cast<DecodeContext *>(draw->pUser);
  if (context == nullptr || context->frameBuffer == nullptr || draw->y < 0 ||
      draw->y >= context->sourceHeight ||
      draw->iWidth != context->sourceWidth) {
    return 0;
  }

  pngDecoder.getLineAsRGB565(draw, context->rgbLine,
                             PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);

  uint8_t *frame = context->frameBuffer->data();
  for (int x = 0; x < draw->iWidth; ++x) {
    if (rgb565Luminance(context->rgbLine[x]) <
        DisplayConfig::kBlackThreshold) {
      const int destinationX = context->rotateClockwise
                                   ? context->sourceHeight - 1 - draw->y
                                   : x;
      const int destinationY = context->rotateClockwise ? x : draw->y;
      const size_t byteOffset =
          static_cast<size_t>(destinationY) * DisplayConfig::kWidth / 8 +
          static_cast<size_t>(destinationX) / 8;
      frame[byteOffset] &=
          static_cast<uint8_t>(~(0x80U >> (destinationX & 7)));
    }
  }
  return 1;
}

bool decodePngToMonochrome(const PsramImage &pngImage,
                           PsramImage &frameBuffer) {
  const int openResult =
      pngDecoder.openRAM(const_cast<uint8_t *>(pngImage.data()),
                         static_cast<int>(pngImage.size()), decodePngLine);
  if (openResult != PNG_SUCCESS) {
    Serial.print("PNG 打开失败，错误码: ");
    Serial.println(openResult);
    return false;
  }

  Serial.print("PNG 尺寸: ");
  Serial.print(pngDecoder.getWidth());
  Serial.print('x');
  Serial.println(pngDecoder.getHeight());

  const int sourceWidth = pngDecoder.getWidth();
  const int sourceHeight = pngDecoder.getHeight();
  const bool isLandscape = sourceWidth == DisplayConfig::kWidth &&
                           sourceHeight == DisplayConfig::kHeight;
  const bool isPortrait = sourceWidth == DisplayConfig::kHeight &&
                          sourceHeight == DisplayConfig::kWidth;
  if (!isLandscape && !isPortrait) {
    Serial.println("PNG 尺寸必须为 800x480 或 480x800");
    pngDecoder.close();
    return false;
  }

  if (frameBuffer.capacity() < DisplayConfig::kFrameBufferSize) {
    if (!frameBuffer.allocate(DisplayConfig::kFrameBufferSize)) {
      Serial.println("无法在 PSRAM 中分配 1-bit 显示帧");
      pngDecoder.close();
      return false;
    }
  }
  memset(frameBuffer.data(), 0xFF, DisplayConfig::kFrameBufferSize);
  frameBuffer.setSize(DisplayConfig::kFrameBufferSize);

  if (isPortrait) {
    Serial.println("竖向 PNG 将顺时针旋转为 800x480");
  }

  DecodeContext context = {
      &frameBuffer, sourceWidth, sourceHeight, isPortrait, {0}};
  const int decodeResult = pngDecoder.decode(&context, PNG_FAST_PALETTE);
  pngDecoder.close();

  if (decodeResult != PNG_SUCCESS) {
    Serial.print("PNG 解码失败，错误码: ");
    Serial.println(decodeResult);
    frameBuffer.setSize(0);
    return false;
  }

  Serial.println("PNG 已转换为 800x480 1-bit 显示帧");
  return true;
}

}  // namespace

bool EpaperDisplay::begin() {
  SPI.begin(DisplayConfig::kClockPin, DisplayConfig::kMisoPin,
            DisplayConfig::kMosiPin, DisplayConfig::kChipSelectPin);
  displayDriver.init(0, true, 10, false);
  initialized_ = true;
  Serial.println("墨水屏 SPI 驱动已初始化");
  return true;
}

bool EpaperDisplay::showPng(const PsramImage &pngImage,
                            float temperatureCelsius,
                            float relativeHumidity,
                            bool showSensorPanel,
                            bool sensorDataValid) {
  if (!initialized_) {
    Serial.println("墨水屏尚未初始化");
    return false;
  }

  if (!decodePngToMonochrome(pngImage, frameBuffer_)) {
    return false;
  }

  if (showSensorPanel) {
    drawSensorPanel(frameBuffer_, temperatureCelsius, relativeHumidity,
                    sensorDataValid);
  }

  Serial.println("正在将显示帧写入 SSD1677...");
  displayDriver.writeImageForFullRefresh(
      frameBuffer_.data(), 0, 0, DisplayConfig::kWidth,
      DisplayConfig::kHeight, false, false, false);
  displayDriver.refresh(false);
  displayDriver.hibernate();
  Serial.println("墨水屏刷新完成，已进入深度休眠");
  return true;
}
