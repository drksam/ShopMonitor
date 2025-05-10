#include "NetworkManager.h"
#include <random>

NetworkManager::NetworkManager() {
  wifiConnected = false;
  apMode = false;
  queueHead = 0;
  queueSize = 0;
  connectTimeoutMs = 10000; // 10 seconds default
  responseTimeoutMs = 5000; // 5 seconds default
  lastReconnectAttempt = 0;
  reconnectAttempts = 0;
  errorLogIndex = 0;
  lastNetworkActivity = 0;
  maintenanceMode = false;
  
  // Initialize error struct
  lastError.code = NET_OK;
  lastError.message = "";
  lastError.timestamp = 0;
  lastError.recovered = false;
  lastError.severity = 0;
  
  // Initialize SPIFFS for error logging and queue persistence
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  } else {
    Serial.println("SPIFFS mounted successfully");
    // Load any existing error log and offline queue
    loadErrorLog();
    loadOfflineQueue();
  }
  
  // Initialize error log array
  for (int i = 0; i < MAX_ERROR_LOG_SIZE; i++) {
    recentErrors[i].code = NET_OK;
    recentErrors[i].message = "";
    recentErrors[i].timestamp = 0;
    recentErrors[i].recovered = false;
    recentErrors[i].severity = 0;
  }
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
  
  // Try to connect to WiFi with exponential backoff
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Wait for connection with timeout and retry
  uint8_t attempts = 0;
  uint16_t delayMs = WIFI_INITIAL_RETRY_DELAY;
  uint32_t startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < connectTimeoutMs && attempts < MAX_WIFI_RETRIES) {
    delay(delayMs);
    Serial.print(".");
    
    // If still not connected, increment attempts and increase delay with jitter
    if (WiFi.status() != WL_CONNECTED) {
      attempts++;
      delayMs *= 2; // Exponential backoff
      addRandomJitter(delayMs);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    apMode = false;
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Reset reconnect attempts counter on successful connection
    reconnectAttempts = 0;
    clearError();
    
    // Setup mDNS once connected
    if (!setupMDNS(nodeName)) {
      logError(NET_MDNS_ERROR, "Failed to set up mDNS");
    }
    
    // Process any pending offline requests
    processOfflineQueue();
    
    return true;
  } else {
    wifiConnected = false;
    logError(NET_WIFI_CONNECT_ERROR, "Failed to connect to WiFi after " + String(attempts) + " attempts");
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
  String apName = "ShopNode_";
  if (nodeName != DEFAULT_NODE_NAME) {
    apName = nodeName + "_";
  }
  
  apName += String((uint32_t)ESP.getEfuseMac(), HEX);
  
  // Use hardcoded password instead of the constant
  const char* password = "Pigfloors";
  
  if (WiFi.softAP(apName.c_str(), password)) {
    Serial.println("AP Mode started.");
    Serial.print("AP SSID: ");
    Serial.println(apName);
    Serial.print("AP Password: ");
    Serial.println(password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    logError(NET_WIFI_CONNECT_ERROR, "Failed to start AP mode");
    Serial.println("Failed to start AP mode");
  }
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

void NetworkManager::reconnect() {
  // Limit reconnection attempts frequency
  uint32_t now = millis();
  if (now - lastReconnectAttempt < (1000 * pow(2, reconnectAttempts))) {
    return; // Too soon to try again
  }
  
  lastReconnectAttempt = now;
  
  if (currentSSID.length() > 0) {
    Serial.println("Attempting to reconnect to WiFi...");
    WiFi.disconnect();
    delay(100);
    WiFi.begin(currentSSID.c_str());
    
    // Wait with shorter timeout for reconnection
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < (connectTimeoutMs / 2)) {
      delay(100);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      apMode = false;
      Serial.println("\nWiFi reconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      // Reset reconnect attempts on success
      reconnectAttempts = 0;
      
      // Try to setup mDNS again
      setupMDNS(currentNodeName);
      
      // Process offline queue
      processOfflineQueue();
    } else {
      reconnectAttempts = min(reconnectAttempts + 1, (uint8_t)10); // Cap at 10
      Serial.println("\nWiFi reconnection failed");
    }
  }
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

bool NetworkManager::sendGET(const String &url, String &response, bool retryOnFail) {
  if (!isConnected()) {
    if (retryOnFail) {
      // Queue for later if retry is enabled
      queueRequest(url, "GET", "");
    }
    logError(NET_WIFI_CONNECT_ERROR, "WiFi not connected during GET request");
    return false;
  }
  
  // Use lambda to wrap the HTTP operation for retry
  auto httpOperation = [this, &url, &response]() -> bool {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(responseTimeoutMs);
    
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      response = http.getString();
      http.end();
      bool success = (httpCode == HTTP_CODE_OK);
      if (!success) {
        logError(NET_SERVER_ERROR, "Server returned non-OK status: " + String(httpCode));
      }
      return success;
    } else {
      String errorMsg = http.errorToString(httpCode);
      logError(NET_HTTP_ERROR, "HTTP GET Error: " + errorMsg);
      http.end();
      return false;
    }
  };
  
  // Use exponential backoff retry for HTTP operation
  if (retryOnFail) {
    return exponentialBackoffRetry(httpOperation, MAX_HTTP_RETRIES, HTTP_INITIAL_RETRY_DELAY);
  } else {
    return httpOperation();
  }
}

bool NetworkManager::sendPOST(const String &url, const String &jsonPayload, String &response, bool retryOnFail) {
  if (!isConnected()) {
    if (retryOnFail) {
      // Queue for later if retry is enabled
      queueRequest(url, "POST", jsonPayload);
    }
    logError(NET_WIFI_CONNECT_ERROR, "WiFi not connected during POST request");
    return false;
  }
  
  // Use lambda to wrap the HTTP operation for retry
  auto httpOperation = [this, &url, &jsonPayload, &response]() -> bool {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(responseTimeoutMs);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
      response = http.getString();
      http.end();
      bool success = (httpCode == HTTP_CODE_OK);
      if (!success) {
        logError(NET_SERVER_ERROR, "Server returned non-OK status: " + String(httpCode));
      }
      return success;
    } else {
      String errorMsg = http.errorToString(httpCode);
      logError(NET_HTTP_ERROR, "HTTP POST Error: " + errorMsg);
      http.end();
      return false;
    }
  };
  
  // Use exponential backoff retry for HTTP operation
  if (retryOnFail) {
    return exponentialBackoffRetry(httpOperation, MAX_HTTP_RETRIES, HTTP_INITIAL_RETRY_DELAY);
  } else {
    return httpOperation();
  }
}

void NetworkManager::queueRequest(const String &url, const String &method, const String &payload) {
  if (queueSize >= MAX_OFFLINE_QUEUE_SIZE) {
    Serial.println("Offline queue full, dropping oldest request");
    // Remove oldest request (circular buffer)
    queueHead = (queueHead + 1) % MAX_OFFLINE_QUEUE_SIZE;
    queueSize--;
  }
  
  // Calculate position for new request
  int pos = (queueHead + queueSize) % MAX_OFFLINE_QUEUE_SIZE;
  
  // Store the request
  offlineQueue[pos].url = url;
  offlineQueue[pos].method = method;
  offlineQueue[pos].payload = payload;
  offlineQueue[pos].timestamp = millis();
  offlineQueue[pos].retries = 0;
  
  queueSize++;
  Serial.println("Request queued for offline processing: " + method + " " + url);
  
  // Save queue to persistent storage
  saveOfflineQueue();
}

void NetworkManager::queueRequest(const String &url, const String &method, const String &payload, bool critical) {
  if (queueSize >= MAX_OFFLINE_QUEUE_SIZE) {
    Serial.println("Offline queue full, dropping oldest request");
    // Remove oldest request (circular buffer)
    queueHead = (queueHead + 1) % MAX_OFFLINE_QUEUE_SIZE;
    queueSize--;
  }
  
  // Calculate position for new request
  int pos = (queueHead + queueSize) % MAX_OFFLINE_QUEUE_SIZE;
  
  // Store the request
  offlineQueue[pos].url = url;
  offlineQueue[pos].method = method;
  offlineQueue[pos].payload = payload;
  offlineQueue[pos].timestamp = millis();
  offlineQueue[pos].retries = 0;
  offlineQueue[pos].critical = critical;
  
  queueSize++;
  Serial.println("Request queued for offline processing: " + method + " " + url);
  
  // Save queue to persistent storage
  saveOfflineQueue();
}

void NetworkManager::processOfflineQueue() {
  if (!isConnected() || queueSize == 0) {
    return;
  }
  
  Serial.print("Processing offline queue. Items: ");
  Serial.println(queueSize);
  
  // First prioritize critical requests
  prioritizeQueue();
  
  // Process up to 5 items at a time to avoid blocking
  int itemsToProcess = min(queueSize, 5);
  
  for (int i = 0; i < itemsToProcess; i++) {
    QueuedRequest &req = offlineQueue[queueHead];
    bool success = false;
    String response;
    
    Serial.print("Processing queued request: ");
    Serial.print(req.method);
    Serial.print(" ");
    Serial.println(req.url);
    
    // Process based on method
    if (req.method == "GET") {
      success = sendGET(req.url, response, false); // Don't retry to avoid recursion
    } else if (req.method == "POST") {
      success = sendPOST(req.url, req.payload, response, false); // Don't retry
    }
    
    if (success) {
      // Remove from queue
      queueHead = (queueHead + 1) % MAX_OFFLINE_QUEUE_SIZE;
      queueSize--;
      Serial.println("Successfully processed queued request");
      lastNetworkActivity = millis(); // Update last successful activity time
    } else {
      // Increment retry counter 
      req.retries++;
      if (req.retries >= MAX_HTTP_RETRIES) {
        // Too many retries, discard
        Serial.println("Discarding queued request after too many retries");
        queueHead = (queueHead + 1) % MAX_OFFLINE_QUEUE_SIZE;
        queueSize--;
      } else {
        // Stop processing for now, will retry next time
        Serial.println("Failed to process queued request, will retry later");
        break;
      }
    }
  }
  
  // Save updated queue
  if (queueSize > 0) {
    saveOfflineQueue();
  } else {
    // Remove the queue file if empty
    SPIFFS.remove("/offline_queue.json");
  }
}

void NetworkManager::saveOfflineQueue() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS for saving queue");
    return;
  }
  
  DynamicJsonDocument doc(8192); // Adjust size as needed
  JsonArray queueArray = doc.createNestedArray("queue");
  
  for (int i = 0; i < queueSize; i++) {
    int pos = (queueHead + i) % MAX_OFFLINE_QUEUE_SIZE;
    JsonObject item = queueArray.createNestedObject();
    item["url"] = offlineQueue[pos].url;
    item["method"] = offlineQueue[pos].method;
    item["payload"] = offlineQueue[pos].payload;
    item["timestamp"] = offlineQueue[pos].timestamp;
    item["retries"] = offlineQueue[pos].retries;
    item["critical"] = offlineQueue[pos].critical;
  }
  
  File file = SPIFFS.open("/offline_queue.json", "w");
  if (!file) {
    Serial.println("Failed to open queue file for writing");
    return;
  }
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to queue file");
  } else {
    Serial.print("Saved ");
    Serial.print(queueSize);
    Serial.println(" items to offline queue file");
  }
  
  file.close();
}

void NetworkManager::loadOfflineQueue() {
  if (!SPIFFS.exists("/offline_queue.json")) {
    Serial.println("No offline queue file found");
    return;
  }
  
  File file = SPIFFS.open("/offline_queue.json", "r");
  if (!file) {
    Serial.println("Failed to open queue file for reading");
    return;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("Failed to parse queue file: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Clear existing queue
  queueHead = 0;
  queueSize = 0;
  
  // Load queue items
  JsonArray queueArray = doc["queue"].as<JsonArray>();
  for (JsonObject item : queueArray) {
    if (queueSize < MAX_OFFLINE_QUEUE_SIZE) {
      int pos = queueSize;
      offlineQueue[pos].url = item["url"].as<String>();
      offlineQueue[pos].method = item["method"].as<String>();
      offlineQueue[pos].payload = item["payload"].as<String>();
      offlineQueue[pos].timestamp = item["timestamp"].as<uint32_t>();
      offlineQueue[pos].retries = item["retries"].as<uint8_t>();
      
      // Handle critical flag (with backward compatibility)
      if (item.containsKey("critical")) {
        offlineQueue[pos].critical = item["critical"].as<bool>();
      } else {
        offlineQueue[pos].critical = false;
      }
      
      queueSize++;
    } else {
      Serial.println("Queue full, discarding remaining items");
      break;
    }
  }
  
  Serial.print("Loaded ");
  Serial.print(queueSize);
  Serial.println(" items from offline queue file");
}

NetworkError NetworkManager::getLastError() {
  return lastError;
}

void NetworkManager::clearError() {
  lastError.code = NET_OK;
  lastError.message = "";
  lastError.timestamp = 0;
}

String NetworkManager::getErrorMessage(uint8_t errorCode) {
  switch(errorCode) {
    case NET_OK:
      return "No error";
    case NET_WIFI_CONNECT_ERROR:
      return "WiFi connection error";
    case NET_HTTP_ERROR:
      return "HTTP request error";
    case NET_SERVER_ERROR:
      return "Server error";
    case NET_TIMEOUT_ERROR:
      return "Network timeout";
    case NET_JSON_ERROR:
      return "JSON parsing error";
    case NET_DNS_ERROR:
      return "DNS resolution error";
    case NET_MDNS_ERROR:
      return "mDNS setup error";
    case NET_TLS_ERROR:
      return "TLS/SSL certificate error";
    case NET_MEMORY_ERROR:
      return "Out of memory error";
    case NET_AUTH_ERROR:
      return "Authentication error";
    case NET_API_RATE_LIMIT:
      return "API rate limit exceeded";
    case NET_API_ERROR:
      return "API error response";
    case NET_STORAGE_ERROR:
      return "Storage access error";
    case NET_CONNECTION_RESET:
      return "Connection was reset";
    case NET_HARDWARE_ERROR:
      return "Hardware network interface error";
    default:
      return "Unknown error";
  }
}

void NetworkManager::setConnectTimeout(uint16_t timeoutMs) {
  connectTimeoutMs = max((uint16_t)1000, timeoutMs); // Minimum 1 second
}

void NetworkManager::setResponseTimeout(uint16_t timeoutMs) {
  responseTimeoutMs = max((uint16_t)500, timeoutMs); // Minimum 0.5 second
}

bool NetworkManager::checkConnectivity(const String &testUrl) {
  if (!isConnected()) {
    return false;
  }
  
  String response;
  bool success = sendGET(testUrl, response, false);
  return success;
}

String NetworkManager::getStatusJson() {
  DynamicJsonDocument doc(1024); // Increased size for more data
  
  doc["connected"] = isConnected();
  doc["ap_mode"] = apMode;
  doc["wifi_healthy"] = isNetworkHealthy();
  doc["maintenance_mode"] = maintenanceMode;
  doc["last_activity"] = millis() - lastNetworkActivity;
  
  if (wifiConnected) {
    doc["ip"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["reconnect_attempts"] = reconnectAttempts;
    doc["wifi_status"] = getWiFiStatusString();
  } else if (apMode) {
    doc["ip"] = WiFi.softAPIP().toString();
    doc["ap_name"] = WiFi.softAPSSID();
    doc["clients"] = WiFi.softAPgetStationNum();
  }
  
  doc["queue_size"] = queueSize;
  doc["critical_queue_items"] = countCriticalQueueItems();
  
  if (lastError.code != NET_OK) {
    JsonObject error = doc.createNestedObject("error");
    error["code"] = lastError.code;
    error["message"] = lastError.message;
    error["time"] = lastError.timestamp;
    error["recovered"] = lastError.recovered;
    error["severity"] = lastError.severity;
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

void NetworkManager::logError(uint8_t code, const String &message) {
  lastError.code = code;
  lastError.message = message;
  lastError.timestamp = millis();
  
  Serial.print("Network error [");
  Serial.print(code);
  Serial.print("]: ");
  Serial.println(message);
  
  saveErrorLog();
}

void NetworkManager::logError(uint8_t code, const String &message, uint8_t severity) {
  // Update last error
  lastError.code = code;
  lastError.message = message;
  lastError.timestamp = millis();
  lastError.recovered = false;
  lastError.severity = severity;
  
  // Add to recent errors log (circular buffer)
  recentErrors[errorLogIndex] = lastError;
  errorLogIndex = (errorLogIndex + 1) % MAX_ERROR_LOG_SIZE;
  
  Serial.print("Network error [");
  Serial.print(code);
  Serial.print("] severity ");
  Serial.print(severity);
  Serial.print(": ");
  Serial.println(message);
  
  saveErrorLog();
}

// New methods for enhanced error handling

String NetworkManager::getErrorLogJson() {
  DynamicJsonDocument doc(4096);
  JsonArray errorArray = doc.createNestedArray("errors");
  
  // Count how many valid entries we have
  int validEntries = 0;
  for (int i = 0; i < MAX_ERROR_LOG_SIZE; i++) {
    if (recentErrors[i].timestamp > 0) {
      validEntries++;
    }
  }
  
  // Add most recent errors first
  int added = 0;
  for (int i = 0; i < MAX_ERROR_LOG_SIZE && added < validEntries; i++) {
    int idx = (errorLogIndex - 1 - i + MAX_ERROR_LOG_SIZE) % MAX_ERROR_LOG_SIZE;
    
    if (recentErrors[idx].timestamp > 0) {
      JsonObject error = errorArray.createNestedObject();
      error["code"] = recentErrors[idx].code;
      error["message"] = recentErrors[idx].message;
      error["time"] = recentErrors[idx].timestamp;
      error["recovered"] = recentErrors[idx].recovered;
      error["severity"] = recentErrors[idx].severity;
      added++;
    }
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

void NetworkManager::saveErrorLog() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS for error log");
    return;
  }
  
  // Save a structured JSON error log
  File file = SPIFFS.open("/network_errors.json", "w");
  if (!file) {
    Serial.println("Failed to open error log file");
    return;
  }
  
  DynamicJsonDocument doc(8192);
  JsonArray errorArray = doc.createNestedArray("errors");
  
  // Add up to 100 most recent errors
  int count = 0;
  for (int i = 0; i < MAX_ERROR_LOG_SIZE && count < 100; i++) {
    int idx = (errorLogIndex - 1 - i + MAX_ERROR_LOG_SIZE) % MAX_ERROR_LOG_SIZE;
    
    if (recentErrors[idx].timestamp > 0) {
      JsonObject error = errorArray.createNestedObject();
      error["code"] = recentErrors[idx].code;
      error["message"] = recentErrors[idx].message;
      error["time"] = recentErrors[idx].timestamp;
      error["recovered"] = recentErrors[idx].recovered;
      error["severity"] = recentErrors[idx].severity;
      count++;
    }
  }
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to error log file");
  }
  
  file.close();
  
  // Also append to a text log for backup
  file = SPIFFS.open("/network_errors.log", "a");
  if (!file) {
    return;
  }
  
  // Write error entry
  file.print(lastError.timestamp);
  file.print(": [");
  file.print(lastError.code);
  file.print("] Severity:");
  file.print(lastError.severity);
  file.print(" - ");
  file.println(lastError.message);
  file.close();
  
  // Keep log file size reasonable
  File readFile = SPIFFS.open("/network_errors.log", "r");
  if (readFile && readFile.size() > 10240) { // 10KB limit
    readFile.close();
    // Rotate log file
    File tempFile = SPIFFS.open("/network_errors.tmp", "w");
    if (tempFile) {
      readFile = SPIFFS.open("/network_errors.log", "r");
      // Skip first half of the file
      readFile.seek(readFile.size() / 2);
      // Copy second half to temp file
      while (readFile.available()) {
        tempFile.write(readFile.read());
      }
      readFile.close();
      tempFile.close();
      // Replace log with temp
      SPIFFS.remove("/network_errors.log");
      SPIFFS.rename("/network_errors.tmp", "/network_errors.log");
    }
  } else if (readFile) {
    readFile.close();
  }
}

void NetworkManager::loadErrorLog() {
  // Load structured error log
  if (!SPIFFS.exists("/network_errors.json")) {
    return; // No error log exists
  }
  
  File file = SPIFFS.open("/network_errors.json", "r");
  if (!file) {
    Serial.println("Failed to open error log file");
    return;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("Failed to parse error log file: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Reset error log
  for (int i = 0; i < MAX_ERROR_LOG_SIZE; i++) {
    recentErrors[i].code = NET_OK;
    recentErrors[i].message = "";
    recentErrors[i].timestamp = 0;
    recentErrors[i].recovered = false;
    recentErrors[i].severity = 0;
  }
  
  // Load errors
  JsonArray errorArray = doc["errors"].as<JsonArray>();
  int i = 0;
  
  for (JsonObject errorObj : errorArray) {
    if (i < MAX_ERROR_LOG_SIZE) {
      recentErrors[i].code = errorObj["code"].as<uint8_t>();
      recentErrors[i].message = errorObj["message"].as<String>();
      recentErrors[i].timestamp = errorObj["time"].as<uint32_t>();
      
      // Handle optional fields with backward compatibility
      if (errorObj.containsKey("recovered")) {
        recentErrors[i].recovered = errorObj["recovered"].as<bool>();
      } else {
        recentErrors[i].recovered = false;
      }
      
      if (errorObj.containsKey("severity")) {
        recentErrors[i].severity = errorObj["severity"].as<uint8_t>();
      } else {
        recentErrors[i].severity = 2; // Default to error severity
      }
      
      i++;
    } else {
      break;
    }
  }
  
  errorLogIndex = i % MAX_ERROR_LOG_SIZE;
  
  Serial.print("Loaded ");
  Serial.print(i);
  Serial.println(" errors from error log file");
}

bool NetworkManager::exponentialBackoffRetry(std::function<bool()> operation, uint8_t maxRetries, uint16_t initialDelayMs) {
  uint16_t delayMs = initialDelayMs;
  
  for (uint8_t attempt = 0; attempt <= maxRetries; attempt++) {
    // First attempt or retry
    bool success = operation();
    if (success) {
      return true;
    }
    
    // If we've reached max retries, stop
    if (attempt >= maxRetries) {
      break;
    }
    
    // Otherwise, wait and retry
    Serial.print("Operation failed. Retrying in ");
    Serial.print(delayMs);
    Serial.println("ms...");
    delay(delayMs);
    
    // Increase delay for next attempt with jitter
    delayMs *= 2;
    addRandomJitter(delayMs);
  }
  
  return false;
}

void NetworkManager::addRandomJitter(uint16_t &delayMs) {
  int jitter = random(-100, 100); // Between -100 and +100 ms
  delayMs = max((int)delayMs + jitter, 100); // Ensure minimum 100ms
}

bool NetworkManager::recoverFromError(uint8_t errorCode) {
  bool recovered = false;
  
  switch(errorCode) {
    case NET_WIFI_CONNECT_ERROR:
      // Attempt to reconnect to WiFi
      Serial.println("Attempting recovery from WiFi connection error");
      WiFi.disconnect(true);
      delay(1000);
      if (currentSSID.length() > 0) {
        WiFi.begin(currentSSID.c_str());
        delay(5000);
        recovered = (WiFi.status() == WL_CONNECTED);
      }
      break;
      
    case NET_HTTP_ERROR:
    case NET_TIMEOUT_ERROR:
    case NET_CONNECTION_RESET:
      // For HTTP errors, just verify we have connectivity
      Serial.println("Attempting recovery from HTTP/connection error");
      recovered = isNetworkHealthy();
      break;
      
    case NET_DNS_ERROR:
      // For DNS errors, try flushing DNS cache (not directly possible on ESP32)
      // and check connectivity
      Serial.println("Attempting recovery from DNS error");
      delay(1000);
      recovered = isNetworkHealthy();
      break;
      
    case NET_MDNS_ERROR:
      // Restart mDNS services
      Serial.println("Attempting recovery from mDNS error");
      MDNS.end();
      delay(500);
      recovered = setupMDNS(currentNodeName);
      break;
      
    case NET_STORAGE_ERROR:
      // Try remounting SPIFFS
      Serial.println("Attempting recovery from storage error");
      SPIFFS.end();
      delay(500);
      recovered = SPIFFS.begin(true);
      break;
      
    case NET_MEMORY_ERROR:
      // For memory errors, try freeing up memory
      Serial.println("Attempting recovery from memory error");
      ESP.restart(); // In extreme cases, reboot
      recovered = false; // This won't actually be reached after restart
      break;
    
    // Other error types may not be automatically recoverable
    default:
      Serial.print("No automatic recovery for error code ");
      Serial.println(errorCode);
      recovered = false;
  }
  
  // If recovery was successful, update the error status
  if (recovered && lastError.code == errorCode) {
    lastError.recovered = true;
    
    // Also update in the error log
    for (int i = 0; i < MAX_ERROR_LOG_SIZE; i++) {
      int idx = (errorLogIndex - 1 - i + MAX_ERROR_LOG_SIZE) % MAX_ERROR_LOG_SIZE;
      
      if (recentErrors[idx].code == errorCode && !recentErrors[idx].recovered) {
        recentErrors[idx].recovered = true;
        break;
      }
    }
    
    saveErrorLog();
  }
  
  return recovered;
}

void NetworkManager::diagnosticLog() {
  Serial.println("\n===== NETWORK DIAGNOSTIC LOG =====");
  Serial.print("ESP32 Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("WiFi Status: ");
  Serial.println(getWiFiStatusString());
  
  if (wifiConnected) {
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    
    // Test connectivity
    Serial.println("Connectivity test:");
    if (isNetworkHealthy()) {
      Serial.println("Network is healthy");
    } else {
      Serial.println("Network health check failed");
    }
  } else if (apMode) {
    Serial.println("Running in AP Mode");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Connected clients: ");
    Serial.println(WiFi.softAPgetStationNum());
  } else {
    Serial.println("Not connected to WiFi");
  }
  
  Serial.print("Offline queue size: ");
  Serial.println(queueSize);
  
  Serial.println("\n=== Recent Errors ===");
  int errorCount = 0;
  for (int i = 0; i < MAX_ERROR_LOG_SIZE; i++) {
    int idx = (errorLogIndex - 1 - i + MAX_ERROR_LOG_SIZE) % MAX_ERROR_LOG_SIZE;
    
    if (recentErrors[idx].timestamp > 0) {
      errorCount++;
      if (errorCount <= 10) { // Show only last 10 errors
        Serial.print(recentErrors[idx].timestamp);
        Serial.print(": [");
        Serial.print(recentErrors[idx].code);
        Serial.print("] ");
        Serial.print(recentErrors[idx].message);
        if (recentErrors[idx].recovered) {
          Serial.println(" (Recovered)");
        } else {
          Serial.println();
        }
      }
    }
  }
  Serial.print("Total errors: ");
  Serial.println(errorCount);
  Serial.println("================================\n");
}

bool NetworkManager::healthCheck() {
  Serial.println("Performing comprehensive health check...");
  
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  bool signalOk = (WiFi.RSSI() > -80); // -80 dBm is fairly weak but usable
  bool dnsOk = (WiFi.dnsIP() != IPAddress(0,0,0,0));
  bool gatewayOk = (WiFi.gatewayIP() != IPAddress(0,0,0,0));
  
  // Test ping to gateway (not directly available on ESP32)
  // Consider using ICMPPing library if available
  bool pingOk = true; // Assume true if can't test
  
  // Try to make a quick HTTP request to test connectivity
  bool httpOk = false;
  String response;
  HTTPClient http;
  http.begin("http://8.8.8.8"); // Just try to connect to Google DNS
  http.setTimeout(2000);
  int httpCode = http.GET();
  http.end();
  httpOk = (httpCode >= 0); // Any response (even error) means connectivity works
  
  // Overall health assessment
  bool healthy = wifiOk && (signalOk || apMode) && (dnsOk || apMode) && (gatewayOk || apMode) && (httpOk || apMode);
  
  Serial.println("Health check results:");
  Serial.print("- WiFi connected: ");
  Serial.println(wifiOk ? "YES" : "NO");
  Serial.print("- Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(signalOk ? " OK" : " POOR");
  Serial.print("- DNS available: ");
  Serial.println(dnsOk ? "YES" : "NO");
  Serial.print("- Gateway available: ");
  Serial.println(gatewayOk ? "YES" : "NO");
  Serial.print("- HTTP connectivity: ");
  Serial.println(httpOk ? "YES" : "NO");
  Serial.print("Overall health: ");
  Serial.println(healthy ? "GOOD" : "POOR");
  
  return healthy;
}

bool NetworkManager::isNetworkHealthy() {
  // Quick network health check
  if (!wifiConnected) return false;
  
  // Check if we have a valid IP
  if (WiFi.localIP() == IPAddress(0,0,0,0)) return false;
  
  // Check if signal is strong enough
  if (WiFi.RSSI() < -90) return false; // -90 dBm is very weak
  
  return true;
}

String NetworkManager::getWiFiStatusString() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "Connected";
    case WL_NO_SHIELD: return "No WiFi shield";
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "No SSID available";
    case WL_SCAN_COMPLETED: return "Scan completed";
    case WL_CONNECT_FAILED: return "Connection failed";
    case WL_CONNECTION_LOST: return "Connection lost";
    case WL_DISCONNECTED: return "Disconnected";
    default: return "Unknown status";
  }
}

void NetworkManager::prioritizeQueue() {
  if (queueSize <= 1) return; // Nothing to prioritize
  
  // This is a simple implementation that moves critical items to the front
  // A more sophisticated approach would be to use a proper priority queue
  
  // Count critical items
  int criticalCount = 0;
  for (int i = 0; i < queueSize; i++) {
    int pos = (queueHead + i) % MAX_OFFLINE_QUEUE_SIZE;
    if (offlineQueue[pos].critical) {
      criticalCount++;
    }
  }
  
  if (criticalCount == 0 || criticalCount == queueSize) {
    return; // Nothing to reorganize
  }
  
  // Create a temporary array to store the reordered queue
  QueuedRequest tempQueue[MAX_OFFLINE_QUEUE_SIZE];
  int tempIndex = 0;
  
  // First, add all critical items
  for (int i = 0; i < queueSize; i++) {
    int pos = (queueHead + i) % MAX_OFFLINE_QUEUE_SIZE;
    if (offlineQueue[pos].critical) {
      tempQueue[tempIndex++] = offlineQueue[pos];
    }
  }
  
  // Then add all non-critical items
  for (int i = 0; i < queueSize; i++) {
    int pos = (queueHead + i) % MAX_OFFLINE_QUEUE_SIZE;
    if (!offlineQueue[pos].critical) {
      tempQueue[tempIndex++] = offlineQueue[pos];
    }
  }
  
  // Copy back to the original queue
  queueHead = 0;
  for (int i = 0; i < queueSize; i++) {
    offlineQueue[i] = tempQueue[i];
  }
  
  Serial.println("Queue prioritized, critical items moved to front");
}

int NetworkManager::countCriticalQueueItems() {
  int count = 0;
  for (int i = 0; i < queueSize; i++) {
    int pos = (queueHead + i) % MAX_OFFLINE_QUEUE_SIZE;
    if (offlineQueue[pos].critical) {
      count++;
    }
  }
  return count;
}