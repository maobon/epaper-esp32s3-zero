#include "EpaperApiClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp32-hal-psram.h>

#include "AppConfig.h"
#include "Secrets.h"

namespace {

using SecureClient = WiFiClientSecure;

bool isSuccessfulHttpStatus(int statusCode) {
  return statusCode >= 200 && statusCode < 300;
}

void configureTls(SecureClient &client) {
  // 正式环境应改为 setCACert() 并配置可信 CA。
  client.setInsecure();
}

class PsramImageWriteStream : public Stream {
 public:
  explicit PsramImageWriteStream(PsramImage &image) : image_(image) {}

  size_t write(uint8_t value) override {
    return write(&value, 1);
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    const size_t required = bytesWritten_ + size;
    if (required < bytesWritten_ || !image_.ensureCapacity(required)) {
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
  size_t bytesWritten_ = 0;
};

}  // namespace

bool EpaperApiClient::authenticate() {
  accessToken_.clear();
  if (!login()) {
    return false;
  }

  Serial.print("access_token: ");
  Serial.println(accessToken_);
  return true;
}

bool EpaperApiClient::fetchImage(const char *imageName,
                                 PsramImage &destination) {
  if (accessToken_.isEmpty()) {
    Serial.println("尚未登录，无法请求图片");
    return false;
  }
  return downloadImage(imageName, destination);
}

bool EpaperApiClient::login() {
  SecureClient secureClient;
  HTTPClient http;
  configureTls(secureClient);

  if (!http.begin(secureClient, AppConfig::kLoginUrl)) {
    Serial.println("无法初始化登录 HTTPS 请求");
    return false;
  }

  http.setTimeout(AppConfig::kHttpTimeoutMs);
  http.addHeader("Content-Type", "application/json");

  JsonDocument requestJson;
  requestJson["username"] = Secrets::kApiUsername;
  requestJson["password"] = Secrets::kApiPassword;
  String requestBody;
  serializeJson(requestJson, requestBody);

  const int httpCode = http.POST(requestBody);
  Serial.print("登录接口 HTTP 状态码: ");
  Serial.println(httpCode);

  if (!isSuccessfulHttpStatus(httpCode)) {
    if (httpCode < 0) {
      Serial.print("登录请求失败: ");
      Serial.println(http.errorToString(httpCode));
    } else {
      Serial.println("登录接口返回非成功状态码");
    }
    http.end();
    return false;
  }

  const String response = http.getString();
  http.end();

  JsonDocument responseJson;
  const DeserializationError jsonError =
      deserializeJson(responseJson, response);
  if (jsonError) {
    Serial.print("JSON 解析失败: ");
    Serial.println(jsonError.c_str());
    return false;
  }

  if (!responseJson["access_token"].is<const char *>()) {
    Serial.println("服务端响应中没有有效的 access_token 字段");
    return false;
  }

  accessToken_ = responseJson["access_token"].as<String>();
  return !accessToken_.isEmpty();
}

bool EpaperApiClient::downloadImage(const char *imageName,
                                    PsramImage &destination) {
  if (!psramFound()) {
    Serial.println("未检测到 PSRAM，请检查开发板的 PSRAM 设置");
    return false;
  }

  SecureClient secureClient;
  HTTPClient http;
  configureTls(secureClient);

  const String imageUrl = String(AppConfig::kEpaperBaseUrl) + imageName;
  if (!http.begin(secureClient, imageUrl)) {
    Serial.println("无法初始化图片 HTTPS 请求");
    return false;
  }

  http.setTimeout(AppConfig::kHttpTimeoutMs);
  http.addHeader("Authorization", String("Bearer ") + accessToken_);
  http.addHeader("Accept", "image/png");

  Serial.print("正在请求图片: ");
  Serial.println(imageName);
  const int httpCode = http.GET();
  Serial.print("图片接口 HTTP 状态码: ");
  Serial.println(httpCode);

  if (!isSuccessfulHttpStatus(httpCode)) {
    if (httpCode < 0) {
      Serial.print("图片请求失败: ");
      Serial.println(http.errorToString(httpCode));
    } else {
      Serial.println("图片接口返回非成功状态码");
    }
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  const size_t initialCapacity =
      contentLength > 0 ? static_cast<size_t>(contentLength)
                        : AppConfig::kInitialImageCapacity;

  PsramImage downloadedImage;
  if (!downloadedImage.allocate(initialCapacity)) {
    Serial.print("PSRAM 分配失败，需要字节数: ");
    Serial.println(initialCapacity);
    http.end();
    return false;
  }

  PsramImageWriteStream imageStream(downloadedImage);
  const int bytesWritten = http.writeToStream(&imageStream);
  http.end();

  if (bytesWritten <= 0 ||
      imageStream.bytesWritten() != static_cast<size_t>(bytesWritten)) {
    Serial.print("图片数据接收失败: ");
    Serial.println(http.errorToString(bytesWritten));
    return false;
  }

  downloadedImage.setSize(imageStream.bytesWritten());
  destination.swap(downloadedImage);

  Serial.print(imageName);
  Serial.print(" 已存入 PSRAM，大小: ");
  Serial.print(destination.size());
  Serial.println(" 字节");
  return true;
}
