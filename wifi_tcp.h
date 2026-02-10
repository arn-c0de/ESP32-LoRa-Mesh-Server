/*
 * wifi_tcp.h
 * WiFi / TCP Client for HomeServer Connection
 * - WiFi connection with automatic reconnect (exponential backoff)
 * - TCP client with circular buffer for offline queuing
 * - EEPROM persistence for WiFi/TCP settings
 * - Heartbeat to keep connection alive
 */

#ifndef WIFI_TCP_H
#define WIFI_TCP_H

#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>

// External references (defined in main .ino)
extern uint8_t nodeID;
extern String deviceName;

// =============================
// COMPILE-TIME DEFAULTS
// =============================
#ifdef WIFI_SSID
  static const char *DEFAULT_WIFI_SSID = WIFI_SSID;
#else
  static const char *DEFAULT_WIFI_SSID = "";
#endif

#ifdef WIFI_PASS
  static const char *DEFAULT_WIFI_PASS = WIFI_PASS;
#else
  static const char *DEFAULT_WIFI_PASS = "";
#endif

#ifdef SERVER_IP
  static const char *DEFAULT_SERVER_IP = SERVER_IP;
#else
  static const char *DEFAULT_SERVER_IP = "192.168.1.100";
#endif

#ifdef SERVER_PORT
  static const uint16_t DEFAULT_SERVER_PORT = SERVER_PORT;
#else
  static const uint16_t DEFAULT_SERVER_PORT = 5001;
#endif

#ifdef TCP_ENABLED
  static const bool DEFAULT_TCP_ENABLED = (TCP_ENABLED != 0);
#else
  static const bool DEFAULT_TCP_ENABLED = false;
#endif

#ifdef TCP_SHARED_SECRET
  static const char *DEFAULT_TCP_SHARED_SECRET = TCP_SHARED_SECRET;
#else
  static const char *DEFAULT_TCP_SHARED_SECRET = "";
#endif

// =============================
// EEPROM ADDRESSES (Bytes 47-162)
// =============================
// Bytes 0-46: Existing (init flag, device name, nodeID, freq, power, BW, SF, CR, hops)
#define EEPROM_WIFI_SSID_LEN_ADDR   47   // Byte 47: WiFi SSID length
#define EEPROM_WIFI_SSID_DATA_ADDR  48   // Bytes 48-79: WiFi SSID data (32 bytes)
#define EEPROM_WIFI_SSID_MAX_LEN    32

#define EEPROM_WIFI_PASS_LEN_ADDR   80   // Byte 80: WiFi password length
#define EEPROM_WIFI_PASS_DATA_ADDR  81   // Bytes 81-143: WiFi password data (63 bytes)
#define EEPROM_WIFI_PASS_MAX_LEN    63

#define EEPROM_SERVER_IP_LEN_ADDR   144  // Byte 144: Server IP length
#define EEPROM_SERVER_IP_DATA_ADDR  145  // Bytes 145-159: Server IP data (15 bytes)
#define EEPROM_SERVER_IP_MAX_LEN    15

#define EEPROM_SERVER_PORT_ADDR     160  // Bytes 160-161: Server port (uint16_t)
#define EEPROM_TCP_ENABLED_ADDR     162  // Byte 162: TCP enabled flag
#define EEPROM_SERVER_PORT_VALID_ADDR 163 // Byte 163: Server port valid flag (1 = set)
#define EEPROM_TCP_SECRET_LEN_ADDR  164  // Byte 164: TCP shared secret length
#define EEPROM_TCP_SECRET_DATA_ADDR 165  // Bytes 165-196: TCP shared secret data (32 bytes)
#define EEPROM_TCP_SECRET_MAX_LEN   32

// =============================
// RUNTIME STATE
// =============================
bool wifiConnected = false;
bool tcpConnected = false;
bool tcpEnabled = false;
bool wifiAutoReconnect = true;

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == SYSTEM_EVENT_STA_DISCONNECTED) {
        Serial.print("[WiFi] Disconnected, reason: ");
        Serial.println(info.wifi_sta_disconnected.reason);
    }
}

String wifiSSID = "";
String wifiPassword = "";
String serverIP = "";
uint16_t serverPort = 5001;
String tcpAuthToken = "";

WiFiClient tcpClient;

// Reconnect backoff
uint32_t lastWiFiAttempt = 0;
uint32_t wifiBackoff = 5000;          // Start at 5s, max 60s
const uint32_t WIFI_BACKOFF_MAX = 60000;

uint32_t lastTCPAttempt = 0;
uint32_t tcpBackoff = 3000;           // Start at 3s, max 30s
const uint32_t TCP_BACKOFF_MAX = 30000;

// Heartbeat
uint32_t lastHeartbeat = 0;
const uint32_t HEARTBEAT_INTERVAL = 30000; // 30 seconds

// =============================
// CIRCULAR BUFFER (offline queue)
// =============================
#define TCP_BUFFER_SIZE 50
String tcpBuffer[TCP_BUFFER_SIZE];
uint8_t tcpBufferHead = 0;
uint8_t tcpBufferTail = 0;
uint8_t tcpBufferCount = 0;

void tcpBufferPush(const String &msg) {
    tcpBuffer[tcpBufferHead] = msg;
    tcpBufferHead = (tcpBufferHead + 1) % TCP_BUFFER_SIZE;
    if (tcpBufferCount < TCP_BUFFER_SIZE) {
        tcpBufferCount++;
    } else {
        // Overwrite oldest
        tcpBufferTail = (tcpBufferTail + 1) % TCP_BUFFER_SIZE;
    }
}

String tcpBufferPop() {
    if (tcpBufferCount == 0) return "";
    String msg = tcpBuffer[tcpBufferTail];
    tcpBufferTail = (tcpBufferTail + 1) % TCP_BUFFER_SIZE;
    tcpBufferCount--;
    return msg;
}

// =============================
// EEPROM LOAD/SAVE FUNCTIONS
// =============================
void loadWiFiSettings() {
    // Load WiFi SSID
    uint8_t len = EEPROM.read(EEPROM_WIFI_SSID_LEN_ADDR);
    if (len > 0 && len <= EEPROM_WIFI_SSID_MAX_LEN) {
        wifiSSID = "";
        for (uint8_t i = 0; i < len; i++) {
            wifiSSID += (char)EEPROM.read(EEPROM_WIFI_SSID_DATA_ADDR + i);
        }
    } else {
        wifiSSID = DEFAULT_WIFI_SSID;
    }

    // Load WiFi Password
    len = EEPROM.read(EEPROM_WIFI_PASS_LEN_ADDR);
    if (len > 0 && len <= EEPROM_WIFI_PASS_MAX_LEN) {
        wifiPassword = "";
        for (uint8_t i = 0; i < len; i++) {
            wifiPassword += (char)EEPROM.read(EEPROM_WIFI_PASS_DATA_ADDR + i);
        }
    } else {
        wifiPassword = DEFAULT_WIFI_PASS;
    }

    // Load Server IP
    len = EEPROM.read(EEPROM_SERVER_IP_LEN_ADDR);
    if (len > 0 && len <= EEPROM_SERVER_IP_MAX_LEN) {
        serverIP = "";
        for (uint8_t i = 0; i < len; i++) {
            serverIP += (char)EEPROM.read(EEPROM_SERVER_IP_DATA_ADDR + i);
        }
    } else {
        serverIP = DEFAULT_SERVER_IP;
    }

    // Load Server Port
    uint16_t port;
    EEPROM.get(EEPROM_SERVER_PORT_ADDR, port);
    // Use explicit validity flag to avoid 0xFFFF ambiguity
    uint8_t portValid = EEPROM.read(EEPROM_SERVER_PORT_VALID_ADDR);
    if (portValid == 1 && port != 0) {
        serverPort = port;
    } else {
        serverPort = DEFAULT_SERVER_PORT;
    }

    // Load TCP Enabled flag
    uint8_t flag = EEPROM.read(EEPROM_TCP_ENABLED_ADDR);
    if (flag == 1) {
        tcpEnabled = true;
    } else if (flag == 0) {
        tcpEnabled = false;
    } else {
        tcpEnabled = DEFAULT_TCP_ENABLED;
    }

    // Load TCP shared secret
    len = EEPROM.read(EEPROM_TCP_SECRET_LEN_ADDR);
    if (len > 0 && len <= EEPROM_TCP_SECRET_MAX_LEN) {
        tcpAuthToken = "";
        for (uint8_t i = 0; i < len; i++) {
            tcpAuthToken += (char)EEPROM.read(EEPROM_TCP_SECRET_DATA_ADDR + i);
        }
    } else {
        tcpAuthToken = DEFAULT_TCP_SHARED_SECRET;
    }
}

void saveWiFiSSID(const String &ssid) {
    String s = ssid.substring(0, EEPROM_WIFI_SSID_MAX_LEN);
    EEPROM.write(EEPROM_WIFI_SSID_LEN_ADDR, s.length());
    for (uint8_t i = 0; i < s.length(); i++) {
        EEPROM.write(EEPROM_WIFI_SSID_DATA_ADDR + i, s[i]);
    }
    EEPROM.commit();
    wifiSSID = s;
}

void saveWiFiPassword(const String &pass) {
    String p = pass.substring(0, EEPROM_WIFI_PASS_MAX_LEN);
    EEPROM.write(EEPROM_WIFI_PASS_LEN_ADDR, p.length());
    for (uint8_t i = 0; i < p.length(); i++) {
        EEPROM.write(EEPROM_WIFI_PASS_DATA_ADDR + i, p[i]);
    }
    EEPROM.commit();
    wifiPassword = p;
}

void saveServerIP(const String &ip) {
    String s = ip.substring(0, EEPROM_SERVER_IP_MAX_LEN);
    EEPROM.write(EEPROM_SERVER_IP_LEN_ADDR, s.length());
    for (uint8_t i = 0; i < s.length(); i++) {
        EEPROM.write(EEPROM_SERVER_IP_DATA_ADDR + i, s[i]);
    }
    EEPROM.commit();
    serverIP = s;
}

void saveServerPort(uint16_t port) {
    EEPROM.put(EEPROM_SERVER_PORT_ADDR, port);
    EEPROM.write(EEPROM_SERVER_PORT_VALID_ADDR, 1);
    EEPROM.commit();
    serverPort = port;
}

void saveTCPAuthToken(const String &token) {
    String t = token.substring(0, EEPROM_TCP_SECRET_MAX_LEN);
    EEPROM.write(EEPROM_TCP_SECRET_LEN_ADDR, t.length());
    for (uint8_t i = 0; i < t.length(); i++) {
        EEPROM.write(EEPROM_TCP_SECRET_DATA_ADDR + i, t[i]);
    }
    EEPROM.commit();
    tcpAuthToken = t;
}

void saveTCPEnabled(bool enabled) {
    EEPROM.write(EEPROM_TCP_ENABLED_ADDR, enabled ? 1 : 0);
    EEPROM.commit();
    tcpEnabled = enabled;
}

// =============================
// WIFI CONNECTION
// =============================
void connectWiFi() {
    if (wifiSSID.length() == 0) {
        Serial.println("[WiFi] No SSID configured");
        return;
    }

    Serial.print("[WiFi] SSID len: ");
    Serial.print(wifiSSID.length());
    Serial.print(", PASS len: ");
    Serial.println(wifiPassword.length());

    Serial.print("[WiFi] Connecting to ");
    Serial.print(wifiSSID);
    Serial.println("...");

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    lastWiFiAttempt = millis();
}

const char *wifiStatusToString(wl_status_t s) {
    switch (s) {
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_DONE";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

void handleWiFiReconnect() {
    if (!wifiAutoReconnect) return;
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            wifiBackoff = 5000; // Reset backoff
            Serial.print("[WiFi] Connected! IP: ");
            Serial.println(WiFi.localIP());
        }
        return;
    }

    // Was connected, now disconnected
    if (wifiConnected) {
        wifiConnected = false;
        tcpConnected = false;
        Serial.println("[WiFi] Disconnected");
    }

    // Exponential backoff reconnect
    if (millis() - lastWiFiAttempt >= wifiBackoff) {
        // Quick scan to verify SSID visibility (not on every loop)
        if (wifiBackoff == 5000) {
            Serial.println("[WiFi] Scanning...");
            int n = WiFi.scanNetworks();
            if (n <= 0) {
                Serial.println("[WiFi] Scan: no networks found");
            } else {
                bool found = false;
                for (int i = 0; i < n; i++) {
                    String s = WiFi.SSID(i);
                    if (s == wifiSSID) found = true;
                }
                Serial.print("[WiFi] Scan: SSID ");
                Serial.println(found ? "FOUND" : "NOT FOUND");
            }
            WiFi.scanDelete();
        }

        Serial.print("[WiFi] Status: ");
        Serial.println(wifiStatusToString(WiFi.status()));
        connectWiFi();
        wifiBackoff = min(wifiBackoff * 2, WIFI_BACKOFF_MAX);
    }
}

// =============================
// TCP CONNECTION
// =============================
void connectTCP() {
    if (!wifiConnected || serverIP.length() == 0) return;

    Serial.print("[TCP] Connecting to ");
    Serial.print(serverIP);
    Serial.print(":");
    Serial.println(serverPort);

    if (tcpClient.connect(serverIP.c_str(), serverPort)) {
        tcpConnected = true;
        tcpBackoff = 3000; // Reset backoff
        Serial.println("[TCP] Connected!");

        // Send HELLO message (optional shared secret)
        String hello = "HELLO:" + String(nodeID) + ":" + deviceName;
        if (tcpAuthToken.length() > 0) {
            hello += ":" + tcpAuthToken;
        }
        hello += "\n";
        tcpClient.print(hello);
        Serial.print("[TCP] Sent: ");
        Serial.print(hello);

        // Drain offline buffer
        while (tcpBufferCount > 0) {
            String buffered = tcpBufferPop();
            tcpClient.print(buffered + "\n");
            Serial.print("[TCP] Drained buffer: ");
            Serial.println(buffered);
        }
    } else {
        Serial.println("[TCP] Connection failed");
    }

    lastTCPAttempt = millis();
}

void handleTCPReconnect() {
    if (!wifiConnected) {
        if (tcpConnected) {
            tcpConnected = false;
            tcpClient.stop();
        }
        return;
    }

    if (tcpClient.connected()) {
        if (!tcpConnected) {
            tcpConnected = true;
            tcpBackoff = 3000;
        }
        return;
    }

    // Was connected, now disconnected
    if (tcpConnected) {
        tcpConnected = false;
        Serial.println("[TCP] Disconnected");
    }

    // Exponential backoff reconnect
    if (millis() - lastTCPAttempt >= tcpBackoff) {
        connectTCP();
        tcpBackoff = min(tcpBackoff * 2, TCP_BACKOFF_MAX);
    }
}

// =============================
// SEND MESSAGE VIA TCP
// =============================
void sendMessageTCP(const String &rawPacket) {
    if (!tcpEnabled) return;

    if (tcpConnected && tcpClient.connected()) {
        tcpClient.print(rawPacket + "\n");
    } else {
        // Buffer for later
        tcpBufferPush(rawPacket);
    }
}

// =============================
// HEARTBEAT
// =============================
void sendHeartbeat() {
    if (!tcpConnected || !tcpClient.connected()) return;

    String hb = "HEARTBEAT:" + String(nodeID) + "\n";
    tcpClient.print(hb);
}

// =============================
// READ SERVER MESSAGES
// =============================
void readServerMessages() {
    if (!tcpConnected || !tcpClient.connected()) return;

    while (tcpClient.available()) {
        String line = tcpClient.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            Serial.print("[TCP-RX] Server: ");
            Serial.println(line);
        }
    }
}

// =============================
// MAIN HANDLER (call from loop)
// =============================
void handleTCP() {
    if (!tcpEnabled) return;

    handleWiFiReconnect();
    handleTCPReconnect();

    // Heartbeat
    if (tcpConnected && (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL)) {
        sendHeartbeat();
        lastHeartbeat = millis();
    }

    // Read any server messages
    readServerMessages();
}

// =============================
// SETUP (call from setup)
// =============================
void setupWiFi() {
    loadWiFiSettings();
    WiFi.onEvent(onWiFiEvent);

    Serial.println("[WiFi] Configuration:");
    Serial.print("  SSID:    ");
    Serial.println(wifiSSID.length() > 0 ? wifiSSID : "(not set)");
    Serial.print("  Server:  ");
    Serial.print(serverIP);
    Serial.print(":");
    Serial.println(serverPort);
    Serial.print("  TCP:     ");
    Serial.println(tcpEnabled ? "ENABLED" : "DISABLED");

    if (tcpEnabled && wifiSSID.length() > 0) {
        connectWiFi();
    }
}

void setWiFiAutoReconnect(bool enabled) {
    wifiAutoReconnect = enabled;
    if (!enabled) {
        WiFi.disconnect(true);
        wifiConnected = false;
        tcpConnected = false;
    } else if (tcpEnabled && wifiSSID.length() > 0) {
        connectWiFi();
    }
}

// =============================
// WIFI DIAGNOSTICS (manual)
// =============================
void wifiTest() {
    Serial.println("========================================");
    Serial.println("  WIFI TEST");
    Serial.println("========================================");
    Serial.print("SSID:     ");
    Serial.println(wifiSSID.length() > 0 ? wifiSSID : "(not set)");
    Serial.print("PASS len: ");
    Serial.println(wifiPassword.length());

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(true);
    delay(200);

    Serial.println("[WiFi] Scanning...");
    int n = WiFi.scanNetworks();
    int targetChannel = 0;
    uint8_t targetBssid[6] = {0};
    bool haveTarget = false;
    if (n <= 0) {
        Serial.println("[WiFi] Scan: no networks found");
    } else {
        bool found = false;
        int shown = 0;
        Serial.println("[WiFi] Scan results (top 10):");
        for (int i = 0; i < n && shown < 10; i++) {
            String s = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            wifi_auth_mode_t auth = WiFi.encryptionType(i);
            int ch = WiFi.channel(i);
            Serial.print("  - ");
            Serial.print(s);
            Serial.print("  RSSI=");
            Serial.print(rssi);
            Serial.print("  CH=");
            Serial.print(ch);
            Serial.print("  AUTH=");
            Serial.println((int)auth);
            shown++;
            if (s == wifiSSID) {
                found = true;
                if (!haveTarget) {
                    const uint8_t *b = WiFi.BSSID(i);
                    if (b) {
                        memcpy(targetBssid, b, 6);
                        targetChannel = ch;
                        haveTarget = true;
                    }
                }
            }
        }
        Serial.print("[WiFi] Scan: SSID ");
        Serial.println(found ? "FOUND" : "NOT FOUND");
    }
    WiFi.scanDelete();

    if (wifiSSID.length() == 0) {
        Serial.println("[WiFi] No SSID configured");
        Serial.println("========================================");
        return;
    }

    Serial.println("[WiFi] Connecting (10s timeout)...");
    if (haveTarget) {
        Serial.print("[WiFi] Using BSSID ");
        char bssidStr[18];
        snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 targetBssid[0], targetBssid[1], targetBssid[2],
                 targetBssid[3], targetBssid[4], targetBssid[5]);
        Serial.print(bssidStr);
        Serial.print(" on CH ");
        Serial.println(targetChannel);
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str(), targetChannel, targetBssid, true);
    } else {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    }
    uint32_t start = millis();
    while (millis() - start < 10000) {
        wl_status_t s = WiFi.status();
        if (s == WL_CONNECTED) {
            Serial.print("[WiFi] CONNECTED, IP: ");
            Serial.println(WiFi.localIP());
            Serial.println("========================================");
            return;
        }
        delay(500);
    }

    Serial.print("[WiFi] Failed, status: ");
    Serial.println(wifiStatusToString(WiFi.status()));
    Serial.println("========================================");
}

// =============================
// STATUS DISPLAY
// =============================
void printTCPStatus() {
    Serial.println("========================================");
    Serial.println("  WiFi / TCP STATUS");
    Serial.println("========================================");
    Serial.print("TCP Enabled:   ");
    Serial.println(tcpEnabled ? "YES" : "NO");
    Serial.print("WiFi SSID:     ");
    Serial.println(wifiSSID.length() > 0 ? wifiSSID : "(not set)");
    Serial.print("WiFi Status:   ");
    if (wifiConnected) {
        Serial.print("CONNECTED (IP: ");
        Serial.print(WiFi.localIP());
        Serial.println(")");
    } else {
        Serial.println("DISCONNECTED");
    }
    Serial.print("Server:        ");
    Serial.print(serverIP);
    Serial.print(":");
    Serial.println(serverPort);
    Serial.print("TCP Status:    ");
    Serial.println(tcpConnected ? "CONNECTED" : "DISCONNECTED");
    Serial.print("Buffer:        ");
    Serial.print(tcpBufferCount);
    Serial.print("/");
    Serial.println(TCP_BUFFER_SIZE);
    Serial.println("========================================");
}

#endif // WIFI_TCP_H
