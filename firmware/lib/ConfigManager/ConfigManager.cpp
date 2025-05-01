#include "ConfigManager.h"

ConfigManager::ConfigManager() {
  configured = false;
  nodeName = DEFAULT_NODE_NAME;
  nodeType = NODE_TYPE_MACHINE_MONITOR;
  serverUrl = DEFAULT_SERVER_URL;
  wifiSSID = "";
  wifiPassword = "";
  
  for (int i = 0; i < 4; i++) {
    machineIDs[i] = "";
  }
}

bool ConfigManager::load() {
  // Load configuration from EEPROM via Preferences
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  if (preferences.isKey("configured")) {
    nodeName = preferences.getString("nodeName", DEFAULT_NODE_NAME);
    nodeType = preferences.getInt("nodeType", NODE_TYPE_MACHINE_MONITOR);
    serverUrl = preferences.getString("serverUrl", DEFAULT_SERVER_URL);
    wifiSSID = preferences.getString("wifiSSID", "");
    wifiPassword = preferences.getString("wifiPass", "");
    
    for (int i = 0; i < 4; i++) {
      String key = "machine" + String(i);
      machineIDs[i] = preferences.getString(key.c_str(), "");
    }
    
    configured = true;
    
    preferences.end();
    return true;
  }
  
  preferences.end();
  return false;
}

bool ConfigManager::save() {
  // Save configuration to EEPROM via Preferences
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  preferences.putBool("configured", true);
  preferences.putString("nodeName", nodeName);
  preferences.putInt("nodeType", nodeType);
  preferences.putString("serverUrl", serverUrl);
  preferences.putString("wifiSSID", wifiSSID);
  preferences.putString("wifiPass", wifiPassword);
  
  for (int i = 0; i < 4; i++) {
    String key = "machine" + String(i);
    preferences.putString(key.c_str(), machineIDs[i]);
  }
  
  preferences.end();
  configured = true;
  return true;
}

void ConfigManager::reset() {
  // Reset configuration to defaults
  Preferences preferences;
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  
  // Reset member variables
  nodeName = DEFAULT_NODE_NAME;
  nodeType = NODE_TYPE_MACHINE_MONITOR;
  serverUrl = DEFAULT_SERVER_URL;
  wifiSSID = "";
  wifiPassword = "";
  
  for (int i = 0; i < 4; i++) {
    machineIDs[i] = "";
  }
  
  configured = false;
}

String ConfigManager::getNodeName() const {
  return nodeName;
}

void ConfigManager::setNodeName(const String &name) {
  nodeName = name;
}

int ConfigManager::getNodeType() const {
  return nodeType;
}

void ConfigManager::setNodeType(int type) {
  if (type >= NODE_TYPE_MACHINE_MONITOR && type <= NODE_TYPE_ACCESSORY_IO) {
    nodeType = type;
  }
}

String ConfigManager::getServerUrl() const {
  return serverUrl;
}

void ConfigManager::setServerUrl(const String &url) {
  serverUrl = url;
}

String ConfigManager::getWifiSSID() const {
  return wifiSSID;
}

void ConfigManager::setWifiSSID(const String &ssid) {
  wifiSSID = ssid;
}

String ConfigManager::getWifiPassword() const {
  return wifiPassword;
}

void ConfigManager::setWifiPassword(const String &password) {
  wifiPassword = password;
}

String ConfigManager::getMachineID(int index) const {
  if (index >= 0 && index < 4) {
    return machineIDs[index];
  }
  return "";
}

void ConfigManager::setMachineID(int index, const String &id) {
  if (index >= 0 && index < 4) {
    machineIDs[index] = id;
  }
}

bool ConfigManager::isConfigured() const {
  return configured;
}