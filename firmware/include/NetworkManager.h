// NetworkManager.h
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Network error codes
#define NET_OK 0
#define NET_WIFI_CONNECT_ERROR 1
#define NET_HTTP_ERROR 2
#define NET_SERVER_ERROR 3
#define NET_TLS_ERROR 4
#define NET_TOKEN_ERROR 5
#define NET_CERT_VERIFY_ERROR 6

// Network constants
#define MAX_HTTP_RETRIES 3
#define HTTP_INITIAL_RETRY_DELAY 1000
#define TOKEN_REFRESH_THRESHOLD 86400000 // 24 hours in milliseconds
#define TOKEN_FILE "/token.json"
#define CERT_FINGERPRINT_FILE "/cert_fingerprint.txt"

class NetworkManager {
  private:
    // WiFi settings
    String ssid;
    String password;
    String hostname;
    bool connected;
    unsigned long lastReconnectAttempt;
    
    // Error handling
    int lastErrorCode;
    String lastErrorMessage;
    
    // Request queue for offline operation
    struct QueuedRequest {
      String url;
      String method;
      String payload;
      QueuedRequest* next;
    };
    QueuedRequest* queueHead;
    QueuedRequest* queueTail;
    
    // HTTP settings
    unsigned long responseTimeoutMs;
    
    // HTTPS support
    bool useHTTPS;
    bool allowInsecureConnection;
    String caCertificate;
    String certFingerprint;  // Added for certificate pinning
    bool useCertFingerprint; // Flag to enable certificate pinning
    
    // API Authentication
    String apiToken;
    unsigned long tokenExpiryTime;
    String nodeId;
    String nodeSecret;
    
    // Private methods
    bool connectToWiFi();
    void processQueue();
    void logError(int code, const String &message);
    bool exponentialBackoffRetry(std::function<bool()> operation, int maxRetries, int initialDelay);
    bool isHTTPSUrl(const String &url);
    bool saveTokenToStorage();
    bool verifyCertificateFingerprint(const String &host); // Added for certificate verification
    
  public:
    NetworkManager();
    
    // Setup & connection
    bool begin(const String &wifiSSID, const String &wifiPassword, const String &deviceHostname = "");
    void disconnect();
    bool reconnect();
    bool isConnected();
    
    // Request methods
    bool sendGET(const String &url, String &response, bool retryOnFail = true);
    bool sendPOST(const String &url, const String &payload, String &response, bool retryOnFail = true);
    
    // Queue management
    void queueRequest(const String &url, const String &method, const String &payload = "");
    
    // Configuration
    void setResponseTimeout(unsigned long timeoutMs);
    void setNodeIdentity(const String &id, const String &secret);
    
    // Error information
    int getLastErrorCode();
    String getLastErrorMessage();
    
    // HTTPS Configuration
    void setUseHTTPS(bool enable);
    void setInsecureServerConnection(bool allow);
    void setCACertificate(const String &cert);
    
    // Certificate pinning
    void setCertFingerprint(const String &fingerprint);
    bool loadCertFingerprintFromStorage();
    bool saveCertFingerprintToStorage();
    bool fetchAndStoreCertFingerprint(const String &url);
    
    // Token authentication
    bool hasValidToken();
    bool loadTokenFromStorage();
    bool requestNewToken(const String &serverUrl, const String &nodeId, const String &secret, String &token);
    unsigned long getTokenExpiryTime() { return tokenExpiryTime; }
    bool refreshTokenIfNeeded(const String &serverUrl);
};

#endif // NETWORK_MANAGER_H