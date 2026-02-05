/*
 * crypt.h
 * AES-256-GCM Encryption for Mesh Messages
 * - Key derivation from passphrase (SHA-256)
 * - Persistent key storage in NVS (Preferences)
 * - Encrypt/Decrypt with authentication tag
 * - Base64 encoding for binary payload
 */

#ifndef CRYPT_H
#define CRYPT_H

#include <Arduino.h>
#include <Preferences.h>
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

// =============================
// CRYPTO CONFIGURATION
// =============================
#define AES_KEY_SIZE 32          // AES-256 uses 32 bytes
#define GCM_IV_SIZE 12           // 12 bytes for GCM IV (recommended)
#define GCM_TAG_SIZE 16          // 16 bytes authentication tag
#define MAX_PLAINTEXT_SIZE 200   // Max plaintext message size
#define MAX_ENCRYPTED_SIZE 400   // Max encrypted payload size (Base64 encoded)

// =============================
// GLOBAL STATE
// =============================
static uint8_t encryptionKey[AES_KEY_SIZE];
static bool encryptionKeySet = false;
static Preferences prefs;

// Random number generator context
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_entropy_context entropy;
static bool rngInitialized = false;

// =============================
// RANDOM NUMBER GENERATOR INIT
// =============================
void initRNG() {
    if (!rngInitialized) {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        
        const char *pers = "esp32_lora_mesh";
        int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                         (const unsigned char *)pers, strlen(pers));
        if (ret == 0) {
            rngInitialized = true;
        } else {
            Serial.println("[E] RNG initialization failed!");
        }
    }
}

// =============================
// GENERATE RANDOM IV
// =============================
void generateRandomIV(uint8_t *iv, size_t len) {
    initRNG();
    if (rngInitialized) {
        mbedtls_ctr_drbg_random(&ctr_drbg, iv, len);
    } else {
        // Fallback to ESP32 hardware RNG
        for (size_t i = 0; i < len; i++) {
            iv[i] = (uint8_t)esp_random();
        }
    }
}

// =============================
// KEY DERIVATION (SHA-256)
// =============================
void deriveKeyFromPassphrase(const String &passphrase, uint8_t *key) {
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 0 = SHA-256 (not SHA-224)
    mbedtls_sha256_update(&sha256_ctx, (const unsigned char *)passphrase.c_str(), passphrase.length());
    mbedtls_sha256_finish(&sha256_ctx, key);
    mbedtls_sha256_free(&sha256_ctx);
}

// =============================
// KEY FINGERPRINT (for display)
// =============================
String getKeyFingerprint() {
    if (!encryptionKeySet) return "NO KEY";
    
    // Show first 6 bytes as hex (xx:xx:xx:xx:xx:xx)
    String fp = "";
    for (int i = 0; i < 6; i++) {
        if (i > 0) fp += ":";
        if (encryptionKey[i] < 0x10) fp += "0";
        fp += String(encryptionKey[i], HEX);
    }
    fp.toUpperCase();
    return fp;
}

// =============================
// SET ENCRYPTION KEY FROM PASSPHRASE
// =============================
bool setEncryptionKeyFromPassphrase(const String &passphrase) {
    if (passphrase.length() == 0) {
        Serial.println("[E] Passphrase cannot be empty");
        return false;
    }
    
    // Derive 256-bit key from passphrase
    deriveKeyFromPassphrase(passphrase, encryptionKey);
    encryptionKeySet = true;
    
    // Save to NVS (Preferences)
    prefs.begin("mesh_crypto", false); // namespace: mesh_crypto, readOnly: false
    prefs.putBytes("aes256", encryptionKey, AES_KEY_SIZE);
    prefs.end();
    
    Serial.println("[E] Encryption key set and saved to NVS");
    Serial.print("[E] Key fingerprint: ");
    Serial.println(getKeyFingerprint());
    
    return true;
}

// =============================
// LOAD ENCRYPTION KEY FROM NVS
// =============================
bool loadEncryptionKey() {
    prefs.begin("mesh_crypto", true); // readOnly: true
    size_t len = prefs.getBytesLength("aes256");
    
    if (len == AES_KEY_SIZE) {
        prefs.getBytes("aes256", encryptionKey, AES_KEY_SIZE);
        encryptionKeySet = true;
        prefs.end();
        
        Serial.println("[E] Encryption key loaded from NVS");
        Serial.print("[E] Key fingerprint: ");
        Serial.println(getKeyFingerprint());
        return true;
    } else {
        encryptionKeySet = false;
        prefs.end();
        Serial.println("[E] No encryption key found in NVS");
        return false;
    }
}

// =============================
// CLEAR ENCRYPTION KEY
// =============================
void clearEncryptionKey() {
    // Clear from memory
    memset(encryptionKey, 0, AES_KEY_SIZE);
    encryptionKeySet = false;
    
    // Clear from NVS
    prefs.begin("mesh_crypto", false);
    prefs.remove("aes256");
    prefs.end();
    
    Serial.println("[E] Encryption key cleared from memory and NVS");
}

// =============================
// CHECK IF KEY IS SET
// =============================
bool isEncryptionKeySet() {
    return encryptionKeySet;
}

// =============================
// ENCRYPT AND ENCODE (AES-256-GCM + Base64)
// =============================
String encryptAndEncode(const String &plaintext) {
    if (!encryptionKeySet) {
        Serial.println("[E] Cannot encrypt: no key set");
        return "";
    }
    
    if (plaintext.length() > MAX_PLAINTEXT_SIZE) {
        Serial.println("[E] Message too long for encryption");
        return "";
    }
    
    // Generate random IV
    uint8_t iv[GCM_IV_SIZE];
    generateRandomIV(iv, GCM_IV_SIZE);
    
    // Prepare buffers
    size_t plaintextLen = plaintext.length();
    uint8_t *ciphertext = (uint8_t *)malloc(plaintextLen);
    uint8_t tag[GCM_TAG_SIZE];
    
    if (!ciphertext) {
        Serial.println("[E] Memory allocation failed");
        return "";
    }
    
    // Initialize GCM context
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, encryptionKey, AES_KEY_SIZE * 8);
    if (ret != 0) {
        Serial.print("[E] GCM setkey failed: ");
        Serial.println(ret);
        free(ciphertext);
        mbedtls_gcm_free(&gcm);
        return "";
    }
    
    // Encrypt
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintextLen,
                                     iv, GCM_IV_SIZE,
                                     NULL, 0,  // No additional authenticated data
                                     (const unsigned char *)plaintext.c_str(), ciphertext,
                                     GCM_TAG_SIZE, tag);
    
    mbedtls_gcm_free(&gcm);
    
    if (ret != 0) {
        Serial.print("[E] GCM encryption failed: ");
        Serial.println(ret);
        free(ciphertext);
        return "";
    }
    
    // Build binary payload: IV || Ciphertext || Tag
    size_t binaryLen = GCM_IV_SIZE + plaintextLen + GCM_TAG_SIZE;
    uint8_t *binary = (uint8_t *)malloc(binaryLen);
    if (!binary) {
        Serial.println("[E] Memory allocation failed");
        free(ciphertext);
        return "";
    }
    
    memcpy(binary, iv, GCM_IV_SIZE);
    memcpy(binary + GCM_IV_SIZE, ciphertext, plaintextLen);
    memcpy(binary + GCM_IV_SIZE + plaintextLen, tag, GCM_TAG_SIZE);
    
    free(ciphertext);
    
    // Base64 encode
    size_t base64Len = 0;
    mbedtls_base64_encode(NULL, 0, &base64Len, binary, binaryLen); // Get required length
    
    char *base64 = (char *)malloc(base64Len + 1);
    if (!base64) {
        Serial.println("[E] Memory allocation failed");
        free(binary);
        return "";
    }
    
    ret = mbedtls_base64_encode((unsigned char *)base64, base64Len, &base64Len, binary, binaryLen);
    free(binary);
    
    if (ret != 0) {
        Serial.print("[E] Base64 encoding failed: ");
        Serial.println(ret);
        free(base64);
        return "";
    }
    
    base64[base64Len] = '\0';
    String result = String(base64);
    free(base64);
    
    return result;
}

// =============================
// DECODE AND DECRYPT (Base64 + AES-256-GCM)
// =============================
bool decodeAndDecrypt(const String &base64Blob, String &outPlaintext) {
    if (!encryptionKeySet) {
        Serial.println("[E] Cannot decrypt: no key set");
        return false;
    }
    
    // Base64 decode
    size_t binaryLen = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &binaryLen, 
                                      (const unsigned char *)base64Blob.c_str(), 
                                      base64Blob.length());
    
    if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        Serial.println("[E] Invalid Base64 data");
        return false;
    }
    
    uint8_t *binary = (uint8_t *)malloc(binaryLen);
    if (!binary) {
        Serial.println("[E] Memory allocation failed");
        return false;
    }
    
    ret = mbedtls_base64_decode(binary, binaryLen, &binaryLen,
                                 (const unsigned char *)base64Blob.c_str(),
                                 base64Blob.length());
    
    if (ret != 0) {
        Serial.print("[E] Base64 decode failed: ");
        Serial.println(ret);
        free(binary);
        return false;
    }
    
    // Check minimum length: IV + Tag
    if (binaryLen < (GCM_IV_SIZE + GCM_TAG_SIZE)) {
        Serial.println("[E] Encrypted payload too short");
        free(binary);
        return false;
    }
    
    // Extract components: IV || Ciphertext || Tag
    uint8_t *iv = binary;
    size_t ciphertextLen = binaryLen - GCM_IV_SIZE - GCM_TAG_SIZE;
    uint8_t *ciphertext = binary + GCM_IV_SIZE;
    uint8_t *tag = binary + GCM_IV_SIZE + ciphertextLen;
    
    // Prepare plaintext buffer
    uint8_t *plaintext = (uint8_t *)malloc(ciphertextLen + 1);
    if (!plaintext) {
        Serial.println("[E] Memory allocation failed");
        free(binary);
        return false;
    }
    
    // Initialize GCM context
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, encryptionKey, AES_KEY_SIZE * 8);
    if (ret != 0) {
        Serial.print("[E] GCM setkey failed: ");
        Serial.println(ret);
        free(binary);
        free(plaintext);
        mbedtls_gcm_free(&gcm);
        return false;
    }
    
    // Decrypt and verify tag
    ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLen,
                                    iv, GCM_IV_SIZE,
                                    NULL, 0,  // No additional authenticated data
                                    tag, GCM_TAG_SIZE,
                                    ciphertext, plaintext);
    
    mbedtls_gcm_free(&gcm);
    free(binary);
    
    if (ret != 0) {
        Serial.print("[E] GCM decryption/auth failed: ");
        Serial.println(ret);
        free(plaintext);
        return false;
    }
    
    // Convert to String
    plaintext[ciphertextLen] = '\0';
    outPlaintext = String((char *)plaintext);
    free(plaintext);
    
    return true;
}

// =============================
// ENCRYPTION INFO
// =============================
void printEncryptionInfo() {
    Serial.println("========================================");
    Serial.println("  ENCRYPTION STATUS");
    Serial.println("========================================");
    Serial.print("Algorithm:     AES-256-GCM\n");
    Serial.print("Key Size:      256 bits (32 bytes)\n");
    Serial.print("IV Size:       96 bits (12 bytes)\n");
    Serial.print("Tag Size:      128 bits (16 bytes)\n");
    Serial.print("Key Status:    ");
    if (encryptionKeySet) {
        Serial.println("SET");
        Serial.print("Fingerprint:   ");
        Serial.println(getKeyFingerprint());
    } else {
        Serial.println("NOT SET");
    }
    Serial.println("========================================");
}

#endif // CRYPT_H
