#include "WifiManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <time.h>
#include <esp_wifi.h>

#include "AppConfig.h"
#include "Secrets.h"
#include "RetryBackoff.h"
#include "NetworkHours.h"

namespace {
unsigned int sessionDepth = 0;  // Sessions are used only by setup()/loop().
std::atomic<bool> radioActive{false};
std::atomic<bool> bootSyncActive{false};
bool bootSyncAttempted = false;
bool policyReady = false;
RetryBackoff wifiBackoff(AppConfig::kWifiRetryInitialMs,
                        AppConfig::kWifiRetryMaximumMs);

bool readClock(tm &local) {
  const time_t now = time(nullptr);
  return now >= 1704067200 && localtime_r(&now, &local) != nullptr;
}

uint32_t quietWaitMs() {
  tm local = {};
  if (!readClock(local)) {
    return 60000;  // Unknown clock: only the explicit boot sync can connect.
  }
  return quietNetworkWaitMs(local.tm_hour, local.tm_min, local.tm_sec,
                            AppConfig::kNetworkResumeHour);
}

void networkGuard(void *) {
  for (;;) {
    tm local = {};
    const bool known = readClock(local);
    const bool blocked = known
        ? quietNetworkWaitMs(local.tm_hour, local.tm_min, local.tm_sec,
                             AppConfig::kNetworkResumeHour) != 0
        : !bootSyncActive.load();
    if (blocked && radioActive.load()) {
      // Run outside loop(): TLS/HTTP may be blocking when midnight arrives.
      // The owning WifiSession later performs normal Arduino Wi-Fi cleanup.
      if (esp_wifi_stop() == ESP_OK) {
        radioActive = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
}

bool initializeWifiPolicy() {
  if (policyReady) return true;
  setenv("TZ", AppConfig::kTimeZone, 1);
  tzset();
  policyReady = xTaskCreate(networkGuard, "network-hours", 3072, nullptr,
                            2, nullptr) == pdPASS;
  return policyReady;
}

bool networkRequestsAllowed() {
  return policyReady && quietWaitMs() == 0;
}

void finishTimeSync() {
  bootSyncActive = false;
  if (!networkRequestsAllowed()) disconnectWifi();
}

WifiSession::WifiSession() { ++sessionDepth; }

uint32_t wifiRetryRemainingMs(uint32_t now) {
  return max(wifiBackoff.remaining(now), quietWaitMs());
}

WifiSession::~WifiSession() {
  if (--sessionDepth == 0) {
    disconnectWifi();
  }
}

bool connectWifi(bool forTimeSync) {
  if (!policyReady) return false;
  tm local = {};
  if (!readClock(local)) {
    if (!forTimeSync || bootSyncAttempted) return false;
    bootSyncAttempted = true;
    bootSyncActive = true;
  } else if (!networkRequestsAllowed()) {
    disconnectWifi();
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiBackoff.succeeded();
    return true;
  }
  if (wifiBackoff.remaining(millis()) != 0) {
    return false;
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  radioActive = true;
  if (!WiFi.mode(WIFI_STA)) {
    wifiBackoff.failed(millis());
    disconnectWifi();
    return false;
  }
  WiFi.begin(Secrets::kWifiSsid, Secrets::kWifiPassword);
  Serial.print("正在连接 Wi-Fi");

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < AppConfig::kWifiConnectTimeoutMs) {
    if (!bootSyncActive.load() && !networkRequestsAllowed()) {
      disconnectWifi();
      return false;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (!bootSyncActive.load() && !networkRequestsAllowed()) {
    disconnectWifi();
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiBackoff.failed(millis());
    disconnectWifi();
    Serial.println("Wi-Fi 连接失败，请检查名称和密码");
    return false;
  }

  wifiBackoff.succeeded();
  Serial.print("Wi-Fi 已连接，IP 地址: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool disconnectWifi() {
  bootSyncActive = false;
  WiFi.setAutoReconnect(false);
  const bool stopped = WiFi.mode(WIFI_OFF);
  if (stopped) {
    radioActive = false;
  }
  // On failure retain tracking so the night guard can still stop the radio.
  return stopped;
}
