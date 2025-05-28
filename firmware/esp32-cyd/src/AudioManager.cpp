#include "AudioManager.h"

#ifdef WITH_AUDIO

#include "constants.h"
#include <WiFiUdp.h>

// Static task reference for multicast audio receiver
static WiFiUDP udp;

AudioManager::AudioManager() {
    bluetoothVolume = 180;      // Default volume (70%)
    builtinVolume = 180;        // Default volume (70%)
    bluetoothEnabled = false;
    builtinAudioEnabled = true;
    audioReceiverActive = false;
    eStopActive = false;
    deviceCount = 0;
    audioReceiverTaskHandle = NULL;
}

AudioManager::~AudioManager() {
    stopAudioReceiver();
    a2dpSink.end();
}

void AudioManager::begin() {
    // Configure built-in audio output
    configureI2S();

    // Setup built-in audio
    builtinAudio.setPinout(I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DATA_PIN);
    builtinAudio.setVolume(builtinVolume); // 0...255
    
    // Enable audio amplifier if available
    pinMode(AUDIO_ENABLE_PIN, OUTPUT);
    digitalWrite(AUDIO_ENABLE_PIN, HIGH);
    
    // Configure Bluetooth A2DP sink
    static const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = (i2s_bits_per_sample_t)16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    a2dpSink.set_i2s_config(i2s_config);
    a2dpSink.set_pin_config({
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCLK_PIN,
        .data_out_num = I2S_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    });
    a2dpSink.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *) {
        Serial.print("Bluetooth connection state: ");
        Serial.println(state);
    });
    
    // Start network audio receiver
    startAudioReceiver();
}

void AudioManager::configureI2S() {
    i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2sPinConfig = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCLK_PIN,
        .data_out_num = I2S_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    i2s_driver_install((i2s_port_t)0, &i2sConfig, 0, NULL);
    i2s_set_pin((i2s_port_t)0, &i2sPinConfig);
    i2s_zero_dma_buffer((i2s_port_t)0);
}

void AudioManager::setBluetooth(bool enabled) {
    if (bluetoothEnabled != enabled) {
        bluetoothEnabled = enabled;
        if (enabled) {
            a2dpSink.start("ShopMonitor-CYD");
        } else {
            a2dpSink.end();
        }
    }
}

void AudioManager::setBuiltinAudio(bool enabled) {
    builtinAudioEnabled = enabled;
    digitalWrite(AUDIO_ENABLE_PIN, enabled ? HIGH : LOW);
}

void AudioManager::setBluetoothVolume(uint8_t volume) {
    bluetoothVolume = volume;
    a2dpSink.set_volume(volume);
}

void AudioManager::setBuiltinVolume(uint8_t volume) {
    builtinVolume = volume;
    builtinAudio.setVolume(volume); 
}

void AudioManager::startAudioReceiver() {
    if (!audioReceiverActive) {
        // Start UDP multicast listener
        udp.begin(UDP_PORT);
        
        // Create a task for audio reception
        xTaskCreatePinnedToCore(
            audioReceiverTask,    // Task function
            "AudioReceiver",      // Name of task
            10000,                // Stack size
            this,                 // Parameter passed to task
            1,                    // Priority (lower number = lower priority)
            &audioReceiverTaskHandle,  // Task handle
            0                     // Core (0 = PRO CPU, 1 = APP CPU)
        );
        
        audioReceiverActive = true;
        Serial.println("Audio receiver started");
    }
}

void AudioManager::stopAudioReceiver() {
    if (audioReceiverActive && audioReceiverTaskHandle != NULL) {
        vTaskDelete(audioReceiverTaskHandle);
        udp.stop();
        audioReceiverActive = false;
        audioReceiverTaskHandle = NULL;
        Serial.println("Audio receiver stopped");
    }
}

bool AudioManager::isAudioReceiverActive() {
    return audioReceiverActive;
}

void AudioManager::audioReceiverTask(void *parameter) {
    AudioManager *audioManager = (AudioManager *)parameter;
    size_t bytesWritten;
    
    while (true) {
        // Check if E-Stop is active - silence audio if it is
        if (audioManager->eStopActive) {
            vTaskDelay(100 / portTICK_PERIOD_MS); // Check every 100ms during E-STOP
            continue;
        }
        
        // Check if a packet is available
        int packetSize = udp.parsePacket();
        if (packetSize) {
            // Read the packet into the buffer
            int len = udp.read(audioManager->audioBuffer, BUFFER_SIZE);
            
            // Output to I2S if built-in audio is enabled
            if (audioManager->builtinAudioEnabled) {
                i2s_write((i2s_port_t)0, audioManager->audioBuffer, len, &bytesWritten, 100);
            }
            
            // Forward to Bluetooth if enabled (this is handled automatically by the A2DP sink)
        }
        
        // Small delay to prevent task from consuming too much CPU
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void AudioManager::scanBluetoothDevices() {
    // Clear previous device list
    deviceCount = 0;
    
    // This is just a placeholder - in a real implementation, we would use
    // ESP32's Bluetooth library to scan for available devices
    Serial.println("Scanning for Bluetooth devices...");
    
    // For demonstration, add a few mock devices
    if (deviceCount < 10) {
        discoveredDevices[deviceCount].name = "BT Speaker 1";
        discoveredDevices[deviceCount].address = "AB:CD:EF:12:34:56";
        discoveredDevices[deviceCount].connected = false;
        deviceCount++;
    }
    
    if (deviceCount < 10) {
        discoveredDevices[deviceCount].name = "BT Headphones";
        discoveredDevices[deviceCount].address = "12:34:56:78:90:AB";
        discoveredDevices[deviceCount].connected = false;
        deviceCount++;
    }
    
    Serial.printf("Found %d Bluetooth devices\n", deviceCount);
}

int AudioManager::getBluetoothDeviceCount() {
    return deviceCount;
}

String AudioManager::getBluetoothDeviceName(int index) {
    if (index >= 0 && index < deviceCount) {
        return discoveredDevices[index].name;
    }
    return "";
}

String AudioManager::getBluetoothDeviceAddress(int index) {
    if (index >= 0 && index < deviceCount) {
        return discoveredDevices[index].address;
    }
    return "";
}

bool AudioManager::connectBluetoothDevice(String address) {
    Serial.printf("Connecting to Bluetooth device: %s\n", address.c_str());
    
    // In a real implementation, we would connect to the specified device
    // For now, just mark it as connected in our list
    for (int i = 0; i < deviceCount; i++) {
        if (discoveredDevices[i].address == address) {
            discoveredDevices[i].connected = true;
            return true;
        }
    }
    
    return false;
}

bool AudioManager::disconnectBluetoothDevice(String address) {
    Serial.printf("Disconnecting Bluetooth device: %s\n", address.c_str());
    
    // In a real implementation, we would disconnect from the specified device
    // For now, just mark it as disconnected in our list
    for (int i = 0; i < deviceCount; i++) {
        if (discoveredDevices[i].address == address) {
            discoveredDevices[i].connected = false;
            return true;
        }
    }
    
    return false;
}

String AudioManager::getConnectedBluetoothDevices() {
    String connectedDevices = "";
    
    for (int i = 0; i < deviceCount; i++) {
        if (discoveredDevices[i].connected) {
            if (connectedDevices.length() > 0) {
                connectedDevices += ", ";
            }
            connectedDevices += discoveredDevices[i].name;
        }
    }
    
    return connectedDevices;
}

void AudioManager::playAlertSound(int alertType) {
    // Don't play alert if E-STOP is active
    if (eStopActive) {
        return;
    }
    
    switch (alertType) {
        case 1: // Simple beep
            generateBeepSound(1000, 500); // 1kHz for 500ms
            break;
        case 2: // Double beep
            generateBeepSound(1000, 200); // 1kHz for 200ms
            delay(100);
            generateBeepSound(1000, 200); // 1kHz for 200ms
            break;
        case 3: // Warning alert
            generateBeepSound(2000, 300); // 2kHz for 300ms
            delay(100);
            generateBeepSound(1500, 300); // 1.5kHz for 300ms
            delay(100);
            generateBeepSound(2000, 300); // 2kHz for 300ms
            break;
        default:
            generateBeepSound(1000, 200); // Default beep
            break;
    }
}

void AudioManager::generateBeepSound(int frequency, int duration) {
    // Generate a simple beep tone directly to I2S
    const int sampleRate = 44100;
    const int numSamples = (duration * sampleRate) / 1000;
    const double amplitude = 0.5 * INT16_MAX; // 50% volume
    
    for (int i = 0; i < numSamples; i++) {
        double time = (double)i / sampleRate;
        double sinVal = sin(2 * PI * frequency * time);
        
        int16_t sample = (int16_t)(sinVal * amplitude);
        uint8_t buffer[4];
        buffer[0] = sample & 0xFF;         // Right channel low byte
        buffer[1] = (sample >> 8) & 0xFF;  // Right channel high byte
        buffer[2] = sample & 0xFF;         // Left channel low byte
        buffer[3] = (sample >> 8) & 0xFF;  // Left channel high byte
        
        size_t bytesWritten = 0;
        i2s_write((i2s_port_t)0, buffer, 4, &bytesWritten, 0);
        
        // If E-STOP is activated during alert, stop immediately
        if (eStopActive) {
            break;
        }
    }
}

void AudioManager::playAudioFile(const char* filename) {
    // This is a placeholder for playing audio files from flash storage
    // In a real implementation, we would read an audio file and play it
    Serial.printf("Playing audio file: %s (placeholder)\n", filename);
}

void AudioManager::stopAllSounds() {
    // Stop all audio output immediately
    i2s_zero_dma_buffer((i2s_port_t)0);
}

void AudioManager::setEStopStatus(bool active) {
    eStopActive = active;
    
    if (active) {
        // Immediately silence all audio
        stopAllSounds();
    }
}

bool AudioManager::isEStopActive() {
    return eStopActive;
}

#endif // WITH_AUDIO