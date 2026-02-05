/*
 * display_ui.h
 * OLED Display Functions
 * - Display initialization
 * - Status display
 * - Message scrolling
 */

#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// External references (defined in main .ino)
extern Adafruit_SSD1306 display;
extern float loraFrequency;
extern int8_t loraPower;
extern uint8_t nodeID;
extern String lastReceivedMsg;
extern int lastRSSI;
extern uint8_t lastSenderID;
extern uint32_t messagesReceived;
extern uint32_t messagesSent;
extern int scrollOffset;

// Pin definitions (already defined in main .ino)
// #define LED_PIN and OLED_* pins are in main file

// Display settings
#define SCREEN_ADDRESS 0x3C

// =============================
// DISPLAY SETUP
// =============================
void setupDisplay() {
    Serial.print("[OLED] Initializing SSD1306 128x32... ");
    
    // Initialize I2C with pins 21, 22 (OLED_SDA, OLED_SCL defined in main .ino)
    Wire.begin(21, 22);
    
    // Initialize display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("FAILED");
        Serial.println("[ERROR] OLED not found at 0x3C! Check wiring.");
        
        // Blink LED slowly to indicate display error
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    } else {
        Serial.println("OK");
        display.clearDisplay();
        display.display();
    }
}

// =============================
// DISPLAY FUNCTIONS
// =============================
extern const char SOFTWARE_VERSION[];

void showStartupMessage() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Line 1: Title + software version
    display.setCursor(0, 0);
    display.print("LoRa Mesh Server v");
    display.println(SOFTWARE_VERSION);
    
    // Line 2: Frequency and Power
    display.setCursor(0, 10);
    display.print("Freq:");
    display.print(loraFrequency, 1);
    display.print("MHz ");
    display.print(loraPower);
    display.println("dBm");
    
    // Line 3: Node ID
    display.setCursor(0, 20);
    display.print("Node ID: ");
    display.println(nodeID);
    
    display.display();
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Line 1: Frequency, Power
    display.setCursor(0, 0);
    display.print(loraFrequency, 1);
    display.print("MHz ");
    display.print(loraPower);
    display.print("dBm N");
    display.println(nodeID);
    
    // Line 2: TX/RX counts and RSSI
    display.setCursor(0, 10);
    display.print("TX:");
    display.print(messagesSent);
    display.print(" RX:");
    display.print(messagesReceived);
    if (lastRSSI != 0) {
        display.print(" ");
        display.print(lastRSSI);
        display.print("dB");
    }
    
    // Line 3: Last message (scrolling if too long)
    display.setCursor(0, 20);
    if (lastReceivedMsg.length() > 0) {
        if (lastReceivedMsg.length() <= 21) {
            // Short message - display normally
            display.print(lastReceivedMsg.substring(0, 21));
        } else {
            // Long message - scroll
            String scrollMsg = lastReceivedMsg + "   "; // Add spacing
            int startPos = scrollOffset % scrollMsg.length();
            String displayText = scrollMsg.substring(startPos);
            if (displayText.length() < 21) {
                displayText += scrollMsg.substring(0, 21 - displayText.length());
            }
            display.print(displayText.substring(0, 21));
        }
    } else {
        display.print("Waiting...");
    }
    
    display.display();
}

#endif // DISPLAY_UI_H
