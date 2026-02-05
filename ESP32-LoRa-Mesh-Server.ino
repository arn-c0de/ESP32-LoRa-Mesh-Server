/*
 * ESP32 LoRa Mesh Server
 * Hardware: ESP32 LoRa32 (TTGO V2.1) with SX1276
 * Display: SSD1306 128x32 OLED (I2C 0x3C)
 * 
 * Features:
 * - LoRa Mesh with Node-ID and Hop-Count routing
 * - OLED Display showing Channel, Frequency, Power, RSSI, last message
 * - Serial commands with '/' prefix: /HELP, /PING, /TX, /FREQ, etc.
 * - Ping/Pong network testing
 * - RadioLib for SX1276 control
 * 
 * Pin Configuration (from HARDWARE.md):
 * LoRa: NSS=5, RST=14, DIO0=26, SCK=18, MISO=19, MOSI=23
 * OLED: SDA=21, SCL=22, I2C Address=0x3C
 * Button: GPIO 33 (INPUT_PULLUP)
 * LED: GPIO 2
 */

#include <RadioLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// =============================
// PIN DEFINITIONS (HARDWARE.md)
// =============================
#define LORA_NSS    5
#define LORA_RST    14
#define LORA_DIO0   26
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   23

#define OLED_SDA    21
#define OLED_SCL    22
#define OLED_RESET  -1

#define BUTTON_PIN  33
#define LED_PIN     2

// Include modular components (after pin definitions)
#include "crypt.h"
#include "mesh_network.h"
#include "commands.h"
#include "display_ui.h"

// =============================
// OLED DISPLAY CONFIGURATION
// =============================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET  -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =============================
// LORA CONFIGURATION
// =============================
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC);

// Software version
const char SOFTWARE_VERSION[] = "1.1"; // Update here for firmware version display

// Default LoRa settings
float loraFrequency = 868.0;        // Default 868 MHz (EU)
int8_t loraPower = 14;              // Default 14 dBm (max for EU)
float loraBandwidth = 125.0;        // 125 kHz
uint8_t loraSpreadFactor = 7;       // SF7
uint8_t loraCodingRate = 5;         // CR 4/5

// =============================
// MESH CONFIGURATION
// =============================
uint8_t nodeID = 1;                 // This node's ID (1-255)
uint8_t defaultHopCount = 3;        // Default hop count for messages
String lastReceivedMsg = "";        // Last received message text
int lastRSSI = 0;                   // Last received RSSI
uint8_t lastSenderID = 0;           // Last sender node ID
uint32_t messagesReceived = 0;
uint32_t messagesSent = 0;

// =============================
// EEPROM STORAGE LAYOUT
// =============================
#define EEPROM_SIZE 256
#define EEPROM_INIT_FLAG_ADDR 0      // Byte 0: init flag (0xAA = initialized)
#define EEPROM_INIT_FLAG_VALUE 0xAA
#define DEVICE_NAME_ADDR 1           // Byte 1: device name length
#define DEVICE_NAME_ADDR_DATA 2      // Bytes 2-33: device name data
#define DEVICE_NAME_MAX_LEN 32
#define NODEID_ADDR 34               // Byte 34: node ID
#define FREQUENCY_ADDR 35            // Bytes 35-38: frequency (float)
#define POWER_ADDR 39                // Byte 39: TX power
#define BANDWIDTH_ADDR 40            // Bytes 40-43: bandwidth (float)
#define SF_ADDR 44                   // Byte 44: spread factor
#define CR_ADDR 45                   // Byte 45: coding rate
#define HOPS_ADDR 46                 // Byte 46: default hop count
String deviceName = "";             // Device name (loaded from EEPROM on startup)

// =============================
// SERIAL COMMAND BUFFER
// =============================
String serialBuffer = "";
const uint16_t MAX_SERIAL_BUFFER = 256;

// =============================
// DISPLAY STATE
// =============================
uint32_t lastDisplayUpdate = 0;
const uint16_t DISPLAY_UPDATE_INTERVAL = 1000; // Update every 1s
uint32_t startupDisplayTime = 0;
bool showingStartup = true;
int scrollOffset = 0;               // For scrolling long messages
uint32_t lastScrollUpdate = 0;
const uint16_t SCROLL_INTERVAL = 300; // Scroll every 300ms

// =============================
// FUNCTION PROTOTYPES
// =============================
void setupLoRa();
void setupPins();
void loadDeviceName();
void loadAllSettings();
void saveAllSettings();

// =============================
// SETUP
// =============================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    loadDeviceName();
    loadAllSettings();
        // Load encryption key from NVS (if saved)
    loadEncryptionKey();
        Serial.println();
    Serial.println("========================================");
    Serial.print("  ESP32 LoRa Mesh Server v");
    Serial.println(SOFTWARE_VERSION);
    Serial.println("  Hardware: TTGO LoRa32 V2.1 + SX1276");
    Serial.println("  Features: Ping/Pong, Mesh Routing");
    Serial.println("========================================");
    
    // Setup pins
    setupPins();
    
    // Setup display
    setupDisplay();
    showStartupMessage();
    
    // Setup LoRa
    setupLoRa();
    
    Serial.println();
    Serial.println("System ready! Type '/HELP' for commands.");
    Serial.println("========================================");
    
    // Show startup message for 3 seconds
    startupDisplayTime = millis();
}

// =============================
// MAIN LOOP
// =============================
void loop() {
    // Check if startup display period is over
    if (showingStartup && (millis() - startupDisplayTime > 3000)) {
        showingStartup = false;
        updateDisplay();
    }
    
    // Handle serial commands
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                processSerialCommand(serialBuffer);
                serialBuffer = "";
            }
        } else if (serialBuffer.length() < MAX_SERIAL_BUFFER) {
            serialBuffer += c;
        }
    }
    
    // Check for incoming LoRa messages
    receiveLoRaMessage();
    
    // Check for ping timeout
    checkPingTimeout();
    
    // Update display periodically
    if (!showingStartup && (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL)) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }
    
    // Scroll long messages
    if (!showingStartup && lastReceivedMsg.length() > 16) {
        if (millis() - lastScrollUpdate > SCROLL_INTERVAL) {
            scrollOffset++;
            if (scrollOffset > lastReceivedMsg.length()) {
                scrollOffset = 0;
            }
            updateDisplay();
            lastScrollUpdate = millis();
        }
    }
    
    delay(10); // Small delay to prevent CPU hogging
}

// =============================
// PIN SETUP
// =============================
void setupPins() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("[PIN] GPIO configured");
}

// =============================
// DEVICE NAME FUNCTIONS
// =============================
void loadDeviceName() {
    deviceName = "";
    uint8_t len = EEPROM.read(DEVICE_NAME_ADDR);
    if (len > 0 && len <= DEVICE_NAME_MAX_LEN) {
        for (int i = 0; i < len; i++) {
            deviceName += (char)EEPROM.read(DEVICE_NAME_ADDR_DATA + i);
        }
    }
}

void saveDeviceName(String name) {
    if (name.length() > DEVICE_NAME_MAX_LEN) {
        name = name.substring(0, DEVICE_NAME_MAX_LEN);
    }
    EEPROM.write(DEVICE_NAME_ADDR, name.length());
    for (int i = 0; i < name.length(); i++) {
        EEPROM.write(DEVICE_NAME_ADDR_DATA + i, name[i]);
    }
    EEPROM.commit();
    deviceName = name;
}

// =============================
// SETTINGS PERSISTENCE FUNCTIONS
// =============================
void saveAllSettings() {
    // Save init flag first
    EEPROM.write(EEPROM_INIT_FLAG_ADDR, EEPROM_INIT_FLAG_VALUE);
    
    // Save Node ID
    EEPROM.write(NODEID_ADDR, nodeID);
    
    // Save Frequency (float, 4 bytes)
    EEPROM.put(FREQUENCY_ADDR, loraFrequency);
    
    // Save TX Power
    EEPROM.write(POWER_ADDR, loraPower);
    
    // Save Bandwidth (float, 4 bytes)
    EEPROM.put(BANDWIDTH_ADDR, loraBandwidth);
    
    // Save Spread Factor
    EEPROM.write(SF_ADDR, loraSpreadFactor);
    
    // Save Coding Rate
    EEPROM.write(CR_ADDR, loraCodingRate);
    
    // Save Default Hop Count
    EEPROM.write(HOPS_ADDR, defaultHopCount);
    
    EEPROM.commit();
}

void loadAllSettings() {
    // Check if EEPROM has been initialized
    uint8_t initFlag = EEPROM.read(EEPROM_INIT_FLAG_ADDR);
    if (initFlag != EEPROM_INIT_FLAG_VALUE) {
        // EEPROM not initialized, save defaults
        Serial.println("[INFO] Initializing EEPROM with default settings");
        saveAllSettings();
        return;
    }
    
    // Load Node ID
    uint8_t savedNodeID = EEPROM.read(NODEID_ADDR);
    if (savedNodeID >= 1 && savedNodeID < 255) {
        nodeID = savedNodeID;
    }
    
    // Load Frequency
    float savedFreq = 0.0;
    EEPROM.get(FREQUENCY_ADDR, savedFreq);
    if (savedFreq >= 410.0 && savedFreq <= 928.0) {
        loraFrequency = savedFreq;
    }
    
    // Load TX Power
    int8_t savedPower = EEPROM.read(POWER_ADDR);
    if (savedPower >= 2 && savedPower <= 20) {
        loraPower = savedPower;
    }
    
    // Load Bandwidth
    float savedBW = 0.0;
    EEPROM.get(BANDWIDTH_ADDR, savedBW);
    if (savedBW > 0 && savedBW <= 500.0) {
        loraBandwidth = savedBW;
    }
    
    // Load Spread Factor
    uint8_t savedSF = EEPROM.read(SF_ADDR);
    if (savedSF >= 6 && savedSF <= 12) {
        loraSpreadFactor = savedSF;
    }
    
    // Load Coding Rate
    uint8_t savedCR = EEPROM.read(CR_ADDR);
    if (savedCR >= 5 && savedCR <= 8) {
        loraCodingRate = savedCR;
    }
    
    // Load Default Hop Count
    uint8_t savedHops = EEPROM.read(HOPS_ADDR);
    if (savedHops <= 10) {
        defaultHopCount = savedHops;
    }
}

// =============================
// LORA SETUP
// =============================
void setupLoRa() {
    Serial.print("[LoRa] Initializing SX1276... ");
    
    // Initialize SPI
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    
    // Initialize radio with default settings
    int state = radio.begin(loraFrequency, loraBandwidth, loraSpreadFactor, 
                           loraCodingRate, RADIOLIB_SX127X_SYNC_WORD, 
                           loraPower, 8, 0);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
        Serial.print("[LoRa] Frequency: ");
        Serial.print(loraFrequency, 1);
        Serial.println(" MHz");
        Serial.print("[LoRa] TX Power: ");
        Serial.print(loraPower);
        Serial.println(" dBm");
        Serial.print("[LoRa] Bandwidth: ");
        Serial.print(loraBandwidth, 0);
        Serial.println(" kHz");
        Serial.print("[LoRa] Spread Factor: ");
        Serial.println(loraSpreadFactor);
        Serial.print("[LoRa] Node ID: ");
        Serial.println(nodeID);
        
        digitalWrite(LED_PIN, HIGH); // LED on = LoRa ready
    } else {
        Serial.print("FAILED, code: ");
        Serial.println(state);
        Serial.println("[ERROR] Check wiring and antenna!");
        
        // Blink LED rapidly to indicate error
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }
}
