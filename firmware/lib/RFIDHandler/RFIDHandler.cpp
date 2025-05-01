#include "RFIDHandler.h"

RFIDHandler::RFIDHandler() : 
  rfidReaders{
    MFRC522(SS_PIN1, RST_PIN),
    MFRC522(SS_PIN2, RST_PIN),
    MFRC522(SS_PIN3, RST_PIN),
    MFRC522(SS_PIN4, RST_PIN)
  } {
  lastUID = "";
}

void RFIDHandler::begin(int nodeType) {
  // Initialize SPI bus for RFID readers
  SPI.begin();
  
  // Initialize RFID readers based on node type
  if (nodeType == NODE_TYPE_MACHINE_MONITOR) {
    for (int i = 0; i < 4; i++) {
      initReader(i);
    }
  } else if (nodeType == NODE_TYPE_OFFICE_READER) {
    initReader(0);
  }
}

void RFIDHandler::initReader(int index) {
  rfidReaders[index].PCD_Init();
  delay(50);
  
  // Check if reader is responding
  byte version = rfidReaders[index].PCD_ReadRegister(MFRC522::VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.print("Warning: RFID Reader ");
    Serial.print(index);
    Serial.println(" not detected or not responding!");
  } else {
    Serial.print("RFID Reader ");
    Serial.print(index);
    Serial.print(" detected. Firmware version: 0x");
    Serial.println(version, HEX);
  }
}

bool RFIDHandler::checkReaders(String currentTags[], int nodeType) {
  bool tagDetected = false;
  int readersToCheck = (nodeType == NODE_TYPE_MACHINE_MONITOR) ? 4 : 1;
  
  for (int i = 0; i < readersToCheck; i++) {
    // Reset the RFID reader
    rfidReaders[i].PCD_Init();
    
    // Look for a card
    if (rfidReaders[i].PICC_IsNewCardPresent() && rfidReaders[i].PICC_ReadCardSerial()) {
      String uid = getUID(rfidReaders[i]);
      lastUID = uid;
      
      Serial.print("RFID tag detected on reader ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(uid);
      
      // Signal tag read with beep
      beepShort();
      
      // If same tag as currently logged in, this is a logout
      if (currentTags[i] == uid) {
        currentTags[i] = "";
        beepSuccess();
        tagDetected = true;
      } 
      // If no user is using this reader/machine, this is a login
      else if (currentTags[i] == "") {
        currentTags[i] = uid;
        beepSuccess();
        tagDetected = true;
      }
      // Different tag while machine is in use
      else {
        beepError();
      }
      
      // Halt PICC and stop encryption
      rfidReaders[i].PICC_HaltA();
      rfidReaders[i].PCD_StopCrypto1();
    }
  }
  
  return tagDetected;
}

String RFIDHandler::getLastTag() {
  return lastUID;
}

String RFIDHandler::getUID(MFRC522 &reader) {
  String uid = "";
  for (byte i = 0; i < reader.uid.size; i++) {
    if (reader.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(reader.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void RFIDHandler::beepSuccess() {
  // Double short beep for success
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
}

void RFIDHandler::beepError() {
  // Long beep for error
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
}

void RFIDHandler::beepShort() {
  // Short beep for simple acknowledgment
  digitalWrite(BUZZER_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZER_PIN, LOW);
}