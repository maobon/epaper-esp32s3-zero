#include "WifiManager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "Secrets.h"

bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(Secrets::kWifiSsid, Secrets::kWifiPassword);
  Serial.print("正在连接 Wi-Fi");

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < AppConfig::kWifiConnectTimeoutMs) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi 连接失败，请检查名称和密码");
    return false;
  }

  Serial.print("Wi-Fi 已连接，IP 地址: ");
  Serial.println(WiFi.localIP());
  return true;
}
