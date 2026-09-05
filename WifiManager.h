#pragma once

#include <stdint.h>

bool initializeWifiPolicy();
bool connectWifi(bool forTimeSync = false);
void finishTimeSync();
bool networkRequestsAllowed();
bool disconnectWifi();
uint32_t wifiRetryRemainingMs(uint32_t now);

// End every synchronous network operation with the radio off, including errors.
class WifiSession {
 public:
  WifiSession();
  ~WifiSession();
  WifiSession(const WifiSession &) = delete;
  WifiSession &operator=(const WifiSession &) = delete;
};
