#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include "include/constants.h"

class NetworkManager {
public:
  NetworkManager();
  
  bool begin(const String &ssid, const String &password, const String &nodeName);
  void startAPMode(const String &nodeName);
  bool setupMDNS(const String &nodeName);
  bool isConnected();
  bool scan(int &networksFound);
  String getLocalIP();
  int getRSSI();
  
  // HTTP helpers
  bool sendGET(const String &url, String &response);
  bool sendPOST(const String &url, const String &jsonPayload, String &response);
  
private:
  bool wifiConnected;
  bool apMode;
  String currentSSID;
  String currentNodeName;
};

#endif // NETWORK_MANAGER_H