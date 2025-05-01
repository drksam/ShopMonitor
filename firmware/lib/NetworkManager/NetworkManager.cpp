#include "NetworkManager.h"

NetworkManager::NetworkManager() {
  wifiConnected = false;
  apMode = false;
}

bool NetworkManager::begin(const String &ssid, const String &password, const String &nodeName) {
  if (ssid.length() == 0) {
    Serial.println("No WiFi SSID provided, starting AP mode");
    Serial.print("AP Password will be: ");
    Serial.println("Pigfloors");
    startAPMode(nodeName);
    return false;
  }

  // Store values
  currentSSID = ssid;
  currentNodeName = nodeName;
  
  // Try to connect to WiFi
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Wait for connection (with timeout)
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < WIFI_TIMEOUT_SECONDS) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    apMode = false;
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup mDNS once connected
    setupMDNS(nodeName);
    
    return true;
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi connection failed. Starting AP mode...");
    startAPMode(nodeName);
    return false;
  }
}

void NetworkManager::startAPMode(const String &nodeName) {
  // Start AP mode
  apMode = true;
  wifiConnected = false;
  WiFi.mode(WIFI_AP);
  
  // Create AP name from node name and chip ID
  String apName = "NooyenNode_";
  if (nodeName != DEFAULT_NODE_NAME) {
    apName = nodeName + "_";
  }
  
  apName += String((uint32_t)ESP.getEfuseMac(), HEX);
  
  // Use hardcoded password instead of the constant
  const char* password = "Pigfloors";
  
  WiFi.softAP(apName.c_str(), password);
  Serial.println("AP Mode started.");
  Serial.print("AP SSID: ");
  Serial.println(apName);
  Serial.print("AP Password: ");
  Serial.println(password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

bool NetworkManager::setupMDNS(const String &nodeName) {
  if (!wifiConnected) return false;
  
  // Set up mDNS responder
  String mdnsName = nodeName;
  mdnsName.replace(" ", "-");
  
  if (MDNS.begin(mdnsName.c_str())) {
    Serial.println("mDNS responder started");
    Serial.print("mDNS name: ");
    Serial.println(mdnsName);
    
    // Add service to mDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService(MDNS_SERVICE_TYPE, "tcp", 80);
    return true;
  } else {
    Serial.println("Error setting up mDNS responder!");
    return false;
  }
}

bool NetworkManager::isConnected() {
  if (apMode) return false;
  return (WiFi.status() == WL_CONNECTED);
}

bool NetworkManager::scan(int &networksFound) {
  networksFound = WiFi.scanNetworks();
  return (networksFound >= 0);
}

String NetworkManager::getLocalIP() {
  if (wifiConnected) {
    return WiFi.localIP().toString();
  } else if (apMode) {
    return WiFi.softAPIP().toString();
  } else {
    return "0.0.0.0";
  }
}

int NetworkManager::getRSSI() {
  if (wifiConnected) {
    return WiFi.RSSI();
  } else {
    return 0;
  }
}

bool NetworkManager::sendGET(const String &url, String &response) {
  if (!isConnected()) return false;
  
  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  } else {
    Serial.print("HTTP GET Error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }
}

bool NetworkManager::sendPOST(const String &url, const String &jsonPayload, String &response) {
  if (!isConnected()) return false;
  
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST(jsonPayload);
  
  if (httpCode > 0) {
    response = http.getString();
    http.end();
    return (httpCode == HTTP_CODE_OK);
  } else {
    Serial.print("HTTP POST Error: ");
    Serial.println(http.errorToString(httpCode));
    http.end();
    return false;
  }
}