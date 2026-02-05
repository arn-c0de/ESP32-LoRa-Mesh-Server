/*
 * commands.h
 * Serial Command Processing
 * - Command parsing
 * - LoRa configuration commands
 * - Messaging commands
 * - Status and help
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include <RadioLib.h>
#include "mesh_network.h"
#include "crypt.h"

// External references (defined in main .ino)
extern SX1276 radio;
extern float loraFrequency;
extern int8_t loraPower;
extern float loraBandwidth;
extern uint8_t loraSpreadFactor;
extern uint8_t loraCodingRate;
extern uint8_t nodeID;
extern uint8_t defaultHopCount;
extern String lastReceivedMsg;
extern int lastRSSI;
extern uint8_t lastSenderID;
extern uint32_t messagesReceived;
extern uint32_t messagesSent;
extern int scrollOffset;
extern String deviceName;
extern void saveDeviceName(String name);
extern void saveAllSettings();

// Forward declarations
extern void updateDisplay();

// =============================
// HELP TEXT
// =============================
void printHelp() {
    Serial.println();
    Serial.println("========================================");
    Serial.println("  AVAILABLE COMMANDS");
    Serial.println("========================================");
    Serial.println("/HELP             - Show this help");
    Serial.println("/STATUS           - Show system status");
    Serial.println("/PING             - Broadcast PING and wait for PONG");
    Serial.println("(Separators: ':', '=' or space — e.g. /TX:Hi, /TX=Hi or /TX Hi)");
    Serial.println();
    Serial.println("--- LoRa Settings ---");
    Serial.println("/FREQ:868.0       - Set frequency in MHz");
    Serial.println("/FREQ?            - Query current frequency");
    Serial.println("/POWER:14         - Set TX power (2-20 dBm)");
    Serial.println("/POWER?           - Query current power");
    Serial.println("/SF:7             - Set spread factor (6-12)");
    Serial.println("/BW:125           - Set bandwidth (kHz)");
    Serial.println();
    Serial.println("--- Mesh Settings ---");
    Serial.println("/NODEID:1         - Set this node's ID (1-255)");
    Serial.println("/HOPS:3           - Set default hop count (0-10)");
    Serial.println();
    Serial.println("--- Messaging ---");
    Serial.println("/TX:Hello         - Broadcast message (alias: /SEND)");
    Serial.println("/TXTO:5,Hi        - Send to specific node (alias: /SENDTO)");
    Serial.println("/NAME:DeviceName  - Set device name (prepended to messages)");
    Serial.println();
    Serial.println("--- Encryption (AES-256-GCM) ---");
    Serial.println("/KEY:passphrase   - Set encryption key from passphrase");
    Serial.println("/ESEND:message    - Send encrypted broadcast message");
    Serial.println("/CLEARKEY         - Clear encryption key from memory/NVS");
    Serial.println("/EINFO            - Show encryption status and fingerprint");
    Serial.println();
    Serial.println("--- Other ---");
    Serial.println("/RESET            - Reset counters and display");
    Serial.println("/FORMAT           - Reset EEPROM to factory defaults");
    Serial.println("========================================");
    Serial.println();
}

// =============================
// COMMAND PROCESSOR
// =============================
void processSerialCommand(String cmd) {
    cmd.trim();
    bool hadSlash = false;
    if (cmd.startsWith("/")) {
        hadSlash = true;
        cmd = cmd.substring(1);
    }
    cmd.toUpperCase();
    
    Serial.print("> ");
    if (hadSlash) Serial.print("/");
    Serial.println(cmd);
    
    // Parse command and arguments
    // Accept separators ':' or '=' or a space. This allows '/SEND:msg', '/SEND=msg' or '/SEND msg'
    int colonPos = cmd.indexOf(':');
    int equalPos = cmd.indexOf('=');
    int spacePos = cmd.indexOf(' ');
    int sepPos = -1;
    if (colonPos >= 0) sepPos = colonPos;
    if (equalPos >= 0 && (sepPos == -1 || equalPos < sepPos)) sepPos = equalPos;
    if (spacePos >= 0 && (sepPos == -1 || spacePos < sepPos)) sepPos = spacePos;

    String command = (sepPos > 0) ? cmd.substring(0, sepPos) : cmd;
    String argument = (sepPos > 0) ? cmd.substring(sepPos + 1) : "";
    argument.trim();
    
    // HELP command
    if (command == "HELP" || command == "?") {
        printHelp();
    }
    // PING command
    else if (command == "PING") {
        sendPing();
    }
    // NAME - Set device name
    else if (command == "NAME") {
        if (argument.length() > 0) {
            saveDeviceName(argument);
            Serial.print("[OK] Device name set to: '");
            Serial.print(deviceName);
            Serial.println("'");
            updateDisplay();
        } else {
            if (deviceName.length() > 0) {
                Serial.print("[INFO] Current device name: '");
                Serial.print(deviceName);
                Serial.println("'");
            } else {
                Serial.println("[INFO] No device name set (messages sent without prefix)");
            }
        }
    }
    // FREQ - Set frequency
    else if (command == "FREQ") {
        if (argument.length() > 0) {
            float newFreq = argument.toFloat();
            if (newFreq >= 410.0 && newFreq <= 525.0) {
                loraFrequency = newFreq;
                int state = radio.setFrequency(loraFrequency);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] Frequency set to ");
                    Serial.print(loraFrequency, 3);
                    Serial.println(" MHz");
                    updateDisplay();
                } else {
                    Serial.print("[ERROR] Failed to set frequency, code: ");
                    Serial.println(state);
                }
            } else if (newFreq >= 863.0 && newFreq <= 870.0) {
                loraFrequency = newFreq;
                int state = radio.setFrequency(loraFrequency);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] Frequency set to ");
                    Serial.print(loraFrequency, 3);
                    Serial.println(" MHz (EU 868 band)");
                    updateDisplay();
                } else {
                    Serial.print("[ERROR] Failed to set frequency, code: ");
                    Serial.println(state);
                }
            } else if (newFreq >= 902.0 && newFreq <= 928.0) {
                loraFrequency = newFreq;
                int state = radio.setFrequency(loraFrequency);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] Frequency set to ");
                    Serial.print(loraFrequency, 3);
                    Serial.println(" MHz (US 915 band)");
                    updateDisplay();
                } else {
                    Serial.print("[ERROR] Failed to set frequency, code: ");
                    Serial.println(state);
                }
            } else {
                Serial.println("[ERROR] Invalid frequency. Use 410-525, 863-870 (EU), or 902-928 (US) MHz");
            }
        } else {
            Serial.print("[INFO] Current frequency: ");
            Serial.print(loraFrequency, 3);
            Serial.println(" MHz");
        }
    }
    // FREQ? - Query frequency
    else if (command == "FREQ?") {
        Serial.print("[INFO] Frequency: ");
        Serial.print(loraFrequency, 3);
        Serial.println(" MHz");
    }
    // POWER - Set TX power
    else if (command == "POWER") {
        if (argument.length() > 0) {
            int newPower = argument.toInt();
            if (newPower >= 2 && newPower <= 20) {
                loraPower = newPower;
                int state = radio.setOutputPower(loraPower);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] TX Power set to ");
                    Serial.print(loraPower);
                    Serial.println(" dBm");
                    updateDisplay();
                } else {
                    Serial.print("[ERROR] Failed to set power, code: ");
                    Serial.println(state);
                }
            } else {
                Serial.println("[ERROR] Invalid power. Use 2-20 dBm (max 14 dBm for EU)");
            }
        } else {
            Serial.print("[INFO] Current TX power: ");
            Serial.print(loraPower);
            Serial.println(" dBm");
        }
    }
    // POWER? - Query power
    else if (command == "POWER?") {
        Serial.print("[INFO] TX Power: ");
        Serial.print(loraPower);
        Serial.println(" dBm");
    }
    // NODEID - Set node ID
    else if (command == "NODEID") {
        if (argument.length() > 0) {
            int newID = argument.toInt();
            if (newID >= 1 && newID <= 255) {
                nodeID = newID;
                saveAllSettings();
                Serial.print("[OK] Node ID set to ");
                Serial.println(nodeID);
                updateDisplay();
            } else {
                Serial.println("[ERROR] Invalid Node ID. Use 1-255");
            }
        } else {
            Serial.print("[INFO] Current Node ID: ");
            Serial.println(nodeID);
        }
    }
    // HOPS - Set default hop count
    else if (command == "HOPS") {
        if (argument.length() > 0) {
            int newHops = argument.toInt();
            if (newHops >= 0 && newHops <= 10) {
                defaultHopCount = newHops;
                saveAllSettings();
                Serial.print("[OK] Default hop count set to ");
                Serial.println(defaultHopCount);
            } else {
                Serial.println("[ERROR] Invalid hop count. Use 0-10");
            }
        } else {
            Serial.print("[INFO] Current default hop count: ");
            Serial.println(defaultHopCount);
        }
    }
    // TX - Transmit message (alias: SEND)
    else if (command == "TX" || command == "SEND") {
        if (argument.length() > 0) {
            sendMeshMessage(0, defaultHopCount, argument); // 0 = broadcast
        } else {
            Serial.println("[ERROR] No message to send. Usage: /TX:Your message here or /SEND:Your message here");
        }
    }
    // TXTO - Transmit to specific node (alias: SENDTO)
    else if (command == "TXTO" || command == "SENDTO") {
        int commaPos = argument.indexOf(',');
        if (commaPos > 0) {
            uint8_t targetID = argument.substring(0, commaPos).toInt();
            String msg = argument.substring(commaPos + 1);
            if (targetID != 0 && msg.length() > 0) {
                sendMeshMessage(targetID, defaultHopCount, msg);
            } else {
                Serial.println("[ERROR] Invalid format. Usage: /TXTO:NodeID,Message or /SENDTO:NodeID,Message");
            }
        } else {
            Serial.println("[ERROR] Usage: /TXTO:NodeID,Message (e.g., /TXTO:5,Hello)");
        }
    }
    // STATUS - Show system status
    else if (command == "STATUS" || command == "INFO") {
        Serial.println("========================================");
        Serial.println("  SYSTEM STATUS");
        Serial.println("========================================");
        Serial.print("Device Name:   ");
        if (deviceName.length() > 0) {
            Serial.println(deviceName);
        } else {
            Serial.println("(not set)");
        }
        Serial.print("Node ID:       ");
        Serial.println(nodeID);
        Serial.print("Frequency:     ");
        Serial.print(loraFrequency, 3);
        Serial.println(" MHz");
        Serial.print("TX Power:      ");
        Serial.print(loraPower);
        Serial.println(" dBm");
        Serial.print("Bandwidth:     ");
        Serial.print(loraBandwidth, 0);
        Serial.println(" kHz");
        Serial.print("Spread Factor: SF");
        Serial.println(loraSpreadFactor);
        Serial.print("Coding Rate:   4/");
        Serial.println(loraCodingRate);
        Serial.print("Default Hops:  ");
        Serial.println(defaultHopCount);
        Serial.print("TX Count:      ");
        Serial.println(messagesSent);
        Serial.print("RX Count:      ");
        Serial.println(messagesReceived);
        if (messagesReceived > 0) {
            Serial.print("Last RSSI:     ");
            Serial.print(lastRSSI);
            Serial.println(" dBm");
            Serial.print("Last Sender:   Node ");
            Serial.println(lastSenderID);
            Serial.print("Last Message:  ");
            Serial.println(lastReceivedMsg);
        }
        Serial.println("========================================");
    }
    // RESET - Reset counters
    else if (command == "RESET") {
        messagesReceived = 0;
        messagesSent = 0;
        lastReceivedMsg = "";
        lastRSSI = 0;
        lastSenderID = 0;
        scrollOffset = 0;
        Serial.println("[OK] Counters and last message cleared");
        updateDisplay();
    }
    // FORMAT - Reset EEPROM to defaults
    else if (command == "FORMAT") {
        Serial.println("[WARNING] Resetting EEPROM to factory defaults...");
        delay(500);
        
        // Clear entire EEPROM
        for (int i = 0; i < 256; i++) {
            EEPROM.write(i, 0xFF);
        }
        
        // Reset variables to defaults
        nodeID = 1;
        loraFrequency = 868.0;
        loraPower = 14;
        loraBandwidth = 125.0;
        loraSpreadFactor = 7;
        loraCodingRate = 5;
        defaultHopCount = 3;
        deviceName = "";
        
        // Save defaults to EEPROM
        saveAllSettings();
        
        Serial.println("[OK] EEPROM formatted and reset to defaults");
        Serial.println("[OK] Restart the device to apply changes");
    }
    else if (command == "SF") {
        if (argument.length() > 0) {
            int newSF = argument.toInt();
            if (newSF >= 6 && newSF <= 12) {
                loraSpreadFactor = newSF;
                int state = radio.setSpreadingFactor(loraSpreadFactor);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] Spreading Factor set to SF");
                    Serial.println(loraSpreadFactor);
                } else {
                    Serial.print("[ERROR] Failed to set SF, code: ");
                    Serial.println(state);
                }
            } else {
                Serial.println("[ERROR] Invalid SF. Use 6-12");
            }
        } else {
            Serial.print("[INFO] Current Spreading Factor: SF");
            Serial.println(loraSpreadFactor);
        }
    }
    // BW - Set bandwidth
    else if (command == "BW") {
        if (argument.length() > 0) {
            float newBW = argument.toFloat();
            if (newBW == 7.8 || newBW == 10.4 || newBW == 15.6 || newBW == 20.8 ||
                newBW == 31.25 || newBW == 41.7 || newBW == 62.5 || newBW == 125.0 ||
                newBW == 250.0 || newBW == 500.0) {
                loraBandwidth = newBW;
                int state = radio.setBandwidth(loraBandwidth);
                if (state == RADIOLIB_ERR_NONE) {
                    saveAllSettings();
                    Serial.print("[OK] Bandwidth set to ");
                    Serial.print(loraBandwidth, 1);
                    Serial.println(" kHz");
                } else {
                    Serial.print("[ERROR] Failed to set BW, code: ");
                    Serial.println(state);
                }
            } else {
                Serial.println("[ERROR] Invalid BW. Valid: 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125, 250, 500 kHz");
            }
        } else {
            Serial.print("[INFO] Current Bandwidth: ");
            Serial.print(loraBandwidth, 1);
            Serial.println(" kHz");
        }
    }
    // KEY - Set encryption key from passphrase
    else if (command == "KEY") {
        if (argument.length() > 0) {
            setEncryptionKeyFromPassphrase(argument);
        } else {
            Serial.println("[ERROR] Usage: /KEY:passphrase or /KEY=passphrase");
        }
    }
    // CLEARKEY - Clear encryption key
    else if (command == "CLEARKEY") {
        clearEncryptionKey();
    }
    // EINFO - Show encryption info
    else if (command == "EINFO") {
        printEncryptionInfo();
    }
    // ESEND - Send encrypted message
    else if (command == "ESEND") {
        if (argument.length() > 0) {
            if (!isEncryptionKeySet()) {
                Serial.println("[ERROR] No encryption key set. Use /KEY:passphrase first");
            } else {
                String encrypted = encryptAndEncode(argument);
                if (encrypted.length() > 0) {
                    // Build encrypted payload: FLAG:<NodeID>:<Base64Blob>
                    String payload = "FLAG:" + String(nodeID) + ":" + encrypted;
                    sendMeshMessage(0, defaultHopCount, payload); // 0 = broadcast
                    Serial.println("[E] Encrypted message sent");
                } else {
                    Serial.println("[ERROR] Encryption failed");
                }
            }
        } else {
            Serial.println("[ERROR] No message to encrypt. Usage: /ESEND:Your message here");
        }
    }
    // Unknown command
    else {
        Serial.println("[ERROR] Unknown command. Type /HELP for available commands.");
    }
    
    Serial.println();
}

#endif // COMMANDS_H
