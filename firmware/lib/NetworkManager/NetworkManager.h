#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // Added for HTTPS support
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "include/constants.h"

// Error codes
#define NET_OK 0
#define NET_WIFI_CONNECT_ERROR 1
#define NET_HTTP_ERROR 2
#define NET_SERVER_ERROR 3
#define NET_TIMEOUT_ERROR 4
#define NET_JSON_ERROR 5
#define NET_DNS_ERROR 6
#define NET_MDNS_ERROR 7
// Additional error codes for enhanced error handling
#define NET_TLS_ERROR 8
#define NET_MEMORY_ERROR 9
#define NET_AUTH_ERROR 10
#define NET_API_RATE_LIMIT 11
#define NET_API_ERROR 12
#define NET_STORAGE_ERROR 13
#define NET_CONNECTION_RESET 14
#define NET_HARDWARE_ERROR 15
#define NET_CERT_VERIFY_ERROR 16  // Certificate verification failed

// File paths for storage
#define CERT_FINGERPRINT_FILE "/cert_fingerprint.dat"

// Max retry attempts for network operations
#define MAX_WIFI_RETRIES 5
#define MAX_HTTP_RETRIES 3
#define WIFI_INITIAL_RETRY_DELAY 500   // ms
#define HTTP_INITIAL_RETRY_DELAY 1000  // ms
#define MAX_OFFLINE_QUEUE_SIZE 50      // Maximum number of requests to queue when offline
#define MAX_ERROR_LOG_SIZE 100         // Maximum number of recent errors to store

struct NetworkError {
  uint8_t code;
  String message;
  uint32_t timestamp;
  bool recovered;       // Flag to indicate if this error was automatically recovered
  uint8_t severity;     // 0=info, 1=warning, 2=error, 3=critical
};

struct QueuedRequest {
  String url;
  String method;  // "GET" or "POST"
  String payload; // Empty for GET requests
  uint32_t timestamp;
  uint8_t retries;
  bool critical;  // If true, this request will be retried with higher priority
};

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
  void reconnect();
  void processOfflineQueue();
  
  // HTTP/HTTPS helpers with improved error handling
  bool sendGET(const String &url, String &response, bool retryOnFail = true);
  bool sendPOST(const String &url, const String &jsonPayload, String &response, bool retryOnFail = true);
  bool httpRequest(const String &url, const String &method, const String &payload, String &response, bool retryOnFail = true);
  
  // HTTPS specific methods
  void setUseHTTPS(bool useHTTPS);
  void setCACert(const char* cert);
  void setInsecureServerConnection(bool allowInsecure);
  bool setServerCertificate(const String &certType);
  bool loadCertificateFromSPIFFS(const String &filename);
  bool saveCertificateToSPIFFS(const String &filename, const String &certContent);
  bool setupSecureClient(WiFiClientSecure &secureClient);
  
  // Certificate pinning methods
  void setCertFingerprint(const String &fingerprint);
  bool verifyCertificateFingerprint(const String &host);
  bool loadCertFingerprintFromStorage();
  bool saveCertFingerprintToStorage();
  bool fetchAndStoreCertFingerprint(const String &url);
  
  // Queue operations for offline mode
  void queueRequest(const String &url, const String &method, const String &payload);
  void queueRequest(const String &url, const String &method, const String &payload, bool critical);
  void saveOfflineQueue();
  void loadOfflineQueue();
  
  // Enhanced error handling
  NetworkError getLastError();
  void clearError();
  String getErrorMessage(uint8_t errorCode);
  void setConnectTimeout(uint16_t timeoutMs);
  void setResponseTimeout(uint16_t timeoutMs);
  bool checkConnectivity(const String &testUrl);
  bool recoverFromError(uint8_t errorCode);  // Attempts automatic recovery based on error type
  void diagnosticLog();                       // Generate diagnostic data for debugging
  bool healthCheck();                         // Full connectivity health check
  
  // Status reporting
  String getStatusJson();
  String getErrorLogJson();                   // Returns recent errors as JSON
  
  // API Authentication methods
  void setAPIToken(const String &token);
  String getAPIToken();
  bool requestNewToken(const String &serverUrl, const String &nodeId, const String &nodeSecret, String &newToken);
  bool hasValidToken();
  void saveTokenToStorage();
  void loadTokenFromStorage();
  bool refreshTokenIfNeeded(const String &serverUrl);
  void setNodeCredentials(const String &id, const String &secret);
  
private:
  bool wifiConnected;
  bool apMode;
  String currentSSID;
  String currentNodeName;
  NetworkError lastError;
  NetworkError recentErrors[MAX_ERROR_LOG_SIZE];
  uint8_t errorLogIndex;
  QueuedRequest offlineQueue[MAX_OFFLINE_QUEUE_SIZE];
  uint8_t queueHead;
  uint8_t queueSize;
  uint16_t connectTimeoutMs;
  uint16_t responseTimeoutMs;
  uint32_t lastReconnectAttempt;
  uint8_t reconnectAttempts;
  uint32_t lastNetworkActivity;
  bool maintenanceMode;
  
  // HTTPS configuration
  bool useHTTPS;
  bool allowInsecureConnection;
  String caCertificate;
  String certFingerprint; // Certificate fingerprint for pinning
  bool useCertFingerprint; // Flag to enable/disable certificate pinning
  
  // API Authentication related fields
  String apiToken;
  uint32_t tokenExpiry;
  String nodeId;
  String nodeSecret;
  
  // Internal helper methods
  void logError(uint8_t code, const String &message, uint8_t severity = 2);
  bool exponentialBackoffRetry(std::function<bool()> operation, uint8_t maxRetries, uint16_t initialDelayMs);
  void addRandomJitter(uint16_t &delayMs);
  void saveErrorLog();
  void loadErrorLog();
  void prioritizeQueue();     // Re-orders queue to prioritize critical requests
  bool isNetworkHealthy();    // Internal network health check
  String getWiFiStatusString(); // Get textual representation of WiFi status
  bool isHTTPSUrl(const String &url); // Check if URL uses HTTPS protocol
  int countCriticalQueueItems(); // Count number of critical items in queue
};

#endif // NETWORK_MANAGER_H