#include "Sht41Sensor.h"

#include <Wire.h>

#include "AppConfig.h"

namespace {

constexpr uint8_t kSoftResetCommand = 0x94;
constexpr uint8_t kHighPrecisionMeasurementCommand = 0xFD;

bool sendCommand(uint8_t command) {
  Wire.beginTransmission(AppConfig::kSht41I2cAddress);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool Sht41Sensor::begin() {
  if (!Wire.begin(AppConfig::kSht41SdaPin, AppConfig::kSht41SclPin)) {
    Serial.println("SHT41 I2C 初始化失败");
    return false;
  }
  Wire.setClock(400000);

  if (!sendCommand(kSoftResetCommand)) {
    Serial.println("未在 I2C 地址 0x44 检测到 SHT41");
    return false;
  }

  delay(2);
  Serial.println("SHT41 初始化成功（SDA=GPIO5，SCL=GPIO6）");
  return true;
}

bool Sht41Sensor::read(float &temperatureCelsius,
                       float &relativeHumidity) {
  if (!sendCommand(kHighPrecisionMeasurementCommand)) {
    Serial.println("SHT41 测量指令发送失败");
    return false;
  }

  delay(10);
  const uint8_t received = Wire.requestFrom(
      AppConfig::kSht41I2cAddress, static_cast<uint8_t>(6));
  if (received != 6) {
    Serial.println("SHT41 返回的数据长度不正确");
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  uint8_t data[6];
  for (uint8_t index = 0; index < sizeof(data); ++index) {
    data[index] = Wire.read();
  }

  if (!hasValidCrc(data, data[2]) || !hasValidCrc(data + 3, data[5])) {
    Serial.println("SHT41 数据 CRC 校验失败");
    return false;
  }

  const uint16_t rawTemperature =
      (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawHumidity =
      (static_cast<uint16_t>(data[3]) << 8) | data[4];

  temperatureCelsius =
      -45.0F + 175.0F * static_cast<float>(rawTemperature) / 65535.0F;
  relativeHumidity =
      -6.0F + 125.0F * static_cast<float>(rawHumidity) / 65535.0F;
  relativeHumidity = constrain(relativeHumidity, 0.0F, 100.0F);
  return true;
}

bool Sht41Sensor::hasValidCrc(const uint8_t *data, uint8_t expectedCrc) {
  uint8_t crc = 0xFF;
  for (uint8_t index = 0; index < 2; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) != 0 ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                               : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc == expectedCrc;
}
