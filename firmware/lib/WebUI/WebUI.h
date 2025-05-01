#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "include/constants.h"
#include "include/dashboard.h"
#include "include/wiring_diagram.h"

class WebUI {
public:
  WebUI(ConfigManager &configManager, NetworkManager &networkManager);
  
  void begin();
  void handleRoot(AsyncWebServerRequest *request);
  void handleWiringDiagram(AsyncWebServerRequest *request);
  void handleStatus(AsyncWebServerRequest *request);
  void handleConfig(AsyncWebServerRequest *request);
  void handleConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  void handleReboot(AsyncWebServerRequest *request);
  void handleReset(AsyncWebServerRequest *request);
  void handleRelay(AsyncWebServerRequest *request);
  void handleScanWifi(AsyncWebServerRequest *request);
  
  // External data accessors
  void setCurrentTags(String tags[]);
  void setActivityCounts(int counts[]);
  void setRelayStates(bool states[]);
  void setInputStates(bool states[]);
  
private:
  AsyncWebServer webServer;
  ConfigManager &configManager;
  NetworkManager &networkManager;
  
  // Status variables
  String currentTags[4];
  int activityCounts[4];
  bool relayStates[4];
  bool inputStates[4];
  
  // Helper methods
  String getNodeStatusJson();
  String getCurrentConfigJson();
};

#endif // WEB_UI_H