#ifndef RFID_HANDLER_H
#define RFID_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "include/constants.h"

class RFIDHandler {
public:
  RFIDHandler();
  
  void begin(int nodeType);
  bool checkReaders(String currentTags[], int nodeType);
  String getLastTag();
  
  static String getUID(MFRC522 &reader);
  
private:
  MFRC522 rfidReaders[4];
  String lastUID;
  
  void initReader(int index);
  void beepSuccess();
  void beepError();
  void beepShort();
};

#endif // RFID_HANDLER_H