#include "WebUI.h"

WebUI::WebUI(ConfigManager &configManager, NetworkManager &networkManager) : 
  webServer(80),
  configManager(configManager),
  networkManager(networkManager) {
  
  // Initialize status variables
  for (int i = 0; i < 4; i++) {
    currentTags[i] = "";
    activityCounts[i] = 0;
    relayStates[i] = false;
    inputStates[i] = false;
  }
}

void WebUI::begin() {
  // Setup web server routes
  webServer.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleRoot(request);
  });
  
  webServer.on("/wiring", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleWiringDiagram(request);
  });
  
  webServer.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleStatus(request);
  });
  
  webServer.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleConfig(request);
  });
  
  webServer.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){
    // This is just a placeholder, the real handling is done in onBody
    request->send(200);
  }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    handleConfigPost(request, data, len, index, total);
  });
  
  webServer.on("/api/reboot", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleReboot(request);
  });
  
  webServer.on("/api/reset", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleReset(request);
  });
  
  webServer.on("/api/relay", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleRelay(request);
  });
  
  webServer.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleScanWifi(request);
  });
  
  // Start web server
  webServer.begin();
  Serial.println("Web server started");
}

void WebUI::handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", DASHBOARD_HTML);
}

void WebUI::handleWiringDiagram(AsyncWebServerRequest *request) {
  request->send(200, "text/html", WIRING_DIAGRAM_HTML);
}

void WebUI::handleStatus(AsyncWebServerRequest *request) {
  request->send(200, "application/json", getNodeStatusJson());
}

void WebUI::handleConfig(AsyncWebServerRequest *request) {
  request->send(200, "application/json", getCurrentConfigJson());
}

void WebUI::handleConfigPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  String json = String((char*)data);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  // Update configuration
  if (doc.containsKey("nodeName")) configManager.setNodeName(doc["nodeName"].as<String>());
  if (doc.containsKey("nodeType")) configManager.setNodeType(doc["nodeType"].as<int>());
  if (doc.containsKey("serverUrl")) configManager.setServerUrl(doc["serverUrl"].as<String>());
  if (doc.containsKey("wifiSSID")) configManager.setWifiSSID(doc["wifiSSID"].as<String>());
  if (doc.containsKey("wifiPass")) configManager.setWifiPassword(doc["wifiPass"].as<String>());
  if (doc.containsKey("machine0")) configManager.setMachineID(0, doc["machine0"].as<String>());
  if (doc.containsKey("machine1")) configManager.setMachineID(1, doc["machine1"].as<String>());
  if (doc.containsKey("machine2")) configManager.setMachineID(2, doc["machine2"].as<String>());
  if (doc.containsKey("machine3")) configManager.setMachineID(3, doc["machine3"].as<String>());
  
  // Save configuration
  if (configManager.save()) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
  } else {
    request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
  }
}

void WebUI::handleReboot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}

void WebUI::handleReset(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Resetting to factory defaults...");
  delay(500);
  configManager.reset();
  ESP.restart();
}

void WebUI::handleRelay(AsyncWebServerRequest *request) {
  if (configManager.getNodeType() != NODE_TYPE_ACCESSORY_IO) {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Not an Accessory IO node\"}");
    return;
  }
  
  if (request->hasParam("relay") && request->hasParam("state")) {
    int relay = request->getParam("relay")->value().toInt();
    bool state = request->getParam("state")->value().toInt() == 1;
    
    if (relay >= 1 && relay <= 4) {
      // Update the relay state (will be read by the main loop)
      relayStates[relay - 1] = state;
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Relay " + String(relay) + " set to " + String(state ? "ON" : "OFF") + "\"}");
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid relay number\"}");
    }
  } else {
    request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing parameters\"}");
  }
}

void WebUI::handleScanWifi(AsyncWebServerRequest *request) {
  Serial.println("Scanning WiFi networks...");
  int networksFound = 0;
  networkManager.scan(networksFound);
  
  String json = "{";
  json += "\"success\":" + String(networksFound >= 0 ? "true" : "false") + ",";
  
  if (networksFound >= 0) {
    json += "\"networks\":[";
    
    for (int i = 0; i < networksFound; i++) {
      if (i > 0) json += ",";
      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"secured\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
      json += "}";
    }
    
    json += "],";
    json += "\"count\":" + String(networksFound);
  } else {
    json += "\"message\":\"Failed to scan networks\"";
  }
  
  json += "}";
  request->send(200, "application/json", json);
  
  // Clean up scan results
  WiFi.scanDelete();
}

void WebUI::setCurrentTags(String tags[]) {
  for (int i = 0; i < 4; i++) {
    currentTags[i] = tags[i];
  }
}

void WebUI::setActivityCounts(int counts[]) {
  for (int i = 0; i < 4; i++) {
    activityCounts[i] = counts[i];
  }
}

void WebUI::setRelayStates(bool states[]) {
  for (int i = 0; i < 4; i++) {
    relayStates[i] = states[i];
  }
}

void WebUI::setInputStates(bool states[]) {
  for (int i = 0; i < 4; i++) {
    inputStates[i] = states[i];
  }
}

String WebUI::getNodeStatusJson() {
  String status = "{";
  status += "\"node_id\":\"" + configManager.getNodeName() + "\",";
  status += "\"node_type\":" + String(configManager.getNodeType()) + ",";
  status += "\"ip_address\":\"" + networkManager.getLocalIP() + "\",";
  status += "\"wifi_signal\":" + String(networkManager.getRSSI()) + ",";
  status += "\"uptime\":" + String(millis() / 1000) + ",";
  status += "\"server_connected\":" + String(networkManager.isConnected() ? "true" : "false");
  
  // Add node-type specific data
  if (configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR) {
    status += ",\"machines\":[";
    for (int i = 0; i < 4; i++) {
      if (i > 0) status += ",";
      status += "{";
      status += "\"machine_id\":\"" + configManager.getMachineID(i) + "\",";
      status += "\"status\":\"" + String(currentTags[i] != "" ? "active" : "idle") + "\",";
      status += "\"rfid\":\"" + currentTags[i] + "\",";
      status += "\"activity_count\":" + String(activityCounts[i]);
      status += "}";
    }
    status += "]";
  } else if (configManager.getNodeType() == NODE_TYPE_OFFICE_READER) {
    status += ",\"last_tag\":\"" + currentTags[0] + "\"";
    // Use the activity time from the main loop for scan time
    status += ",\"scan_time\":" + String(millis());
  } else if (configManager.getNodeType() == NODE_TYPE_ACCESSORY_IO) {
    status += ",\"inputs\":[";
    for (int i = 0; i < 4; i++) {
      if (i > 0) status += ",";
      status += inputStates[i] ? "true" : "false";
    }
    status += "],\"outputs\":[";
    for (int i = 0; i < 4; i++) {
      if (i > 0) status += ",";
      status += relayStates[i] ? "true" : "false";
    }
    status += "]";
  }
  
  status += "}";
  return status;
}

String WebUI::getCurrentConfigJson() {
  String config = "{";
  config += "\"nodeName\":\"" + configManager.getNodeName() + "\",";
  config += "\"nodeType\":" + String(configManager.getNodeType()) + ",";
  config += "\"serverUrl\":\"" + configManager.getServerUrl() + "\",";
  config += "\"wifiSSID\":\"" + configManager.getWifiSSID() + "\",";
  config += "\"machine0\":\"" + configManager.getMachineID(0) + "\",";
  config += "\"machine1\":\"" + configManager.getMachineID(1) + "\",";
  config += "\"machine2\":\"" + configManager.getMachineID(2) + "\",";
  config += "\"machine3\":\"" + configManager.getMachineID(3) + "\"";
  config += "}";
  return config;
}