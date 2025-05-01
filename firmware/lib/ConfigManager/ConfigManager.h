#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "include/constants.h"

class ConfigManager {
public:
  ConfigManager();
  
  bool load();
  bool save();
  void reset();
  
  // Configuration getters/setters
  String getNodeName() const;
  void setNodeName(const String &name);
  
  int getNodeType() const;
  void setNodeType(int type);
  
  String getServerUrl() const;
  void setServerUrl(const String &url);
  
  String getWifiSSID() const;
  void setWifiSSID(const String &ssid);
  
  String getWifiPassword() const;
  void setWifiPassword(const String &password);
  
  String getMachineID(int index) const;
  void setMachineID(int index, const String &id);
  
  bool isConfigured() const;
  
private:
  bool configured;
  String nodeName;
  int nodeType;
  String serverUrl;
  String wifiSSID;
  String wifiPassword;
  String machineIDs[4];
};

#endif // CONFIG_MANAGER_H