#include "EpaperDisplay.h"

#include <PNGdec.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSerifBold18pt7b.h>
#include <stdio.h>

#include "DisplayConfig.h"

namespace {

// A small paged buffer leaves enough internal RAM for Wi-Fi/TLS and JSON.
// The news drawing callback is replayed once per 40-row band by GxEPD2.
GxEPD2_BW<GxEPD2_426_GDEQ0426T82, 40>
    displayDriver(GxEPD2_426_GDEQ0426T82(
        DisplayConfig::kChipSelectPin, DisplayConfig::kDataCommandPin,
        DisplayConfig::kResetPin, DisplayConfig::kBusyPin));

class ScaledTextCanvas : public Adafruit_GFX {
 public:
  explicit ScaledTextCanvas(Adafruit_GFX &target)
      : Adafruit_GFX(DisplayConfig::kWidth / 2, DisplayConfig::kHeight / 2),
        target_(target) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    drawScaledRect(x, y, 1, 1, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t width,
                     uint16_t color) override {
    drawScaledRect(x, y, width, 1, color);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t height,
                     uint16_t color) override {
    drawScaledRect(x, y, 1, height, color);
  }

 private:
  void drawScaledRect(int16_t x, int16_t y, int16_t width, int16_t height,
                      uint16_t color) {
    target_.fillRect(x * 2, y * 2, width * 2, height * 2, color);
  }

  Adafruit_GFX &target_;
};

PNG pngDecoder;
U8G2_FOR_ADAFRUIT_GFX unicodeText;
U8G2_FOR_ADAFRUIT_GFX largeUnicodeText;
const uint8_t *const kChineseFont = u8g2_font_wqy16_t_gb2312;
ScaledTextCanvas largeTextCanvas(displayDriver);

// Sensor panel coordinates are expressed in the 480x800 portrait image space.
// The display buffer itself is 800x480, so pixels are rotated when written.
constexpr int kSensorPanelX = 320;
constexpr int kSensorPanelY = 14;
constexpr int kSensorPanelWidth = 155;
constexpr int kSensorPanelHeight = 36;
constexpr int kSensorTextScale = 2;
constexpr int kNewsLeftMargin = 30;
constexpr int kNewsTitleX = kNewsLeftMargin;
constexpr int kNewsFirstRowY = 82;
constexpr int kNewsRowHeight = 130;
constexpr int kNewsTitleWidth =
    DisplayConfig::kWidth - kNewsTitleX - kNewsLeftMargin;
constexpr int kNewsTextTopPadding = 6;
constexpr int kNewsSummaryGap = 32;
constexpr int kChineseNewsRowHeight = 78;
constexpr int kChineseScale = 2;
constexpr int kChineseTitleLineAdvance = 42;
constexpr int kChineseBulletX = kNewsLeftMargin - 12;
constexpr int kChineseBulletRadius = 4;
static_assert(kSensorPanelX + kSensorPanelWidth <= DisplayConfig::kHeight);
static_assert(kSensorPanelY + kSensorPanelHeight <= DisplayConfig::kWidth);

struct DecodeContext {
  PsramImage *frameBuffer;
  int sourceWidth;
  int sourceHeight;
  bool rotateClockwise;
  uint16_t rgbLine[DisplayConfig::kWidth];
  int decodedRows = 0;
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

String normalizeNewsText(const String &text) {
  String normalized;
  normalized.reserve(text.length());
  for (size_t index = 0; index < text.length();) {
    const uint8_t current = static_cast<uint8_t>(text[index]);
    if (current >= 0x20 && current < 0x7F) {
      normalized += static_cast<char>(current);
      ++index;
      continue;
    }

    if (current == 0xC2 && index + 1 < text.length() &&
        static_cast<uint8_t>(text[index + 1]) == 0xA0) {
      normalized += ' ';
      index += 2;
      continue;
    }
    if (current == 0xE2 && index + 2 < text.length() &&
        static_cast<uint8_t>(text[index + 1]) == 0x80) {
      const uint8_t punctuation = static_cast<uint8_t>(text[index + 2]);
      if (punctuation == 0x98 || punctuation == 0x99) {
        normalized += '\'';
      } else if (punctuation == 0x9C || punctuation == 0x9D) {
        normalized += '"';
      } else if (punctuation == 0x93 || punctuation == 0x94) {
        normalized += '-';
      } else if (punctuation == 0xA6) {
        normalized += "...";
      }
      index += 3;
      continue;
    }

    const size_t sequenceLength =
        (current & 0xE0) == 0xC0 ? 2 : (current & 0xF0) == 0xE0 ? 3 : 4;
    normalized += '?';
    index += sequenceLength;
  }
  return normalized;
}

uint16_t newsTextWidth(const String &text) {
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  displayDriver.getTextBounds(text, 0, 0, &boundsX, &boundsY, &width, &height);
  return width;
}

String takeWrappedLine(const String &text, size_t &offset,
                       uint16_t maximumWidth) {
  while (offset < text.length() && text[offset] == ' ') {
    ++offset;
  }
  if (offset >= text.length()) {
    return String();
  }

  const size_t lineStart = offset;
  size_t lastSpace = SIZE_MAX;
  for (size_t index = lineStart; index < text.length(); ++index) {
    if (text[index] == ' ') {
      lastSpace = index;
    }
    if (newsTextWidth(text.substring(lineStart, index + 1)) <= maximumWidth) {
      continue;
    }

    size_t lineEnd = lastSpace != SIZE_MAX && lastSpace > lineStart
                         ? lastSpace
                         : index;
    if (lineEnd == lineStart) {
      lineEnd = index + 1;
    }
    String line = text.substring(lineStart, lineEnd);
    line.trim();
    offset = lineEnd;
    return line;
  }

  String line = text.substring(lineStart);
  line.trim();
  offset = text.length();
  return line;
}

struct NewsTitleLayout {
  String firstLine;
  String secondLine;
  int firstBaseline = 0;
  int lineAdvance = 0;
};

struct NewsSummaryLayout {
  String firstLine;
  String secondLine;
  String thirdLine;
  int firstBaseline = 0;
  int lineAdvance = 0;
};

NewsTitleLayout prepareNewsTitle(const String &rawTitle, int y) {
  NewsTitleLayout layout;
  const String title = normalizeNewsText(rawTitle);
  displayDriver.setFont(&FreeSansBold12pt7b);
  displayDriver.setTextSize(1);
  size_t offset = 0;
  layout.firstLine = takeWrappedLine(title, offset, kNewsTitleWidth);
  if (offset < title.length()) {
    while (!layout.firstLine.isEmpty() &&
           newsTextWidth(layout.firstLine + "...") > kNewsTitleWidth) {
      layout.firstLine.remove(layout.firstLine.length() - 1);
    }
    layout.firstLine.trim();
    layout.firstLine += "...";
  }

  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t boundsWidth = 0;
  uint16_t boundsHeight = 0;
  displayDriver.getTextBounds(layout.firstLine, 0, 0, &boundsX, &boundsY,
                              &boundsWidth, &boundsHeight);
  int ascent = boundsY < 0 ? -boundsY : 0;

  if (!layout.secondLine.isEmpty()) {
    displayDriver.getTextBounds(layout.secondLine, 0, 0, &boundsX, &boundsY,
                                &boundsWidth, &boundsHeight);
    const int secondAscent = boundsY < 0 ? -boundsY : 0;
    ascent = secondAscent > ascent ? secondAscent : ascent;
  }

  layout.lineAdvance = pgm_read_byte(&FreeSansBold12pt7b.yAdvance);
  layout.firstBaseline = y + kNewsTextTopPadding + ascent;
  return layout;
}

void drawNewsTitle(const NewsTitleLayout &layout) {
  displayDriver.setFont(&FreeSansBold12pt7b);
  displayDriver.setTextSize(1);
  displayDriver.setCursor(kNewsTitleX, layout.firstBaseline);
  displayDriver.print(layout.firstLine);
  if (!layout.secondLine.isEmpty()) {
    displayDriver.setCursor(kNewsTitleX,
                            layout.firstBaseline + layout.lineAdvance);
    displayDriver.print(layout.secondLine);
  }
}

NewsSummaryLayout prepareNewsSummary(const String &rawSummary,
                                     const NewsTitleLayout &titleLayout) {
  NewsSummaryLayout layout;
  const String summary = normalizeNewsText(rawSummary);
  displayDriver.setFont(&FreeSans12pt7b);
  displayDriver.setTextSize(1);
  size_t offset = 0;
  layout.firstLine = takeWrappedLine(summary, offset, kNewsTitleWidth);
  layout.secondLine = takeWrappedLine(summary, offset, kNewsTitleWidth);
  layout.thirdLine = takeWrappedLine(summary, offset, kNewsTitleWidth);
  if (offset < summary.length()) {
    while (!layout.thirdLine.isEmpty() &&
           newsTextWidth(layout.thirdLine + "...") > kNewsTitleWidth) {
      layout.thirdLine.remove(layout.thirdLine.length() - 1);
    }
    layout.thirdLine.trim();
    layout.thirdLine += "...";
  }

  layout.lineAdvance = pgm_read_byte(&FreeSans12pt7b.yAdvance);
  const int titleLastBaseline =
      titleLayout.firstBaseline +
      (!titleLayout.secondLine.isEmpty() ? titleLayout.lineAdvance : 0);
  layout.firstBaseline = titleLastBaseline + kNewsSummaryGap;
  return layout;
}

void drawNewsSummary(const NewsSummaryLayout &layout) {
  if (layout.firstLine.isEmpty()) {
    return;
  }
  displayDriver.setFont(&FreeSans12pt7b);
  displayDriver.setTextSize(1);
  displayDriver.setCursor(kNewsTitleX, layout.firstBaseline);
  displayDriver.print(layout.firstLine);
  if (!layout.secondLine.isEmpty()) {
    displayDriver.setCursor(kNewsTitleX,
                            layout.firstBaseline + layout.lineAdvance);
    displayDriver.print(layout.secondLine);
  }
  if (!layout.thirdLine.isEmpty()) {
    displayDriver.setCursor(kNewsTitleX,
                            layout.firstBaseline + 2 * layout.lineAdvance);
    displayDriver.print(layout.thirdLine);
  }
}

size_t utf8CharacterLength(const String &text, size_t offset) {
  const uint8_t firstByte = static_cast<uint8_t>(text[offset]);
  if ((firstByte & 0x80) == 0) {
    return 1;
  }
  if ((firstByte & 0xE0) == 0xC0) {
    return 2;
  }
  if ((firstByte & 0xF0) == 0xE0) {
    return 3;
  }
  return 4;
}

uint16_t chineseTextWidth(const String &text) {
  return largeUnicodeText.getUTF8Width(text.c_str());
}

String takeChineseWrappedLine(const String &text, size_t &offset,
                              uint16_t maximumWidth) {
  while (offset < text.length() && text[offset] == ' ') {
    ++offset;
  }
  if (offset >= text.length()) {
    return String();
  }

  const size_t lineStart = offset;
  size_t lastSpace = SIZE_MAX;
  for (size_t index = lineStart; index < text.length();) {
    const size_t characterLength = utf8CharacterLength(text, index);
    const size_t characterEnd =
        index + characterLength <= text.length() ? index + characterLength
                                                  : text.length();
    if (text[index] == ' ') {
      lastSpace = index;
    }
    if (chineseTextWidth(text.substring(lineStart, characterEnd)) >
        maximumWidth) {
      size_t lineEnd = lastSpace != SIZE_MAX && lastSpace > lineStart
                           ? lastSpace
                           : index;
      if (lineEnd == lineStart) {
        lineEnd = characterEnd;
      }
      String line = text.substring(lineStart, lineEnd);
      line.trim();
      offset = lineEnd;
      return line;
    }
    index = characterEnd;
  }

  String line = text.substring(lineStart);
  line.trim();
  offset = text.length();
  return line;
}

void removeLastUtf8Character(String &text) {
  if (text.isEmpty()) {
    return;
  }
  size_t characterStart = text.length() - 1;
  while (characterStart > 0 &&
         (static_cast<uint8_t>(text[characterStart]) & 0xC0) == 0x80) {
    --characterStart;
  }
  text.remove(characterStart);
}

void addChineseEllipsis(String &line, uint16_t maximumWidth) {
  while (!line.isEmpty() && chineseTextWidth(line + "...") > maximumWidth) {
    removeLastUtf8Character(line);
  }
  line.trim();
  line += "...";
}

struct ChineseNewsLayout {
  String titleLines[2];
  int titleFirstBaseline = 0;
};

ChineseNewsLayout prepareChineseNewsItem(const NewsItem &item, int rowTop) {
  ChineseNewsLayout layout;
  largeUnicodeText.setFont(kChineseFont);

  size_t titleOffset = 0;
  layout.titleLines[0] =
      takeChineseWrappedLine(item.title, titleOffset,
                             kNewsTitleWidth / kChineseScale);
  layout.titleLines[1] =
      takeChineseWrappedLine(item.title, titleOffset,
                             kNewsTitleWidth / kChineseScale);
  if (titleOffset < item.title.length()) {
    addChineseEllipsis(layout.titleLines[1], kNewsTitleWidth / kChineseScale);
  }

  const int titleLineCount = layout.titleLines[1].isEmpty() ? 1 : 2;
  const int titleBlockHeight =
      titleLineCount == 1 ? 32 : kChineseTitleLineAdvance + 32;
  layout.titleFirstBaseline =
      rowTop + (kChineseNewsRowHeight - titleBlockHeight) / 2 + 27;
  return layout;
}

void drawChineseNewsItem(const ChineseNewsLayout &layout) {
  largeUnicodeText.setFont(kChineseFont);
  const int bulletY = layout.titleFirstBaseline - 9;
  displayDriver.fillCircle(kChineseBulletX, bulletY, kChineseBulletRadius,
                           GxEPD_BLACK);
  for (size_t index = 0; index < 2; ++index) {
    if (!layout.titleLines[index].isEmpty()) {
      largeUnicodeText.drawUTF8(
          kNewsTitleX / kChineseScale,
          (layout.titleFirstBaseline +
           static_cast<int>(index) * kChineseTitleLineAdvance) /
              kChineseScale,
          layout.titleLines[index].c_str());
    }
  }
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
  ++context->decodedRows;
  return 1;
}

bool decodePngToMonochrome(const PsramImage &pngImage,
                           PsramImage &frameBuffer) {
  if (pngImage.data() == nullptr || pngImage.size() == 0) {
    return false;
  }
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
  const int decodeResult =
      pngDecoder.decode(&context, PNG_FAST_PALETTE | PNG_CHECK_CRC);
  pngDecoder.close();

  if (decodeResult != PNG_SUCCESS || context.decodedRows != sourceHeight) {
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
  unicodeText.begin(displayDriver);
  unicodeText.setFontMode(1);
  unicodeText.setFontDirection(0);
  unicodeText.setForegroundColor(GxEPD_BLACK);
  unicodeText.setBackgroundColor(GxEPD_WHITE);
  largeUnicodeText.begin(largeTextCanvas);
  largeUnicodeText.setFontMode(1);
  largeUnicodeText.setFontDirection(0);
  largeUnicodeText.setForegroundColor(GxEPD_BLACK);
  largeUnicodeText.setBackgroundColor(GxEPD_WHITE);
  initialized_ = true;
  // Downloads may take a long time or fail; do not leave the controller awake
  // while waiting for the first frame. GxEPD2 resets it on the next draw.
  displayDriver.hibernate();
  Serial.println("墨水屏 SPI 驱动已初始化");
  return true;
}

bool EpaperDisplay::prepareFrame(const PsramImage &pngImage,
                                 PsramImage &destination) {
  PsramImage decoded;
  if (!decodePngToMonochrome(pngImage, decoded)) {
    return false;
  }
  destination.swap(decoded);
  return true;
}

bool EpaperDisplay::showFrame(const PsramImage &image,
                            float temperatureCelsius,
                            float relativeHumidity,
                            bool showSensorPanel,
                            bool sensorDataValid) {
  if (!initialized_) {
    Serial.println("墨水屏尚未初始化");
    return false;
  }

  if (image.data() == nullptr ||
      image.size() != DisplayConfig::kFrameBufferSize) {
    return false;
  }

  const uint8_t *pixels = image.data();
  if (showSensorPanel) {
    if (frameBuffer_.capacity() < DisplayConfig::kFrameBufferSize &&
        !frameBuffer_.allocate(DisplayConfig::kFrameBufferSize)) {
      return false;
    }
    memcpy(frameBuffer_.data(), image.data(), image.size());
    frameBuffer_.setSize(image.size());
    drawSensorPanel(frameBuffer_, temperatureCelsius, relativeHumidity,
                    sensorDataValid);
    pixels = frameBuffer_.data();
  }

  Serial.println("正在将显示帧写入 SSD1677...");
  displayDriver.epd2.writeImageForFullRefresh(
      pixels, 0, 0, DisplayConfig::kWidth,
      DisplayConfig::kHeight, false, false, false);
  displayDriver.epd2.refresh(false);
  displayDriver.hibernate();
  Serial.println("墨水屏刷新完成，已进入深度休眠");
  return true;
}

bool EpaperDisplay::showNews(const NewsList &news, size_t firstItemIndex,
                             size_t maximumItemCount, size_t pageNumber,
                             size_t pageCount, bool dataValid) {
  if (!initialized_) {
    Serial.println("墨水屏尚未初始化");
    return false;
  }

  const bool hasVisibleNews = dataValid && firstItemIndex < news.count;
  size_t visibleItemCount = 0;
  NewsTitleLayout titleLayouts[AppConfig::kNewsItemsPerPage];
  NewsSummaryLayout summaryLayouts[AppConfig::kNewsItemsPerPage];
  if (hasVisibleNews) {
    const size_t remainingItemCount = news.count - firstItemIndex;
    visibleItemCount =
        remainingItemCount < maximumItemCount ? remainingItemCount
                                               : maximumItemCount;
    if (visibleItemCount > AppConfig::kNewsItemsPerPage) {
      visibleItemCount = AppConfig::kNewsItemsPerPage;
    }
    for (size_t visibleIndex = 0; visibleIndex < visibleItemCount;
         ++visibleIndex) {
      const size_t itemIndex = firstItemIndex + visibleIndex;
      const int rowTop = kNewsFirstRowY + visibleIndex * kNewsRowHeight;
      titleLayouts[visibleIndex] =
          prepareNewsTitle(news.items[itemIndex].title, rowTop);
      summaryLayouts[visibleIndex] = prepareNewsSummary(
          news.items[itemIndex].summary, titleLayouts[visibleIndex]);

      size_t summaryLineCount = 0;
      if (!summaryLayouts[visibleIndex].firstLine.isEmpty()) {
        ++summaryLineCount;
      }
      if (!summaryLayouts[visibleIndex].secondLine.isEmpty()) {
        ++summaryLineCount;
      }
      if (!summaryLayouts[visibleIndex].thirdLine.isEmpty()) {
        ++summaryLineCount;
      }
      const int verticalOffset =
          static_cast<int>(3 - summaryLineCount) *
          summaryLayouts[visibleIndex].lineAdvance / 2;
      titleLayouts[visibleIndex].firstBaseline += verticalOffset;
      summaryLayouts[visibleIndex].firstBaseline += verticalOffset;
    }
  }

  Serial.println("正在绘制新闻列表页面...");
  displayDriver.setRotation(0);
  displayDriver.setFullWindow();
  displayDriver.firstPage();
  do {
    displayDriver.fillScreen(GxEPD_WHITE);
    displayDriver.setTextColor(GxEPD_BLACK);
    displayDriver.setTextWrap(false);

    displayDriver.setFont(&FreeSerifBold18pt7b);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(kNewsLeftMargin, 47);
    displayDriver.print("New York Times");
    displayDriver.setFont(&FreeSans9pt7b);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(625, 45);
    displayDriver.print("NEWS  ");
    displayDriver.print(pageNumber);
    displayDriver.print('/');
    displayDriver.print(pageCount);
    displayDriver.fillRect(kNewsLeftMargin, 67,
                           DisplayConfig::kWidth - 2 * kNewsLeftMargin, 3,
                           GxEPD_BLACK);

    if (!hasVisibleNews) {
      displayDriver.setFont(&FreeSerifBold18pt7b);
      displayDriver.setTextSize(1);
      displayDriver.setCursor(226, 235);
      displayDriver.print("NEWS UNAVAILABLE");
      displayDriver.setFont(&FreeSans9pt7b);
      displayDriver.setCursor(275, 272);
      displayDriver.print("Waiting for the next refresh");
    } else {
      for (size_t visibleIndex = 0; visibleIndex < visibleItemCount;
           ++visibleIndex) {
        drawNewsTitle(titleLayouts[visibleIndex]);
        drawNewsSummary(summaryLayouts[visibleIndex]);
      }
    }
  } while (displayDriver.nextPage());

  displayDriver.hibernate();
  Serial.println("新闻列表页面刷新完成，已进入深度休眠");
  return true;
}

bool EpaperDisplay::showChineseNews(const NewsList &news,
                                    size_t firstItemIndex,
                                    size_t maximumItemCount,
                                    size_t pageNumber, size_t pageCount,
                                    bool dataValid) {
  if (!initialized_) {
    Serial.println("墨水屏尚未初始化");
    return false;
  }

  const bool hasVisibleNews = dataValid && firstItemIndex < news.count;
  size_t visibleItemCount = 0;
  ChineseNewsLayout layouts[AppConfig::kChineseNewsItemsPerPage];
  if (hasVisibleNews) {
    const size_t remainingItemCount = news.count - firstItemIndex;
    visibleItemCount = remainingItemCount < maximumItemCount
                           ? remainingItemCount
                           : maximumItemCount;
    if (visibleItemCount > AppConfig::kChineseNewsItemsPerPage) {
      visibleItemCount = AppConfig::kChineseNewsItemsPerPage;
    }
    for (size_t visibleIndex = 0; visibleIndex < visibleItemCount;
         ++visibleIndex) {
      layouts[visibleIndex] = prepareChineseNewsItem(
          news.items[firstItemIndex + visibleIndex],
          kNewsFirstRowY + visibleIndex * kChineseNewsRowHeight);
    }
  }

  Serial.println("正在绘制中文新闻列表页面...");
  displayDriver.setRotation(0);
  displayDriver.setFullWindow();
  displayDriver.firstPage();
  do {
    displayDriver.fillScreen(GxEPD_WHITE);
    displayDriver.setFont(&FreeSerifBold18pt7b);
    displayDriver.setTextSize(1);
    displayDriver.setCursor(kNewsLeftMargin, 47);
    displayDriver.print("NHK Latest News");

    largeUnicodeText.setFont(kChineseFont);
    String pageLabel = String(pageNumber) + '/' + pageCount;
    const int pageLabelX =
        DisplayConfig::kWidth - kNewsLeftMargin -
        largeUnicodeText.getUTF8Width(pageLabel.c_str()) * kChineseScale;
    largeUnicodeText.drawUTF8(pageLabelX / kChineseScale,
                              45 / kChineseScale, pageLabel.c_str());
    displayDriver.fillRect(kNewsLeftMargin, 67,
                           DisplayConfig::kWidth - 2 * kNewsLeftMargin, 3,
                           GxEPD_BLACK);

    if (!hasVisibleNews) {
      const char *message = "中文新闻暂不可用，等待下次刷新";
      const int messageX =
          (DisplayConfig::kWidth -
           largeUnicodeText.getUTF8Width(message) * kChineseScale) /
          2;
      largeUnicodeText.drawUTF8(messageX / kChineseScale,
                                245 / kChineseScale, message);
    } else {
      for (size_t visibleIndex = 0; visibleIndex < visibleItemCount;
           ++visibleIndex) {
        drawChineseNewsItem(layouts[visibleIndex]);
      }
    }
  } while (displayDriver.nextPage());

  displayDriver.hibernate();
  Serial.println("中文新闻列表页面刷新完成，已进入深度休眠");
  return true;
}
