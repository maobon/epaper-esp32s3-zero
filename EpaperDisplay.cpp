#include "EpaperDisplay.h"

#include <PNGdec.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

#include "DisplayConfig.h"

namespace {

GxEPD2_426_GDEQ0426T82 displayDriver(
    DisplayConfig::kChipSelectPin, DisplayConfig::kDataCommandPin,
    DisplayConfig::kResetPin, DisplayConfig::kBusyPin);
PNG pngDecoder;

struct DecodeContext {
  PsramImage *frameBuffer;
  int sourceWidth;
  int sourceHeight;
  bool rotateClockwise;
  uint16_t rgbLine[DisplayConfig::kWidth];
};

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

  if (!frameBuffer.allocate(DisplayConfig::kFrameBufferSize)) {
    Serial.println("无法在 PSRAM 中分配 1-bit 显示帧");
    pngDecoder.close();
    return false;
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
    frameBuffer.clear();
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

bool EpaperDisplay::showPng(const PsramImage &pngImage) {
  if (!initialized_) {
    Serial.println("墨水屏尚未初始化");
    return false;
  }

  PsramImage frameBuffer;
  if (!decodePngToMonochrome(pngImage, frameBuffer)) {
    return false;
  }

  Serial.println("正在将显示帧写入 SSD1677...");
  displayDriver.writeImageForFullRefresh(
      frameBuffer.data(), 0, 0, DisplayConfig::kWidth,
      DisplayConfig::kHeight, false, false, false);
  displayDriver.refresh(false);
  displayDriver.hibernate();
  Serial.println("墨水屏刷新完成，已进入深度休眠");
  return true;
}
