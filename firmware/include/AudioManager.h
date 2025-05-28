#pragma once

#ifdef WITH_AUDIO

#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include <Audio.h>
#include <SPI.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <esp_now.h>

// Audio broadcasting defines
#define SAMPLE_RATE 44100    // Audio sample rate
#define BUFFER_SIZE 1024     // Broadcast buffer size
#define UDP_PORT 3333        // Audio broadcast UDP port

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Initialization
    void begin();
    void setBluetooth(bool enabled);
    void setBuiltinAudio(bool enabled);
    
    // Volume control
    void setBluetoothVolume(uint8_t volume); // 0-255
    void setBuiltinVolume(uint8_t volume);   // 0-255
    
    // Network audio
    void startAudioReceiver();
    void stopAudioReceiver();
    bool isAudioReceiverActive();
    
    // Bluetooth device management
    void scanBluetoothDevices();
    int getBluetoothDeviceCount();
    String getBluetoothDeviceName(int index);
    String getBluetoothDeviceAddress(int index);
    bool connectBluetoothDevice(String address);
    bool disconnectBluetoothDevice(String address);
    String getConnectedBluetoothDevices();
    
    // Alert sounds
    void playAlertSound(int alertType);
    void stopAllSounds();
    
    // E-Stop related
    void setEStopStatus(bool active);
    bool isEStopActive();

private:
    // Audio components
    BluetoothA2DPSink a2dpSink;
    Audio builtinAudio;
    
    // Audio settings
    uint8_t bluetoothVolume;
    uint8_t builtinVolume;
    bool bluetoothEnabled;
    bool builtinAudioEnabled;
    bool audioReceiverActive;
    bool eStopActive;
    
    // Bluetooth device list
    struct BluetoothDevice {
        String name;
        String address;
        bool connected;
    };
    BluetoothDevice discoveredDevices[10];
    int deviceCount;
    
    // Network audio receiver task
    TaskHandle_t audioReceiverTaskHandle;
    static void audioReceiverTask(void *parameter);
    
    // Buffer for receiving audio data
    uint8_t audioBuffer[BUFFER_SIZE];
    
    // I2S configuration for built-in audio
    void configureI2S();
    i2s_config_t i2sConfig;
    i2s_pin_config_t i2sPinConfig;
    
    // Alert sounds
    void generateBeepSound(int frequency, int duration);
    void playAudioFile(const char* filename);
};

#endif // WITH_AUDIO