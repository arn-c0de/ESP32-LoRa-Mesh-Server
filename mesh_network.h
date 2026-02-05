/*
 * mesh_network.h
 * LoRa Mesh Network Functions
 * - Packet parsing and building
 * - Message sending and receiving
 * - Ping/Pong handling
 */

#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include <Arduino.h>
#include <RadioLib.h>

// Forward declarations for encryption functions (implemented in crypt.h)
extern bool isEncryptionKeySet();
extern bool decodeAndDecrypt(const String &base64Blob, String &outPlaintext);

// External references (defined in main .ino)
extern SX1276 radio;
extern uint8_t nodeID;
extern uint8_t defaultHopCount;
extern String lastReceivedMsg;
extern int lastRSSI;
extern uint8_t lastSenderID;
extern uint32_t messagesReceived;
extern uint32_t messagesSent;
extern int scrollOffset;
extern String deviceName;

// Forward declarations
extern void updateDisplay();

// =============================
// PACKET STRUCTURE
// =============================
// Format: [NodeID:1B][ToID:1B][HopCount:1B][Data...]
struct MeshPacket {
    uint8_t fromID;
    uint8_t toID;
    uint8_t hopCount;
    String data;
};

// =============================
// PING/PONG TRACKING
// =============================
uint32_t lastPingTime = 0;
bool waitingForPong = false;
const uint16_t PING_TIMEOUT = 5000; // 5 seconds

// =============================
// PACKET FUNCTIONS
// =============================
String buildMeshPacket(uint8_t toID, uint8_t hops, String data) {
    String packet = String(nodeID) + ":" + String(toID) + ":" + String(hops) + ":" + data;
    return packet;
}

bool parseMeshPacket(String rawMsg, MeshPacket &packet) {
    // Parse format: FromID:ToID:HopCount:Data
    int firstColon = rawMsg.indexOf(':');
    if (firstColon == -1) return false;
    
    int secondColon = rawMsg.indexOf(':', firstColon + 1);
    if (secondColon == -1) return false;
    
    int thirdColon = rawMsg.indexOf(':', secondColon + 1);
    if (thirdColon == -1) return false;
    
    packet.fromID = rawMsg.substring(0, firstColon).toInt();
    packet.toID = rawMsg.substring(firstColon + 1, secondColon).toInt();
    packet.hopCount = rawMsg.substring(secondColon + 1, thirdColon).toInt();
    packet.data = rawMsg.substring(thirdColon + 1);
    
    return true;
}

// =============================
// MESSAGE TRANSMISSION
// =============================
void sendMeshMessage(uint8_t toID, uint8_t hops, String data) {
    // Prepend device name if set (but NOT for encrypted FLAG: messages)
    String msgToSend = data;
    if (deviceName.length() > 0 && !data.startsWith("FLAG:")) {
        msgToSend = deviceName + ":" + data;
    }
    
    String packet = buildMeshPacket(toID, hops, msgToSend);
    
    Serial.print("[TX] Sending to ");
    Serial.print(toID == 0 ? "BROADCAST" : String(toID));
    Serial.print(" (hops: ");
    Serial.print(hops);
    Serial.print("): ");
    Serial.println(data);
    
    // Transmit packet
    int state = radio.transmit(packet);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[TX] Success");
        messagesSent++;
        
        // Brief LED blink
        digitalWrite(LED_PIN, LOW);
        delay(50);
        digitalWrite(LED_PIN, HIGH);
        
        updateDisplay();
    } else {
        Serial.print("[TX] Failed, code: ");
        Serial.println(state);
    }
}

// =============================
// PING/PONG HANDLING
// =============================
void handlePingPong(MeshPacket &packet) {
    // Check if message is PING - respond with PONG
    if (packet.data.equalsIgnoreCase("PING")) {
        Serial.println("[PING] Received PING request, sending PONG...");
        delay(random(50, 200)); // Random delay to avoid collisions
        sendMeshMessage(packet.fromID, defaultHopCount, "PONG");
    }
    // Check if message is PONG response
    else if (packet.data.equalsIgnoreCase("PONG") && waitingForPong) {
        Serial.print("[PONG] Received from Node ");
        Serial.print(packet.fromID);
        Serial.print(" (RSSI: ");
        Serial.print(lastRSSI);
        Serial.print(" dBm, RTT: ");
        Serial.print(millis() - lastPingTime);
        Serial.println(" ms)");
        waitingForPong = false;
    }
}

// =============================
// MESSAGE RECEPTION
// =============================
void receiveLoRaMessage() {
    // Use byte array instead of String to avoid truncation
    uint8_t buffer[256];
    int state = radio.receive(buffer, 256);

    if (state == RADIOLIB_ERR_NONE) {
        size_t len = radio.getPacketLength();
        // Convert byte array to String
        String msg = "";
        for (size_t i = 0; i < len; i++) {
            msg += (char)buffer[i];
        }
        
        // Get RSSI
        lastRSSI = radio.getRSSI();
        
        // DEBUG: Show what radio.receive() returned
        Serial.print("[DEBUG] Radio received (length=");
        Serial.print(msg.length());
        Serial.print("): ");
        Serial.println(msg);
        
        // Parse mesh packet
        MeshPacket packet;
        if (parseMeshPacket(msg, packet)) {
            lastSenderID = packet.fromID;
            
            Serial.print("[DEBUG] Parsed - FromID: ");
            Serial.print(packet.fromID);
            Serial.print(", ToID: ");
            Serial.print(packet.toID);
            Serial.print(", Hops: ");
            Serial.print(packet.hopCount);
            Serial.print(", Data length: ");
            Serial.println(packet.data.length());
            
            // Check if message is for us or broadcast
            if (packet.toID == 0 || packet.toID == nodeID) {
                // Handle PING/PONG first
                handlePingPong(packet);
                
                // Display regular messages (not PING/PONG system messages)
                if (!packet.data.equalsIgnoreCase("PING") && !packet.data.equalsIgnoreCase("PONG")) {
                    String displayMsg = packet.data;
                    bool isEncrypted = false;
                    bool decryptSuccess = false;
                    
                    // DEBUG: Show raw data
                    Serial.print("[DEBUG] Raw data: ");
                    Serial.println(packet.data);
                    
                    // Check if message is encrypted (starts with FLAG:)
                    if (packet.data.startsWith("FLAG:")) {
                        isEncrypted = true;
                        Serial.println("[DEBUG] Detected FLAG message");
                        
                        // Parse FLAG:<SenderID>:<Base64Blob>
                        int firstColon = packet.data.indexOf(':', 5); // Skip "FLAG:"
                        Serial.print("[DEBUG] First colon at: ");
                        Serial.println(firstColon);
                        
                        if (firstColon > 5) {
                            String senderInfo = packet.data.substring(5, firstColon);
                            String base64Blob = packet.data.substring(firstColon + 1);
                            
                            Serial.print("[DEBUG] Sender: ");
                            Serial.println(senderInfo);
                            Serial.print("[DEBUG] Base64 length: ");
                            Serial.println(base64Blob.length());
                            Serial.print("[DEBUG] Key set: ");
                            Serial.println(isEncryptionKeySet() ? "YES" : "NO");
                            
                            if (isEncryptionKeySet()) {
                                String decrypted;
                                if (decodeAndDecrypt(base64Blob, decrypted)) {
                                    displayMsg = decrypted;
                                    decryptSuccess = true;
                                    Serial.println("[DEBUG] Decryption SUCCESS");
                                } else {
                                    displayMsg = "[ENCRYPTED - AUTH FAIL / KEY MISMATCH]";
                                    Serial.println("[DEBUG] Decryption FAILED");
                                }
                            } else {
                                displayMsg = "[ENCRYPTED - NO KEY SET]";
                                Serial.println("[DEBUG] No key set");
                            }
                        } else {
                            displayMsg = "[ENCRYPTED - MALFORMED]";
                            Serial.println("[DEBUG] Malformed FLAG message");
                        }
                    }
                    
                    Serial.println();
                    Serial.println("========================================");
                    Serial.print("[RX] From Node ");
                    Serial.print(packet.fromID);
                    Serial.print(" (RSSI: ");
                    Serial.print(lastRSSI);
                    Serial.print(" dBm, Hops: ");
                    Serial.print(packet.hopCount);
                    Serial.println(")");
                    
                    if (isEncrypted) {
                        if (decryptSuccess) {
                            Serial.print("[MSG] [ENCRYPTED - OK] ");
                        } else {
                            Serial.print("[MSG] ");
                        }
                    } else {
                        Serial.print("[MSG] ");
                    }
                    Serial.println(displayMsg);
                    Serial.println("========================================");
                    
                    lastReceivedMsg = displayMsg;
                    messagesReceived++;
                    scrollOffset = 0; // Reset scroll
                    
                    updateDisplay();
                }
            }
            
            // Rebroadcast if hop count > 0 and not from us
            if (packet.hopCount > 0 && packet.fromID != nodeID) {
                delay(random(50, 150)); // Random delay to avoid collisions
                
                Serial.print("[MESH] Rebroadcasting (hops: ");
                Serial.print(packet.hopCount - 1);
                Serial.println(")");
                
                String rebroadcastPacket = buildMeshPacket(packet.toID, 
                                                           packet.hopCount - 1, 
                                                           packet.data);
                radio.transmit(rebroadcastPacket);
            }
        } else {
            // Invalid packet format
            Serial.print("[RX] Invalid packet format: ");
            Serial.println(msg);
        }
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // Timeout is normal, just continue listening
    } else if (state != RADIOLIB_ERR_CRC_MISMATCH) {
        // Only print error if it's not a common CRC mismatch
        Serial.print("[RX] Error, code: ");
        Serial.println(state);
    }
}

// =============================
// PING COMMAND
// =============================
void sendPing() {
    Serial.println("[PING] Broadcasting PING request...");
    waitingForPong = true;
    lastPingTime = millis();
    sendMeshMessage(0, defaultHopCount, "PING");
    Serial.println("[PING] Waiting for PONG responses (5s timeout)...");
}

// Check for ping timeout
void checkPingTimeout() {
    if (waitingForPong && (millis() - lastPingTime > PING_TIMEOUT)) {
        Serial.println("[PING] Timeout - no responses received");
        waitingForPong = false;
    }
}

#endif // MESH_NETWORK_H
